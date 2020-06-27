# SPDX-License-Identifier: Apache-2.0

get_filename_component(board_drivers_dir "${CMAKE_CURRENT_LIST_DIR}/drivers" ABSOLUTE)

set(board_include_dirs
  ${board_drivers_dir}
  ${ZEPHYR_BASE}/drivers
)

set(board_sources
  "${board_drivers_dir}/blixt-breaker.c"
  pinmux.c
)

zephyr_library()
zephyr_library_include_directories(${board_include_dirs})
zephyr_library_sources(${board_sources})
