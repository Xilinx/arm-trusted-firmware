/*
 * Copyright (c) 2019-2021, Xilinx, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Top-level SMC handler for Versal power management calls and
 * IPI setup functions for communication with PMC.
 */

#include <errno.h>
#include <plat_private.h>
#include <stdbool.h>
#include <common/runtime_svc.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>
#include "pm_api_sys.h"
#include "pm_client.h"
#include "pm_ipi.h"
#include <drivers/arm/gicv3.h>
#include "../drivers/arm/gic/v3/gicv3_private.h"
#include "../lib/psci/psci_private.h"

#define MODE				0x80000000U
#define XSCUGIC_SGIR_EL1_INITID_SHIFT    24U
#define INVALID_SGI    0xFF
#define PM_INIT_SUSPEND_CB	(30U)
#define PM_NOTIFY_CB		(32U)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_asgi1r_el1, S3_0_C12_C11_6)

/* pm_up = true - UP, pm_up = false - DOWN */
static bool pm_up;
static unsigned int sgi = INVALID_SGI;

static void notify_os(void)
{
	int cpu;
	unsigned int reg;

	cpu = plat_my_core_pos() + 1;

	reg = (cpu | (sgi << XSCUGIC_SGIR_EL1_INITID_SHIFT));
	write_icc_asgi1r_el1(reg);
}

static uint64_t __unused __dead2 versal_sgi_irq_handler(uint32_t id,
							uint32_t flags,
							void *handle,
							void *cookie)
{
	unsigned int cpu_id = plat_my_core_pos();
	const struct pm_proc *proc = pm_get_proc(cpu_id);

	VERBOSE("Entering wfi %d\n", cpu_id);

	plat_ic_clear_interrupt_pending(id);

	dsb();

	/* Prevent interrupts from spuriously waking up this cpu */
	plat_versal_gic_cpuif_disable();

	pm_ipi_irq_clear(primary_proc);
	mmio_write_32(FPD_APU_PWRCTL, mmio_read_32(FPD_APU_PWRCTL) |
			proc->pwrdn_mask);

	/* enter wfi and stay there */
	while (1) {
		wfi();
	}
}

static void request_cpu_idle(void)
{
	int i;
	uint8_t state;
	static int idle_requests = 0;
	int active_cores = 0;

	VERBOSE("CPU idle request received\n");

	for (i = 0; i < psci_plat_core_count; i++) {
		state = psci_get_aff_info_state_by_idx(i);
		if (state == AFF_STATE_ON) {
			active_cores++;
		}
	}
	idle_requests++;

	if (idle_requests < active_cores) {
		pm_ipi_irq_clear(primary_proc);
	} else {
		idle_requests = 0;
		for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
			/* trigger SGI to active cores */
			VERBOSE("Raise SGI for %d\n", i);
			plat_ic_raise_el3_sgi(VERSAL_CPU_IDLE_SGI, i);
		}
	}
}

static uint64_t ipi_fiq_handler(uint32_t id, uint32_t flags, void *handle,
				void *cookie)
{
	uint32_t payload[4] = {0};

	VERBOSE("Received IPI FIQ from firmware\r\n");

	(void)plat_ic_acknowledge_interrupt();

	pm_get_callbackdata(payload, ARRAY_SIZE(payload), 0, 0);
	switch (payload[0]) {
	case PM_INIT_SUSPEND_CB:
		if (sgi != INVALID_SGI) {
			notify_os();
			pm_ipi_irq_clear(primary_proc);
		}
		break;
	case PM_NOTIFY_CB:
		if (payload[2] == EVENT_CPU_IDLE_FORCE_PWRDWN) {
			request_cpu_idle();
		} else if (sgi != INVALID_SGI) {
			notify_os();
		}
		break;
	default:
		pm_ipi_irq_clear(primary_proc);
		WARN("Invalid IPI payload\r\n");
	}

	/* Clear FIQ */
	plat_ic_end_of_interrupt(id);

	return 0;
}

/**
 * pm_register_sgi() - PM register the IPI interrupt
 *
 * @sgi -  SGI number to be used for communication.
 * @reset -  Reset to invalid SGI when reset=1.
 * @return	On success, the initialization function must return 0.
 *		Any other return value will cause the framework to ignore
 *		the service
 *
 * Update the SGI number to be used.
 *
 */
int pm_register_sgi(unsigned int sgi_num, unsigned int reset)
{
	if (reset == 1) {
		sgi = INVALID_SGI;
		return 0;
	}

	if (sgi != INVALID_SGI) {
		return -EBUSY;
	}

	if (sgi_num >= GICV3_MAX_SGI_TARGETS) {
		return -EINVAL;
	}

	sgi = sgi_num;
	return 0;
}

/**
 * pm_setup() - PM service setup
 *
 * @return	On success, the initialization function must return 0.
 *		Any other return value will cause the framework to ignore
 *		the service
 *
 * Initialization functions for Versal power management for
 * communicaton with PMC.
 *
 * Called from sip_svc_setup initialization function with the
 * rt_svc_init signature.
 */
