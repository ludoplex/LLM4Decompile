/*
 * LLM4Decompile - Evaluate Tool
 *
 * Evaluates decompiled C code by compiling and running it with test cases.
 * Measures re-executability rate for decompilation quality assessment.
 *
 * This tool replaces the Python evaluation scripts in the original
 * implementation with a portable C executable using Cosmopolitan Libc.
 *
 * SPDX-License-Identifier: MIT
 */

#include "common.h"
#include <getopt.h>
#include <unistd.h>

/* Default values */
#define DEFAULT_TIMEOUT 10
#define DEFAULT_NUM_WORKERS 4

/* Command-line options */
static struct option long_options[] = {
    {"decompiled",   required_argument, 0, 'd'},
    {"test",         required_argument, 0, 't'},
    {"original",     required_argument, 0, 'r'},
    {"output",       required_argument, 0, 'o'},
    {"timeout",      required_argument, 0, 'T'},
    {"verbose",      no_argument,       0, 'v'},
    {"help",         no_argument,       0, 'h'},
    {"version",      no_argument,       0, 'V'},
    {0, 0, 0, 0}
};

static bool verbose = false;

static void print_usage(const char *program) {
    printf("Usage: %s [OPTIONS] -d <decompiled.c> -t <test.c>\n\n", program);
    printf("Evaluate decompiled C code by compiling and running with tests.\n\n");
    printf("Options:\n");
    printf("  -d, --decompiled <file>  Decompiled C function file\n");
    printf("  -t, --test <file>        Test case file (with main() and assertions)\n");
    printf("  -r, --original <file>    Original C function file (optional, for includes)\n");
    printf("  -o, --output <file>      Output results file (optional)\n");
    printf("  -T, --timeout <sec>      Execution timeout in seconds (default: %d)\n", DEFAULT_TIMEOUT);
    printf("  -v, --verbose            Verbose output\n");
    printf("  -h, --help               Show this help message\n");
    printf("  -V, --version            Show version information\n");
    printf("\n");
    printf("Output:\n");
    printf("  Prints evaluation result: PASS (compiled and ran), COMPILE_FAIL, or RUN_FAIL\n");
    printf("  Exit code: 0 = PASS, 1 = COMPILE_FAIL, 2 = RUN_FAIL\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s -d decompiled_func.c -t test_cases.c -r original.c\n", program);
}

/*
 * Combine includes, decompiled function, and test code
 */
static char *combine_code(const char *decompiled, const char *test_code, const char *original) {
    /* Extract includes from all sources */
    char *dec_includes = extract_includes(decompiled);
    char *test_includes = extract_includes(test_code);
    char *orig_includes = original ? extract_includes(original) : strdup("");

    /* Remove includes from code */
    char *dec_code = remove_includes(decompiled);
    char *test_code_clean = remove_includes(test_code);

    /* Calculate total size */
    size_t total_size = strlen(dec_includes) + strlen(test_includes) +
                        strlen(orig_includes) + strlen(dec_code) +
                        strlen(test_code_clean) + 16;

    char *combined = malloc(total_size);
    if (!combined) {
        free(dec_includes);
        free(test_includes);
        free(orig_includes);
        free(dec_code);
        free(test_code_clean);
        return NULL;
    }

    /* Build combined code */
    snprintf(combined, total_size, "%s%s%s\n%s\n%s",
             orig_includes, dec_includes, test_includes, dec_code, test_code_clean);

    free(dec_includes);
    free(test_includes);
    free(orig_includes);
    free(dec_code);
    free(test_code_clean);

    return combined;
}

/*
 * Evaluate decompiled code
 */
