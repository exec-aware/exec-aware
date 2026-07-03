/*
 * SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests exec-awareness in musl's dlopen implementation.
 */
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>

#define SECBIT_EXEC_RESTRICT_FILE (1 << 8)

static int failures;

static void expect_dlopen(const char *lib, int should_succeed, const char *desc)
{
    void *h = dlopen(lib, RTLD_NOW);
    int ok = (h != NULL);
    if (ok) dlclose(h);

    if (ok == should_succeed) {
        printf("PASS: %s\n", desc);
    } else {
        printf("FAIL: %s\n      expected dlopen to %s; got: %s\n",
               desc,
               should_succeed ? "succeed" : "fail",
               ok ? "succeeded" : dlerror());
        failures++;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s lib.so\n", argv[0]);
        return 2;
    }
    const char *lib = argv[1];

    /* Audit mode */
    chmod(lib, 0755);
    expect_dlopen(lib, 1, "audit mode, executable .so: dlopen succeeds");
    chmod(lib, 0644);
    expect_dlopen(lib, 1, "audit mode, non-executable .so: dlopen succeeds");

    /* Enable enforcement */
    int bits = prctl(PR_GET_SECUREBITS);
    if (bits < 0 || prctl(PR_SET_SECUREBITS, bits | SECBIT_EXEC_RESTRICT_FILE) < 0) {
        perror("prctl");
        return 2;
    }

    /* Enforcement mode */
    chmod(lib, 0755);
    expect_dlopen(lib, 1, "enforcement mode, executable .so: dlopen succeeds");
    chmod(lib, 0644);
    expect_dlopen(lib, 0, "enforcement mode, non-executable .so: dlopen blocked");

    if (failures) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll tests passed\n");
    return 0;
}
