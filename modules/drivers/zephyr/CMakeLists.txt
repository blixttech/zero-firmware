# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_include_directories(.)
zephyr_library_sources_ifdef(CONFIG_INPUT_CAPTURE_MCUX_FTM      input_capture_mcux_ftm.c)
zephyr_library_sources_ifdef(CONFIG_COUNTER_CTD_MCUX_PIT        counter_ctd_mcux_pit.c)
zephyr_include_directories(.)