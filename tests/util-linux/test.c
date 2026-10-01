/*
 * SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests exec-awareness in util-linux's setpriv and enosys.
 *
 * Usage: ./test <util-linux-bin-dir>
 */
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SECBIT_EXEC_RESTRICT_FILE    (1 << 8)
#define SECBIT_EXEC_DENY_INTERACTIVE (1 << 10)

static char setpriv[PATH_MAX], enosys[PATH_MAX];
static char dir[256], marker[PATH_MAX], filter[PATH_MAX];
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

static void report(int ok, const char *desc)
{
    printf("%s: %s\n", ok ? "PASS" : "FAIL", desc);
    if (!ok) failures++;
}

/* Run argv[], which creates the marker file if it gets to run its command. */
static void check(int want_run, char *const argv[], const char *desc)
{
    unlink(marker);
    int rc = run(argv);
    int ran = access(marker, F_OK) == 0;
    report(want_run ? (rc == 0 && ran) : (rc != 0 && !ran), desc);
}

static void set_secbits(int add)
{
    int cur = prctl(PR_GET_SECUREBITS);
    if (cur < 0 || prctl(PR_SET_SECUREBITS, cur | add) < 0) {
        perror("prctl(PR_SET_SECUREBITS)");
        exit(2);
    }
}

static void check_setpriv(int want_run, const char *desc)
{
    check(want_run, (char *[]){ setpriv, "--seccomp-filter", filter,
                                "--", "touch", marker, NULL }, desc);
}

static void check_enosys(int want_run, const char *desc)
{
    check(want_run, (char *[]){ enosys, "-s", "fallocate",
                                "--", "touch", marker, NULL }, desc);
}

int main(int argc, char **argv)
{
    /* Re-executed under setpriv: succeed if the securebits match */
    if (argc == 3 && !strcmp(argv[1], "--secbits"))
        return prctl(PR_GET_SECUREBITS) == atoi(argv[2]) ? 0 : 1;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <util-linux-bin-dir>\n", argv[0]);
        return 2;
    }
    snprintf(setpriv, sizeof(setpriv), "%s/setpriv", argv[1]);
    snprintf(enosys, sizeof(enosys), "%s/enosys", argv[1]);

    const char *tmp = getenv("TMPDIR");
    snprintf(dir, sizeof(dir), "%s/util-linux-exec-test-XXXXXX", tmp ? tmp : "/tmp");
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 2; }
    snprintf(marker, sizeof(marker), "%s/marker", dir);
    snprintf(filter, sizeof(filter), "%s/filter.bpf", dir);

    /* === setpriv --securebits === */
    printf("=== setpriv --securebits ===\n");

    char self[PATH_MAX], bits[32];
    if (!realpath(argv[0], self)) { perror("realpath"); return 2; }
    snprintf(bits, sizeof(bits), "%d", SECBIT_EXEC_RESTRICT_FILE | SECBIT_EXEC_DENY_INTERACTIVE);
    report(run((char *[]){ setpriv, "--securebits", "+exec_restrict_file,+exec_deny_interactive",
                           "--", self, "--secbits", bits, NULL }) == 0,
           "sets exec_restrict_file and exec_deny_interactive");

    /* === Audit mode (no securebits set) === */
    printf("=== Audit mode ===\n");

    /* The filter makes fallocate() fail, which neither setpriv nor
     * touch need. */
    char dump[PATH_MAX + 16];
    snprintf(dump, sizeof(dump), "--dump=%s", filter);
    report(run((char *[]){ enosys, "-s", "fallocate", dump, NULL }) == 0,
           "enosys --dump runs");
    chmod(filter, 0644);
    check_setpriv(1, "setpriv loads non-executable filter");
    check_enosys(1, "enosys loads filter");

    /* === EXEC_RESTRICT_FILE === */
    printf("=== EXEC_RESTRICT_FILE ===\n");
    set_secbits(SECBIT_EXEC_RESTRICT_FILE);

    check_setpriv(0, "setpriv non-executable filter blocked");
    check_enosys(1, "enosys still loads filter");

    chmod(filter, 0755);
    check_setpriv(1, "setpriv loads executable filter");

    /* === EXEC_DENY_INTERACTIVE ===
     * EXEC_RESTRICT_FILE remains set.
     */
    printf("=== EXEC_DENY_INTERACTIVE ===\n");
    set_secbits(SECBIT_EXEC_DENY_INTERACTIVE);

    check_enosys(0, "enosys filter from command line blocked");
    report(run((char *[]){ enosys, "-s", "fallocate", "--dump", NULL }) == 0,
           "enosys --dump still runs");
    check_setpriv(1, "setpriv still loads executable filter");

    unlink(marker);
    unlink(filter);
    rmdir(dir);

    if (failures) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll tests passed\n");
    return 0;
}
