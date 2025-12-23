/*
 * Copyright The Transfer List Library Contributors
 *
 * SPDX-License-Identifier: MIT OR GPL-2.0-or-later
 */

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <logging.h>
#include <private/math_utils.h>
#include <transfer_list.h>

void transfer_list_dump(struct transfer_list_header *tl)
{
	struct transfer_list_entry *te = NULL;
	int i = 0;

	if (!tl) {
		return;
	}
	info("Dump transfer list:\n");
	info("signature  0x%x\n", tl->signature);
	info("checksum   0x%x\n", tl->checksum);
	info("version    0x%x\n", tl->version);
	info("hdr_size   0x%x\n", tl->hdr_size);
	info("alignment  0x%x\n", tl->alignment);
	info("size       0x%x\n", tl->size);
	info("max_size   0x%x\n", tl->max_size);
	info("flags      0x%x\n", tl->flags);
	while (true) {
		te = transfer_list_next(tl, te);
		if (!te) {
			break;
		}

		info("Entry %d:\n", i++);
		transfer_entry_dump(te);
	}
}

void transfer_entry_dump(struct transfer_list_entry *te)
{
	if (te) {
		info("tag_id     0x%x\n", te->tag_id);
		info("hdr_size   0x%x\n", te->hdr_size);
		info("data_size  0x%x\n", te->data_size);
		info("data_addr  0x%lx\n",
		     (unsigned long)transfer_list_entry_data(te));
	}
}

struct transfer_list_header *transfer_list_init(void *addr, size_t max_size)
{
	struct transfer_list_header *tl = addr;

	if (!addr || max_size == 0) {
		return NULL;
	}

	if (!libtl_is_aligned((uintptr_t)addr,
			      1 << TRANSFER_LIST_INIT_MAX_ALIGN) ||
	    !libtl_is_aligned(max_size, 1 << TRANSFER_LIST_INIT_MAX_ALIGN) ||
	    max_size < sizeof(*tl)) {
		return NULL;
	}

	memset(tl, 0, max_size);
	tl->signature = TRANSFER_LIST_SIGNATURE;
	tl->version = TRANSFER_LIST_VERSION;
	tl->hdr_size = sizeof(*tl);
	tl->alignment = TRANSFER_LIST_INIT_MAX_ALIGN; /* initial max align */
	tl->size = sizeof(*tl); /* initial size is the size of header */
	tl->max_size = max_size;
	tl->flags = TL_FLAGS_HAS_CHECKSUM;

	transfer_list_update_checksum(tl);

	return tl;
}

struct transfer_list_header *
transfer_list_relocate(struct transfer_list_header *tl, void *addr,
		       size_t max_size)
{
	uintptr_t new_addr, align_mask, align_off;
	struct transfer_list_header *new_tl;
	uint32_t new_max_size;

	if (!tl || !addr || max_size == 0) {
		return NULL;
	}

	align_mask = (1 << tl->alignment) - 1;
	align_off = (uintptr_t)tl & align_mask;
	new_addr = ((uintptr_t)addr & ~align_mask) + align_off;

	if (new_addr < (uintptr_t)addr) {
		new_addr += (1 << tl->alignment);
	}

	new_max_size = max_size - (new_addr - (uintptr_t)addr);

	/* the new space is not sufficient for the tl */
	if (tl->size > new_max_size) {
		return NULL;
	}

	new_tl = (struct transfer_list_header *)new_addr;
	memmove(new_tl, tl, tl->size);
	new_tl->max_size = new_max_size;

	transfer_list_update_checksum(new_tl);

	return new_tl;
}

