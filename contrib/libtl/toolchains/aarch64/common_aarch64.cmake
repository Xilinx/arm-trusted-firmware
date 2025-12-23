#
# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: Copyright LibTL Contributors.
#

include_guard()
include(CheckCCompilerFlag)

include(${CMAKE_CURRENT_LIST_DIR}/../common.cmake)

set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_PROCESSOR aarch64)

add_compile_options(
	-ffreestanding
	-mbranch-protection=standard
	-mgeneral-regs-only
	-mstrict-align
	-fpie)

add_link_options(
	"$<$<CONFIG:Debug>:-fno-omit-frame-pointer>"
	"$<$<CONFIG:Relase>:-fomit-frame-pointer>")

add_link_options(
	-nostdlib
	-Wl,-pie)

# Detect applicable "march=" option supported by compiler
function(detect_and_set_march)
	set (march_list 9.2 9.1 9 8.8 8.7 8.6 8.5)

	foreach(v ${march_list})
		string(REPLACE "." "_" n ${v})
		check_c_compiler_flag("-march=armv${v}-a" COMPILER_SUPPORTS_${n})
		if(COMPILER_SUPPORTS_${n})
			add_compile_options("-march=armv${v}-a")
			return()
		endif()
	endforeach()
	message(FATAL_ERROR "Suitable -march not detected. Please upgrade aarch64 compiler." )
endfunction()
