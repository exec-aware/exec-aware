/*
 * SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests exec-awareness in bash.
 *
 * Usage: ./test <bash-binary>
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

static const char *bash;
static char script[64];
static char sourced[64];
static char marker[64];
static int failures;

/* Fork and exec argv[] with envp[]; redirect stdin from `in`;
 * suppress stdout+stderr. */
static int run(const char *in, char *const envp[], char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(2); }
    if (pid == 0) {
        int in_fd = open(in, O_RDONLY);
        int null_w = open("/dev/null", O_WRONLY);
        dup2(in_fd, STDIN_FILENO);
        dup2(null_w, STDOUT_FILENO);
        dup2(null_w, STDERR_FILENO);
        close(in_fd);
        close(null_w);
        execve(argv[0], argv, envp);
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

/* Pass if `marker` exists exactly when `want_exists` is set, then remove it. */
static void check_marker(int want_exists, const char *desc)
{
    int exists = access(marker, F_OK) == 0;
    int ok = exists == want_exists;
    printf("%s: %s\n", ok ? "PASS" : "FAIL", desc);
    if (!ok) failures++;
    unlink(marker);
}

static void set_secbits(int add)
{
    int cur = prctl(PR_GET_SECUREBITS);
    if (cur < 0 || prctl(PR_SET_SECUREBITS, cur | add) < 0) {
        perror("prctl(PR_SET_SECUREBITS)");
        exit(2);
    }
}

static void write_file(const char *path, const char *content)
{
    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd < 0) { perror("open"); exit(2); }
    if (write(fd, content, strlen(content)) < 0) { perror("write"); exit(2); }
    close(fd);
}

/* Build a wrapper script that sources `inner`.  The wrapper exits 42 if
 * bash refuses to source `inner`, which exits 0. */
static void write_wrapper(const char *wrapper, const char *inner)
{
    char buf[256];
    snprintf(buf, sizeof(buf), ". %s\nexit 42\n", inner);
    write_file(wrapper, buf);
}