enum transfer_list_ops
transfer_list_check_header(const struct transfer_list_header *tl)
{
	if (!tl) {
		return TL_OPS_NON;
	}

	if (tl->signature != TRANSFER_LIST_SIGNATURE) {
		warn("Bad transfer list signature %#" PRIx32 "\n",
		     tl->signature);
		return TL_OPS_NON;
	}

	if (!tl->max_size) {
		warn("Bad transfer list max size %#" PRIx32 "\n", tl->max_size);
		return TL_OPS_NON;
	}

	if (tl->size > tl->max_size) {
		warn("Bad transfer list size %#" PRIx32 "\n", tl->size);
		return TL_OPS_NON;
	}

	if (tl->hdr_size != sizeof(struct transfer_list_header)) {
		warn("Bad transfer list header size %#" PRIx32 "\n",
		     tl->hdr_size);
		return TL_OPS_NON;
	}

	if (!transfer_list_verify_checksum(tl)) {
		warn("Bad transfer list checksum %#" PRIx32 "\n", tl->checksum);
		return TL_OPS_NON;
	}

	if (tl->version == 0) {
		warn("Transfer list version is invalid\n");
		return TL_OPS_NON;
	} else if (tl->version == TRANSFER_LIST_VERSION) {
		info("Transfer list version is valid for all operations\n");
		return TL_OPS_ALL;
	} else if (tl->version > TRANSFER_LIST_VERSION) {
		info("Transfer list version is valid for read-only\n");
		return TL_OPS_RO;
	}

	info("Old transfer list version is detected\n");
	return TL_OPS_CUS;
}

struct transfer_list_entry *transfer_list_next(struct transfer_list_header *tl,
					       struct transfer_list_entry *last)
{
	struct transfer_list_entry *te = NULL;
	uintptr_t tl_ev = 0;
	uintptr_t va = 0;
	uintptr_t ev = 0;
	size_t sz = 0;

	if (!tl) {
		return NULL;
	}

	tl_ev = (uintptr_t)tl + tl->size;

	if (last) {
		va = (uintptr_t)last;
		/* check if the total size overflow */
		if (libtl_add_overflow(last->hdr_size, last->data_size, &sz)) {
			return NULL;
		}
		/* roundup to the next entry */
		if (libtl_add_with_round_up_overflow(
			    va, sz, TRANSFER_LIST_GRANULE, &va)) {
			return NULL;
		}
	} else {
		va = (uintptr_t)tl + tl->hdr_size;
	}

	te = (struct transfer_list_entry *)va;

	if (va + sizeof(*te) > tl_ev || te->hdr_size < sizeof(*te) ||
	    libtl_add_overflow(te->hdr_size, te->data_size, &sz) ||
	    libtl_add_overflow(va, sz, &ev) || ev > tl_ev) {
		return NULL;
	}

	return te;
}

/*******************************************************************************
 * Enumerate the prev transfer entry
 * Return pointer to the prev transfer entry or NULL on error
 ******************************************************************************/
struct transfer_list_entry *transfer_list_prev(struct transfer_list_header *tl,
					       struct transfer_list_entry *last)
{
	struct transfer_list_entry *prev;
	struct transfer_list_entry *te = NULL;

	if (!last || !tl || (tl + tl->hdr_size == (void *)last)) {
		return NULL;
	}

	do {
		prev = te;
		te = transfer_list_next(tl, te);
	} while (te && te != last);

	return (te != NULL) ? prev : NULL;
}

/*******************************************************************************
 * Calculate the byte sum of a transfer list
 * Return byte sum of the transfer list
 ******************************************************************************/
static uint8_t calc_byte_sum(const struct transfer_list_header *tl)
{
	uint8_t *b = (uint8_t *)tl;
	uint8_t cs = 0;
	size_t n = 0;

	for (n = 0; n < tl->size; n++) {
		cs += b[n];
	}

	return cs;
}

void transfer_list_update_checksum(struct transfer_list_header *tl)
{
	uint8_t cs;

	if (!tl || !(tl->flags & TL_FLAGS_HAS_CHECKSUM)) {
		return;
	}

	cs = calc_byte_sum(tl);
	cs -= tl->checksum;
	cs = 256 - cs;
	tl->checksum = cs;
	assert(transfer_list_verify_checksum(tl));
}

