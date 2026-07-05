<!--
SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>

SPDX-License-Identifier: Apache-2.0
-->

# exec-aware patch workflow

This explains the recommended workflow for creating exec-aware patches.

## Tools

- [git][]
- [stacked-git][]
- A patched [util-linux][]

## Steps

First, obtain the source code for the upstream project.
If it is not already in git, create a repository and commit all the files.

Second, initialize stacked git and create a new patch:

```sh
stg init
stg new my-exec-aware-patch -m 'commit description'
```

After making changes, run `stg refresh` to incorporate edits into the patch.
Repeat `stg refresh` as needed while iterating.
When finished, export the patch series back to this repository.

## What to patch

The core principle, drawn from the [ClipOS][] project, is:
**exec permission is a property of every file's role, not just its entry point.**
Any filesystem object that an interpreter opens with the intent to execute its
content as code must be checked.

### EXEC_RESTRICT_FILE: filesystem-path sources

Use `EXEC_RESTRICT_FILE` for every file the interpreter opens from the filesystem
for execution. This includes:

**Entry-point files** — the script or program passed as a command-line argument:

```sh
sh script.sh                  # check fd opened for script.sh
python prog.py                # check fd opened for prog.py
lua prog.lua                  # check fd opened for prog.lua
```

**Sourced and included files** — files loaded for execution in the current
interpreter context, such as the shell `.`/`source` builtin.

```sh
. helper.sh                   # check fd opened for helper.sh
```

```sh
$ printf 'echo "hi from helper.sh"' >> helper.sh
$ printf '#!/bin/sh\n. helper.sh\n' >> myscript.sh
$ chmod 755 myscript.sh
$ chmod 644 helper.sh
$ setpriv --securebits +exec_restrict_file -- /bin/patched-sh myscript.sh
/bin/patched-sh: can't execute 'helper.sh': blocked by EXEC_RESTRICT_FILE
```

**Module and library imports** — files loaded via `import`, `require`, `use`,
or any equivalent language primitive. A module is code; its execute permission
must be checked the same way as any other code file:

```python
# Python
import mymodule               # check fd opened for mymodule.py
```

```perl
# Perl
require MyModule;             # check fd opened for MyModule.pm
```

**Environment-variable startup files** — files named by environment variables
that the interpreter loads as code (e.g. `LUA_INIT=@startup.lua`,
`PYTHONSTARTUP=/home/user/startup.py`). These are filesystem objects and fall
under `EXEC_RESTRICT_FILE`.

### EXEC_DENY_INTERACTIVE: non-filesystem sources

Use `EXEC_DENY_INTERACTIVE` for code that does not come from a filesystem
object:

- **Command-line code arguments**: `-c 'code'`, `-e 'code'`, etc.
  ```sh
  /bin/sh -c 'echo blocked'
  perl -e 'print "blocked\n"'
  ```
- **Environment-variable inline code**: env vars whose value is a code string
  rather than a file reference (e.g. `LUA_INIT='print("hi")'`).
- **Standard input** — try `execveat(0, "", …, AT_EMPTY_PATH | AT_EXECVE_CHECK)`
  first. If it succeeds (stdin is a regular file with execute permission, as in
  `/bin/sh < prog.sh`), allow execution. Only deny when the check fails (stdin
  is a pipe, TTY, socket, or non-executable file).
  ```sh
  echo 'echo blocked' | /bin/sh          # pipe: check fails → deny
  /bin/sh < prog.sh                      # executable file: check passes → allow
  ```

### Quick-reference table

| Source | Securebit |
|--------|-----------|
| Script file argument | `EXEC_RESTRICT_FILE` |
| Sourced/included file (`.`, `dofile`, `execfile`) | `EXEC_RESTRICT_FILE` |
| Module/library import (`import`, `require`, `use`) | `EXEC_RESTRICT_FILE` |
| Env-var startup file (`LUA_INIT=@file`, `PYTHONSTARTUP`) | `EXEC_RESTRICT_FILE` |
| CLI code argument (`-c`, `-e`) | `EXEC_DENY_INTERACTIVE` |
| Env-var inline code (`LUA_INIT='code'`) | `EXEC_DENY_INTERACTIVE` |
| Stdin — non-executable file, pipe, or TTY | `EXEC_DENY_INTERACTIVE` |
| Stdin — regular file with execute permission | allow |

## What belongs in a patch

Each individual patch should implement exec-awareness checks for a single
component of the project.
If this is challenging to do (for example if multiple components share the
same file-loading code path), do not split the patches.
Having multiple patch files is done to patch each part of a project separately,
not to split the single feature's implementation.

## Exec awareness decision tree

When deciding whether to add a check, ask:
**"Is this opening a filesystem object to execute its content as code?"**

- If yes → consult `EXEC_RESTRICT_FILE` after checking executability.
- If yes, but the source is not a file (CLI arg, inline env var, pipe, TTY) → consult `EXEC_DENY_INTERACTIVE`.
- If no (opening a file for data, user I/O, or configuration that is not eval'd) → no check needed.

The distinction is intent and role.
A `.py` file executed by `import` is code; a `.json` file read by `open()` is
data.
Config files that get `eval`'d and data files with embedded scripts require
judgment: when in doubt, consider whether a security administrator would want to
independently control execution permission for that specific file path.

[git]: https://git-scm.com/
[stacked-git]: https://stacked-git.github.io/
[ClipOS]: https://clip-project.github.io/
[util-linux]: https://github.com/util-linux/util-linux/pull/4461
