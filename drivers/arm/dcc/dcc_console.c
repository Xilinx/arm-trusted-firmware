/*
 * Copyright (c) 2015-2022, Xilinx Inc.
 * Copyright (c) 2022-2025, Advanced Micro Devices, Inc. All rights reserved.
 * Written by Michal Simek.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stddef.h>
#include <arch_helpers.h>
#include <drivers/arm/dcc.h>
#include <drivers/console.h>
#include <drivers/delay_timer.h>
#include <lib/mmio.h>

/* DCC Status Bits */
#define DCC_STATUS_RX		BIT(30)
#define DCC_STATUS_TX		BIT(29)
#define TIMEOUT_COUNT_US	U(0x10624)

struct dcc_console {
	console_t console;
};

/*
 * __dcc_getstatus - Reads the status of the DCC.
 *
 * This function retrieves the current status of the DCC by reading the
 * read_mdccsr_el0 function.
 * The status indicates whether the DCC is ready to transmit or receive data.
 *
 * Returns:
 * The value of the MDCCSR_EL0 register, which contains the DCC status bits.
 */
static inline u_register_t __dcc_getstatus(void)
{
	return read_mdccsr_el0();
}

#if ENABLE_CONSOLE_GETC
static inline char __dcc_getchar(void)
{
	char c;

	c = read_dbgdtrrx_el0();

	return c;
}
#endif

static inline void __dcc_putchar(char c)
{
	/*
	 * The typecast is to make absolutely certain that 'c' is
	 * zero-extended.
	 */
	write_dbgdtrtx_el0((unsigned char)c);
}

/*
 * dcc_status_timeout repeatedly checks the status of the DCC by reading the
 * MDCCSR_EL0 register and masking it with the provided `mask`. It waits until
 * the specified bit(s) are cleared or the timeout period elapses.
 *
 * Parameters:
 * @mask: The bitmask representing the DCC status bit(s) to monitor.
 *
 * Returns:
 *  0           - If the specified bit(s) are cleared within the timeout period.
 * -ETIMEDOUT   - If the timeout period elapses before the bit(s) are cleared.
 */
static int32_t dcc_status_timeout(uint32_t mask)
{
	const unsigned int timeout_count = TIMEOUT_COUNT_US;
	uint64_t timeout;
	u_register_t status;

	/* Initialize the timeout value based on the specified count */
	timeout = timeout_init_us(timeout_count);

	do {
		/* Read the DCC status and apply the mask */
		status = (__dcc_getstatus() & mask);

		/* If the timeout has elapsed, return -ETIMEDOUT */
		if (timeout_elapsed(timeout)) {
			return -ETIMEDOUT;
		}
	/*
	 * If the status is zero, it means the DCC is ready for
	 * transmission or reception, so we can exit the loop.
	 */
	} while ((status != 0U));

	/* All required bits are cleared */
	return 0;
}

/*
 * dcc_console_putc - Writes a character to DCC.
 *
 * Parameters:
 * @ch: The character to be transmitted.
 * @console: Pointer to the console structure (not used in this implementation).
 *
 * Returns:
 * The character (`ch`) if the transmission is successful.
 * -ETIMEDOUT if the transmit status bit does not clear within the timeout period.
 */
static int32_t dcc_console_putc(int32_t ch, console_t *console)
{
	int32_t status;

	(void)console;

	/* Wait for the DCC to be ready to transmit, return error if not */
	status = dcc_status_timeout(DCC_STATUS_TX);
	if (status != 0U) {
		return status;
	}

	/* Transmit the character */
	__dcc_putchar(ch);

	/* Return transmitted character on success */
	return ch;
}

#if ENABLE_CONSOLE_GETC
static int32_t dcc_console_getc(console_t *console)
{
	int32_t status;

	status = dcc_status_timeout(DCC_STATUS_RX);
	if (status != 0U) {
		return status;
	}

	return __dcc_getchar();
}
#endif

/**
 * dcc_console_flush() - Function to force a write of all buffered data
 *		          that hasn't been output.
 * @console		Console struct
 *
 */
static void dcc_console_flush(console_t *console)
{
	int32_t status;

	(void)console;

	status = dcc_status_timeout(DCC_STATUS_TX);
	if (status != 0U) {
		return;
	}
}

static struct dcc_console dcc_console = {
	.console = {
		.flags = CONSOLE_FLAG_BOOT |
			CONSOLE_FLAG_RUNTIME |
			CONSOLE_FLAG_CRASH,
		.putc = dcc_console_putc,
#if ENABLE_CONSOLE_GETC
		.getc = dcc_console_getc,
#endif
		.flush = dcc_console_flush,
	},
};

/*
 * console_dcc_register - Register the DCC console for debug output.
 *
 * Initializes and registers the given console_t structure for DCC use.
 *
 * Parameters:
 * @console: Pointer to the console_t structure to register.
 *
 * Returns:
 * 0 on success, negative error code on failure.
 */
int console_dcc_register(console_t *console)
{
	/* Copy predefined DCC console configuration into the provided structure */
	memcpy(console, &dcc_console.console, sizeof(console_t));

	/* Register the initialized console with the system */
	return console_register(console);
}

void console_dcc_unregister(console_t *console)
{
	dcc_console_flush(console);
	(void)console_unregister(console);
}
