#
# Copyright (c) 2025-2026, Advanced Micro Devices, Inc. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Custom Package Integration for AMD Platforms
#

# Flag to track if custom package is included
CUSTOM_PKG_INCLUDED := 0

# ============================================================================
# Custom Package Integration (supports multiple packages)
# ============================================================================
ifdef CUSTOM_PKG_PATH
    # Split CUSTOM_PKG_PATH by space or semicolon to support multiple paths
    # Use strip to remove leading/trailing spaces
    CUSTOM_PKG_PATHS := $(strip $(subst ;, ,$(CUSTOM_PKG_PATH)))

    $(foreach pkg_path,$(CUSTOM_PKG_PATHS), \
        $(eval PKG_PATH_CLEAN := $(strip $(pkg_path))) \
        $(eval PKG_MK := $(PKG_PATH_CLEAN)/custom_pkg.mk) \
        $(if $(wildcard $(PKG_MK)), \
            $(eval include $(PKG_MK)) \
            $(eval CUSTOM_PKG_INCLUDED := 1) \
            $(info Including custom package from: $(PKG_PATH_CLEAN)), \
            $(error CUSTOM_PKG_PATH contains $(PKG_PATH_CLEAN) but custom_pkg.mk not found) \
        ) \
    )
endif

# ============================================================================
# Build Summary
# ============================================================================
$(info ============================================================)
$(info Custom Package Configuration:)
$(info   Custom Package: $(if $(filter 1,$(CUSTOM_PKG_INCLUDED)),ENABLED,DISABLED))
$(if $(filter 1,$(CUSTOM_PKG_INCLUDED)), \
    $(foreach pkg_path,$(CUSTOM_PKG_PATHS), \
        $(info   - $(strip $(pkg_path))) \
    ) \
)
$(info ============================================================)
