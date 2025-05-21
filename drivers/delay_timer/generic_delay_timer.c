/*
 * Copyright (c) 2016-2024, Arm Limited and Contributors. All rights reserved.
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <arch_features.h>
#include <arch_helpers.h>
#include <common/bl_common.h>
#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <drivers/generic_delay_timer.h>
#include <lib/utils_def.h>
#include <plat/common/platform.h>

static timer_ops_t ops;

/*
 * timeout_cnt_us2cnt - Converts a timeout duration in microseconds to a corresponding
 * count value based on the system's counter frequency, retrieved using read_cntfrq_el0().
 *
 * Parameters:
 * @us: The timeout duration in microseconds.
 *
 * Returns:
 * The equivalent timeout duration as a 64-bit value.
 */
static uint64_t timeout_cnt_us2cnt(uint32_t us)
{
	return ((uint64_t)us * (uint64_t)read_cntfrq_el0()) / 1000000ULL;
}

/*
 * generic_delay_timeout_init_us - Initializes a timeout value based on a specified
 * duration in microseconds.
 *
 * Parameters:
 * @us: The timeout duration in microseconds.
 *
 * Returns:
 * It returns the calculated timeout as a 64-bit unsigned integer.
 */
static uint64_t generic_delay_timeout_init_us(uint32_t us)
{
	uint64_t cnt = timeout_cnt_us2cnt(us);

	cnt += read_cntpct_el0();

	return cnt;
}

/*
 * generic_delay_timeout_elapsed - Checks if a timeout has elapsed.
 *
 * Parameters:
 * @expire_cnt: The expiration count in timer ticks, calculated during timeout
 *              initialization.
 *
 * Returns:
 * true  - If the current counter value exceeds the expiration count (timeout elapsed).
 * false - If the current counter value is less than or equal to the expiration count
 *         (timeout not yet elapsed).
 */
static bool generic_delay_timeout_elapsed(uint64_t expire_cnt)
{
	return read_cntpct_el0() > expire_cnt;
}

static uint32_t generic_delay_get_timer_value(void)
{
	/*
	 * Generic delay timer implementation expects the timer to be a down
	 * counter. We apply bitwise NOT operator to the tick values returned
	 * by read_cntpct_el0() to simulate the down counter. The value is
	 * clipped from 64 to 32 bits.
	 */
	return (uint32_t)(~read_cntpct_el0());
}

/*
 * generic_delay_timer_init_args - Initialize the generic delay‑timer driver.
 *
 * This helper populates the global @ops structure with the clock‑multiplication
 * and clock‑division factors supplied by the caller, links in the standard
 * delay‑timer helper callbacks, and hands the populated structure to
 * timer_init(). A verbose trace message is emitted so that the firmware log
 * captures the exact multiplier/divider pair chosen at run time.
 *
 * Parameters:
 * @mult: Clock‑multiplier applied to the raw counter value to obtain
 *        microsecond resolution.
 * @div : Clock‑divider applied after multiplication; together with @mult it
 *        defines the tick‑to‑time conversion ratio.
 *
 * Returns:
 * None.
 */
void generic_delay_timer_init_args(uint32_t mult, uint32_t div)
{
	/* Set function to read current timer value */
	ops.get_timer_value	= generic_delay_get_timer_value;

	/* Set clock multiplier and divider */
	ops.clk_mult		= mult;
	ops.clk_div		= div;

	/* Set timeout functions */
	ops.timeout_init_us	= generic_delay_timeout_init_us;
	ops.timeout_elapsed	= generic_delay_timeout_elapsed;

	/* Initialize the timer with the configured operations */
	timer_init(&ops);

	/* Log the configured multiplier and divider */
	VERBOSE("Generic delay timer configured with mult=%u and div=%u\n",
		mult, div);
}

void generic_delay_timer_init(void)
{
	assert(is_armv7_gentimer_present());

	/* Value in ticks */
	unsigned int mult = MHZ_TICKS_PER_SEC;

	/* Value in ticks per second (Hz) */
	unsigned int div  = plat_get_syscnt_freq2();

	/* Reduce multiplier and divider by dividing them repeatedly by 10 */
	while (((mult % 10U) == 0U) && ((div % 10U) == 0U)) {
		mult /= 10U;
		div /= 10U;
	}

	generic_delay_timer_init_args(mult, div);
}
