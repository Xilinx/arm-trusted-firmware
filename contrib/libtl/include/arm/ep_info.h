/*
 * Copyright The Transfer List Library Contributors
 *
 * SPDX-License-Identifier: MIT OR GPL-2.0-or-later
 */

#ifndef EP_INFO_H
#define EP_INFO_H

#include <stdint.h>

struct param_header {
	uint8_t type; /* type of the structure */
	uint8_t version; /* version of this structure */
	uint16_t size; /* size of this structure in bytes */
	uint32_t attr; /* attributes: unused bits SBZ */
};

struct aapcs_params {
#ifdef __aarch64__
	uint64_t arg0;
	uint64_t arg1;
	uint64_t arg2;
	uint64_t arg3;
	uint64_t arg4;
	uint64_t arg5;
	uint64_t arg6;
	uint64_t arg7;
#else
	uint32_t arg0;
	uint32_t arg1;
	uint32_t arg2;
	uint32_t arg3;
#endif
};

struct entry_point_info {
	struct param_header h;
	uintptr_t pc;
	uint32_t spsr;
#ifndef __aarch64__
	uintptr_t lr_svc;
#endif
	struct aapcs_params args;
};

#define GET_SPSR_RW(mode) (((mode) >> 0x4U) & 0x1U)

#endif