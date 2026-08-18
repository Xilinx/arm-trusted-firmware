/*
 * Copyright (c) 2023-2026, Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Header file to contain common macros across different platforms */
#ifndef PLAT_COMMON_H
#define PLAT_COMMON_H

#define __bf_shf(x)            (__builtin_ffsll(x) - 1)
#define FIELD_GET(_mask, _reg)						\
	({								\
		(typeof(_mask))(((_reg) & (_mask)) >> __bf_shf(_mask));	\
	})

/*******************************************************************************
 * interrupt handling related constants
 ******************************************************************************/
#define ARM_IRQ_SEC_SGI_0	8U
#define ARM_IRQ_SEC_SGI_1	9U
#define ARM_IRQ_SEC_SGI_2	10U
#define ARM_IRQ_SEC_SGI_3	11U
#define ARM_IRQ_SEC_SGI_4	12U
#define ARM_IRQ_SEC_SGI_5	13U
#define ARM_IRQ_SEC_SGI_6	14U
#define ARM_IRQ_SEC_SGI_7	15U

/*******************************************************************************
 * PMC_TAP accessor functions - platform-specific implementations, and shared
 * PMC_TAP register layout constants
 ******************************************************************************/

/* IDCODE[27:12] = FAMILY + SUB_FAMILY + DEVICE_CODE + SVD (stable part number) */
#define PMC_TAP_IDCODE_SVD_SHIFT	U(12)

#ifndef __ASSEMBLER__

#include <stdint.h>

uint32_t get_pmc_tap_idcode(void);
uint32_t get_pmc_tap_version(void);

#endif /* __ASSEMBLER__ */

#endif /* PLAT_COMMON_H */
