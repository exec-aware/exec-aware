/*
 * SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Linked against lib.so, so it is loaded as a DT_NEEDED dependency */
int exec_aware_test_marker(void);

int main(void) { return exec_aware_test_marker(); }
