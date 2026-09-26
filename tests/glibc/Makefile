# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

CC ?= gcc
# Dynamic loader of the glibc under test
LDSO ?= /lib64/ld-linux-x86-64.so.2

# Use the glibc under test instead of the toolchain's glibc
USE_LDSO = -Wl,--dynamic-linker=$(LDSO) -Wl,-rpath,$(dir $(LDSO))

.PHONY: all check clean

all: lib.so prog test

lib.so: lib.c
	$(CC) -shared -fPIC -Wl,-soname,lib.so -o $@ $<

prog: prog.c lib.so
	$(CC) $(USE_LDSO) -Wl,-rpath,'$$ORIGIN' -o $@ $< -L. -l:lib.so

test: test.c
	$(CC) $(USE_LDSO) -o $@ $<

check: all
	./test $(LDSO) ./lib.so ./prog

clean:
	rm -f lib.so prog test
