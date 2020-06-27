if(NOT BOARD)
    set(BOARD bcb_v1)
endif()

if(NOT BOARD_ROOT)
    get_filename_component(BOARD_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

if(NOT DTS_ROOT)
    get_filename_component(DTS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

get_filename_component(ZEPHYR_BASE "${CMAKE_CURRENT_LIST_DIR}/../zephyr-os/zephyr" ABSOLUTE)
set(ENV{ZEPHYR_BASE} "${ZEPHYR_BASE}")

# Need to specify SYSCALL_INCLUDE_DIRS here since this is an out of tree board
get_filename_component(breaker_board_dir "${BOARD_ROOT}/boards/arm/${BOARD}" ABSOLUTE)
list(APPEND SYSCALL_INCLUDE_DIRS ${breaker_board_dir})