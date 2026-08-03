#
# Copyright (c) 2026, Advanced Micro Devices, Inc. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Versal Premium Gen 2 build variant of the Versal Gen 1 (plat/xilinx/versal)
# port. This file is included by plat/xilinx/versal/platform.mk ONLY when
# PLAT_VARIANT=PREMIUM_GEN2 is selected, so a stock `PLAT=versal` build never
# sees any of the settings below.
#
# Versal Premium Gen 2:
#   - always executes BL31 out of DDR (dynamic xlat maps windows on demand),
#   - uses the shared plat/amd/common transfer-list (TL) handoff support.
#

# Emit -DPLAT_VERSAL_PREMIUM_GEN2 so plat/xilinx/versal/include/platform_def.h
# selects the Versal Premium Gen 2 memory map and handoff settings.
$(eval $(call add_define,PLAT_VERSAL_PREMIUM_GEN2))

# Firmware handoff protocol using transfer lists.
TRANSFER_LIST                   ?= 0

# Premium Gen 2 always uses dynamic xlat tables and the transfer-list
# handoff. This file is included at the end of platform.mk (after the stock
# PLAT_XLAT_TABLES_DYNAMIC add_define block), so emit the define here.
# TRANSFER_LIST is a core TF-A build flag and is add_define'd by the top-level
# Makefile automatically.
ifeq (${TRANSFER_LIST},1)
    override PLAT_XLAT_TABLES_DYNAMIC := 1
    $(eval $(call add_define,PLAT_XLAT_TABLES_DYNAMIC))
endif

ifeq (${TRANSFER_LIST},0)
    XILINX_OF_BOARD_DTB_ADDR ?= 0x1000000
    $(eval $(call add_define,XILINX_OF_BOARD_DTB_ADDR))
endif

# Optionally override the non-secure firmware handoff base address.
ifeq (${TRANSFER_LIST},1)
    ifdef NS_FW_HANDOFF_BASE
        $(eval $(call add_define,NS_FW_HANDOFF_BASE))
    endif
endif

# Shared AMD common transfer-list headers.
PLAT_INCLUDES		+=	-Iplat/amd/common/include/

# Transfer-list handoff sources (replaces the stock plat/xilinx/common/plat_fdt.c
# which platform.mk filters out of BL31_SOURCES for the Versal Premium Gen 2
# variant). These sources call into the TL library, so they only build (and
# are only needed) when TRANSFER_LIST=1; otherwise keep the stock DT handoff.
ifeq (${TRANSFER_LIST},1)
    include lib/transfer_list/transfer_list.mk
    BL31_SOURCES	+=	plat/amd/common/plat_fdt.c			\
				plat/amd/common/plat_xfer_list.c
else
    BL31_SOURCES	+=	plat/xilinx/common/plat_fdt.c
endif
