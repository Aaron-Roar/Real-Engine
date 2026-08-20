# Copyright 2026 Aaron Rohrer
# SPDX-License-Identifier: LGPL-3.0-only

cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(rohr_fly_to_finish LANGUAGES C)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/rohr_example.cmake)
rohr_example_bootstrap()

add_executable(fly_to_finish src/main.c)
rohr_add_example_runtime(fly_to_finish)
target_link_libraries(fly_to_finish PRIVATE rohr_engine)
rohr_stage_example_assets(fly_to_finish fly-to-finish)
