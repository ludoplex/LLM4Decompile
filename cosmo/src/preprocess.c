/*
 * LLM4Decompile - Preprocess Tool
 *
 * Compiles C source code to binary and extracts assembly instructions
 * for decompilation by LLM4Decompile models.
 *
 * This tool replaces the Python preprocessing scripts in the original
 * implementation with a portable C executable using Cosmopolitan Libc.
 *
 * SPDX-License-Identifier: MIT
 */

#include "common.h"
#include <getopt.h>
#include <unistd.h>

/* Default values */
#define DEFAULT_FUNC_NAME "func0"
#define DEFAULT_TIMEOUT 30

/* Command-line options */
static struct option long_options[] = {
    {"input",     required_argument, 0, 'i'},
    {"output",    required_argument, 0, 'o'},
    {"function",  required_argument, 0, 'f'},
    {"opt-level", required_argument, 0, 'O'},
    {"all-opts",  no_argument,       0, 'a'},
    {"timeout",   required_argument, 0, 't'},
    {"help",      no_argument,       0, 'h'},
    {"version",   no_argument,       0, 'V'},
    {0, 0, 0, 0}
};

static void print_usage(const char *program) {
    printf("Usage: %s [OPTIONS] -i <input.c> -o <output>\n\n", program);
    printf("Preprocess C source code for LLM4Decompile.\n");
    printf("Compiles to binary and extracts assembly for the specified function.\n\n");
    printf("Options:\n");
    printf("  -i, --input <file>      Input C source file\n");
    printf("  -o, --output <file>     Output assembly file (without extension)\n");
    printf("  -f, --function <name>   Function name to extract (default: %s)\n", DEFAULT_FUNC_NAME);
    printf("  -O, --opt-level <N>     Optimization level: 0, 1, 2, or 3 (default: 0)\n");
    printf("  -a, --all-opts          Generate assembly for all optimization levels\n");
    printf("  -t, --timeout <sec>     Compilation timeout in seconds (default: %d)\n", DEFAULT_TIMEOUT);
    printf("  -h, --help              Show this help message\n");
    printf("  -V, --version           Show version information\n");
    printf("\n");
    printf("Output:\n");
    printf("  Creates .asm files containing assembly with LLM prompts.\n");
    printf("  With --all-opts, creates <output>_O0.asm through <output>_O3.asm\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s -i sample.c -o sample -f func0 -a\n", program);
    printf("  This creates sample_O0.asm, sample_O1.asm, sample_O2.asm, sample_O3.asm\n");
}

/*
 * Compile C source to object file
 */
static ResultCode compile_to_object(const char *input_file, const char *output_file,
                                    OptLevel opt_level, int timeout) {
    char cmd[MAX_PATH_LEN * 2];
    /* gcc -c [opt_flag] [input] -o [output] -lm */
    snprintf(cmd, sizeof(cmd), "gcc -c %s %s -o %s -lm 2>&1",
             opt_level_str(opt_level), input_file, output_file);

    int ret = run_command_timeout(cmd, timeout);
    if (ret != 0) {
        print_error("Compilation failed for %s with %s (exit code: %d)",
                   input_file, opt_level_name(opt_level), ret);
        return RESULT_ERROR_COMPILE;
    }

    return RESULT_SUCCESS;
}

/*
 * Disassemble object file using objdump
 */
static ResultCode disassemble(const char *obj_file, const char *asm_file, int timeout) {
    char cmd[MAX_PATH_LEN * 2];
    snprintf(cmd, sizeof(cmd), "objdump -d %s > %s 2>&1", obj_file, asm_file);

    int ret = run_command_timeout(cmd, timeout);
    if (ret != 0) {
        print_error("Disassembly failed for %s (exit code: %d)", obj_file, ret);
        return RESULT_ERROR_DISASM;
    }

    return RESULT_SUCCESS;
}

/*
 * Process a single optimization level
 */
