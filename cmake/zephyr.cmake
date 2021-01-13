if(NOT BOARD)
    set(BOARD bcb_v1)
endif()

if(NOT BOARD_ROOT)
    get_filename_component(BOARD_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

if(NOT DTS_ROOT)
    get_filename_component(DTS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../modules/extensions/zephyr" ABSOLUTE)
endif()

get_filename_component(ZEPHYR_BASE "${CMAKE_CURRENT_LIST_DIR}/../zephyr-os/zephyr" ABSOLUTE)
set(ENV{ZEPHYR_BASE} "${ZEPHYR_BASE}")

include("${CMAKE_CURRENT_LIST_DIR}/extensions.cmake")

list(APPEND ZEPHYR_EXTRA_MODULES
    "${CMAKE_CURRENT_LIST_DIR}/../modules/extensions"
)

list_directories(includes_dirs
    "${CMAKE_CURRENT_LIST_DIR}/../modules/extensions/*/include")
list(APPEND SYSCALL_INCLUDE_DIRS ${includes_dirs})

find_package(Zephyr REQUIRED HINTS "$ENV{ZEPHYR_BASE}")

zephyr_include_directories(${includes_dirs})
