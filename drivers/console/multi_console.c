/*
 * Copyright (c) 2018-2023, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include <drivers/console.h>

console_t *console_list;
static uint32_t console_state = CONSOLE_FLAG_BOOT;

IMPORT_SYM(console_t *, __STACKS_START__, stacks_start)
IMPORT_SYM(console_t *, __STACKS_END__, stacks_end)

int console_register(console_t *console)
{
	/* Assert that the struct is not on the stack (common mistake). */
	assert((console < stacks_start) || (console >= stacks_end));

	/* Check that we won't make a circle in the list. */
	if (console_is_registered(console) == 1) {
		return 1;
	}

	console->next = console_list;
	console_list = console;

	/* Return 1 for convenient tail-calling from console_xxx_register(). */
	return 1;
}

console_t *console_unregister(console_t *to_be_deleted)
{
	console_t **ptr;

	assert(to_be_deleted != NULL);

	for (ptr = &console_list; *ptr != NULL; ptr = &(*ptr)->next) {
		if (*ptr == to_be_deleted) {
			*ptr = (*ptr)->next;
			return to_be_deleted;
		}
	}

	return NULL;
}

/*
 * console_is_registered - Check if a console is already registered.
 *
 * This function traverses the global linked list of registered consoles
 * (console_list) to determine if the specified console_t object is already
 * present. It helps prevent duplicate registration of the same console
 * instance, which could otherwise lead to list corruption or undefined
 * behavior in the console framework.
 *
 * Parameters:
 *   @to_find: Pointer to the console_t object to search for in the list.
 *             Must not be NULL.
 *
 * Returns:
 *   1 - If the console is found in the list (already registered).
 *   0 - If the console is not found (not registered).
 */
int console_is_registered(console_t *to_find)
{
	console_t *console;

	/* Check for valid input */
	assert(to_find != NULL);

	/* Traverse the linked list of registered consoles */
	for (console = console_list; console != NULL; console = console->next) {
		/* Check if the current console matches the one we're looking for */
		if (console == to_find) {
			return 1; /* Match found, return 1 */
		}
	}

	return 0; /* No match found, return 0 */
}

/*
 * console_switch_state - This function switches the current state of the console to
 * the specified new state of the console framework.
 *
 * Parameters:
 * @new_state: The new state to set for the console framework.
 *
 * Returns:
 * None.
 */
void console_switch_state(unsigned int new_state)
{
	console_state = new_state;
}

/*
 * console_set_scope - Sets the scope of a console.
 *
 * Parameters:
 * @console: Pointer to the console structure whose scope is being updated.
 * @scope: The new scope to set for the console.
 *
 * Returns:
 * None.
 */
void console_set_scope(console_t *console, unsigned int scope)
{
	assert(console != NULL);

	console->flags = (console->flags & ~CONSOLE_FLAG_SCOPE_MASK) | scope;
}

/*
 * do_putc - Writes a character to the specified console.
 *
 * Parameters:
 * @c: The character to be written.
 * @console: Pointer to the console structure where the character will be written.
 *
 * Returns:
 * 0 or positive value - If the character is successfully written.
 * Negative value      - If an error occurs during the write operation.
 */
static int do_putc(int c, console_t *console)
{
	int ret;

	if ((c == (int)'\n') &&
	    ((console->flags & CONSOLE_FLAG_TRANSLATE_CRLF) != 0U)) {
		ret = console->putc('\r', console);
		if (ret < 0)
			return ret;
	}

	return console->putc(c, console);
}

/*
 * console_putc - Writes a character to all active consoles.
 *
 * This function iterates through the list of registered consoles and attempts to
 * write the specified character to each console that is both active and has a
 * valid `putc` function pointer. The function records the most appropriate return
 * value based on the success or failure of the write attempts.
 *
 * Parameters:
 * @c: The character to be written.
 *
 * Returns:
 * 0              - If the character is successfully written to at least one console.
 * Negative value - If no valid console is available or an error occurs during
 *                  the write operation.
 */
int console_putc(int c)
{
	int err = ERROR_NO_VALID_CONSOLE;  /* Default to failure state */
	console_t *console;

	/*
	 * If the character is not printable, we don't write it to the console.
	 * This is to avoid writing control characters that may not be handled
	 * correctly by all consoles.
	 */
	for (console = console_list; console != NULL; console = console->next) {
		if (((console->flags & console_state) != 0U) && (console->putc != NULL)) {
			int ret = do_putc(c, console); /* Write to active console */
			if ((err == ERROR_NO_VALID_CONSOLE) || (ret < err)) {
				err = ret; /* Update error state */
			}
		}
	}
	return err; /* Return the error state, which is 0 if at least one console succeeded */
}

/*
 * putchar - Writes a character to all active consoles.
 *
 * Parameters:
 * @c: The character to be written.
 *
 * Returns:
 * The character (`c`) - If the character is successfully written.
 * EOF                 - If no valid console is available or an error occurs.
 */
int putchar(int c)
{
	int ret = 0;

	if (console_putc(c) == 0) {
		ret = c;
	} else {
		ret = EOF;
	}

	return ret;
}

#if ENABLE_CONSOLE_GETC
int console_getc(void)
{
	int err = ERROR_NO_VALID_CONSOLE;
	console_t *console;

	do {	/* Keep polling while at least one console works correctly. */
		for (console = console_list; console != NULL;
		     console = console->next)
			if (((console->flags & console_state) != 0U) && (console->getc != NULL)) {
				int ret = console->getc(console);
				if (ret >= 0) {
					return ret;
				}
				if (err != ERROR_NO_PENDING_CHAR) {
					err = ret;
				}
			}
	} while (err == ERROR_NO_PENDING_CHAR);

	return err;
}
#endif

/*
 * console_flush - Flushes all active consoles.
 *
 * This function iterates through the list of registered consoles and invokes
 * the flush operation for each console that is currently active and has a
 * valid `flush` function pointer. Flushing ensures that any buffered output
 * data is written out, maintaining output consistency.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 */
void console_flush(void)
{
	console_t *console;

	/* Travere the linked list of registered consoles */
	for (console = console_list; console != NULL; console = console->next) {
		/* Check if the console is active and supports flushing */
		if (((console->flags & console_state) != 0U) && (console->flush != NULL)) {
			console->flush(console); /* Call the flush function for the console */
		}
	}
}