bool transfer_list_verify_checksum(const struct transfer_list_header *tl)
{
	if (!tl) {
		return false;
	}

	if (!(tl->flags & TL_FLAGS_HAS_CHECKSUM)) {
		return true;
	}

	return !calc_byte_sum(tl);
}

bool transfer_list_set_data_size(struct transfer_list_header *tl,
				 struct transfer_list_entry *te,
				 uint32_t new_data_size)
{
	uintptr_t tl_old_ev, new_ev = 0, old_ev = 0, merge_ev, ru_new_ev;
	struct transfer_list_entry *dummy_te = NULL;
	size_t gap = 0;
	size_t mov_dis = 0;
	size_t sz = 0;

	if (!tl || !te) {
		return false;
	}
	tl_old_ev = (uintptr_t)tl + tl->size;

	/*
	 * calculate the old and new end of TE
	 * both must be roundup to align with TRANSFER_LIST_GRANULE
	 */
	if (libtl_add_overflow(te->hdr_size, te->data_size, &sz) ||
	    libtl_add_with_round_up_overflow((uintptr_t)te, sz,
					     TRANSFER_LIST_GRANULE, &old_ev)) {
		return false;
	}
	if (libtl_add_overflow(te->hdr_size, new_data_size, &sz) ||
	    libtl_add_with_round_up_overflow((uintptr_t)te, sz,
					     TRANSFER_LIST_GRANULE, &new_ev)) {
		return false;
	}

	if (new_ev > old_ev) {
		/*
		 * When next transfer list is dummy,
		 *   - if te can be extended in boundary of dummy entry,
		 *     extend it to dummy entry and set new dummy entry.
		 *
		 *   - otherwise, merge dummy entry with existing te and
		 *     extend transfer list as much as it requires.
		 */
		dummy_te = transfer_list_next(tl, te);
		if (dummy_te && (dummy_te->tag_id == TL_TAG_EMPTY)) {
			merge_ev = libtl_align_up(old_ev + dummy_te->hdr_size +
							  dummy_te->data_size,
						  TRANSFER_LIST_GRANULE);
			if (merge_ev >= new_ev) {
				gap = merge_ev - new_ev;
				goto set_dummy;
			} else {
				old_ev = merge_ev;
			}
		}

		/*
		 * move distance should be roundup
		 * to meet the requirement of TE data max alignment
		 * ensure that the increased size doesn't exceed
		 * the max size of TL
		 */
		mov_dis = new_ev - old_ev;
		if (libtl_round_up_overflow(mov_dis, 1 << tl->alignment,
					    &mov_dis) ||
		    tl->size + mov_dis > tl->max_size) {
			return false;
		}
		ru_new_ev = old_ev + mov_dis;
		memmove((void *)ru_new_ev, (void *)old_ev, tl_old_ev - old_ev);
		tl->size += mov_dis;
		gap = ru_new_ev - new_ev;
	} else {
		gap = old_ev - new_ev;
	}

set_dummy:
	if (gap >= sizeof(*dummy_te)) {
		/* create a dummy TE to fill up the gap */
		dummy_te = (struct transfer_list_entry *)new_ev;
		dummy_te->tag_id = TL_TAG_EMPTY;
		dummy_te->hdr_size = sizeof(*dummy_te);
		dummy_te->data_size = gap - sizeof(*dummy_te);
	}

	te->data_size = new_data_size;

	transfer_list_update_checksum(tl);
	return true;
}

bool transfer_list_rem(struct transfer_list_header *tl,
		       struct transfer_list_entry *te)
{
	struct transfer_list_entry *prev;
	struct transfer_list_entry *next;

	if (!tl || !te || (uintptr_t)te > (uintptr_t)tl + tl->size) {
		return false;
	}

	prev = transfer_list_prev(tl, te);
	next = transfer_list_next(tl, te);

