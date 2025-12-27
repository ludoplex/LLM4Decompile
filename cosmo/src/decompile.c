/*
 * LLM4Decompile - Main Decompilation Tool
 *
 * Main command-line interface for LLM4Decompile that orchestrates the
 * decompilation workflow: preprocess -> inference -> evaluate.
 *
 * This tool replaces the Python CLI and provides a portable C executable
 * using Cosmopolitan Libc that runs on multiple operating systems.
 *
 * SPDX-License-Identifier: MIT
 */

#include "common.h"
#include <getopt.h>
#include <unistd.h>

/* Default values */
#define DEFAULT_FUNC_NAME "func0"
#define DEFAULT_TIMEOUT 30
#define DEFAULT_MODEL_PATH "LLM4Binary/llm4decompile-6.7b-v1.5"

/* Command-line options */
static struct option long_options[] = {
    {"input",      required_argument, 0, 'i'},
    {"output",     required_argument, 0, 'o'},
    {"function",   required_argument, 0, 'f'},
    {"opt-level",  required_argument, 0, 'O'},
    {"all-opts",   no_argument,       0, 'a'},
    {"preprocess", no_argument,       0, 'p'},
    {"evaluate",   no_argument,       0, 'e'},
    {"model",      required_argument, 0, 'm'},
    {"server",     required_argument, 0, 's'},
    {"timeout",    required_argument, 0, 't'},
    {"verbose",    no_argument,       0, 'v'},
    {"help",       no_argument,       0, 'h'},
    {"version",    no_argument,       0, 'V'},
    {0, 0, 0, 0}
};

static bool verbose = false;

static void print_usage(const char *program) {
    printf("LLM4Decompile - Decompile Binary Code with Large Language Models\n\n");
    printf("Usage: %s [OPTIONS] COMMAND\n\n", program);
    printf("Commands:\n");
    printf("  preprocess   Compile C code and extract assembly for decompilation\n");
    printf("  evaluate     Evaluate decompiled code against test cases\n");
    printf("  info         Show information about this build\n");
    printf("\n");
    printf("Global Options:\n");
    printf("  -v, --verbose    Verbose output\n");
    printf("  -h, --help       Show this help message\n");
    printf("  -V, --version    Show version information\n");
    printf("\n");
    printf("Preprocess Options:\n");
    printf("  -i, --input <file>      Input C source file\n");
    printf("  -o, --output <file>     Output base name (without extension)\n");
    printf("  -f, --function <name>   Function to extract (default: %s)\n", DEFAULT_FUNC_NAME);
    printf("  -O, --opt-level <N>     Optimization level: 0-3 (default: 0)\n");
    printf("  -a, --all-opts          Generate for all optimization levels\n");
    printf("  -t, --timeout <sec>     Compilation timeout (default: %d)\n", DEFAULT_TIMEOUT);
    printf("\n");
    printf("Evaluate Options:\n");
    printf("  -i, --input <file>      Decompiled C function file\n");
    printf("  -r, --test <file>       Test cases file\n");
    printf("  -o, --output <file>     Results output file\n");
    printf("  -t, --timeout <sec>     Execution timeout (default: %d)\n", DEFAULT_TIMEOUT);
    printf("\n");
    printf("Examples:\n");
    printf("  # Preprocess a C file for decompilation\n");
    printf("  %s preprocess -i sample.c -o sample -f func0 -a\n\n", program);
    printf("  # Evaluate decompiled code\n");
    printf("  %s evaluate -i decompiled.c -r tests.c -o results.csv\n\n", program);
    printf("  # Show version and build info\n");
    printf("  %s info\n\n", program);
    printf("For more information, visit: https://github.com/albertan017/LLM4Decompile\n");
}

static void print_info(void) {
    printf("LLM4Decompile - Cosmopolitan C Build\n");
    printf("=====================================\n\n");
    printf("Version: %s\n", LLM4DECOMPILE_VERSION_STRING);
    printf("Build type: Actually Portable Executable (APE)\n");
    printf("\n");
    printf("Supported platforms:\n");
    printf("  - Linux (x86_64, ARM64)\n");
    printf("  - macOS (x86_64, ARM64)\n");
    printf("  - Windows (x86_64)\n");
    printf("  - FreeBSD (x86_64)\n");
    printf("  - OpenBSD (x86_64)\n");
    printf("  - NetBSD (x86_64)\n");
    printf("\n");
    printf("This executable is built using jart/cosmopolitan libc,\n");
    printf("creating a single binary that runs on all supported platforms.\n");
    printf("\n");
    printf("The APE format also allows this file to be treated as a ZIP archive.\n");
    printf("You can extract bundled resources using: unzip %s\n", "llm4decompile.com");
    printf("\n");
    printf("Required tools:\n");
    printf("  - gcc (GNU Compiler Collection) for compilation\n");
    printf("  - objdump (binutils) for disassembly\n");
    printf("\n");
    printf("Optional:\n");
    printf("  - LLM inference server for actual decompilation\n");
    printf("    (Use llama.cpp, vLLM, or similar with LLM4Decompile models)\n");
    printf("\n");
    printf("License: MIT\n");
    printf("Homepage: https://github.com/albertan017/LLM4Decompile\n");
}

/*
 * Forward declarations for subcommands
 * These would normally be in separate files but for simplicity
 * we include the core logic here
 */
