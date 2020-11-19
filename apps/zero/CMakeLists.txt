# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/zephyr.cmake")

project(blinky)
target_sources(app PRIVATE src/main.c
                           src/udp.c)