	if (prev && prev->tag_id == TL_TAG_EMPTY) {
		prev->data_size += libtl_align_up(te->hdr_size + te->data_size,
						  TRANSFER_LIST_GRANULE);
		te = prev;
	}

	if (next && next->tag_id == TL_TAG_EMPTY) {
		te->data_size +=
			libtl_align_up(next->hdr_size + next->data_size,
				       TRANSFER_LIST_GRANULE);
	}

	te->tag_id = TL_TAG_EMPTY;
	transfer_list_update_checksum(tl);
	return true;
}

struct transfer_list_entry *transfer_list_add(struct transfer_list_header *tl,
					      uint32_t tag_id,
					      uint32_t data_size,
					      const void *data)
{
	uintptr_t tl_ev;
	struct transfer_list_entry *te = NULL;
	uint8_t *te_data = NULL;
	uintptr_t te_end;

	if (!tl || (tag_id & (1 << 24))) {
		return NULL;
	}

	/*
	 * skip the step 1 (optional step)
	 * new TE will be added into the tail
	 */
	tl_ev = (uintptr_t)tl + tl->size;
	te = (struct transfer_list_entry *)libtl_align_up(
		tl_ev, TRANSFER_LIST_GRANULE);

	te_end = libtl_align_up((uintptr_t)te + sizeof(*te) + data_size,
				TRANSFER_LIST_GRANULE);

	if (te_end > (uintptr_t)tl + tl->max_size) {
		return NULL;
	}

	te->tag_id = tag_id;
	te->hdr_size = sizeof(*te);
	te->data_size = data_size;
	tl->size += te_end - tl_ev;

	if (data) {
		/* get TE data pointer */
		te_data = transfer_list_entry_data(te);
		if (!te_data) {
			return NULL;
		}
		memmove(te_data, data, data_size);
	}

	transfer_list_update_checksum(tl);

	return te;
}

struct transfer_list_entry *
transfer_list_add_with_align(struct transfer_list_header *tl, uint32_t tag_id,
			     uint32_t data_size, const void *data,
			     uint8_t alignment)
{
	struct transfer_list_entry *te = NULL;
	uintptr_t tl_ev, ev, new_tl_ev;
	size_t dummy_te_data_sz = 0;

	if (!tl) {
		return NULL;
	}

	tl_ev = (uintptr_t)tl + tl->size;
	ev = tl_ev + sizeof(struct transfer_list_entry);

	if (!libtl_is_aligned(ev, 1 << alignment)) {
		/*
		 * TE data address is not aligned to the new alignment
		 * fill the gap with an empty TE as a placeholder before
		 * adding the desire TE
		 */
		new_tl_ev = libtl_align_up(ev, 1 << alignment) -
			    sizeof(struct transfer_list_entry);
		dummy_te_data_sz =
			new_tl_ev - tl_ev - sizeof(struct transfer_list_entry);
		if (!transfer_list_add(tl, TL_TAG_EMPTY, dummy_te_data_sz,
				       NULL)) {
			return NULL;
		}
	}

	te = transfer_list_add(tl, tag_id, data_size, data);

	if (alignment > tl->alignment) {
		tl->alignment = alignment;
		transfer_list_update_checksum(tl);
	}

	return te;
}

struct transfer_list_entry *transfer_list_find(struct transfer_list_header *tl,
					       uint32_t tag_id)
{
	struct transfer_list_entry *te = NULL;

	do {
		te = transfer_list_next(tl, te);
	} while (te && (te->tag_id != tag_id));

	return te;
}

void *transfer_list_entry_data(struct transfer_list_entry *entry)
{
	if (!entry) {
		return NULL;
	}
	return (uint8_t *)entry + entry->hdr_size;
}

struct transfer_list_header *transfer_list_ensure(void *addr, size_t size)
{
	struct transfer_list_header *tl = NULL;

	if (transfer_list_check_header(addr) == TL_OPS_ALL) {
		return (struct transfer_list_header *)addr;
	}

	tl = transfer_list_init((void *)addr, size);

	return tl;
}
