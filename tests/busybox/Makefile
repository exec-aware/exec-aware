# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

CC      ?= gcc
BUSYBOX ?= busybox

.PHONY: all check clean

all: test

test: test.c
	$(CC) -o $@ $<

check: test
	./test $(BUSYBOX)

clean:
	rm -f test
