/*
 * LLM4Decompile - Common Utilities Implementation
 *
 * Shared utilities for the Cosmopolitan C implementation of LLM4Decompile.
 *
 * SPDX-License-Identifier: MIT
 */

#include "common.h"
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <time.h>

/* Optimization level strings */
static const char *opt_strings[] = {"-O0", "-O1", "-O2", "-O3"};
static const char *opt_names[] = {"O0", "O1", "O2", "O3"};

const char *opt_level_str(OptLevel level) {
    if (level >= OPT_COUNT) return "-O0";
    return opt_strings[level];
}

const char *opt_level_name(OptLevel level) {
    if (level >= OPT_COUNT) return "O0";
    return opt_names[level];
}

OptLevel parse_opt_level(const char *str) {
    if (!str) return OPT_O0;
    if (strcmp(str, "O0") == 0 || strcmp(str, "-O0") == 0) return OPT_O0;
    if (strcmp(str, "O1") == 0 || strcmp(str, "-O1") == 0) return OPT_O1;
    if (strcmp(str, "O2") == 0 || strcmp(str, "-O2") == 0) return OPT_O2;
    if (strcmp(str, "O3") == 0 || strcmp(str, "-O3") == 0) return OPT_O3;
    return OPT_O0;
}

char *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        if (out_size) *out_size = 0;
        return NULL;
    }

    size_t read_size = fread(buf, 1, size, f);
    fclose(f);

    buf[read_size] = '\0';
    if (out_size) *out_size = read_size;
    return buf;
}

ResultCode write_file(const char *path, const char *data, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return RESULT_ERROR_IO;

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    return (written == size) ? RESULT_SUCCESS : RESULT_ERROR_IO;
}

/* Timeout handling for child processes */
static volatile sig_atomic_t child_timed_out = 0;

static void timeout_handler(int sig) {
    (void)sig;
    child_timed_out = 1;
}

int run_command(const char *cmd, char *out_buf, size_t out_size, int timeout_sec) {
    if (out_buf && out_size > 0) {
        out_buf[0] = '\0';
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    /* Parent process */
    close(pipefd[1]);

    /* Set up timeout */
    child_timed_out = 0;
    struct sigaction sa, old_sa;
    if (timeout_sec > 0) {
        sa.sa_handler = timeout_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGALRM, &sa, &old_sa);
        alarm((unsigned int)timeout_sec);
    }

    /* Read output */
    size_t total_read = 0;
    if (out_buf && out_size > 0) {
        ssize_t n;
        while ((n = read(pipefd[0], out_buf + total_read, out_size - total_read - 1)) > 0) {
            total_read += (size_t)n;
            if (total_read >= out_size - 1) break;
        }
        out_buf[total_read] = '\0';
    } else {
        /* Drain the pipe */
        char drain_buf[4096];
        while (read(pipefd[0], drain_buf, sizeof(drain_buf)) > 0) {
            /* discard */
        }
    }
    close(pipefd[0]);

    /* Wait for child */
    int status;
    pid_t waited = waitpid(pid, &status, 0);

    /* Clear alarm */
    if (timeout_sec > 0) {
        alarm(0);
        sigaction(SIGALRM, &old_sa, NULL);
    }

    if (child_timed_out) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        return -2; /* Timeout */
    }

    if (waited == -1) {
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return -1;
}

int run_command_timeout(const char *cmd, int timeout_sec) {
    return run_command(cmd, NULL, 0, timeout_sec);
}

char *create_temp_dir(void) {
    char template[MAX_PATH_LEN];
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";

    snprintf(template, sizeof(template), "%s/llm4decompile.XXXXXX", tmpdir);

    char *result = mkdtemp(template);
    if (!result) return NULL;

    char *path = strdup(result);
    return path;
}

static int remove_dir_callback(const char *path, const struct stat *st,
                                int flag, struct FTW *ftw);

/* Simple recursive directory removal */
ResultCode remove_dir_recursive(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return RESULT_ERROR_IO;

    struct dirent *entry;
    char filepath[MAX_PATH_LEN];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(filepath, &st) == -1) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            remove_dir_recursive(filepath);
        } else {
            unlink(filepath);
        }
    }

    closedir(dir);
    rmdir(path);
    return RESULT_SUCCESS;
}

bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

const char *get_file_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) return "";
    return dot + 1;
}

char *extract_function_asm(const char *disasm, const char *func_name) {
    if (!disasm || !func_name) return NULL;

    /* Find function start marker like "<func0>:" */
    char marker[256];
    snprintf(marker, sizeof(marker), "<%s>:", func_name);

    const char *start = strstr(disasm, marker);
    if (!start) return NULL;

    /* Find the end (next empty line or next function) */
    const char *end = start + strlen(marker);
    const char *next_func = strstr(end, "\n\n");
    if (!next_func) {
        next_func = end + strlen(end);
    }

    size_t len = (size_t)(next_func - start);
    char *result = malloc(len + 1);
    if (!result) return NULL;

    memcpy(result, start, len);
    result[len] = '\0';

    return result;
}

