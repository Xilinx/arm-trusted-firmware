/*
 * Copyright (c) 2015-2020, Xilinx Inc.
 * Written by Michal Simek.
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

#include <lib/mmio.h>
#include <stddef.h>
#include <arch_helpers.h>
#include <drivers/arm/dcc.h>
#include <drivers/console.h>

/* DCC Status Bits */
#define DCC_STATUS_RX		(1 << 30)
#define DCC_STATUS_TX		(1 << 29)

struct dcc_console {
	struct console console;
};

static inline uint32_t __dcc_getstatus(void)
{
	uint32_t ret;

	__asm__ volatile("mrs %0, mdccsr_el0" : "=r" (ret));

	return ret;
}

static inline char __dcc_getchar(void)
{
	char c;

	__asm__ volatile("mrs %0, dbgdtrrx_el0" : "=r" (c));
	isb();

	return c;
}

static inline void __dcc_putchar(char c)
{
	/*
	 * The typecast is to make absolutely certain that 'c' is
	 * zero-extended.
	 */
	__asm__ volatile("msr dbgdtrtx_el0, %0"
		     : : "r" ((unsigned long)(unsigned char)c));
	isb();
}

static int32_t dcc_console_putc(int32_t ch, struct console *console)
{
	while (__dcc_getstatus() & DCC_STATUS_TX)
			;

	__dcc_putchar(ch);
	return ch;
}

static int32_t dcc_console_getc(struct console *console)
{
	while (!(__dcc_getstatus() & DCC_STATUS_RX))
		  ;

	return __dcc_getchar();
}

int32_t dcc_console_init(unsigned long base_addr, uint32_t uart_clk,
		      uint32_t baud_rate)
{
	return 0; /* No init needed */
}

/**
 * dcc_console_flush() - Function to force a write of all buffered data
 *		          that hasnt been output.
 * @console		Console struct
 *
 */
static void dcc_console_flush(struct console *console)
{
	while (__dcc_getstatus() & DCC_STATUS_TX)
		;

}

static struct dcc_console dcc_console = {
	.console = {
		.flags = CONSOLE_FLAG_BOOT |
			CONSOLE_FLAG_RUNTIME,
		.putc = dcc_console_putc,
		.getc = dcc_console_getc,
		.flush = dcc_console_flush,
	},
};

int console_dcc_register(void)
{
	return console_register(&dcc_console.console);
}
