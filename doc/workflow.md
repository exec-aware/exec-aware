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

Programs which behave like interpreters must perform the executability check
on every file they load at startup.
This means:

```sh
$ # Check the file descriptor after open()
$ /bin/sh myscript.sh
$ # Check file descriptor 0
$ /bin/sh < myscript.sh
```

But this *does not* need to happen for programming language features such
as `import` or `eval`.

```
$ cat myscript.sh
#!/bin/sh
. otherfile.sh
$ cat otherfile.sh
echo "Hello from otherfile.sh"
$ chmod 755 myscript.sh
$ chmod 644 otherfile.sh
$ setpriv --securebits +exec_restrict_file -- /bin/patched-sh otherfile.sh
/bin/patched-sh: otherfile.sh: Permission denied
$ setpriv --securebits +exec_restrict_file -- /bin/patched-sh myscript.sh
Hello from otherfile.sh
```

To help decide whether the check should be added, consider if it would make
sense for the file you are checking to be given a [shebang][].
If the answer is no (ex. if the file is imported from `main.py`),
don't perform the check.
The whole idea of the exec-aware patches is to align the behavior
of a shebang script with executing the interpreter against a file
directly.

The `EXEC_RESTRICT_FILE` securebit is used to determine whether a load from a
file is checked.
And "file" here is about filesystem objects -- execution through stdin is
handled by the other securebit.

Additionally, the `EXEC_DENY_INTERACTIVE` check must be able to deny execution
from non-file sources.
This includes, but is not limited to:

- Command line arguments (ex. `/bin/sh -c 'echo "blocked"'`)
- Environment variables (ex. `export LUA_INIT='print("blocked")'; lua`)
- Standard input (ex. `echo 'echo "blocked"' | /bin/sh`).
  Try `execveat(0, "", …, AT_EMPTY_PATH | AT_EXECVE_CHECK)` first;
  if it succeeds (ex. if stdin is a regular executable file as in
  `/bin/sh < prog.sh`), allow. Only deny if the check fails
  (stdin is a pipe, TTY, etc.).

## What belongs in a patch

Each individual patch should implement exec-awareness checks and interpreter
checks for a single component of the project.
If this is challenging to do (for example if multiple components share the
same entrypoint), do not split the patches.
Having multiple patch files is done to patch each part of a project separately,
not to split the single feature's implementation.

[git]: https://git-scm.com/
[stacked-git]: https://stacked-git.github.io/
[shebang]: https://en.wikipedia.org/wiki/Shebang_(Unix)
[util-linux]: https://github.com/util-linux/util-linux/pull/4461
