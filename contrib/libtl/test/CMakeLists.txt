#
# Copyright (c) 2024, Arm Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#

file(GLOB TEST_SOURCES "*.c")

foreach(src IN ITEMS ${TEST_SOURCES})
	get_filename_component(suite_name ${src} NAME_WE)
	add_executable(${suite_name} ${src})

	target_link_libraries(${suite_name} unity tl)
	add_test(${suite_name} ${suite_name})
endforeach()

enable_testing()
