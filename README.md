<!--
SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>

SPDX-License-Identifier: Apache-2.0
-->

# exec-aware

Patches that let you enforce executability checks.

## Executability Checking

Linux added the [Executability Check][] API in 6.14, which allows userspace
programs to check that a program is executable before running it.
This is combined with an unprivileged securebit setting that allows programs
to opt into enforcement.

## Background

Let's assume that you are implementing a simple web server in Python that
handles untrusted user data.
It is a useful security invariant to enforce that your application code
(ie. the .py files you wrote) should not be confused with the application
state (ie. your clients' files that you are storing).
Your application's files don't change during runtime, and can be executed.
The untrused state files will definitely change, and you should make sure
you don't accidentally execute them.
This enforced separation is sometimes known as [W^X][].

On Linux, you can enforce some parts of W^X via [eBPF][].
[systemd][] [261][systemd-261]'s RestrictFileSystemAccess= is an example
of this in action.
But it has some major limitations: installing eBPF programs is a privileged
action, and the eBPF code has no way to distinguish between a call to
`open()` a file for Python to interpret and a call to `open()` a file
to serve to the web.

## How it should work

The core principle, drawn from the [ClipOS][] project, is that exec permission
is a property of every file's *role*, not just for native binaries.
Any file an interpreter opens to execute as code should be checked — including
entry-point scripts, sourced/included files, and module/library imports.

Programs that are *exec-aware* should perform the following actions:

1. Call `execveat(fd, "", NULL, NULL, AT_EXECVE_CHECK | AT_EMPTY_PATH)` on
   every file descriptor opened to execute its content as code. This includes
   the script passed on the command line, files loaded via `.` or `source`,
   and files loaded via `import`, `require`, or any equivalent.

2. If that call fails (the file is not executable):
    - For file descriptors opened from the filesystem by path: query
      `SECBIT_EXEC_RESTRICT_FILE`. If set, abort.
    - For stdin or other interactive file descriptor: query
      `SECBIT_EXEC_DENY_INTERACTIVE`. If set, abort.
      This distinction is just for debugging convenience.

3. Before executing code from a non-file-descriptor source -- a command-line
   argument such as `sh -c '...'`, or an environment variable whose value is
   code rather than a file path -- query `SECBIT_EXEC_DENY_INTERACTIVE`. If
   set, the program must not interpret it.

## Seeing which files are checked

You can list every file an exec-aware program checks by running it under
[strace][]. This needs no special privileges:

```sh
strace -f -qq -y -e trace=execveat -e signal=none -- myprog
```

- `-f` follows child processes and threads.
- `-qq` hides strace's messages about attaching to and exiting processes.
- `-y` prints the path behind each file descriptor.
- `-e trace=execveat` shows only `execveat()` calls.
- `-e signal=none` hides signal deliveries.

For example, running a patched `dash` on a script that sources a
non-executable `helper.sh`:

```console
$ strace -f -qq -y -e trace=execveat -e signal=none -- dash myscript.sh
execveat(3</home/user/myscript.sh>, "", NULL, NULL, AT_EMPTY_PATH|AT_EXECVE_CHECK) = 0
execveat(3</home/user/helper.sh>, "", NULL, NULL, AT_EMPTY_PATH|AT_EXECVE_CHECK) = -1 EACCES (Permission denied)
hi from helper.sh
```

Every line containing `AT_EXECVE_CHECK` records one check. The checked file
appears in angle brackets after the file descriptor, and the return value
gives the result: `0` means the file may be executed, and `-1 EACCES` means
it may not. Code read from a pipe on stdin appears as `0<pipe:[...]>`.
Lines without `AT_EXECVE_CHECK` are real `execveat()` calls, not checks.
Use `-o FILE` to write the trace to a file instead of mixing it with the
program's stderr.

## What this repo contains

The [`patches`](./patches/README.md) directory contains git patch files
that implement execution awareness for the project at a specific version.
These are maintained via [stgit][].

The `packaging` directory is used to build these projects for downstream
consumption.

## Q/A

Q: Why do you always call `execveat()`, even if the securebit isn't set?

A: This enables a form of "auditing mode" for the program.
With this call, an administrator can use eBPF or similar to determine
which files were executed, to avoid issues when the check is enforced.

Q: How do you enable this enforcement mode?

A: Once my `util-linux` [commit][util-linux-commit] is added to a release,
you will be able to use:

```sh
setpriv --securebits +exec_restrict_file,+exec_restrict_file_locked,+exec_deny_interactive,+exec_deny_interactive_locked -- myprog
```

The `_locked` variants prevent child processes from turning off the securebit:
when testing you may want to remove those settings so that you can turn off
the setting as needed:

```sh
$ # Assume /bin/sh isn't exec-aware for this example
$ setpriv --securebits +exec_restrict_file,+exec_deny_interactive -- /bin/sh
sh$ python -c 'print("success")' || echo failed
failed
sh$ setpriv --securebits -exec_deny_interactive -- python -c 'print("success")'
success
```

I also submitted a [PR][systemd-pr] to `systemd`, so in version 262 or later you can use

```conf
[Exec]
# Note: these settings will be silently ignored on systemd 261 or below
SecureBits=exec-restrict-file exec-restrict-file-locked
SecureBits=exec-deny-interactive exec-deny-interactive-locked
```

Q: Why don't you enforce the check if `prctl(PR_GET_SECUREBITS)` fails?

A: This allows the binary to run without issue on older kernels, or in
situations where the `prctl` syscall has been disabled in some manner.

This does highlight a weakness in the security model, which is that a bad
actor may be able to chain together otherwise benign binaries to get code
execution. Ex.

```sh
$ setpriv --securebits +exec_deny_interactive -- python -c 'print("success")'
$ setpriv --securebits +exec_deny_interactive -- enosys --syscall prctl -- python -c 'print("success")'
success
```

[Executability Check]: https://docs.kernel.org/userspace-api/check_exec.html
[ClipOS]: https://clip-project.github.io/
[W^X]: https://en.wikipedia.org/wiki/W%5EX
[eBPF]: https://ebpf.io
[systemd]: https://systemd.io
[systemd-261]: https://mastodon.social/@pid_eins/116792552048067669
[stgit]: https://stacked-git.github.io/
[strace]: https://strace.io/
[util-linux-commit]: https://github.com/util-linux/util-linux/commit/eeebf6c451c7efe334ad9d0580cf2c60ad750ad6
[systemd-pr]: https://github.com/systemd/systemd/pull/43376
