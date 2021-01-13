include("${CMAKE_CURRENT_LIST_DIR}/cmake/zephyr_library_extensions.cmake")
zephyr_include_directories("${CMAKE_CURRENT_LIST_DIR}/include")

add_subdirectory(drivers)
add_subdirectory(lib)
