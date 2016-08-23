/*
 * Copyright (c) 2016, ARM Limited and Contributors. All rights reserved.
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

#ifndef __XILSPD_PRIVATE_H__
#define __XILSPD_PRIVATE_H__

#include <arch.h>
#include <context.h>
#include <interrupt_mgmt.h>
#include <platform_def.h>
#include <psci.h>

/*******************************************************************************
 * Secure Payload state information e.g. SP is suspended, uninitialised etc
 * and macros to access the state information in the per-cpu 'state' flags
 ******************************************************************************/
#define XILSP_PSTATE_OFF		0
#define XILSP_PSTATE_ON		1
#define XILSP_PSTATE_SUSPEND	2
#define XILSP_PSTATE_SHIFT	0
#define XILSP_PSTATE_MASK	0x3
#define get_xilsp_pstate(state)	((state >> XILSP_PSTATE_SHIFT) \
							& XILSP_PSTATE_MASK)
#define clr_xilsp_pstate(state)	(state &= ~(XILSP_PSTATE_MASK \
					    << XILSP_PSTATE_SHIFT))
#define set_xilsp_pstate(st, pst) { \
					clr_xilsp_pstate(st); \
					st |= (pst & XILSP_PSTATE_MASK) << \
						XILSP_PSTATE_SHIFT; \
				}

/*******************************************************************************
 * Secure Payload execution state information i.e. aarch32 or aarch64
 ******************************************************************************/
#define XILSP_AARCH32		MODE_RW_32
#define XILSP_AARCH64		MODE_RW_64

/*******************************************************************************
 * The XILSPD should know the type of XILSP
 ******************************************************************************/
#define XILSP_TYPE_UP		PSCI_TOS_NOT_UP_MIG_CAP
#define XILSP_TYPE_UPM		PSCI_TOS_UP_MIG_CAP
#define XILSP_TYPE_MP		PSCI_TOS_NOT_PRESENT_MP

/*******************************************************************************
 * XILSP migrate type information as known to the XILSPD. We assume that
 * the XILSPD is dealing with an MP Secure Payload.
 ******************************************************************************/
#define XILSP_MIGRATE_INFO		XILSP_TYPE_MP

/*******************************************************************************
 * Number of cpus that the present on this platform. TODO: Rely on a topology
 * tree to determine this in the future to avoid assumptions about mpidr
 * allocation
 ******************************************************************************/
#define XILSPD_CORE_COUNT		PLATFORM_CORE_COUNT

/*******************************************************************************
 * Constants that allow assembler code to preserve callee-saved registers of the
 * C runtime context while performing a security state switch.
 ******************************************************************************/
#define XILSPD_C_RT_CTX_X19		0x0
#define XILSPD_C_RT_CTX_X20		0x8
#define XILSPD_C_RT_CTX_X21		0x10
#define XILSPD_C_RT_CTX_X22		0x18
#define XILSPD_C_RT_CTX_X23		0x20
#define XILSPD_C_RT_CTX_X24		0x28
#define XILSPD_C_RT_CTX_X25		0x30
#define XILSPD_C_RT_CTX_X26		0x38
#define XILSPD_C_RT_CTX_X27		0x40
#define XILSPD_C_RT_CTX_X28		0x48
#define XILSPD_C_RT_CTX_X29		0x50
#define XILSPD_C_RT_CTX_X30		0x58
#define XILSPD_C_RT_CTX_SIZE	0x60
#define XILSPD_C_RT_CTX_ENTRIES		(XILSPD_C_RT_CTX_SIZE >> DWORD_SHIFT)

#ifndef __ASSEMBLY__

#include <cassert.h>
#include <stdint.h>

/* AArch64 callee saved general purpose register context structure. */
DEFINE_REG_STRUCT(c_rt_regs, XILSPD_C_RT_CTX_ENTRIES);

/*
 * Compile time assertion to ensure that both the compiler and linker
 * have the same double word aligned view of the size of the C runtime
 * register context.
 */
CASSERT(sizeof(c_rt_regs_t) == XILSPD_C_RT_CTX_SIZE,
		assert_spd_c_rt_regs_size_mismatch);

/*******************************************************************************
 * Structure which helps the SPD to maintain the per-cpu state of the SP.
 * 'state'          - collection of flags to track SP state e.g. on/off
 * 'mpidr'          - mpidr to associate a context with a cpu
 * 'c_rt_ctx'       - stack address to restore C runtime context from after
 *                    returning from a synchronous entry into the SP.
 * 'cpu_ctx'        - space to maintain SP architectural state
 ******************************************************************************/
typedef struct xilsp_context {
	uint32_t state;
	uint64_t mpidr;
	uint64_t c_rt_ctx;
	cpu_context_t cpu_ctx;
} xilsp_context_t;

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
struct xilsp_vectors;

/*******************************************************************************
 * Function & Data prototypes
 ******************************************************************************/
uint64_t xilspd_enter_sp(uint64_t *c_rt_ctx);
void __dead2 xilspd_exit_sp(uint64_t c_rt_ctx, uint64_t ret);
uint64_t xilspd_synchronous_sp_entry(xilsp_context_t *xilsp_ctx);
void __dead2 xilspd_synchronous_sp_exit(xilsp_context_t *xilsp_ctx,
				uint64_t ret);
void xilspd_init_xilsp_ep_state(struct entry_point_info *xilsp_ep,
				uint32_t rw,
				uint64_t pc,
				xilsp_context_t *xilsp_ctx);

extern xilsp_context_t xilspd_sp_context[XILSPD_CORE_COUNT];
extern struct xilsp_vectors *xilsp_vectors;
#endif /*__ASSEMBLY__*/

#endif /* __XILSPD_PRIVATE_H__ */
