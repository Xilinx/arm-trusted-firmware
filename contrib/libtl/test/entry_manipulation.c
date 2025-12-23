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
char *test_page_data = NULL;

uint8_t byte_sum(const char *ptr, size_t len)
{
	uint8_t sum = 0;

	for (size_t i = 0; i < len; i++) {
		sum += ptr[i];
	}

	return sum;
}

static void setup_test_entries(struct transfer_list_header *tl,
			       unsigned int tag_base, unsigned int num_entries,
			       struct transfer_list_entry **entries)
{
	unsigned int i;
	unsigned int te_data_size;
	struct transfer_list_entry *te;

	te_data_size = tl->max_size / (num_entries * 2);

	for (i = 0; i < num_entries; i++) {
		TEST_ASSERT(transfer_list_add(tl, tag_base + i, te_data_size,
					      test_page_data));
		TEST_ASSERT(te = transfer_list_find(tl, tag_base + i));
		TEST_ASSERT(byte_sum((void *)tl, tl->size) == 0);
		entries[i] = te;
	}
}

void test_add()
{
	struct transfer_list_header *tl;
	struct transfer_list_entry *te;

	TEST_ASSERT(tl = transfer_list_init((void *)buffer, TL_SIZE));

	TEST_ASSERT(te = transfer_list_add(tl, test_tag, sizeof(test_data),
					   &test_data));
	TEST_ASSERT_EQUAL(0, byte_sum((char *)tl, tl->max_size));
	TEST_ASSERT(*(int *)transfer_list_entry_data(te) == test_data);

	/* Try to add a TE larger greater than allocated TL space */
	TEST_ASSERT_NULL(te = transfer_list_add(tl, 2, TL_SIZE, &test_data));
	TEST_ASSERT_EQUAL(0, byte_sum((char *)tl, tl->max_size));
	TEST_ASSERT_NULL(transfer_list_find(tl, 0x2));

	unsigned int tags[4] = { TAG_GENERIC_START, TAG_GENERIC_END,
				 TAG_NON_STANDARD_START, TAG_NON_STANDARD_END };

	for (size_t i = 0; i < 4; i++) {
		TEST_ASSERT(te = transfer_list_add(tl, tags[i],
						   sizeof(test_data),
						   &test_data));
		TEST_ASSERT_EQUAL(0, byte_sum((char *)tl, tl->max_size));
		TEST_ASSERT(te = transfer_list_find(tl, tags[i]));
		TEST_ASSERT(*(int *)transfer_list_entry_data(te) == test_data);
	}

	transfer_list_dump(tl);
	/* Add some out of bound tags. */
	TEST_ASSERT_NULL(
		transfer_list_add(tl, 1 << 24, sizeof(test_data), &test_data));

	TEST_ASSERT_NULL(
		transfer_list_add(tl, -1, sizeof(test_data), &test_data));
}

void test_add_with_align()
{
	struct transfer_list_header *tl =
		transfer_list_init(buffer, TL_MAX_SIZE);
	struct transfer_list_entry *te;

	unsigned int test_id = 1;
	const unsigned int entry_size = 0xff;
	int *data;

	TEST_ASSERT(tl->size == tl->hdr_size);

	/*
	 * When a new TE with a larger alignment requirement than already exists
	 * appears, the TE should be added and TL alignement updated.
	 */
	for (char align = 0; align < (1 << 4); align++, test_id++) {
		TEST_ASSERT(
			te = transfer_list_add_with_align(
				tl, test_id, entry_size, &test_data, align));
		TEST_ASSERT(tl->alignment >= align);
		TEST_ASSERT(te = transfer_list_find(tl, test_id));
		TEST_ASSERT(data = transfer_list_entry_data(te));
		TEST_ASSERT_FALSE((uintptr_t)data % (1 << align));
		TEST_ASSERT_EQUAL(*(int *)data, test_data);
	}
}

void test_rem()
{
	struct transfer_list_header *tl = transfer_list_init(buffer, TL_SIZE);
	struct transfer_list_entry *te[3];
	unsigned int tag_base = test_tag;

	TEST_ASSERT_EQUAL(tl->size, tl->hdr_size);

	setup_test_entries(tl, tag_base, 3, te);

	/* Remove the TE and make sure it does not present in the TL. */
	TEST_ASSERT_TRUE(transfer_list_rem(tl, te[0]));
	TEST_ASSERT(byte_sum((void *)tl, tl->size) == 0);
	TEST_ASSERT_NULL(transfer_list_find(tl, tag_base));

	/* Remove the TE3 and make sure it does not present in the TL. */
	TEST_ASSERT_TRUE(transfer_list_rem(tl, te[2]));
	TEST_ASSERT(byte_sum((void *)tl, tl->size) == 0);
	TEST_ASSERT_NULL(transfer_list_find(tl, tag_base + 2));

	/* Remove the TE2 and make sure it does not present in the TL. */
	TEST_ASSERT_TRUE(transfer_list_rem(tl, te[1]));
	TEST_ASSERT(byte_sum((void *)tl, tl->size) == 0);
	TEST_ASSERT_NULL(transfer_list_find(tl, tag_base + 1));

	/*
	 * Should have only one TL_TAG_EMPTY entry.
	 */
	TEST_ASSERT(te[0] = transfer_list_find(tl, TL_TAG_EMPTY));
	TEST_ASSERT(transfer_list_next(tl, te[0]) == NULL);
}

void test_set_data_size()
{
	struct transfer_list_header *tl = transfer_list_init(buffer, TL_SIZE);
	struct transfer_list_entry *te[3];
	unsigned int tag_base = test_tag;
	unsigned int tl_size;

	TEST_ASSERT_EQUAL(tl->size, tl->hdr_size);

	setup_test_entries(tl, tag_base, 3, te);

	/* Remove the TE2 and make sure it does not present in the TL. */
	TEST_ASSERT_TRUE(transfer_list_rem(tl, te[1]));
	TEST_ASSERT(byte_sum((void *)tl, tl->size) == 0);
	TEST_ASSERT_NULL(transfer_list_find(tl, tag_base + 1));

	/*
	 * Increase te size to tl->max_size / 4, this shouldn't increase the
	 * current transfer list's size since it will be increased with
	 * adjacent TL_TAG_EMPTY entry.
	 */
	tl_size = tl->size;
	TEST_ASSERT(transfer_list_set_data_size(tl, te[0], tl->max_size / 4));
	TEST_ASSERT(byte_sum((void *)tl, tl->size) == 0);
	TEST_ASSERT(te[0]->data_size == tl->max_size / 4);
	TEST_ASSERT(tl_size == tl->size);

	/*
	 * Increase te size to tl->max_size / 2, this increases
	 * the transfer list size.
	 */
	tl_size = tl->size;
	TEST_ASSERT(transfer_list_set_data_size(tl, te[0], tl->max_size / 2));
	TEST_ASSERT(byte_sum((void *)tl, tl->size) == 0);
	TEST_ASSERT(te[0]->data_size == tl->max_size / 2);
	TEST_ASSERT(tl_size < tl->size);
}

void setUp(void)
{
	buffer = malloc(TL_MAX_SIZE);
	test_page_data = malloc(TL_SIZE);
	memset(test_page_data, 0xff, TL_SIZE);
}

void tearDown(void)
{
	free(test_page_data);
	free(buffer);
	buffer = NULL;
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_add);
	RUN_TEST(test_add_with_align);
	RUN_TEST(test_rem);
	RUN_TEST(test_set_data_size);
	return UNITY_END();
}
