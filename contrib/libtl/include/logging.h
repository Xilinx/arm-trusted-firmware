/*
 * Copyright The Transfer List Library Contributors
 *
 * SPDX-License-Identifier: MIT OR GPL-2.0-or-later
 */

#ifndef LOGGING_H
#define LOGGING_H

struct logger_interface {
	void (*info)(const char *fmt, ...);
	void (*warn)(const char *fmt, ...);
	void (*error)(const char *fmt, ...);
};

extern struct logger_interface *logger;

#define info(...)                                  \
	do {                                       \
		if (logger && logger->info)        \
			logger->info(__VA_ARGS__); \
	} while (0)

#define warn(...)                                  \
	do {                                       \
		if (logger && logger->warn)        \
			logger->warn(__VA_ARGS__); \
	} while (0)

#define error(...)                                  \
	do {                                        \
		if (logger && logger->error)        \
			logger->error(__VA_ARGS__); \
	} while (0)

void libtl_register_logger(struct logger_interface *user_logger);

#endif