static void make_temp(char *buf, size_t size, const char *template)
{
    strncpy(buf, template, size - 1);
    int fd = mkstemp(buf);
    if (fd < 0) { perror("mkstemp"); exit(2); }
    close(fd);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <bash>\n", argv[0]);
        return 2;
    }
    bash = argv[1];

    make_temp(script, sizeof(script), "/tmp/bash-exec-test-XXXXXX");
    make_temp(sourced, sizeof(sourced), "/tmp/bash-exec-src-XXXXXX");
    make_temp(marker, sizeof(marker), "/tmp/bash-exec-marker-XXXXXX");
    unlink(marker);
    write_file(sourced, "exit 0\n");

    char *const noenv[] = { NULL };
    char bash_env[128], ps4[128], func[128], bash_env_expand[128];
    snprintf(bash_env, sizeof(bash_env), "BASH_ENV=%s", sourced);
    snprintf(bash_env_expand, sizeof(bash_env_expand),
             "BASH_ENV=$(: > %s)", marker);
    snprintf(ps4, sizeof(ps4), "PS4=$(: > %s)", marker);
    snprintf(func, sizeof(func), "BASH_FUNC_f%%%%=() { : > %s; }", marker);
    char *const env_bash_env[] = { bash_env, NULL };
    char *const env_bash_env_expand[] = { bash_env_expand, NULL };
    char *const env_ps4[] = { ps4, NULL };
    char *const env_func[] = { func, NULL };

    /* === Audit mode (no securebits set) === */
    printf("=== Audit mode ===\n");

    chmod(script, 0644);
    chmod(sourced, 0644);
    write_wrapper(script, sourced);
    check(1, run("/dev/null", noenv, (char *[]){ (char *)bash, script, NULL }),
          "non-executable script and sourced file run");
    check(1, run("/dev/null", noenv, (char *[]){ (char *)bash, "-c", "exit 0", NULL }),
          "-c runs");
    check(1, run("/dev/null", noenv, (char *[]){ (char *)bash, NULL }),
          "stdin runs");

    /* Environment code runs in audit mode. */
    write_file(script, "f\nset -x\n:\nexit 0\n");
    run("/dev/null", env_func, (char *[]){ (char *)bash, script, NULL });
    check_marker(1, "exported function is imported");
    run("/dev/null", env_ps4, (char *[]){ (char *)bash, script, NULL });
    check_marker(1, "PS4 is imported");
    run("/dev/null", env_bash_env_expand, (char *[]){ (char *)bash, script, NULL });
    check_marker(1, "BASH_ENV is expanded");

    /* === EXEC_RESTRICT_FILE === */
    printf("=== EXEC_RESTRICT_FILE ===\n");
    set_secbits(SECBIT_EXEC_RESTRICT_FILE);

    chmod(script, 0755);
    chmod(sourced, 0755);
    write_wrapper(script, sourced);
    check(1, run("/dev/null", noenv, (char *[]){ (char *)bash, script, NULL }),
          "executable script and sourced file run");

    /* Non-executable sourced file: wrapper is executable but inner is not. */
    chmod(sourced, 0644);
    check(0, run("/dev/null", noenv, (char *[]){ (char *)bash, script, NULL }),
          "non-executable sourced file blocked");

    /* Non-executable entry-point: inner is executable, wrapper is not. */
    chmod(script, 0644);
    chmod(sourced, 0755);
    check(0, run("/dev/null", noenv, (char *[]){ (char *)bash, script, NULL }),
          "non-executable entry-point script blocked");

    /* $BASH_ENV exits 0 before the script runs; the script exits 42. */
    chmod(script, 0755);
    write_file(script, "exit 42\n");
    check(1, run("/dev/null", env_bash_env, (char *[]){ (char *)bash, script, NULL }),
          "executable BASH_ENV runs");
    chmod(sourced, 0644);
    check(0, run("/dev/null", env_bash_env, (char *[]){ (char *)bash, script, NULL }),
          "non-executable BASH_ENV blocked");

    check(1, run("/dev/null", noenv, (char *[]){ (char *)bash, "-c", "exit 0", NULL }),
          "-c still runs");

    /* === EXEC_DENY_INTERACTIVE ===
     * EXEC_RESTRICT_FILE remains set.  These entry points use exec_deny_interactive()
     * and are unaffected by the file-restriction bit.
     */
    printf("=== EXEC_DENY_INTERACTIVE ===\n");
    set_secbits(SECBIT_EXEC_DENY_INTERACTIVE);

    check(0, run("/dev/null", noenv, (char *[]){ (char *)bash, "-c", "exit 0", NULL }),
          "-c blocked");
    /* stdin = /dev/null triggers the stdin exec check */
    check(0, run("/dev/null", noenv, (char *[]){ (char *)bash, NULL }),
          "stdin blocked");
    check(0, run("/dev/null", noenv, (char *[]){ (char *)bash, "-s", NULL }),
          "stdin with -s blocked");

    /* Executable file on stdin passes the check. */
    write_file(script, "exit 0\n");
    check(1, run(script, noenv, (char *[]){ (char *)bash, NULL }),
          "executable file on stdin runs");
    chmod(script, 0644);
    check(0, run(script, noenv, (char *[]){ (char *)bash, NULL }),
          "non-executable file on stdin blocked");

    /* Code from the environment is not run. */
    chmod(script, 0755);
    write_file(script, "f\nset -x\n:\nexit 0\n");
    run("/dev/null", env_func, (char *[]){ (char *)bash, script, NULL });
    check_marker(0, "exported function not imported");
    run("/dev/null", env_ps4, (char *[]){ (char *)bash, script, NULL });
    check_marker(0, "PS4 not imported");
    run("/dev/null", env_bash_env_expand, (char *[]){ (char *)bash, script, NULL });
    check_marker(0, "BASH_ENV not expanded");

    /* Executable script still runs: check_fd_exec succeeds,
     * exec_deny_interactive is not consulted for file sources. */
    chmod(sourced, 0755);
    write_wrapper(script, sourced);
    check(1, run("/dev/null", noenv, (char *[]){ (char *)bash, script, NULL }),
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
