/*
 * LLM4Decompile - Common Header
 * 
 * Shared definitions, structures, and utilities for the Cosmopolitan C
 * implementation of LLM4Decompile.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LLM4DECOMPILE_COMMON_H
#define LLM4DECOMPILE_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Version information */
#define LLM4DECOMPILE_VERSION_MAJOR 2
#define LLM4DECOMPILE_VERSION_MINOR 0
#define LLM4DECOMPILE_VERSION_PATCH 0
#define LLM4DECOMPILE_VERSION_STRING "2.0.0-cosmo"

/* Maximum buffer sizes */
#define MAX_PATH_LEN 4096
#define MAX_LINE_LEN 8192
#define MAX_ASM_SIZE (1024 * 1024)  /* 1MB max assembly size */
#define MAX_SOURCE_SIZE (512 * 1024) /* 512KB max source size */

/* Optimization levels */
typedef enum {
    OPT_O0 = 0,
    OPT_O1 = 1,
    OPT_O2 = 2,
    OPT_O3 = 3,
    OPT_COUNT = 4
} OptLevel;

/* Result codes */
typedef enum {
    RESULT_SUCCESS = 0,
    RESULT_ERROR_IO = 1,
    RESULT_ERROR_COMPILE = 2,
    RESULT_ERROR_DISASM = 3,
    RESULT_ERROR_MEMORY = 4,
    RESULT_ERROR_ARGS = 5,
    RESULT_ERROR_NOT_FOUND = 6,
    RESULT_ERROR_TIMEOUT = 7,
    RESULT_ERROR_RUNTIME = 8
} ResultCode;

/* Evaluation result structure */
typedef struct {
    bool compiled;
    bool executed;
    int exit_code;
    char error_msg[256];
} EvalResult;

/* Statistics structure */
typedef struct {
    int total;
    int compiled;
    int executed;
} Stats;

/* Assembly extraction result */
typedef struct {
    char *assembly;
    size_t length;
    OptLevel opt_level;
    ResultCode status;
} AsmResult;

/* Function declarations - common.c */

/**
 * Get the optimization level string (e.g., "-O0", "-O1", etc.)
 */
const char *opt_level_str(OptLevel level);

/**
 * Get the optimization level name (e.g., "O0", "O1", etc.)
 */
const char *opt_level_name(OptLevel level);

/**
 * Parse optimization level from string
 */
OptLevel parse_opt_level(const char *str);

/**
 * Read entire file into memory
 * Caller must free the returned buffer
 */
char *read_file(const char *path, size_t *out_size);

/**
 * Write buffer to file
 */
ResultCode write_file(const char *path, const char *data, size_t size);

/**
 * Execute a command and capture output
 * Returns exit code, output is written to out_buf if provided
 */
int run_command(const char *cmd, char *out_buf, size_t out_size, int timeout_sec);

/**
 * Execute a command with timeout, returns exit code
 */
int run_command_timeout(const char *cmd, int timeout_sec);

/**
 * Create a temporary directory
 * Returns dynamically allocated path (caller must free)
 */
char *create_temp_dir(void);

/**
 * Remove a directory and its contents
 */
ResultCode remove_dir_recursive(const char *path);

/**
 * Check if a file exists
 */
bool file_exists(const char *path);

/**
 * Get file extension
 */
const char *get_file_ext(const char *path);

/**
 * Extract function from disassembly output
 * func_name is the function to extract (e.g., "func0")
 * Returns dynamically allocated string (caller must free)
 */
char *extract_function_asm(const char *disasm, const char *func_name);

/**
 * Clean assembly by removing binary code and comments
 * Returns dynamically allocated string (caller must free)
 */
char *clean_assembly(const char *raw_asm);

/**
 * Format assembly with prompts for LLM input
 * Returns dynamically allocated string (caller must free)
 */
char *format_asm_prompt(const char *asm_code, OptLevel opt_level);

/**
 * Extract #include directives from C code
 * Returns dynamically allocated string (caller must free)
 */
char *extract_includes(const char *c_code);

/**
 * Remove #include directives from C code
 * Returns dynamically allocated string (caller must free)
 */
char *remove_includes(const char *c_code);

/**
 * Print error message to stderr
 */
void print_error(const char *fmt, ...);

/**
 * Print info message to stdout
 */
void print_info(const char *fmt, ...);

/**
 * Print version information
 */
void print_version(const char *program_name);

/**
 * Get the directory containing the current executable
 * Portable across Linux, macOS, BSD, and Windows
 * Returns dynamically allocated path (caller must free), or NULL on failure
 */
char *get_executable_dir(void);

#endif /* LLM4DECOMPILE_COMMON_H */
