/*
 * SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests exec-awareness in util-linux's setpriv, enosys, unshare, and
 * nsenter.
 *
 * Usage: ./test <util-linux-bin-dir>
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SECBIT_EXEC_RESTRICT_FILE           (1 << 8)
#define SECBIT_EXEC_RESTRICT_FILE_LOCKED    (1 << 9)
#define SECBIT_EXEC_DENY_INTERACTIVE        (1 << 10)
#define SECBIT_EXEC_DENY_INTERACTIVE_LOCKED (1 << 11)

static char setpriv[PATH_MAX], enosys[PATH_MAX];
static char unshare_bin[PATH_MAX], nsenter_bin[PATH_MAX];
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

/* Fork a child that creates a user namespace and waits until *release
 * is closed. Returns the child's PID, or -1 if it can't create one. */
static pid_t spawn_userns(int *release)
{
    int ready[2], wait[2];
    if (pipe(ready) < 0 || pipe(wait) < 0) { perror("pipe"); exit(2); }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(2); }
    if (pid == 0) {
        char c = unshare(CLONE_NEWUSER) == 0;
        close(ready[0]);
        close(wait[1]);
        if (write(ready[1], &c, 1) != 1) _exit(1);
        /* Returns when the parent closes the pipe */
        _exit(read(wait[0], &c, 1) < 0);
    }
    close(ready[1]);
    close(wait[0]);
    char ok = 0;
    if (read(ready[0], &ok, 1) != 1) ok = 0;
    close(ready[0]);
    *release = wait[1];
    if (!ok) {
        close(wait[1]);
        waitpid(pid, NULL, 0);
        return -1;
    }
    return pid;
}

/* Check that a new user namespace can map our UID, as unshare
 * --map-root-user does. Some systems (e.g. Ubuntu's AppArmor userns
 * restriction) allow creating one but deny writing the map. Returns 0
 * or the errno of the failure. */
static int can_map_uid(void)
{
    int fds[2];
    if (pipe(fds) < 0) { perror("pipe"); exit(2); }

    char map[64];
    snprintf(map, sizeof(map), "0 %d 1", (int)getuid());

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(2); }
    if (pid == 0) {
        int fd, e = 0;
        close(fds[0]);
        if (unshare(CLONE_NEWUSER) < 0 ||
            (fd = open("/proc/self/uid_map", O_WRONLY)) < 0 ||
            write(fd, map, strlen(map)) < 0)
            e = errno;
        _exit(write(fds[1], &e, sizeof(e)) != sizeof(e));
    }
    close(fds[1]);
    int e = EIO;
    if (read(fds[0], &e, sizeof(e)) != sizeof(e)) e = EIO;
    close(fds[0]);
    waitpid(pid, NULL, 0);
    return e;
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
    snprintf(unshare_bin, sizeof(unshare_bin), "%s/unshare", argv[1]);
    snprintf(nsenter_bin, sizeof(nsenter_bin), "%s/nsenter", argv[1]);

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

    /* === User namespaces ===
     * Creating or joining one clears the securebits, so unshare and
     * nsenter must set the exec ones again, locks included.
     */
    printf("=== User namespaces ===\n");
    set_secbits(SECBIT_EXEC_RESTRICT_FILE_LOCKED | SECBIT_EXEC_DENY_INTERACTIVE_LOCKED);
    snprintf(bits, sizeof(bits), "%d", prctl(PR_GET_SECUREBITS));

    int release;
    pid_t target = spawn_userns(&release);
    if (target < 0) {
        printf("SKIP: cannot create user namespaces\n");
    } else {
        char target_pid[32];
        snprintf(target_pid, sizeof(target_pid), "%d", (int)target);

        report(run((char *[]){ unshare_bin, "--user",
                               "--", self, "--secbits", bits, NULL }) == 0,
               "unshare --user keeps exec securebits");
        int map_err = can_map_uid();
        if (map_err)
            printf("SKIP: unshare --map-root-user: cannot write uid_map: %s\n",
                   strerror(map_err));
        else
            report(run((char *[]){ unshare_bin, "--map-root-user",
                                   "--", self, "--secbits", bits, NULL }) == 0,
                   "unshare --map-root-user keeps exec securebits");
        report(run((char *[]){ nsenter_bin, "--user", "--preserve-credentials",
                               "--target", target_pid,
                               "--", self, "--secbits", bits, NULL }) == 0,
               "nsenter --user keeps exec securebits");

        close(release);
        waitpid(target, NULL, 0);
    }

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
