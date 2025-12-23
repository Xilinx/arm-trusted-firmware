/*
 * Copyright The Transfer List Library Contributors
 *
 * SPDX-License-Identifier: MIT OR GPL-2.0-or-later
 */

#if !defined(TEST_H)
#define TEST_H

#define TL_MAX_SIZE 0x10000
#define TL_SIZE 0x1000

enum {
	TAG_GENERIC_START = 0,
	TAG_GENERIC_END = 0x7fffff,
	TAG_NON_STANDARD_START = 0xfff000,
	TAG_NON_STANDARD_END = 0xffffff,
	TAG_OUT_OF_BOUND = (1 << 24),
};

const unsigned int test_tag = 1;
int test_data = 0xdeadbeef;

#endif /* TEST_H */
