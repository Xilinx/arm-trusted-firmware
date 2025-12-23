/*
 * Copyright (c) 2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h>
#include <string.h>

#include <logging.h>
#include <tpm_event_log.h>

uint8_t *transfer_list_event_log_extend(struct transfer_list_header *tl,
					size_t req_size)
{
	size_t existing_offset = EVENT_LOG_RESERVED_BYTES;
	struct transfer_list_entry *existing_entry, *new_entry;
	uint8_t *new_data_ptr;

	if (tl == NULL || req_size == 0U) {
		error("Invalid arguments to event log extend.\n");
		return NULL;
	}

	existing_entry = transfer_list_find(tl, TL_TAG_TPM_EVLOG);
	if (existing_entry != NULL) {
		existing_offset = existing_entry->data_size;

		if (transfer_list_set_data_size(tl, existing_entry,
						req_size + existing_offset)) {
			info("TPM event log entry resized: new space %zu bytes at offset %zu\n",
			     req_size, existing_offset);

			return transfer_list_entry_data(existing_entry) +
			       existing_offset;
		}
	}

	/* Add new entry (resize failed or no existing entry) */
	new_entry = transfer_list_add(tl, TL_TAG_TPM_EVLOG,
				      req_size + existing_offset, NULL);

	if (new_entry == NULL) {
		error("Failed to add TPM event log entry to transfer list.\n");
		return NULL;
	}

	new_data_ptr = transfer_list_entry_data(new_entry);

	if (existing_entry != NULL) {
		info("Copying existing event log (%zu bytes) to new entry at %p\n",
		     existing_offset, new_data_ptr);

		memmove(new_data_ptr, transfer_list_entry_data(existing_entry),
			existing_offset);

		transfer_list_rem(tl, existing_entry);
	}

	return new_data_ptr + existing_offset;
}

uint8_t *transfer_list_event_log_finish(struct transfer_list_header *tl,
					uintptr_t cursor)
{
	uintptr_t entry_data_base;
	size_t final_log_size;
	struct transfer_list_entry *entry;

	entry = transfer_list_find(tl, TL_TAG_TPM_EVLOG);
	entry_data_base = (uintptr_t)transfer_list_entry_data(entry);

	if (cursor < entry_data_base ||
	    cursor >= entry_data_base + entry->data_size) {
		error("Invalid cursor: outside event log bounds.\n");
		return NULL;
	}

	final_log_size = cursor - entry_data_base;

	if (!transfer_list_set_data_size(tl, entry, final_log_size)) {
		error("Unable to resize event log TE.\n");
		return NULL;
	}

	transfer_list_update_checksum(tl);

	info("TPM event log finalized: trimmed to %zu bytes",
	     final_log_size - EVENT_LOG_RESERVED_BYTES);

	return (uint8_t *)(entry_data_base + EVENT_LOG_RESERVED_BYTES);
}
