#
# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: Copyright LibTL Contributors.
#

include_guard()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

add_compile_options(
	"-fno-common"
	"-ffunction-sections"
	"-fdata-sections"
	"-Wall"
	"-Werror"
	"-gdwarf-4"
	"$<$<CONFIG:Debug>:-Og>"
	"$<$<CONFIG:Release>:-g>"
	)

add_link_options(
	"-Wl,--gc-sections"
	"-g"
	)
