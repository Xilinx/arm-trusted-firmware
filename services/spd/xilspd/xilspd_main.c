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


/*******************************************************************************
 * This is the Secure Payload Dispatcher (SPD). The dispatcher is meant to be a
 * plug-in component to the Secure Monitor, registered as a runtime service. The
 * SPD is expected to be a functional extension of the Secure Payload (SP) that
 * executes in Secure EL1. The Secure Monitor will delegate all SMCs targeting
 * the Trusted OS/Applications range to the dispatcher. The SPD will either
 * handle the request locally or delegate it to the Secure Payload. It is also
 * responsible for initialising and maintaining communication with the SP.
 ******************************************************************************/
#include <arch_helpers.h>
#include <assert.h>
#include <bl_common.h>
#include <bl31.h>
#include <context_mgmt.h>
#include <debug.h>
#include <errno.h>
#include <platform.h>
#include <runtime_svc.h>
#include <stddef.h>
#include <string.h>
#include <uuid.h>
#include <xilsp.h>
#include "xilspd_private.h"

/*******************************************************************************
 * Address of the entrypoint vector table in the Secure Payload. It is
 * initialised once on the primary core after a cold boot.
 ******************************************************************************/
xilsp_vectors_t *xilsp_vectors;

/*******************************************************************************
 * Array to keep track of per-cpu Secure Payload state
 ******************************************************************************/
xilsp_context_t xilspd_sp_context[XILSPD_CORE_COUNT];

int32_t xilspd_init(void);

/*******************************************************************************
 * Secure Payload Dispatcher setup. The SPD finds out the SP entrypoint and
 * type (aarch32/aarch64) if not already known and initialises the context for
 * entry into the SP for its initialisation.
 ******************************************************************************/
int32_t xilspd_setup(void)
{
	entry_point_info_t *xilsp_ep_info;
	uint32_t linear_id;

	linear_id = plat_my_core_pos();

	/*
	 * Get information about the XILSP (BL32) image. Its absence is a
	 * critical failure.  TODO: Add support to conditionally include
	 * the SPD service
	 */
	xilsp_ep_info = bl31_plat_get_next_image_ep_info(SECURE);
	if (!xilsp_ep_info) {
		WARN("No XILSP provided by BL2 boot loader, Booting device without XILSP initialization. SMC's destined for XILSP will return SMC_UNK\n");
		return 1;
	}

	/*
	 * If there's no valid entry point for SP, we return a non-zero value
	 * signalling failure initializing the service. We bail out without
	 * registering any handlers
	 */
	if (!xilsp_ep_info->pc)
		return 1;

	xilspd_init_xilsp_ep_state(xilsp_ep_info,
			(xilsp_ep_info->spsr >> MODE_RW_SHIFT) & MODE_RW_MASK,
			xilsp_ep_info->pc,
			&xilspd_sp_context[linear_id]);
	/*
	 * All XILSPD initialization done. Now register our init function with
	 * BL31 for deferred invocation
	 */
	bl31_register_bl32_init(&xilspd_init);

	return 0;
}

/*******************************************************************************
 * This function passes control to the XILSP image (BL32) for the first
 * time on the primary cpu after a cold boot. It assumes that a valid secure
 * context has already been created by xilspd_setup() which can be directly
 * used. It also assumes that a valid non-secure context has been initialised
 * by PSCI so it does not need to save and restore any non-secure state. This
 * function performs a synchronous entry into the Secure payload. The SP passes
 * control back to this routine through a SMC.
 ******************************************************************************/
int32_t xilspd_init(void)
{
	uint32_t linear_id = plat_my_core_pos();
	xilsp_context_t *xilsp_ctx = &xilspd_sp_context[linear_id];
	entry_point_info_t *xilsp_entry_point;
	uint64_t rc;

	/*
	 * Get information about the XILSP (BL32) image. Its absence
	 * is a critical failure.
	 */
	xilsp_entry_point = bl31_plat_get_next_image_ep_info(SECURE);
	assert(xilsp_entry_point);

	cm_init_my_context(xilsp_entry_point);

	/*
	 * Arrange for an entry into the XILSP. It will be returned via
	 * XILSP_ENTRY_DONE case
	 */
	rc = xilspd_synchronous_sp_entry(xilsp_ctx);
	assert(rc != 0);

	return rc;
}


/*******************************************************************************
 * This function is responsible for handling all SMCs in the Trusted OS/App
 * range from the non-secure state as defined in the SMC Calling Convention
 * Document. It is also responsible for communicating with the XILSP to
 * delegate work and return results back to the non-secure state. Lastly it
 * will also return any information that the XILSP needs to do the work
 * assigned to it.
 ******************************************************************************/
