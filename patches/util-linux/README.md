<!--
SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>

SPDX-License-Identifier: Apache-2.0
-->

# util-linux

Website: https://github.com/util-linux/util-linux

util-linux doesn't interpret code itself, but a few of its programs can be
used to get around the checks in exec-aware programs.
These patches close those gaps.
The first two patches backport `setpriv --securebits` support for the exec
securebits ([util-linux#4461][]) and its man page follow-up, which were
merged after 2.42.

## seccomp filters

A seccomp filter decides what every system call returns, including the
`prctl(PR_GET_SECUREBITS)` and `execveat()` calls that exec-aware programs
rely on.
A filter that makes `prctl()` fail turns the checks off, and one that makes
`execveat()` return 0 makes every file pass them, so loading a filter is
running code:

- `setpriv --seccomp-filter FILE` loads the filter from a file, so the file
  is checked under `SECBIT_EXEC_RESTRICT_FILE`.
  The check uses the securebits `setpriv` was started with, since the filter
  is loaded before `--securebits` is applied.
- `enosys` builds its filter from its command line, so it refuses to run a
  command when `SECBIT_EXEC_DENY_INTERACTIVE` is set.
  `enosys --dump` still works, so an administrator can approve a filter by
  saving it and giving it execute permission:

```sh
enosys --syscall fallocate --dump=fallocate.bpf
chmod +x fallocate.bpf
setpriv --seccomp-filter fallocate.bpf -- myprog
```

## Not patched

`flock -c`, `script -c`, `scriptlive -c`, `su -c`, `runuser -c`, `newgrp`,
and the shell escapes in `more` and `pg` run their commands with
`$SHELL -c`.
The shell checks `SECBIT_EXEC_DENY_INTERACTIVE` itself if it is exec-aware.
`scriptlive` types its typescript into an interactive shell over a
pseudoterminal, which an exec-aware shell treats like any other terminal
input.

## v2.42

Version: 2.42.4

Url: https://www.kernel.org/pub/linux/utils/util-linux/v2.42/util-linux-2.42.4.tar.xz

Signature: https://www.kernel.org/pub/linux/utils/util-linux/v2.42/util-linux-2.42.4.tar.sign

Sha256: fbd62a100ab7bb8746ba0661255c3c48185b1e9021507c624da01fbc696330ec

The patches change man pages, so building from the release tarball needs
asciidoctor to regenerate them.

[util-linux#4461]: https://github.com/util-linux/util-linux/pull/4461
