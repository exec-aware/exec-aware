<!--
SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>

SPDX-License-Identifier: Apache-2.0
-->

# python

Website: https://www.python.org/

The installed standard library must be executable for Python to start when
`SECBIT_EXEC_RESTRICT_FILE` is set, since it is imported through the same
checks as any other module.
`make install` installs it with mode 644, so mark the `.py` and `.pyc` files
executable afterwards, as [python.nix](../../packaging/nix/python.nix) does.

## multiprocessing

When `SECBIT_EXEC_DENY_INTERACTIVE` is set, only the `fork` start method
works.
The `spawn` and `forkserver` start methods (`forkserver` is the default on
Linux) start new interpreters with `-c` and send them the code to run as
pickles over a pipe, so they are blocked.
With `forkserver`, starting a process raises `ConnectionResetError`.
With `spawn`, a `Process` exits with code 1 and a `Pool` hangs, restarting
its workers as they fail.
Select `fork` explicitly:

```python
multiprocessing.set_start_method('fork')
# or
executor = ProcessPoolExecutor(mp_context=multiprocessing.get_context('fork'))
```

`multiprocessing.shared_memory` still works, but its resource tracker is
also started with `-c`, so it prints a `PermissionError` and cannot clean up
segments that were never unlinked.

Starting the children with `-m` instead is not a fix: a module that runs
pickles read from a file descriptor would let anyone run code from a pipe.

## v3.14

Version: 3.14.7

Download url: https://www.python.org/ftp/python/3.14.7/Python-3.14.7.tar.xz

Signature: https://www.python.org/ftp/python/3.14.7/Python-3.14.7.tar.xz.sigstore

Signing identity: hugo@python.org (issuer https://github.com/login/oauth)

Sha256: 3b48dac8fb59f62eaa67ac83c1eb12bda1b7a08406dd286e252c11a66be27f81
