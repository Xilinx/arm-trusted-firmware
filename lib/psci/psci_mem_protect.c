/*
 * Copyright (c) 2017-2018, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <limits.h>

#include <lib/utils.h>

#include "psci_private.h"

/*
 * psci_mem_protect - Enable or disable memory protection using platform hooks.
 *
 * This function is used to control platform-specific memory protection features.
 * It ensures the platform supports memory protection by validating the presence of
 * required function pointers. Then it performs the following steps:
 *
 * 1. Calls the platform's `read_mem_protect()` to retrieve the current protection state.
 * 2. Calls the platform's `write_mem_protect(enable)` to update the protection state.
 * 3. Returns the original protection state (1 if enabled, 0 if disabled).
 *
 * Parameters:
 * @enable: A boolean-like flag indicating the requested protection state:
 *          - 0 to disable memory protection
 *          - Non-zero to enable memory protection
 *
 * Returns:
 *  - 1U if memory protection was previously enabled
 *  - 0U if memory protection was previously disabled
 *  - PSCI_E_NOT_SUPPORTED if the platform does not support the operation
 */
u_register_t psci_mem_protect(unsigned int enable)
{
	int val = 0;

	/* Validate the platform's memory protection operations */
	assert(psci_plat_pm_ops->read_mem_protect != NULL);
	assert(psci_plat_pm_ops->write_mem_protect != NULL);

	/* Read the current memory protection state */
	if (psci_plat_pm_ops->read_mem_protect(&val) < 0) {
		return (u_register_t) PSCI_E_NOT_SUPPORTED;
	}

	/* Attempt to set the new memory protection state */
	if (psci_plat_pm_ops->write_mem_protect(enable) < 0) {
		return (u_register_t) PSCI_E_NOT_SUPPORTED;
	}

	/* Return the previous protection state (1 if enabled, 0 if disabled) */
	return (val != 0) ? 1U : 0U;
}

/*
 * psci_mem_chk_range : Checks if a memory range is protected by the memory protection mechanism.
 * Parameters:
 * @base: The base address of the memory range.
 * @length: The length of the memory range.
 * Return:
 * - PSCI_E_SUCCESS: The memory range is protected.
 * - PSCI_E_DENIED: The memory range is not protected or invalid.
 */
u_register_t psci_mem_chk_range(uintptr_t base, u_register_t length)
{
	int ret;

	assert(psci_plat_pm_ops->mem_protect_chk != NULL);

	if ((length == 0U) || check_uptr_overflow(base, length - 1U)) {
		return (u_register_t) PSCI_E_DENIED;
	}

	ret = psci_plat_pm_ops->mem_protect_chk(base, length);
	return (ret < 0) ?
	       (u_register_t) PSCI_E_DENIED : (u_register_t) PSCI_E_SUCCESS;
}
