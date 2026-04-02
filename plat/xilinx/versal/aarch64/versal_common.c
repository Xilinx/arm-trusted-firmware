/*
 * Copyright (c) 2018-2020, Arm Limited and Contributors. All rights reserved.
 * Copyright (c) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat/common/platform.h>

#include <plat_common.h>
#include <plat_ipi.h>
#include <plat_private.h>
#include <pm_api_sys.h>
#include <versal_def.h>

uint32_t platform_id, platform_version;
uint32_t cpu_clock;

/*
 * Table of regions to map using the MMU.
 * This doesn't include TZRAM as the 'mem_layout' argument passed to
 * configure_mmu_elx() will give the available subset of that,
 */
const mmap_region_t plat_versal_mmap[] = {
	MAP_REGION_FLAT(DEVICE0_BASE, DEVICE0_SIZE, MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(DEVICE1_BASE, DEVICE1_SIZE, MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(CRF_BASE, CRF_SIZE, MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(PLAT_ARM_CCI_BASE, PLAT_ARM_CCI_SIZE, MT_DEVICE | MT_RW |
			MT_SECURE),
	{ 0 }
};

/*
 * plat_get_mmap - Retrieves the platform-specific memory map.
 *
 * Returns:
 * A pointer to the `mmap_region_t` array containing the platform's memory map.
 */
const mmap_region_t *plat_get_mmap(void)
{
	return plat_versal_mmap;
}

void versal_config_setup(void)
{
	/* Configure IPI data for versal */
	versal_ipi_config_table_init();
}


/*
 * board_detection - Detect and initialize platform identification details.
 *
 * This function retrieves platform-specific information (such as platform ID and
 * version) by invoking `pm_get_chipid()`. The information is essential for setting
 * up the correct hardware configuration and behavior during boot.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None. This function updates global variables `platform_id` and `platform_version`.
 * It calls `panic()` on critical failure and does not return in that case.
 */
void board_detection(void)
{
	uint32_t plat_info[2];

#if (TFA_NO_PM == 0)
	/* Initialize the platform-specific configuration */
	if (pm_get_chipid(plat_info) != PM_RET_SUCCESS) {
		/* If the call is failed we cannot proceed with further
		 * setup. TF-A to panic in this situation.
		 */
		NOTICE("Failed to read the chip information");
		panic();
	}
#else
	plat_info[1] = mmio_read_32(PMC_TAP_VERSION);
#endif

	/* Extract platform ID and version from the retrieved information */
	platform_id = FIELD_GET(PLATFORM_MASK, plat_info[1]);
	platform_version = FIELD_GET(PLATFORM_VERSION_MASK, plat_info[1]);

	/* Set the CPU clock frequency based on the platform ID */
	if (platform_id == VERSAL_COSIM) {
		platform_id = VERSAL_QEMU;
	}
}

/*
 * board_name_decode - Get a human-readable name for the detected platform.
 *
 * This function decodes the platform identifier stored in the global variable
 * `platform_id` and maps it to a corresponding string representation. This helps
 * in identifying the platform during boot logs, diagnostics, or debug prints.
 *
 * The function covers known platform identifiers:
 *  - VERSAL_SPP    : "IPP"
 *  - VERSAL_EMU    : "EMU"
 *  - VERSAL_QEMU   : "QEMU"
 *  - VERSAL_SILICON: "SILICON"
 *
 * If `platform_id` does not match any known value, it defaults to "unknown".
 *
 * Parameters:
 * None.
 *
 * Returns:
 * const char* - A pointer to a string representing the platform name.
 */
const char *board_name_decode(void)
{
	const char *platform;

	/* Match the known platform ID to its string name */
	switch (platform_id) {
	case VERSAL_SPP:
		platform = "IPP";
		break;
	case VERSAL_EMU:
		platform = "EMU";
		break;
	case VERSAL_QEMU:
		platform = "QEMU";
		break;
	case VERSAL_SILICON:
		platform = "SILICON";
		break;
	default:
		/* If the platform ID is not recognized, return "unknown" */
		platform = "unknown";
	}

	/* Return the platform name */
	return platform;
}


/*
 * get_uart_clk - Retrieve the UART clock frequency based on the current platform.
 *
 * This function returns the appropriate UART clock frequency (in Hz) for the
 * detected platform. The value is determined by evaluating the global variable
 * `platform_id`, which is set during early platform detection.
 *
 * The UART clock is critical for configuring baud rates in UART-based communication.
 *
 * Platform-specific values:
 *  - VERSAL_SPP     : 25 MHz
 *  - VERSAL_EMU     : 212 KHz (Emulation platform uses a much lower clock)
 *  - VERSAL_QEMU    : 100 MHz (QEMU shares the same value as silicon)
 *  - VERSAL_SILICON : 100 MHz
 *
 * If the platform ID is unrecognized, the function triggers a system panic since
 * continuing without a valid clock configuration is unsafe.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * uint32_t - UART clock frequency in Hertz.
 *            This value is platform-specific.
 */
uint32_t get_uart_clk(void)
{
	uint32_t uart_clock;

	/* Determine the UART clock frequency based on the platform ID */
	switch (platform_id) {
	case VERSAL_SPP:
		uart_clock = 25000000; /* 25 MHz for SPP */
		break;
	case VERSAL_EMU:
		uart_clock = 212000; /* 212 KHz for EMU */
		break;
	case VERSAL_QEMU:
	case VERSAL_SILICON:
		uart_clock = 100000000; /* 100 MHz for QEMU and Silicon */
		break;
	default:
		/* If the platform ID is not recognized, panic */
		panic();
	}

	/* Return the determined UART clock frequency */
	return uart_clock;
}
