/*
 * Copyright (c) 2018-2021, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <plat/common/platform.h>

#include <plat_private.h>

/*
 * plat_core_pos_by_mpidr - Calculates the linear core position from an MPIDR value.
 *
 * This platform-specific function translates an MPIDR (Multiprocessor Affinity Register)
 * value into a linear core index used by the platform. It checks if the MPIDR corresponds
 * to a CPU within cluster 0 and that the CPU ID is less than the total number of cores
 * (`PLATFORM_CORE_COUNT`). If these conditions are met, it calls `versal_calc_core_pos()`
 * to compute the linear core index. If the MPIDR is invalid, it returns -1.
 *
 * Parameters:
 * @mpidr - The MPIDR value identifying the target core's affinity.
 *
 * Returns:
 * int32_t:
 *   >= 0  - The calculated linear index of the core.
 *   -1    - If the MPIDR value is invalid or unsupported.
 */

int plat_core_pos_by_mpidr(u_register_t mpidr)
{
	int ret = -1;

	/* Check if the MPIDR is valid for this platform */
	if (((mpidr & MPIDR_CLUSTER_MASK) == 0U) &&
	       ((mpidr & MPIDR_CPU_MASK) < PLATFORM_CORE_COUNT)) {
			/* Calculate the core position based on the MPIDR */
		ret = (int)versal_calc_core_pos(mpidr);
	}

	/* If the MPIDR is invalid, return -1 */
	return ret;
}
