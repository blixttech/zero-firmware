cmake_minimum_required(VERSION 3.13.1)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/zephyr.cmake")

project(hello_world)
target_sources(app PRIVATE src/main.c)
