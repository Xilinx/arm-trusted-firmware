/*
 * Copyright (c) 2023-2024, Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <common/debug.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>

#include <platform_def.h>
#include <plat_clkfunc.h>
#include <plat_private.h>

/*
 * plat_get_syscnt_freq2 - Retrieves the system counter frequency.
 *
 * This function reads the system counter frequency from a memory-mapped register
 * located at IOU_SCNTRS_BASE + IOU_SCNTRS_BASE_FREQ_OFFSET. If the read value is
 * non-zero, it is considered valid and returned. Otherwise, it logs a message
 * indicating that the counter frequency was not properly initialized (i.e., value is 0),
 * and defaults to using the `cpu_clock` as a fallback value for system counter frequency.
 * This ensures the platform always has a usable frequency value, preventing runtime
 * issues caused by an uninitialized counter.
 *
 * Parameters:
 * - None.
 *
 * Returns:
 * - The 32-bit frequency value in Hz to be used by the platform timer.
 */
unsigned int plat_get_syscnt_freq2(void)
{
	uint32_t counter_freq = 0;
	uint32_t ret = 0;

	/* Read the counter frequency from the IOU_SCNTRS_BASE register */
	counter_freq = mmio_read_32(IOU_SCNTRS_BASE +
				    IOU_SCNTRS_BASE_FREQ_OFFSET);

	/* If the counter frequency is non-zero, use it; otherwise, use cpu_clock */
	if (counter_freq != 0U) {
		ret = counter_freq;
	} else {
		/* Log a message indicating the counter frequency was not set */
		INFO("Indicates counter frequency %dHz setting to %dHz\n",
		     counter_freq, cpu_clock);
		ret = cpu_clock;
	}

	return (unsigned int)ret;
}

void set_cnt_freq(void)
{
	uint64_t counter_freq;

	/* Configure counter frequency */
	counter_freq = read_cntfrq_el0();
	if (counter_freq == 0U) {
		write_cntfrq_el0(plat_get_syscnt_freq2());
	}
}
