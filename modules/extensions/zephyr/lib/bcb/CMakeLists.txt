# SPDX-License-Identifier: Apache-2.0

set(breaker_lib_sources
    bcb_etime.c
    bcb_ctrl.c
    bcb_msmnt.c
    #bcb_zd.c
    bcb_leds.c
)

zephyr_library()
zephyr_library_include_directories(.)
zephyr_library_sources(${breaker_lib_sources})
zephyr_include_directories(.)

zephyr_library_sources_ifdef(CONFIG_BCB_SHELL               bcb_shell.c)