/*
 * Copyright (c) 2025, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TPM_EVENT_LOG_H
#define TPM_EVENT_LOG_H

#include <stdint.h>

#include <transfer_list.h>

/**
 * Initializes or extends the TPM event log in the transfer list.
 *
 * If an event log entry exists, attempts to resize it. Otherwise, adds a new entry.
 * Copies old data if needed. Updates free to reflect available space.
 *
 * @param tl         Pointer to the transfer list header.
 * @param req_size   Requested size (bytes)
 * @return           Pointer to start of free space in the log, or NULL.
 */
uint8_t *transfer_list_event_log_extend(struct transfer_list_header *tl,
					size_t req_size);

/**
 * Finalizes the event log after writing is complete.
 *
 * Resizes the event log to match actual data written, updates checksum,
 * and flushes cache for the next stage.
 *
 * @param tl         Pointer to the transfer list header.
 * @param cursor     End offset of written log data.
 * @return           Pointer to start of log (past reserved bytes), or NULL.
 */
uint8_t *transfer_list_event_log_finish(struct transfer_list_header *tl,
					uintptr_t cursor);

#define EVENT_LOG_RESERVED_BYTES 4U

#endif /* TPM_EVENT_LOG_H */