int pm_setup(void)
{
	int status, ret = 0;

	status = pm_ipi_init(primary_proc);

	if (status < 0) {
		INFO("BL31: PM Service Init Failed, Error Code %d!\n", status);
		ret = status;
	} else {
		pm_up = true;
	}

	/* register IRQ handler for CPU idle SGI */
	ret = request_intr_type_el3(VERSAL_CPU_IDLE_SGI, versal_sgi_irq_handler);
	if (ret) {
		INFO("BL31: registering SGI interrupt failed\n");
		goto err;
	}

	/*
	 * Enable IPI IRQ
	 * assume the rich OS is OK to handle callback IRQs now.
	 * Even if we were wrong, it would not enable the IRQ in
	 * the GIC.
	 */
	pm_ipi_irq_enable(primary_proc);

	ret = request_intr_type_el3(PLAT_VERSAL_IPI_IRQ, ipi_fiq_handler);
	if (ret) {
		WARN("BL31: registering IPI interrupt failed\n");
		goto err;
	}

	ret = pm_register_notifier(XPM_DEVID_ACPU_0,
				   EVENT_CPU_IDLE_FORCE_PWRDWN, 0U, 1U,
				   0U);
	if (ret) {
		WARN("BL31: registering notifier failed for acpu_0\r\n");
	}

	ret = pm_register_notifier(XPM_DEVID_ACPU_1,
				   EVENT_CPU_IDLE_FORCE_PWRDWN, 0U, 1U,
				   0U);
	if (ret) {
		WARN("BL31: registering notifier failed for acpu_1\r\n");
	}

	gicd_write_irouter(gicv3_driver_data->gicd_base, PLAT_VERSAL_IPI_IRQ,
			   MODE);

err:
	return ret;
}

/**
 * eemi_for_compatibility() - EEMI calls handler for deprecated calls
 *
 * @return - If EEMI API found then, uintptr_t type address, else 0
 *
 * Some EEMI API's use case needs to be changed in Linux driver, so they
 * can take advantage of common EEMI handler in TF-A. As of now the old
 * implementation of these APIs are required to maintain backward compatibility
 * until their use case in linux driver changes.
 */
static uintptr_t eemi_for_compatibility(uint32_t api_id, uint32_t *pm_arg,
					void *handle, uint32_t security_flag)
{
	enum pm_ret_status ret;

	switch (api_id) {

	case PM_IOCTL:
	{
		uint32_t value;

		ret = pm_api_ioctl(pm_arg[0], pm_arg[1], pm_arg[2],
				   pm_arg[3], &value, security_flag);
		if (ret == PM_RET_ERROR_NOTSUPPORTED)
			return (uintptr_t)0;

		SMC_RET1(handle, (uint64_t)ret | ((uint64_t)value) << 32);
	}

	case PM_QUERY_DATA:
	{
		uint32_t data[8] = { 0 };

		ret = pm_query_data(pm_arg[0], pm_arg[1], pm_arg[2],
				      pm_arg[3], data, security_flag);

		SMC_RET2(handle, (uint64_t)ret  | ((uint64_t)data[0] << 32),
				 (uint64_t)data[1] | ((uint64_t)data[2] << 32));
	}

	case PM_FEATURE_CHECK:
	{
		uint32_t version;

		ret = pm_feature_check(pm_arg[0], &version, security_flag);
		SMC_RET1(handle, (uint64_t)ret | ((uint64_t)version << 32));
	}

	default:
		return (uintptr_t)0;
	}
}

/**
 * eemi_psci_debugfs_handler() - EEMI API invoked from PSCI
 *
 * These EEMI APIs performs CPU specific power management tasks.
 * These EEMI APIs are invoked either from PSCI or from debugfs in kernel.
 * These calls require CPU specific processing before sending IPI request to
 * Platform Management Controller. For example enable/disable CPU specific
 * interrupts. This requires separate handler for these calls and may not be
 * handled using common eemi handler
 */
static uintptr_t eemi_psci_debugfs_handler(uint32_t api_id, uint32_t *pm_arg,
					   void *handle, uint32_t security_flag)
{
	enum pm_ret_status ret;

	switch (api_id) {

	case PM_SELF_SUSPEND:
		ret = pm_self_suspend(pm_arg[0], pm_arg[1], pm_arg[2],
				      pm_arg[3], security_flag);
		SMC_RET1(handle, (uint64_t)ret);

	case PM_FORCE_POWERDOWN:
		ret = pm_force_powerdown(pm_arg[0], pm_arg[1], security_flag);
		SMC_RET1(handle, (uint64_t)ret);

	case PM_REQ_SUSPEND:
		ret = pm_req_suspend(pm_arg[0], pm_arg[1], pm_arg[2],
				     pm_arg[3], security_flag);
		SMC_RET1(handle, (uint64_t)ret);

	case PM_ABORT_SUSPEND:
		ret = pm_abort_suspend(pm_arg[0], security_flag);
		SMC_RET1(handle, (uint64_t)ret);

	case PM_SYSTEM_SHUTDOWN:
		ret = pm_system_shutdown(pm_arg[0], pm_arg[1], security_flag);
		SMC_RET1(handle, (uint64_t)ret);

	default:
		return (uintptr_t)0;
	}
}

