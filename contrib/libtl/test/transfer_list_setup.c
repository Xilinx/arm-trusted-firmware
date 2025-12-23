/*
 * Copyright The Transfer List Library Contributors
 *
 * SPDX-License-Identifier: MIT OR GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "transfer_list.h"
#include "unity.h"

void *buffer = NULL;

void test_init()
{
	struct transfer_list_header *tl = buffer;

	/* Attempt to init TL with 0 size */
	TEST_ASSERT_NULL(transfer_list_init(tl, 0));
	TEST_ASSERT(transfer_list_check_header(tl) == TL_OPS_NON);

	/* Init TL with an invalid memory address */
	TEST_ASSERT_NULL(transfer_list_init((void *)1, TL_SIZE));
	TEST_ASSERT(transfer_list_check_header(tl) == TL_OPS_NON);

	/* Valid memory address and large enough TL */
	TEST_ASSERT(transfer_list_init(tl, TL_SIZE));
	TEST_ASSERT(transfer_list_check_header(tl) == TL_OPS_ALL);
	TEST_ASSERT_NULL(transfer_list_next(tl, NULL));
}

void test_init_alignment()
{
	struct transfer_list_header *tl = buffer;
	memset(tl, 0, TL_SIZE);

	TEST_ASSERT_NULL(transfer_list_init((void *)tl + 0xff, TL_SIZE));
	TEST_ASSERT(transfer_list_check_header(tl) == TL_OPS_NON);
}

void test_relocate()
{
	struct transfer_list_header *tl, *new_tl;
	struct transfer_list_entry *te;

	void *new_buf = malloc(TL_SIZE * 2);
	unsigned int test_tag = 0x1;
	int test_payload = 0xdeadbeef;

	tl = buffer;
	memset(tl, 0, TL_SIZE);

	TEST_ASSERT(transfer_list_check_header(tl) == TL_OPS_NON);

	tl = transfer_list_init(tl, TL_SIZE / 2);

	// Add TE's until we run out of space
	while ((te = transfer_list_add(tl, test_tag, tl->max_size / 8,
				       &test_payload))) {
		TEST_ASSERT(te = transfer_list_find(tl, test_tag++));
		TEST_ASSERT(*(int *)transfer_list_entry_data(te) ==
			    test_payload++);
	}

	// Relocate the TL and make sure all the information we put in is still there.
	TEST_ASSERT(
		new_tl = transfer_list_relocate(tl, new_buf, tl->max_size * 2));
	TEST_ASSERT(transfer_list_check_header(new_tl) == TL_OPS_ALL);

	unsigned int i = test_tag;
	while ((te = transfer_list_find(tl, --i))) {
		int *data = transfer_list_entry_data(te);
		TEST_ASSERT_NOT_NULL(data);
		TEST_ASSERT(*data == --test_payload);
	}

	// Add the last TE we failed to add into the relocated TL.
	TEST_ASSERT(te = transfer_list_add(new_tl, test_tag, TL_SIZE / 8,
					   &test_payload));
	TEST_ASSERT(*(int *)transfer_list_find(new_tl, test_tag) =
			    ++test_payload);
}

void test_bad_reloc()
{
	struct transfer_list_header *tl = transfer_list_init(buffer, TL_SIZE);
	void *new_buf = malloc(TL_SIZE);

	/* Relocate to invalid memory address. */
	TEST_ASSERT_NULL(transfer_list_relocate(tl, 0, tl->max_size));

	/*
	 * Try relocate to area with insufficent memory, check at the end that we
	 * still have a valid TL in the original location.
	 */
	TEST_ASSERT_NULL(transfer_list_relocate(tl, new_buf, 0));
	TEST_ASSERT(transfer_list_check_header(tl) == TL_OPS_ALL);
}

void setUp(void)
{
	buffer = malloc(TL_MAX_SIZE);
}

void tearDown(void)
{
	free(buffer);
	buffer = NULL;
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_bad_reloc);
	RUN_TEST(test_init_alignment);
	RUN_TEST(test_init);
	RUN_TEST(test_relocate);
	return UNITY_END();
}
