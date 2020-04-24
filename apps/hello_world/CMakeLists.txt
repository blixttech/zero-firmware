cmake_minimum_required(VERSION 3.13.1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/zephyr.cmake")
include(${ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

project(hello_world)

target_sources(app PRIVATE src/main.c)
