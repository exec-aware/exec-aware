/*
 * SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests exec-awareness in the Python interpreter.
 *
 * Usage: ./test <python-binary>
 */
#define _XOPEN_SOURCE 700
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SECBIT_EXEC_RESTRICT_FILE    (1 << 8)
#define SECBIT_EXEC_DENY_INTERACTIVE (1 << 10)

static const char *python;
static int failures;

/* Fork and exec argv[]; redirect stdin from 'input' (or /dev/null if NULL);
 * suppress stdout+stderr. */
static int run_with_stdin(const char *input, char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(2); }
    if (pid == 0) {
        int in = open(input ? input : "/dev/null", O_RDONLY);
        int null_w = open("/dev/null", O_WRONLY);
        dup2(in, STDIN_FILENO);
        dup2(null_w, STDOUT_FILENO);
        dup2(null_w, STDERR_FILENO);
        close(in);
        close(null_w);
        execv(argv[0], argv);
        _exit(127);
    }
    int st;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static int run(char *const argv[])
{
    return run_with_stdin(NULL, argv);
}

static void check(int want_success, int rc, const char *desc)
{
    int ok = want_success ? (rc == 0) : (rc != 0 && rc != 127);
    printf("%s: %s\n", ok ? "PASS" : "FAIL", desc);
    if (!ok) failures++;
}

static void set_secbits(int add)
{
    int cur = prctl(PR_GET_SECUREBITS);
    if (cur < 0 || prctl(PR_SET_SECUREBITS, cur | add) < 0) {
        perror("prctl(PR_SET_SECUREBITS)");
        exit(2);
    }
}

static int remove_entry(const char *path, const struct stat *sb, int type,
                        struct FTW *ftw)
{
    (void)sb; (void)type; (void)ftw;
    return remove(path);
}

static void write_file(const char *name, const char *content)
{
    int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror(name); exit(2); }
    if (write(fd, content, strlen(content)) < 0) { perror("write"); exit(2); }
    close(fd);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <python>\n", argv[0]);
        return 2;
    }
    python = argv[1];
    char *py = (char *)python;

    /* The children must not pick up settings from the environment. */
    unsetenv("PYTHONDONTWRITEBYTECODE");
    unsetenv("PYTHONINSPECT");
    unsetenv("PYTHONPATH");
    unsetenv("PYTHONSTARTUP");

    /* Work in a fresh directory: -m finds modules through the current
     * directory, and main.py imports mod.py from its own directory. */
    char dir[] = "/tmp/python-exec-test-XXXXXX";
    if (mkdtemp(dir) == NULL) { perror("mkdtemp"); return 2; }
    if (chdir(dir) < 0) { perror("chdir"); return 2; }

    write_file("main.py", "import mod\n");
    write_file("mod.py", "pass\n");
    write_file("plain.py", "pass\n");
    chmod("main.py", 0755);

    /* === Audit mode (no securebits set) === */
    printf("=== Audit mode ===\n");
    check(1, run((char *[]){ py, "plain.py", NULL }),
          "non-executable script runs");
    check(1, run((char *[]){ py, "-B", "main.py", NULL }),
          "non-executable module imports");
    check(1, run((char *[]){ py, "-c", "pass", NULL }),
          "-c runs");
    check(1, run((char *[]){ py, NULL }),
          "stdin runs");

    /* === EXEC_RESTRICT_FILE === */
    printf("=== EXEC_RESTRICT_FILE ===\n");
    set_secbits(SECBIT_EXEC_RESTRICT_FILE);

    chmod("plain.py", 0755);
    check(1, run((char *[]){ py, "plain.py", NULL }),
          "executable script runs");
    chmod("plain.py", 0644);
    check(0, run((char *[]){ py, "plain.py", NULL }),
          "non-executable script blocked");
    check(0, run((char *[]){ py, "-B", "main.py", NULL }),
          "non-executable module blocked");
    check(0, run((char *[]){ py, "-m", "plain", NULL }),
          "non-executable -m module blocked");

    /* The first run writes mod's bytecode cache, the second imports it. */
    chmod("mod.py", 0755);
    check(1, run((char *[]){ py, "main.py", NULL }),
          "executable module imports");
    struct stat st;
    int cached = stat("__pycache__", &st) == 0;
    check(1, run((char *[]){ py, "main.py", NULL }),
          "executable module imports from its bytecode cache");
    if (!cached) {
        printf("FAIL: no bytecode cache was written\n");
        failures++;
    }

    /* -c is not a file: EXEC_RESTRICT_FILE does not apply. */
    check(1, run((char *[]){ py, "-c", "pass", NULL }),
          "-c still runs");

    /* === EXEC_DENY_INTERACTIVE ===
     * EXEC_RESTRICT_FILE remains set.
     */
    printf("=== EXEC_DENY_INTERACTIVE ===\n");
    set_secbits(SECBIT_EXEC_DENY_INTERACTIVE);

    check(0, run((char *[]){ py, "-c", "pass", NULL }),
          "-c blocked");
    /* stdin = /dev/null: not a regular file, so the check fails */
    check(0, run((char *[]){ py, NULL }),
          "stdin blocked");
    check(0, run_with_stdin("plain.py", (char *[]){ py, "-", NULL }),
          "non-executable file on stdin blocked");
    chmod("plain.py", 0755);
    check(1, run_with_stdin("plain.py", (char *[]){ py, "-", NULL }),
          "executable file on stdin runs");
    /* -i runs the script, then the REPL on stdin */
    check(0, run((char *[]){ py, "-i", "plain.py", NULL }),
          "-i REPL blocked");

    check(1, run((char *[]){ py, "main.py", NULL }),
          "executable script still runs with both bits set");

    if (chdir("/") < 0 || nftw(dir, remove_entry, 8, FTW_DEPTH | FTW_PHYS) < 0)
        perror(dir);

    if (failures) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll tests passed\n");
    return 0;
}
