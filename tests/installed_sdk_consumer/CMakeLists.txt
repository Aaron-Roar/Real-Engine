cmake_minimum_required(VERSION 3.20)

project(rohr_installed_sdk_consumer LANGUAGES C)

find_package(Rohr CONFIG REQUIRED)

add_executable(rohr_installed_sdk_consumer main.c)
target_link_libraries(rohr_installed_sdk_consumer PRIVATE Rohr::Engine)
