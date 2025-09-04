# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Add our modules directory to Zephyr Modules
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/modules)


# Set project, dts and board directories
set(PROJ_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/)
set(DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/dts)
set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})


find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(RelaySwitching)

target_sources(app PRIVATE 
src/main.c
src/xmodem.c
src/pray2_tests.c
src/sys_flash.c
src/sd_pray2_io.c
)

# Optionally set include paths that every module can see
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})