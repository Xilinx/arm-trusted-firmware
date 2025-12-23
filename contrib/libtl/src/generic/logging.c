/*
 * Copyright The Transfer List Library Contributors
 *
 * SPDX-License-Identifier: MIT OR GPL-2.0-or-later
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "logging.h"

static struct logger_interface _logger;
struct logger_interface *logger = NULL;

static void default_log(const char *level, const char *fmt, va_list args)
{
	printf("[%s] ", level);
	vprintf(fmt, args);
}

static void libtl_info(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	default_log("INFO", fmt, args);
	va_end(args);
}

static void libtl_warn(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	default_log("WARN", fmt, args);
	va_end(args);
}

void libtl_error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	default_log("ERROR", fmt, args);
	va_end(args);
}

void libtl_register_logger(struct logger_interface *user_logger)
{
	if (user_logger != NULL) {
		logger = user_logger;
	}

	_logger.info = libtl_info;
	_logger.warn = libtl_warn;
	_logger.error = libtl_error;

	logger = &_logger;
};
