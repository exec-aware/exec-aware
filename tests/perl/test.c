/*
 * SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests exec-awareness in the Perl interpreter.
 *
 * Usage: ./test <perl-binary>
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

static const char *perl;
static char dir[64];
static char script[128];   /* exit 0; */
static char req[128];      /* require Mod; exit 0; */
static char dofile[128];   /* exit 0 if 'do $ARGV[0]' succeeds */
static char helper[128];   /* 1; */
static char module[128];   /* Mod.pm */
static char devel[128];    /* Devel/Mod.pm */
static int failures;

/*
 * Fork and exec argv[] with stdin redirected from `in`; suppress
 * stdout+stderr. If `env` is not NULL, it is added to the environment.
 */
static int run_with(const char *in, char *env, char *const argv[])
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
        if (env)
            putenv(env);
        execv(argv[0], argv);
        _exit(127);
    }
    int st;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static int run(char *const argv[])
{
    return run_with("/dev/null", NULL, argv);
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

static void write_file(char *path, const char *name, const char *content)
{
    snprintf(path, 128, "%s/%s", dir, name);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror(path); exit(2); }
    if (write(fd, content, strlen(content)) < 0) { perror(path); exit(2); }
    close(fd);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <perl>\n", argv[0]);
        return 2;
    }
    perl = argv[1];

    /* Create a directory of minimal Perl files; it doubles as an @INC entry. */
    strncpy(dir, "/tmp/perl-exec-test-XXXXXX", sizeof(dir) - 1);
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 2; }
    char devel_dir[128];
    snprintf(devel_dir, sizeof(devel_dir), "%s/Devel", dir);
    if (mkdir(devel_dir, 0755) < 0) { perror("mkdir"); return 2; }

    write_file(script, "script.pl", "exit 0;\n");
    write_file(req, "req.pl", "require Mod; exit 0;\n");
    write_file(dofile, "do.pl", "exit(defined(do $ARGV[0]) ? 0 : 1);\n");
    write_file(helper, "helper.pl", "1;\n");
    write_file(module, "Mod.pm", "package Mod; 1;\n");
    /* -d:Mod needs a DB::DB routine to call for each statement */
    write_file(devel, "Devel/Mod.pm", "package Devel::Mod; sub DB::DB {} 1;\n");
    chmod(req, 0755);
    chmod(dofile, 0755);
    chmod(devel, 0755);

    char *inc = malloc(strlen(dir) + 3);
    sprintf(inc, "-I%s", dir);

    /* === Audit mode (no securebits set) === */
    printf("=== Audit mode ===\n");
    check(1, run((char *[]){ (char *)perl, script, NULL }),
          "non-executable script runs");
    check(1, run((char *[]){ (char *)perl, inc, req, NULL }),
          "non-executable module loads");

    /* === EXEC_RESTRICT_FILE === */
    printf("=== EXEC_RESTRICT_FILE ===\n");
    set_secbits(SECBIT_EXEC_RESTRICT_FILE);

    chmod(script, 0755);
    check(1, run((char *[]){ (char *)perl, script, NULL }),
          "executable script runs");
    chmod(script, 0644);
    check(0, run((char *[]){ (char *)perl, script, NULL }),
          "non-executable script blocked");
    chmod(script, 0755);

    chmod(module, 0755);
    check(1, run((char *[]){ (char *)perl, inc, req, NULL }),
          "require of executable module runs");
    chmod(module, 0644);
    check(0, run((char *[]){ (char *)perl, inc, req, NULL }),
          "require of non-executable module blocked");
    check(0, run((char *[]){ (char *)perl, inc, "-MMod", script, NULL }),
          "-M of non-executable module blocked");
    chmod(module, 0755);

    chmod(helper, 0755);
    check(1, run((char *[]){ (char *)perl, dofile, helper, NULL }),
          "do of executable file runs");
    chmod(helper, 0644);
    check(0, run((char *[]){ (char *)perl, dofile, helper, NULL }),
          "do of non-executable file blocked");

    check(1, run((char *[]){ (char *)perl, "-Mstrict", "-Mwarnings", script, NULL }),
          "core modules load");

    /* === EXEC_DENY_INTERACTIVE ===
     * EXEC_RESTRICT_FILE remains set; these entry points use
     * exec_deny_interactive() and are unaffected by the file-restriction bit.
     * Code that is blocked would otherwise exit 0.
     */
    printf("=== EXEC_DENY_INTERACTIVE ===\n");
    set_secbits(SECBIT_EXEC_DENY_INTERACTIVE);

    check(0, run((char *[]){ (char *)perl, "-e", "exit 0", NULL }),
          "-e blocked");
    check(0, run((char *[]){ (char *)perl, "-E", "exit 0", NULL }),
          "-E blocked");
    /* stdin = /dev/null: not an executable file */
    check(0, run((char *[]){ (char *)perl, NULL }),
          "stdin blocked");
    check(0, run((char *[]){ (char *)perl, "-", NULL }),
          "stdin as '-' blocked");
    check(1, run_with(script, NULL, (char *[]){ (char *)perl, NULL }),
          "stdin from executable file runs");

    check(1, run((char *[]){ (char *)perl, inc, "-MMod", script, NULL }),
          "-M with a module name runs");
    check(1, run((char *[]){ (char *)perl, inc, "-M-Mod", script, NULL }),
          "-M- with a module name runs");
    check(1, run((char *[]){ (char *)perl, inc, "-mMod", script, NULL }),
          "-m with a module name runs");
    check(1, run((char *[]){ (char *)perl, inc, "-MMod=a,b", script, NULL }),
          "-M with an import list runs");
    check(1, run((char *[]){ (char *)perl, "-M5.010", script, NULL }),
          "-M with a version runs");
    check(0, run((char *[]){ (char *)perl, inc, "-MMod;exit(0)", script, NULL }),
          "-M with code blocked");
    check(0, run((char *[]){ (char *)perl, inc, "-MMod qw(a)", script, NULL }),
          "-M with a quoted import list blocked");
    check(1, run_with("/dev/null", "PERL5OPT=-MMod",
                      (char *[]){ (char *)perl, inc, script, NULL }),
          "PERL5OPT -M with a module name runs");
    check(0, run_with("/dev/null", "PERL5OPT=-MMod;exit(0)",
                      (char *[]){ (char *)perl, inc, script, NULL }),
          "PERL5OPT -M with code blocked");

    check(0, run((char *[]){ (char *)perl, "-d", script, NULL }),
          "-d blocked");
    check(0, run_with("/dev/null", "PERL5DB=exit(0)",
                      (char *[]){ (char *)perl, "-d", script, NULL }),
          "-d with PERL5DB blocked");
    check(1, run((char *[]){ (char *)perl, inc, "-d:Mod", script, NULL }),
          "-d:Mod runs");
    check(1, run((char *[]){ (char *)perl, inc, "-d:Mod=a,b", script, NULL }),
          "-d:Mod with an import list runs");
    check(0, run((char *[]){ (char *)perl, inc, "-d:Mod;exit(0)", script, NULL }),
          "-d:Mod with code blocked");
    check(0, run((char *[]){ (char *)perl, inc, "-d:Mod=a});exit(0);({", script, NULL }),
          "-d:Mod with code in the import list blocked");

    check(1, run((char *[]){ (char *)perl, "-F:", script, NULL }),
          "-F with a plain pattern runs");
    check(0, run((char *[]){ (char *)perl, "-F/:/", script, NULL }),
          "-F with a verbatim pattern blocked");

    /* Executable files still run: check_fd_exec succeeds, exec_restrict_file
     * is never reached, exec_deny_interactive is not consulted for file sources. */
    check(1, run((char *[]){ (char *)perl, script, NULL }),
          "executable script still runs with both bits set");
    check(1, run((char *[]){ (char *)perl, inc, req, NULL }),
          "executable module still loads with both bits set");

    unlink(script);
    unlink(req);
    unlink(dofile);
    unlink(helper);
    unlink(module);
    unlink(devel);
    rmdir(devel_dir);
    rmdir(dir);
    free(inc);

    if (failures) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll tests passed\n");
    return 0;
}