char *clean_assembly(const char *raw_asm) {
    if (!raw_asm) return NULL;

    size_t len = strlen(raw_asm);
    char *result = malloc(len + 1);
    if (!result) return NULL;

    size_t out_idx = 0;
    const char *line_start = raw_asm;

    while (*line_start) {
        /* Find end of line */
        const char *line_end = strchr(line_start, '\n');
        if (!line_end) line_end = line_start + strlen(line_start);

        /* Copy line, filtering binary code and comments */
        const char *p = line_start;

        /* Skip lines that look like address-only lines (all zeros) */
        bool skip_line = false;
        if (line_end - line_start > 0) {
            int zeros = 0;
            for (const char *c = line_start; c < line_end && *c != '\t'; c++) {
                if (*c == '0') zeros++;
            }
            if (zeros > 8) skip_line = true;
        }

        if (!skip_line) {
            /* Find the instruction part (after tabs) */
            int tab_count = 0;
            for (p = line_start; p < line_end; p++) {
                if (*p == '\t') tab_count++;
                if (tab_count >= 2) {
                    p++;
                    break;
                }
            }

            /* Copy instruction until comment or end of line */
            while (p < line_end && *p != '#' && *p != ';') {
                result[out_idx++] = *p++;
            }

            /* Trim trailing whitespace */
            while (out_idx > 0 && (result[out_idx-1] == ' ' || result[out_idx-1] == '\t')) {
                out_idx--;
            }

            result[out_idx++] = '\n';
        }

        /* Move to next line */
        if (*line_end == '\n') {
            line_start = line_end + 1;
        } else {
            break;
        }
    }

    result[out_idx] = '\0';
    return result;
}

char *format_asm_prompt(const char *asm_code, OptLevel opt_level) {
    if (!asm_code) return NULL;

    const char *opt_name = opt_level_name(opt_level);
    size_t asm_len = strlen(asm_code);

    /* Format: "# This is the assembly code with O0 optimization:\n<asm>\n# What is the source code?\n" */
    size_t total_len = 64 + strlen(opt_name) + asm_len + 32;
    char *result = malloc(total_len);
    if (!result) return NULL;

    snprintf(result, total_len,
             "# This is the assembly code with %s optimization:\n%s\n# What is the source code?\n",
             opt_name, asm_code);

    return result;
}

char *extract_includes(const char *c_code) {
    if (!c_code) return NULL;

    size_t len = strlen(c_code);
    char *result = malloc(len + 1);
    if (!result) return NULL;

    size_t out_idx = 0;
    const char *line_start = c_code;

    while (*line_start) {
        const char *line_end = strchr(line_start, '\n');
        if (!line_end) line_end = line_start + strlen(line_start);

        /* Check if line contains #include */
        size_t line_len = (size_t)(line_end - line_start);
        if (strstr(line_start, "#include") != NULL &&
            strstr(line_start, "#include") < line_end) {
            memcpy(result + out_idx, line_start, line_len);
            out_idx += line_len;
            result[out_idx++] = '\n';
        }

        if (*line_end == '\n') {
            line_start = line_end + 1;
        } else {
            break;
        }
    }

    result[out_idx] = '\0';
    return result;
}

char *remove_includes(const char *c_code) {
    if (!c_code) return NULL;

    size_t len = strlen(c_code);
    char *result = malloc(len + 1);
    if (!result) return NULL;

    size_t out_idx = 0;
    const char *line_start = c_code;

    while (*line_start) {
        const char *line_end = strchr(line_start, '\n');
        if (!line_end) line_end = line_start + strlen(line_start);

        /* Check if line does NOT contain #include */
        size_t line_len = (size_t)(line_end - line_start);
        if (strstr(line_start, "#include") == NULL ||
            strstr(line_start, "#include") >= line_end) {
            memcpy(result + out_idx, line_start, line_len);
            out_idx += line_len;
            result[out_idx++] = '\n';
        }

        if (*line_end == '\n') {
            line_start = line_end + 1;
        } else {
            break;
        }
    }

    result[out_idx] = '\0';
    return result;
}

void print_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void print_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
}

void print_version(const char *program_name) {
    printf("%s version %s\n", program_name, LLM4DECOMPILE_VERSION_STRING);
    printf("Built with Cosmopolitan Libc - Actually Portable Executable\n");
    printf("Runs on: Linux, macOS, Windows, FreeBSD, OpenBSD, NetBSD\n");
}
