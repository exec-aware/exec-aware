/*
 * SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests exec-awareness in glibc's dynamic loader.
 *
 * Every object ld.so loads from the filesystem is checked, so this covers
 * dlopen, DT_NEEDED dependencies, and running a program via ld.so directly.
 */
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SECBIT_EXEC_RESTRICT_FILE (1 << 8)

static int failures;

static void report(int ok, int should_succeed, const char *what,
                   const char *err, const char *desc)
{
    if (ok == should_succeed) {
        printf("PASS: %s\n", desc);
    } else {
        printf("FAIL: %s\n      expected %s to %s; got: %s\n",
               desc, what,
               should_succeed ? "succeed" : "fail",
               ok ? "succeeded" : err);
        failures++;
    }
}

static void expect_dlopen(const char *lib, int should_succeed, const char *desc)
{
    void *h = dlopen(lib, RTLD_NOW);
    int ok = (h != NULL);
    const char *err = ok ? NULL : dlerror();
    if (ok) dlclose(h);

    report(ok, should_succeed, "dlopen", err, desc);
}

/* Run argv[0] with stderr silenced; succeeds if it exits with status 0 */
static void expect_run(char *const argv[], int should_succeed, const char *desc)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        failures++;
        return;
    }
    if (pid == 0) {
        int fd = open("/dev/null", O_WRONLY);
        if (fd >= 0) dup2(fd, STDERR_FILENO);
        execv(argv[0], argv);
        _exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        failures++;
        return;
    }
    int ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    report(ok, should_succeed, argv[0], "non-zero exit", desc);
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s ld.so lib.so prog\n", argv[0]);
        return 2;
    }
    char *ldso = argv[1];
    char *lib = argv[2];
    char *prog = argv[3];
    char *const run_prog[] = { prog, NULL };
    char *const run_ldso[] = { ldso, prog, NULL };

    /* Audit mode */
    chmod(lib, 0755);
    expect_dlopen(lib, 1, "audit mode, executable .so: dlopen succeeds");
    chmod(lib, 0644);
    expect_dlopen(lib, 1, "audit mode, non-executable .so: dlopen succeeds");
    expect_run(run_prog, 1, "audit mode, non-executable DT_NEEDED: prog runs");
    chmod(lib, 0755);
    chmod(prog, 0644);
    expect_run(run_ldso, 1, "audit mode, non-executable prog: ld.so prog runs");
    chmod(prog, 0755);

    /* Enable enforcement (inherited by child processes) */
    int bits = prctl(PR_GET_SECUREBITS);
    if (bits < 0 || prctl(PR_SET_SECUREBITS, bits | SECBIT_EXEC_RESTRICT_FILE) < 0) {
        perror("prctl");
        return 2;
    }

    /* Enforcement mode */
    chmod(lib, 0755);
    expect_dlopen(lib, 1, "enforcement mode, executable .so: dlopen succeeds");
    expect_run(run_prog, 1, "enforcement mode, executable DT_NEEDED: prog runs");
    expect_run(run_ldso, 1, "enforcement mode, executable prog: ld.so prog runs");
    chmod(lib, 0644);
    expect_dlopen(lib, 0, "enforcement mode, non-executable .so: dlopen blocked");
    expect_run(run_prog, 0, "enforcement mode, non-executable DT_NEEDED: prog blocked");
    chmod(lib, 0755);
    chmod(prog, 0644);
    expect_run(run_ldso, 0, "enforcement mode, non-executable prog: ld.so prog blocked");
    chmod(prog, 0755);

    if (failures) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll tests passed\n");
    return 0;
}