static ResultCode process_opt_level(const char *input_file, const char *output_base,
                                    const char *func_name, OptLevel opt_level,
                                    int timeout) {
    ResultCode rc;
    char *temp_dir = NULL;
    char obj_path[MAX_PATH_LEN];
    char disasm_path[MAX_PATH_LEN];
    char output_path[MAX_PATH_LEN];

    /* Create temporary directory for intermediate files */
    temp_dir = create_temp_dir();
    if (!temp_dir) {
        print_error("Failed to create temporary directory");
        return RESULT_ERROR_IO;
    }

    /* Build paths */
    snprintf(obj_path, sizeof(obj_path), "%s/output_%s.o",
             temp_dir, opt_level_name(opt_level));
    snprintf(disasm_path, sizeof(disasm_path), "%s/output_%s.s",
             temp_dir, opt_level_name(opt_level));
    snprintf(output_path, sizeof(output_path), "%s_%s.asm",
             output_base, opt_level_name(opt_level));

    print_info("Processing %s with %s optimization...",
               input_file, opt_level_name(opt_level));

    /* Compile to object file */
    rc = compile_to_object(input_file, obj_path, opt_level, timeout);
    if (rc != RESULT_SUCCESS) {
        goto cleanup;
    }

    /* Disassemble */
    rc = disassemble(obj_path, disasm_path, timeout);
    if (rc != RESULT_SUCCESS) {
        goto cleanup;
    }

    /* Read disassembly */
    size_t disasm_size;
    char *disasm = read_file(disasm_path, &disasm_size);
    if (!disasm) {
        print_error("Failed to read disassembly output");
        rc = RESULT_ERROR_IO;
        goto cleanup;
    }

    /* Extract function assembly */
    char *func_asm = extract_function_asm(disasm, func_name);
    free(disasm);

    if (!func_asm) {
        print_error("Function '%s' not found in disassembly", func_name);
        rc = RESULT_ERROR_NOT_FOUND;
        goto cleanup;
    }

    /* Clean assembly */
    char *clean_asm = clean_assembly(func_asm);
    free(func_asm);

    if (!clean_asm) {
        print_error("Failed to clean assembly");
        rc = RESULT_ERROR_MEMORY;
        goto cleanup;
    }

    /* Format with prompts */
    char *prompt = format_asm_prompt(clean_asm, opt_level);
    free(clean_asm);

    if (!prompt) {
        print_error("Failed to format prompt");
        rc = RESULT_ERROR_MEMORY;
        goto cleanup;
    }

    /* Write output */
    rc = write_file(output_path, prompt, strlen(prompt));
    free(prompt);

    if (rc != RESULT_SUCCESS) {
        print_error("Failed to write output file: %s", output_path);
        goto cleanup;
    }

    print_info("Created: %s", output_path);
    rc = RESULT_SUCCESS;

cleanup:
    if (temp_dir) {
        remove_dir_recursive(temp_dir);
        free(temp_dir);
    }
    return rc;
}

int main(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *output_base = NULL;
    const char *func_name = DEFAULT_FUNC_NAME;
    OptLevel opt_level = OPT_O0;
    bool all_opts = false;
    int timeout = DEFAULT_TIMEOUT;
    int opt;

    while ((opt = getopt_long(argc, argv, "i:o:f:O:at:hV", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                input_file = optarg;
                break;
            case 'o':
                output_base = optarg;
                break;
            case 'f':
                func_name = optarg;
                break;
            case 'O':
                opt_level = parse_opt_level(optarg);
                break;
            case 'a':
                all_opts = true;
                break;
            case 't':
                timeout = atoi(optarg);
                if (timeout <= 0) timeout = DEFAULT_TIMEOUT;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'V':
                print_version("llm4decompile-preprocess");
                return 0;
            default:
                print_usage(argv[0]);
                return RESULT_ERROR_ARGS;
        }
    }

    /* Validate arguments */
    if (!input_file || !output_base) {
        print_error("Input file and output base are required");
        print_usage(argv[0]);
        return RESULT_ERROR_ARGS;
    }

    if (!file_exists(input_file)) {
        print_error("Input file not found: %s", input_file);
        return RESULT_ERROR_NOT_FOUND;
    }

    /* Process optimization levels */
    ResultCode rc = RESULT_SUCCESS;

    if (all_opts) {
        for (OptLevel level = OPT_O0; level < OPT_COUNT; level++) {
            ResultCode level_rc = process_opt_level(input_file, output_base,
                                                     func_name, level, timeout);
            if (level_rc != RESULT_SUCCESS && rc == RESULT_SUCCESS) {
                rc = level_rc;
            }
        }
    } else {
        rc = process_opt_level(input_file, output_base, func_name, opt_level, timeout);
    }

    if (rc == RESULT_SUCCESS) {
        print_info("Preprocessing complete.");
    }

    return rc;
}
