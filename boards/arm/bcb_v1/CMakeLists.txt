# SPDX-License-Identifier: Apache-2.0

get_filename_component(board_dir "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(board_drivers_dir "${board_dir}/drivers" ABSOLUTE)

set(board_include_dirs
  ${board_dir}/drivers
  ${ZEPHYR_BASE}/drivers
)

set(board_sources
  "${board_dir}/drivers/blixt-breaker.c"
  pinmux.c
)

zephyr_library()
zephyr_library_include_directories(${board_include_dirs})
zephyr_library_sources(${board_sources})

zephyr_include_directories(${board_dir})