static int cmd_preprocess(int argc, char *argv[]);
static int cmd_evaluate(int argc, char *argv[]);

/*
 * Simple preprocessing - delegates to external preprocess tool or handles internally
 */
static int cmd_preprocess(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *output_base = NULL;
    const char *func_name = DEFAULT_FUNC_NAME;
    OptLevel opt_level = OPT_O0;
    bool all_opts = false;
    int timeout = DEFAULT_TIMEOUT;

    /* Reset getopt */
    optind = 1;

    int opt;
    while ((opt = getopt_long(argc, argv, "i:o:f:O:at:vh", long_options, NULL)) != -1) {
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
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                break;
        }
    }

    if (!input_file || !output_base) {
        print_error("preprocess: input file and output base are required");
        printf("Usage: %s preprocess -i <input.c> -o <output_base> [-f func_name] [-O level] [-a]\n", argv[0]);
        return RESULT_ERROR_ARGS;
    }

    if (!file_exists(input_file)) {
        print_error("Input file not found: %s", input_file);
        return RESULT_ERROR_NOT_FOUND;
    }

    /* Execute preprocessing using the standalone preprocess tool */
    char cmd[MAX_PATH_LEN * 4];
    char preprocess_path[MAX_PATH_LEN];

    /* Try to find preprocess tool in same directory as this executable */
    char exe_path[MAX_PATH_LEN];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        char *last_slash = strrchr(exe_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            snprintf(preprocess_path, sizeof(preprocess_path),
                     "%s/llm4decompile-preprocess", exe_path);
        } else {
            strcpy(preprocess_path, "llm4decompile-preprocess");
        }
    } else {
        strcpy(preprocess_path, "llm4decompile-preprocess");
    }

    /* Build command */
    snprintf(cmd, sizeof(cmd), "%s -i '%s' -o '%s' -f '%s' -t %d %s",
             preprocess_path, input_file, output_base, func_name, timeout,
             all_opts ? "-a" : opt_level_str(opt_level));

    if (verbose) {
        printf("Executing: %s\n", cmd);
    }

    int ret = run_command_timeout(cmd, timeout * 5);
    return ret;
}

/*
 * Evaluation command
 */
static int cmd_evaluate(int argc, char *argv[]) {
    const char *decompiled_file = NULL;
    const char *test_file = NULL;
    const char *output_file = NULL;
    int timeout = DEFAULT_TIMEOUT;

    /* Reset getopt */
    optind = 1;

    int opt;
    /* Note: 'r' is used for test file in evaluate mode */
    while ((opt = getopt_long(argc, argv, "i:r:o:t:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                decompiled_file = optarg;
                break;
            case 'r':
                test_file = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 't':
                timeout = atoi(optarg);
                if (timeout <= 0) timeout = DEFAULT_TIMEOUT;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                break;
        }
    }

    if (!decompiled_file || !test_file) {
        print_error("evaluate: decompiled file and test file are required");
        printf("Usage: %s evaluate -i <decompiled.c> -r <test.c> [-o results.csv]\n", argv[0]);
        return RESULT_ERROR_ARGS;
    }

    /* Execute evaluation using the standalone evaluate tool */
    char cmd[MAX_PATH_LEN * 4];
    char evaluate_path[MAX_PATH_LEN];

    /* Try to find evaluate tool in same directory as this executable */
    char exe_path[MAX_PATH_LEN];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        char *last_slash = strrchr(exe_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            snprintf(evaluate_path, sizeof(evaluate_path),
                     "%s/llm4decompile-evaluate", exe_path);
        } else {
            strcpy(evaluate_path, "llm4decompile-evaluate");
        }
    } else {
        strcpy(evaluate_path, "llm4decompile-evaluate");
    }

    /* Build command */
    if (output_file) {
        snprintf(cmd, sizeof(cmd), "%s -d '%s' -t '%s' -o '%s' -T %d %s",
                 evaluate_path, decompiled_file, test_file, output_file, timeout,
                 verbose ? "-v" : "");
    } else {
        snprintf(cmd, sizeof(cmd), "%s -d '%s' -t '%s' -T %d %s",
                 evaluate_path, decompiled_file, test_file, timeout,
                 verbose ? "-v" : "");
    }

    if (verbose) {
        printf("Executing: %s\n", cmd);
    }

    int ret = run_command_timeout(cmd, timeout * 2);
    return ret;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return RESULT_ERROR_ARGS;
    }

    /* Check for global options first */
    const char *command = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version("llm4decompile");
            return 0;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        }
        /* First non-option argument is the command */
        if (argv[i][0] != '-' && !command) {
            command = argv[i];
        }
    }

    if (!command) {
        print_usage(argv[0]);
        return RESULT_ERROR_ARGS;
    }

    /* Dispatch to subcommand */
    if (strcmp(command, "preprocess") == 0) {
        return cmd_preprocess(argc, argv);
    } else if (strcmp(command, "evaluate") == 0) {
        return cmd_evaluate(argc, argv);
    } else if (strcmp(command, "info") == 0) {
        print_info();
        return 0;
    } else if (strcmp(command, "help") == 0) {
        print_usage(argv[0]);
        return 0;
    } else if (strcmp(command, "version") == 0) {
        print_version("llm4decompile");
        return 0;
    } else {
        print_error("Unknown command: %s", command);
        print_usage(argv[0]);
        return RESULT_ERROR_ARGS;
    }
}
