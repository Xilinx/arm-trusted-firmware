# Copyright (c) 2021, ARM Limited and Contributors. All rights reserved.
# Portions copyright (c) 2021, ProvenRun S.A.S. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause

PNCD_DIR		:=	services/spd/pncd
SPD_INCLUDES		:=	-Iinclude/bl32/pnc
SPD_INCLUDES		+=	-Iinclude/common/

SPD_SOURCES		:=	services/spd/pncd/pncd_common.c		\
				services/spd/pncd/pncd_helpers.S	\
				services/spd/pncd/pncd_main.c

NEED_BL32		:=	yes

# Eagerly save floating-point registers when switching worlds
SPD_PNCD_CTX_EAGER_SAVE_FPREGS	:= 0

# IRQ number used to notify NS world when SMC_ACTION_FROM_S is received
SPD_PNCD_NS_IRQ			:= 126

# IRQ number used to notify S world when SMC_ACTION_FROM_NS is received
SPD_PNCD_S_IRQ			:= 15

$(eval $(call assert_booleans, SPD_PNCD_CTX_EAGER_SAVE_FPREGS))
$(eval $(call assert_numerics, SPD_PNCD_NS_IRQ SPD_PNCD_S_IRQ))

ifneq (SPD_PNCD_CTX_EAGER_SAVE_FPREGS,0)
	CTX_INCLUDE_FPREGS := 1
endif

$(eval $(call add_defines,\
    $(sort \
        SPD_PNCD_CTX_EAGER_SAVE_FPREGS \
        SPD_PNCD_NS_IRQ \
        SPD_PNCD_S_IRQ \
)))

ENABLE_PSCI_FROM_BL32	:= 1
