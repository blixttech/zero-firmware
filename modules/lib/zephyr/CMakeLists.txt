# SPDX-License-Identifier: Apache-2.0

set(breaker_lib_sources
    bcb_ctrl.c
)

zephyr_library()
zephyr_library_include_directories(.)
zephyr_library_sources(${breaker_lib_sources})
zephyr_include_directories(.)