/**
 * TF_A_specific_handler() - SMC handler for TF-A specific functionality
 *
 * These EEMI calls performs functionality that does not require
 * IPI transaction. The handler ends in TF-A and returns requested data to
 * kernel from TF-A.
 */
static uintptr_t TF_A_specific_handler(uint32_t api_id, uint32_t *pm_arg,
				       void *handle, uint32_t security_flag)
{
	switch (api_id) {

	case PM_GET_CALLBACK_DATA:
	{
		uint32_t result[4] = {0};

		pm_get_callbackdata(result, ARRAY_SIZE(result), security_flag, 1);
		SMC_RET2(handle,
			(uint64_t)result[0] | ((uint64_t)result[1] << 32),
			(uint64_t)result[2] | ((uint64_t)result[3] << 32));
	}

	case PM_GET_TRUSTZONE_VERSION:
		SMC_RET1(handle, (uint64_t)PM_RET_SUCCESS |
			 ((uint64_t)VERSAL_TZ_VERSION << 32));

	default:
		return (uintptr_t)0;
	}
}

/**
 * eemi_handler() - Prepare EEMI payload and perform IPI transaction
 *
 * EEMI - Embedded Energy Management Interface is Xilinx proprietary protocol
 * to allow communication between power management controller and different
 * processing clusters.
 *
 * This handler prepares EEMI protocol payload received from kernel and performs
 * IPI transaction.
 */
static uintptr_t eemi_handler(uint32_t api_id, uint32_t *pm_arg,
			      void *handle, uint32_t security_flag)
{
	enum pm_ret_status ret;
	uint32_t buf[PAYLOAD_ARG_CNT] = {0};

	ret = pm_handle_eemi_call(security_flag, api_id, pm_arg[0], pm_arg[1],
				  pm_arg[2], pm_arg[3], pm_arg[4],
				  (uint64_t *)buf);
	SMC_RET2(handle, (uint64_t)ret | ((uint64_t)buf[0] << 32),
		 (uint64_t)buf[1] | ((uint64_t)buf[2] << 32));
}

/**
 * pm_smc_handler() - SMC handler for PM-API calls coming from EL1/EL2.
 * @smc_fid - Function Identifier
 * @x1 - x4 - SMC64 Arguments from kernel
 *	      x3 and x4 are Unused
 * @cookie  - Unused
 * @handler - Pointer to caller's context structure
 *
 * @return  - Unused
 *
 * Determines that smc_fid is valid and supported PM SMC Function ID from the
 * list of pm_api_ids, otherwise completes the request with
 * the unknown SMC Function ID
 *
 * The SMC calls for PM service are forwarded from SIP Service SMC handler
 * function with rt_svc_handle signature
 */
uint64_t pm_smc_handler(uint32_t smc_fid, uint64_t x1, uint64_t x2, uint64_t x3,
			uint64_t x4, void *cookie, void *handle, uint64_t flags)
{
	uintptr_t ret;
	uint32_t pm_arg[PAYLOAD_ARG_CNT] = {0};
	uint32_t security_flag = SECURE_FLAG;
	uint32_t api_id;

	/* Handle case where PM wasn't initialized properly */
	if (!pm_up)
		SMC_RET1(handle, SMC_UNK);

	/*
	 * Mark BIT24 payload (i.e 1st bit of pm_arg[3] ) as non-secure (1)
	 * if smc called is non secure
	 */
	if (is_caller_non_secure(flags)) {
		security_flag = NON_SECURE_FLAG;
	}

	pm_arg[0] = (uint32_t)x1;
	pm_arg[1] = (uint32_t)(x1 >> 32);
	pm_arg[2] = (uint32_t)x2;
	pm_arg[3] = (uint32_t)(x2 >> 32);
	(void)(x3);
	(void)(x4);
	api_id = smc_fid & FUNCID_NUM_MASK;

	ret = eemi_for_compatibility(api_id, pm_arg, handle, security_flag);
	if (ret != (uintptr_t)0)
		return ret;

	ret = eemi_psci_debugfs_handler(api_id, pm_arg, handle, flags);
	if (ret !=  (uintptr_t)0)
		return ret;

	ret = TF_A_specific_handler(api_id, pm_arg, handle, security_flag);
	if (ret !=  (uintptr_t)0)
		return ret;

	ret = eemi_handler(api_id, pm_arg, handle, security_flag);

	return ret;
}
