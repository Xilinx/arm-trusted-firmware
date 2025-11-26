/*
 * Copyright (c) 2025-2026, Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <common/runtime_svc.h>

/*
 * Default stub implementations when no custom package is used
 */
void custom_early_setup(void)
{
	/* Default: no-op */
}

void custom_runtime_setup(void)
{
	/* Default: no-op */
}

void custom_mmap_add(void)
{
	/* Default: no-op */
}

uintptr_t custom_smc_handler(uint32_t smc_fid,
			     u_register_t x1,
			     u_register_t x2,
			     u_register_t x3,
			     u_register_t x4,
			     void *cookie,
			     void *handle,
			     u_register_t flags)
{
	/* Default: unsupported SMC */
	SMC_RET1(handle, SMC_UNK);
}