uint64_t xilspd_smc_handler(uint32_t smc_fid,
			 uint64_t x1,
			 uint64_t x2,
			 uint64_t x3,
			 uint64_t x4,
			 void *cookie,
			 void *handle,
			 uint64_t flags)
{
	cpu_context_t *ns_cpu_context;
	uint32_t linear_id = plat_my_core_pos(), ns;
	xilsp_context_t *xilsp_ctx = &xilspd_sp_context[linear_id];

	/* Determine which security state this SMC originated from */
	ns = is_caller_non_secure(flags);

	switch (smc_fid) {

	/*
	 * This function ID is used only by the SP to indicate it has
	 * finished initialising itself after a cold boot
	 */
	case XILSP_ENTRY_DONE:
		if (ns)
			SMC_RET1(handle, SMC_UNK);
		/*
		 * Stash the SP entry points information. This is done
		 * only once on the primary cpu
		 */
		assert(xilsp_vectors == NULL);
		xilsp_vectors = (xilsp_vectors_t *) x1;

		if (xilsp_vectors)
			set_xilsp_pstate(xilsp_ctx->state, XILSP_PSTATE_ON);
		/*
		 * SP reports completion. The SPD must have initiated
		 * the original request through a synchronous entry
		 * into the SP. Jump back to the original C runtime
		 * context.
		 */
		xilspd_synchronous_sp_exit(xilsp_ctx, x1);
		break;
	case XILSP_ARITH:
		if (ns) {
			/*
			 * This is a fresh request from the non-secure client.
			 * Figure out which registers need to be preserved, save
			 * the non-secure state and send the request to the
			 * secure payload.
			 */
			assert(handle == cm_get_context(NON_SECURE));

			cm_el1_sysregs_context_save(NON_SECURE);

			/*
			 * We are done stashing the non-secure context. Ask the
			 * secure payload to do the work now.
			 */

			/*
			 * Verify if there is a valid context to use, copy the
			 * operation type and parameters to the secure context
			 * and jump to the fast smc entry point in the secure
			 * payload. Entry into S-EL1 will take place upon exit
			 * from this function.
			 */
			assert(&xilsp_ctx->cpu_ctx == cm_get_context(SECURE));

			/* Set appropriate entry for SMC.
			 * We expect the XILSP to manage the PSTATE.I and
			 * PSTATE.F flags as appropriate.
			 */
			cm_set_elr_el3(SECURE, (uint64_t)
					&xilsp_vectors->fast_smc_entry);
			cm_el1_sysregs_context_restore(SECURE);
			cm_set_next_eret_context(SECURE);
			write_ctx_reg(get_gpregs_ctx(&xilsp_ctx->cpu_ctx),
					CTX_GPREG_X4,
					read_ctx_reg(get_gpregs_ctx(handle),
					CTX_GPREG_X4));
			write_ctx_reg(get_gpregs_ctx(&xilsp_ctx->cpu_ctx),
					CTX_GPREG_X5,
					read_ctx_reg(get_gpregs_ctx(handle),
					CTX_GPREG_X5));
			write_ctx_reg(get_gpregs_ctx(&xilsp_ctx->cpu_ctx),
					CTX_GPREG_X6,
					read_ctx_reg(get_gpregs_ctx(handle),
					CTX_GPREG_X6));
			/* Propagate hypervisor client ID */
			write_ctx_reg(get_gpregs_ctx(&xilsp_ctx->cpu_ctx),
					CTX_GPREG_X7,
					read_ctx_reg(get_gpregs_ctx(handle),
					CTX_GPREG_X7));
			SMC_RET4(&xilsp_ctx->cpu_ctx, smc_fid, x1, x2, x3);
		} else {
			/*
			 * This is the result from the secure client of an
			 * earlier request. The results are in x1-x4. Copy it
			 * into the non-secure context, save the secure state
			 * and return to the non-secure state.
			 */
			assert(handle == cm_get_context(SECURE));
			cm_el1_sysregs_context_save(SECURE);

			/* Get a reference to the non-secure context */
			ns_cpu_context = cm_get_context(NON_SECURE);
			assert(ns_cpu_context);

			/* Restore non-secure state */
			cm_el1_sysregs_context_restore(NON_SECURE);
			cm_set_next_eret_context(NON_SECURE);
			SMC_RET4(ns_cpu_context, x1, x2, x3, x4);
		}
		break;

	default:
		break;
	}

	SMC_RET1(handle, SMC_UNK);
}

/* Define a SPD runtime service descriptor for fast SMC calls */
DECLARE_RT_SVC(
	xilspd_fast,

	OEN_TOS_START,
	OEN_TOS_END,
	SMC_TYPE_FAST,
	xilspd_setup,
	xilspd_smc_handler
);
