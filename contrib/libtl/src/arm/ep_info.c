/*
 * Copyright The Transfer List Library Contributors
 *
 * SPDX-License-Identifier: MIT OR GPL-2.0-or-later
 */

#include <ep_info.h>
#include <stddef.h>
#include <transfer_list.h>

struct entry_point_info *
transfer_list_set_handoff_args(struct transfer_list_header *tl,
			       struct entry_point_info *ep_info)
{
	struct transfer_list_entry *te = NULL;
	void *dt = NULL;

	if (!ep_info || !tl || transfer_list_check_header(tl) == TL_OPS_NON) {
		return NULL;
	}

	te = transfer_list_find(tl, TL_TAG_FDT);
	dt = transfer_list_entry_data(te);

#ifdef __aarch64__
	if (GET_SPSR_RW(ep_info->spsr) == 0U) {
		ep_info->args.arg0 = (uintptr_t)dt;
		ep_info->args.arg1 = TRANSFER_LIST_HANDOFF_X1_VALUE(
			REGISTER_CONVENTION_VERSION);
		ep_info->args.arg2 = 0;
	} else
#endif
	{
		ep_info->args.arg0 = 0;
		ep_info->args.arg1 = TRANSFER_LIST_HANDOFF_R1_VALUE(
			REGISTER_CONVENTION_VERSION);
		ep_info->args.arg2 = (uintptr_t)dt;
	}

	ep_info->args.arg3 = (uintptr_t)tl;

	return ep_info;
}
