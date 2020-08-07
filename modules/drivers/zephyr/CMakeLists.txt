# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_include_directories(.)
zephyr_library_sources_ifdef(CONFIG_INPUT_CAPTURE_MCUX_FTM      input_capture_mcux_ftm.c)
zephyr_include_directories(.)