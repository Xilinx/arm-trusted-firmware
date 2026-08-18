/*
 * Copyright (c) 2023-2026, Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>

#include <common/debug.h>
#include <lib/mmio.h>
#include <lib/smccc.h>
#include <plat/common/platform.h>
#include <services/arm_arch_svc.h>

#include <plat_common.h>
#include <plat_private.h>
#include <plat_startup.h>
#include <pm_api_sys.h>

/**
 * plat_is_smccc_feature_available() - This function checks whether SMCCC
 *                                     feature is availabile for platform.
 * @fid: SMCCC function id.
 *
 * Return: SMC_ARCH_CALL_SUCCESS - if SMCCC feature is available.
 *         SMC_ARCH_CALL_NOT_SUPPORTED - Otherwise.
 *
 */
int32_t plat_is_smccc_feature_available(u_register_t fid)
{
	int32_t ret = 0;

	if (fid == SMCCC_ARCH_SOC_ID) {
		ret = SMC_ARCH_CALL_SUCCESS;
	} else {
		ret = SMC_ARCH_CALL_NOT_SUPPORTED;
	}

	return ret;
}

/**
 * plat_get_soc_version() - Get the SOC version of the platform.
 *
 * Return: SiP defined SoC version in JEP-106.
 *
 * This function is called when the SoC_ID_type == 0.
 * For further details please refer to section 7.4 of SMC Calling Convention.
 */
int32_t plat_get_soc_version(void)
{
	uint32_t manfid = SOC_ID_SET_JEP_106(JEDEC_XILINX_BKID, JEDEC_XILINX_MFID);
	/*
	 * IDCODE[27:12] = FAMILY + SUB_FAMILY + DEVICE_CODE + SVD - stable
	 * 16-bit SoC part-number that does not change across steppings.
	 */
	uint32_t soc_id = (get_pmc_tap_idcode() >> PMC_TAP_IDCODE_SVD_SHIFT) &
			  SOC_ID_IMPL_DEF_MASK;
	uint32_t result = manfid | soc_id;

	return (int32_t)result;
}

/**
 * plat_get_soc_revision() - Get the SOC revision for the platform.
 *
 * Return: SiP defined SoC revision.
 *
 * This function is called when the  SoC_ID_type == 1
 * For further details please refer to section 7.4 of SMC Calling Convention
 */
int32_t plat_get_soc_revision(void)
{
	/*
	 * VERSION[30:0] carries PMC_VERSION[7:0], PS_VERSION[15:8],
	 * RTL_VERSION[23:16], PLATFORM[27:24], and PLATFORM_VERSION[30:28].
	 * PS_VERSION and PMC_VERSION change across silicon steppings.
	 */
	uint32_t result = get_pmc_tap_version() & SOC_ID_REV_MASK;

	return (int32_t)result;
}

/**
 * plat_get_soc_name() - SoC name for all Versal-family platforms.
 *
 * PLAT_SOC_NAME is defined per platform in its *_def.h:
 *   versal_def.h     -> "Versal"
 *   versal_net_def.h -> "Versal NET"
 *   def.h (versal2)  -> "Versal Gen 2"
 *
 * PMC_TAP.IDCODE is appended so Linux can identify the exact SoC variant.
 * This file is compiled for all three platforms via platform.mk.
 *
 * @soc_name: Buffer to store the SoC name string.
 *
 * Return: SMC_ARCH_CALL_SUCCESS on success.
 */
int32_t plat_get_soc_name(char *soc_name)
{
	int32_t ret = SMC_ARCH_CALL_SUCCESS;
	int rc = snprintf(soc_name, SMCCC_SOC_NAME_LEN, PLAT_SOC_NAME " %08x",
			  get_pmc_tap_idcode());

	/* snprintf return value should be checked to detect truncation */
	if (rc < 0 || rc >= (int)SMCCC_SOC_NAME_LEN) {
		ret = SMC_ARCH_CALL_NOT_SUPPORTED;
	}

	return ret;
}
