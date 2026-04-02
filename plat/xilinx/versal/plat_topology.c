/*
 * Copyright (c) 2018, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <plat/common/platform.h>
#include <platform_def.h>

static const uint8_t plat_power_domain_tree_desc[] = {1, PLATFORM_CORE_COUNT};

/*
 * plat_get_power_domain_tree_desc : Retrieves the platform's power domain tree descriptor.
 * The power domain tree descriptor specifies the hierarchy of power domains.
 * The first element represents the number of system level power domain.
 * followed by the number of cores at the next level.
 *
 * Parameters:
 *  - None.
 *
 * Return:
 *  - const uint8_t *: Pointer to the power domain tree descriptor array.
 */
const uint8_t *plat_get_power_domain_tree_desc(void)
{
	/* Return the platform's power domain tree descriptor */
	return plat_power_domain_tree_desc;
}
