/*
 * SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests exec-awareness in dash.
 *
 * Usage: ./test <dash-binary>
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SECBIT_EXEC_RESTRICT_FILE    (1 << 8)
#define SECBIT_EXEC_DENY_INTERACTIVE (1 << 10)

static const char *dash;
static char script[64];
static char sourced[64];
static int failures;

/* Fork and exec argv[]; redirect stdin from /dev/null; suppress stdout+stderr. */
static int run(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(2); }
    if (pid == 0) {
        int null_r = open("/dev/null", O_RDONLY);
        int null_w = open("/dev/null", O_WRONLY);
        dup2(null_r, STDIN_FILENO);
        dup2(null_w, STDOUT_FILENO);
        dup2(null_w, STDERR_FILENO);
        close(null_r);
        close(null_w);
        execv(argv[0], argv);
        _exit(127);
    }
    int st;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
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

/* Build a small wrapper script that sources `sourced` then exits 0. */
static void write_wrapper(const char *wrapper, const char *inner)
{
    int fd = open(wrapper, O_WRONLY | O_TRUNC);
    if (fd < 0) { perror("open wrapper"); exit(2); }
    char buf[256];
    int n = snprintf(buf, sizeof(buf), ". %s\nexit 0\n", inner);
    if (write(fd, buf, n) < 0) { perror("write wrapper"); exit(2); }
    close(fd);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <dash>\n", argv[0]);
        return 2;
    }
    dash = argv[1];

    /* Main script: sources another file then exits 0. */
    strncpy(script, "/tmp/dash-exec-test-XXXXXX", sizeof(script) - 1);
    int fd = mkstemp(script);
    if (fd < 0) { perror("mkstemp script"); return 2; }
    close(fd);

    /* Sourced file: just exits 0. */
    strncpy(sourced, "/tmp/dash-exec-src-XXXXXX", sizeof(sourced) - 1);
    fd = mkstemp(sourced);
    if (fd < 0) { perror("mkstemp sourced"); return 2; }
    const char src[] = "exit 0\n";
    if (write(fd, src, sizeof(src) - 1) < 0) { perror("write sourced"); return 2; }
    close(fd);

    /* === Audit mode (no securebits set) === */
    printf("=== Audit mode ===\n");

    chmod(script, 0644);
    chmod(sourced, 0644);
    write_wrapper(script, sourced);
    check(1, run((char *[]){ (char *)dash, script, NULL }),
          "non-executable script runs");
    check(1, run((char *[]){ (char *)dash, "-c", "exit 0", NULL }),
          "-c runs");

    /* === EXEC_RESTRICT_FILE === */
    printf("=== EXEC_RESTRICT_FILE ===\n");
    set_secbits(SECBIT_EXEC_RESTRICT_FILE);

    chmod(script, 0755);
    chmod(sourced, 0755);
    write_wrapper(script, sourced);
    check(1, run((char *[]){ (char *)dash, script, NULL }),
          "executable script runs");

    /* Non-executable sourced file: wrapper is executable but inner is not. */
    chmod(sourced, 0644);
    write_wrapper(script, sourced);
    check(0, run((char *[]){ (char *)dash, script, NULL }),
          "non-executable sourced file blocked");

    /* Non-executable entry-point: inner is executable, wrapper is not. */
    chmod(script, 0644);
    chmod(sourced, 0755);
    write_wrapper(script, sourced);
    check(0, run((char *[]){ (char *)dash, script, NULL }),
          "non-executable entry-point script blocked");

    /* === EXEC_DENY_INTERACTIVE ===
     * EXEC_RESTRICT_FILE remains set.  These entry points use exec_deny_interactive()
     * and are unaffected by the file-restriction bit.
     */
    printf("=== EXEC_DENY_INTERACTIVE ===\n");
    set_secbits(SECBIT_EXEC_DENY_INTERACTIVE);

    check(0, run((char *[]){ (char *)dash, "-c", "exit 0", NULL }),
          "-c blocked");
    /* stdin = /dev/null triggers the stdin exec check */
    check(0, run((char *[]){ (char *)dash, NULL }),
          "stdin blocked");

    /* Executable script still runs: check_fd_exec succeeds,
     * exec_deny_interactive is not consulted for file sources. */
    chmod(script, 0755);
    chmod(sourced, 0755);
    write_wrapper(script, sourced);
    check(1, run((char *[]){ (char *)dash, script, NULL }),
          "executable script still runs with both bits set");

    unlink(script);
    unlink(sourced);

    if (failures) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll tests passed\n");
    return 0;
}
