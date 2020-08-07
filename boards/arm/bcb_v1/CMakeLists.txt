# SPDX-License-Identifier: Apache-2.0

get_filename_component(board_dir "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

set(board_include_dirs
  ${board_dir}
)

set(board_sources
  ${board_dir}/pinmux.c
)

zephyr_library()
zephyr_library_include_directories(${board_include_dirs})
zephyr_library_sources(${board_sources})