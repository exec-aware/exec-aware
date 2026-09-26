# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

CC   ?= gcc
PERL ?= perl

.PHONY: all check clean

all: test

test: test.c
	$(CC) -o $@ $<

check: test
	./test $(PERL)

clean:
	rm -f test
