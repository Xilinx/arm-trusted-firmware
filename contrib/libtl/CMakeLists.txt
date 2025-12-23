#
# Copyright The Transfer List Library Contributors
#
# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#

cmake_minimum_required(VERSION 3.15)

project(libtl VERSION 0.9 LANGUAGES C CXX ASM)

#
# Set global flags.
#
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS TRUE)

add_library(cxx_compiler_flags INTERFACE)
target_compile_features(cxx_compiler_flags INTERFACE cxx_std_11)

SET(TARGET_GROUP release CACHE STRING "Specify the Build Target [\"release\" by default]")


add_library(tl
    STATIC
        ${PROJECT_SOURCE_DIR}/src/generic/transfer_list.c
        ${PROJECT_SOURCE_DIR}/src/generic/tpm_event_log.c
        ${PROJECT_SOURCE_DIR}/src/generic/logging.c
)

target_include_directories(tl
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(tl PUBLIC cxx_compiler_flags)

if(PROJECT_API)
    include(${PROJECT_SOURCE_DIR}/cmake/ProjectApi.cmake)
endif()

if(TARGET_GROUP STREQUAL test)
    include(CTest)

    # Check if local unity/ directory exists
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unity/CMakeLists.txt")
        message(STATUS "Using local Unity framework.")
        add_subdirectory(unity)
    else()
        message(STATUS "Fetching Unity framework using FetchContent...")
        include(FetchContent)
        FetchContent_Declare(
            unity
            GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
            GIT_TAG master
        )
        FetchContent_MakeAvailable(unity)
    endif()

    add_subdirectory(test)
endif()
