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

list(APPEND ZEPHYR_EXTRA_MODULES
    "${CMAKE_CURRENT_LIST_DIR}/../modules/lib"
    "${CMAKE_CURRENT_LIST_DIR}/../modules/drivers"
)

list(APPEND SYSCALL_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../include"
    "${CMAKE_CURRENT_LIST_DIR}/../modules/drivers/zephyr"
)

find_package(Zephyr REQUIRED HINTS "$ENV{ZEPHYR_BASE}")

zephyr_include_directories("${CMAKE_CURRENT_LIST_DIR}/../include")