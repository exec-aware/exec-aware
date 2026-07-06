# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

CC   ?= gcc
DASH ?= dash

.PHONY: all check clean

all: test

test: test.c
	$(CC) -o $@ $<

check: test
	./test $(DASH)

clean:
	rm -f test