static EvalResult evaluate(const char *decompiled, const char *test_code,
                           const char *original, int timeout) {
    EvalResult result = {false, false, -1, ""};
    char *temp_dir = NULL;
    char c_file[MAX_PATH_LEN];
    char exe_file[MAX_PATH_LEN];
    char cmd[MAX_PATH_LEN * 2];
    char *combined = NULL;

    /* Create temporary directory */
    temp_dir = create_temp_dir();
    if (!temp_dir) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to create temporary directory");
        return result;
    }

    /* Build file paths */
    snprintf(c_file, sizeof(c_file), "%s/test.c", temp_dir);
    snprintf(exe_file, sizeof(exe_file), "%s/test", temp_dir);

    /* Combine code */
    combined = combine_code(decompiled, test_code, original);
    if (!combined) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to combine code");
        goto cleanup;
    }

    /* Write combined code to file */
    if (write_file(c_file, combined, strlen(combined)) != RESULT_SUCCESS) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Failed to write test file");
        goto cleanup;
    }

    if (verbose) {
        print_info("Combined code written to: %s", c_file);
    }

    /* Compile the code */
    snprintf(cmd, sizeof(cmd), "gcc %s -o %s -lm 2>&1", c_file, exe_file);

    char compile_output[4096] = {0};
    int compile_ret = run_command(cmd, compile_output, sizeof(compile_output), timeout);

    if (compile_ret != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Compilation failed: %s", compile_output);
        if (verbose) {
            print_error("Compile error: %s", compile_output);
        }
        goto cleanup;
    }

    result.compiled = true;

    if (verbose) {
        print_info("Compilation successful");
    }

    /* Run the executable */
    snprintf(cmd, sizeof(cmd), "%s", exe_file);

    char run_output[4096] = {0};
    int run_ret = run_command(cmd, run_output, sizeof(run_output), timeout);

    result.exit_code = run_ret;

    if (run_ret == 0) {
        result.executed = true;
        if (verbose) {
            print_info("Execution successful");
        }
    } else if (run_ret == -2) {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Execution timed out after %d seconds", timeout);
        if (verbose) {
            print_error("Execution timed out");
        }
    } else {
        snprintf(result.error_msg, sizeof(result.error_msg),
                "Execution failed with exit code: %d", run_ret);
        if (verbose) {
            print_error("Execution failed: %s", run_output);
        }
    }

cleanup:
    free(combined);
    if (temp_dir) {
        remove_dir_recursive(temp_dir);
        free(temp_dir);
    }
    return result;
}

int main(int argc, char *argv[]) {
    const char *decompiled_file = NULL;
    const char *test_file = NULL;
    const char *original_file = NULL;
    const char *output_file = NULL;
    int timeout = DEFAULT_TIMEOUT;
    int opt;

    while ((opt = getopt_long(argc, argv, "d:t:r:o:T:vhV", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                decompiled_file = optarg;
                break;
            case 't':
                test_file = optarg;
                break;
            case 'r':
                original_file = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'T':
                timeout = atoi(optarg);
                if (timeout <= 0) timeout = DEFAULT_TIMEOUT;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'V':
                print_version("llm4decompile-evaluate");
                return 0;
            default:
                print_usage(argv[0]);
                return RESULT_ERROR_ARGS;
        }
    }

    /* Validate arguments */
    if (!decompiled_file || !test_file) {
        print_error("Decompiled file and test file are required");
        print_usage(argv[0]);
        return RESULT_ERROR_ARGS;
    }

    if (!file_exists(decompiled_file)) {
        print_error("Decompiled file not found: %s", decompiled_file);
        return RESULT_ERROR_NOT_FOUND;
    }

    if (!file_exists(test_file)) {
        print_error("Test file not found: %s", test_file);
        return RESULT_ERROR_NOT_FOUND;
    }

    /* Read input files */
    size_t dec_size, test_size, orig_size = 0;
    char *decompiled = read_file(decompiled_file, &dec_size);
    char *test_code = read_file(test_file, &test_size);
    char *original = NULL;

    if (!decompiled || !test_code) {
        print_error("Failed to read input files");
        free(decompiled);
        free(test_code);
        return RESULT_ERROR_IO;
    }

    if (original_file && file_exists(original_file)) {
        original = read_file(original_file, &orig_size);
    }

    /* Run evaluation */
    EvalResult result = evaluate(decompiled, test_code, original, timeout);

    free(decompiled);
    free(test_code);
    free(original);

    /* Print result */
    if (result.executed) {
        printf("PASS\n");
    } else if (result.compiled) {
        printf("RUN_FAIL\n");
        if (result.error_msg[0]) {
            fprintf(stderr, "Error: %s\n", result.error_msg);
        }
    } else {
        printf("COMPILE_FAIL\n");
        if (result.error_msg[0]) {
            fprintf(stderr, "Error: %s\n", result.error_msg);
        }
    }

    /* Write to output file if specified */
    if (output_file) {
        FILE *f = fopen(output_file, "a");
        if (f) {
            fprintf(f, "%s,%s,%s,%d,%d\n",
                    decompiled_file,
                    result.compiled ? "compiled" : "compile_fail",
                    result.executed ? "executed" : "run_fail",
                    result.exit_code,
                    timeout);
            fclose(f);
        }
    }

    /* Return appropriate exit code */
    if (result.executed) {
        return 0;
    } else if (result.compiled) {
        return 2;  /* RUN_FAIL */
    } else {
        return 1;  /* COMPILE_FAIL */
    }
}
