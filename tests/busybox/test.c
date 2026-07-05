/*
 * SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests exec-awareness in busybox ash, hush, and awk.
 *
 * Usage: ./test <busybox-binary>
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

static const char *busybox;
static char sh_script[64];
static char awk_prog[64];
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

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <busybox>\n", argv[0]);
        return 2;
    }
    busybox = argv[1];

    /* Shell script: a trivial exit. */
    strncpy(sh_script, "/tmp/bb-sh-test-XXXXXX", sizeof(sh_script) - 1);
    int fd = mkstemp(sh_script);
    if (fd < 0) { perror("mkstemp"); return 2; }
    const char sh[] = "exit 0\n";
    if (write(fd, sh, sizeof(sh) - 1) < 0) { perror("write"); return 2; }
    close(fd);

    /* Awk program file. */
    strncpy(awk_prog, "/tmp/bb-awk-test-XXXXXX", sizeof(awk_prog) - 1);
    fd = mkstemp(awk_prog);
    if (fd < 0) { perror("mkstemp"); return 2; }
    const char awk[] = "BEGIN { exit 0 }\n";
    if (write(fd, awk, sizeof(awk) - 1) < 0) { perror("write"); return 2; }
    close(fd);

    /* === Audit mode (no securebits) === */
    printf("=== Audit mode ===\n");

    chmod(sh_script, 0644);
    check(1, run((char *[]){ (char *)busybox, "ash",  sh_script, NULL }),
          "ash: non-executable script runs");
    check(1, run((char *[]){ (char *)busybox, "hush", sh_script, NULL }),
          "hush: non-executable script runs");
    check(1, run((char *[]){ (char *)busybox, "ash",  "-c", "exit 0", NULL }),
          "ash: -c runs");
    check(1, run((char *[]){ (char *)busybox, "hush", "-c", "exit 0", NULL }),
          "hush: -c runs");

    chmod(awk_prog, 0644);
    check(1, run((char *[]){ (char *)busybox, "awk", "-f", awk_prog, "/dev/null", NULL }),
          "awk: non-executable -f progfile runs");
    check(1, run((char *[]){ (char *)busybox, "awk", "BEGIN{exit 0}", "/dev/null", NULL }),
          "awk: inline program runs");
    check(1, run((char *[]){ (char *)busybox, "awk", "-e", "BEGIN{exit 0}", "/dev/null", NULL }),
          "awk: -e program runs");

    /* === EXEC_RESTRICT_FILE === */
    printf("=== EXEC_RESTRICT_FILE ===\n");
    set_secbits(SECBIT_EXEC_RESTRICT_FILE);

    chmod(sh_script, 0755);
    check(1, run((char *[]){ (char *)busybox, "ash",  sh_script, NULL }),
          "ash: executable script runs");
    check(1, run((char *[]){ (char *)busybox, "hush", sh_script, NULL }),
          "hush: executable script runs");
    chmod(sh_script, 0644);
    check(0, run((char *[]){ (char *)busybox, "ash",  sh_script, NULL }),
          "ash: non-executable script blocked");
    check(0, run((char *[]){ (char *)busybox, "hush", sh_script, NULL }),
          "hush: non-executable script blocked");

    chmod(awk_prog, 0755);
    check(1, run((char *[]){ (char *)busybox, "awk", "-f", awk_prog, "/dev/null", NULL }),
          "awk: executable -f progfile runs");
    chmod(awk_prog, 0644);
    check(0, run((char *[]){ (char *)busybox, "awk", "-f", awk_prog, "/dev/null", NULL }),
          "awk: non-executable -f progfile blocked");

    /* === EXEC_DENY_INTERACTIVE ===
     * EXEC_RESTRICT_FILE remains set.  These entry points use exec_deny_interactive()
     * and are unaffected by the file-restriction bit.
     */
    printf("=== EXEC_DENY_INTERACTIVE ===\n");
    set_secbits(SECBIT_EXEC_DENY_INTERACTIVE);

    check(0, run((char *[]){ (char *)busybox, "ash",  "-c", "exit 0", NULL }),
          "ash: -c blocked");
    check(0, run((char *[]){ (char *)busybox, "hush", "-c", "exit 0", NULL }),
          "hush: -c blocked");
    /* stdin = /dev/null triggers the stdin exec check in ash_main / hush_main */
    check(0, run((char *[]){ (char *)busybox, "ash",  NULL }),
          "ash: stdin blocked");
    check(0, run((char *[]){ (char *)busybox, "hush", NULL }),
          "hush: stdin blocked");

    check(0, run((char *[]){ (char *)busybox, "awk", "BEGIN{exit 0}", "/dev/null", NULL }),
          "awk: inline program blocked");
    check(0, run((char *[]){ (char *)busybox, "awk", "-e", "BEGIN{exit 0}", "/dev/null", NULL }),
          "awk: -e program blocked");

    /* Executable script files still run: exec_restrict_file check passes (0755),
     * and exec_deny_interactive is not consulted for file-backed sources. */
    chmod(sh_script, 0755);
    check(1, run((char *[]){ (char *)busybox, "ash",  sh_script, NULL }),
          "ash: executable script still runs with both bits set");
    check(1, run((char *[]){ (char *)busybox, "hush", sh_script, NULL }),
          "hush: executable script still runs with both bits set");

    chmod(awk_prog, 0755);
    check(1, run((char *[]){ (char *)busybox, "awk", "-f", awk_prog, "/dev/null", NULL }),
          "awk: executable -f progfile still runs with both bits set");

    unlink(sh_script);
    unlink(awk_prog);

    if (failures) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll tests passed\n");
    return 0;
}
