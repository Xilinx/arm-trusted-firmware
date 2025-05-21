/*
 * Copyright (c) 2017-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <plat/common/platform.h>

/* Allow platforms to override the log prefix string */
#pragma weak plat_log_get_prefix

static const char *plat_prefix_str[] = {
	"ERROR:   ", "NOTICE:  ", "WARNING: ", "INFO:    ", "VERBOSE: "};

/*
 * plat_log_get_prefix - Retrieves the logging prefix string based on the provided log level.
 *
 * This function normalizes the given `log_level` to ensure it falls within the supported range
 * defined by `LOG_LEVEL_ERROR` and `LOG_LEVEL_VERBOSE`. If the log level is lower than the
 * minimum (`LOG_LEVEL_ERROR`), it is clamped to the minimum. If it exceeds the maximum
 * (`LOG_LEVEL_VERBOSE`), it is clamped to the maximum. After normalization, the log level is
 * divided by 10 and adjusted by an offset of 1 to access the corresponding prefix string from
 * the `plat_prefix_str` array. This helps in generating appropriate log output with a consistent
 * prefix for each severity level.
 *
 * Parameters:
 * - log_level: Logging level whose prefix string is to be retrieved.
 *
 * Returns:
 * - A constant character pointer to the log prefix string corresponding to the log level.
 */
const char *plat_log_get_prefix(unsigned int log_level)
{
	unsigned int level;

	/* Normalize the log level to ensure it is within the valid range */
	if (log_level < LOG_LEVEL_ERROR) {
		level = LOG_LEVEL_ERROR;
	} else if (log_level > LOG_LEVEL_VERBOSE) {
		level = LOG_LEVEL_VERBOSE;
	} else {
		level = log_level; /* Use the provided level directly */
	}

	/* Normalize the level to access the prefix string */
	return plat_prefix_str[(level / 10U) - 1U];
}
