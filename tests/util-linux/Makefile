# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

CC             ?= gcc
# Directory containing the patched setpriv and enosys
UTIL_LINUX_BIN ?= /usr/bin

.PHONY: all check clean

all: test

test: test.c
	$(CC) -o $@ $<

check: test
	./test $(UTIL_LINUX_BIN)

clean:
	rm -f test
