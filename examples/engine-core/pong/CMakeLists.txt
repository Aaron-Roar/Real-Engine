cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(rohr_pong LANGUAGES C)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/RohrExample.cmake)
rohr_example_bootstrap()

set(generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(generated_header ${generated_dir}/game_components.h)
set(generated_source ${generated_dir}/game_components.c)

add_executable(pong_component_generator src/generate_components.c)
target_link_libraries(pong_component_generator PRIVATE rohr_tools)

add_custom_command(
    OUTPUT ${generated_header} ${generated_source}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${generated_dir}
    COMMAND $<TARGET_FILE:pong_component_generator> ${generated_header} ${generated_source}
    DEPENDS pong_component_generator
    COMMENT "Generating Pong app components"
    VERBATIM
)

add_executable(pong src/main.c ${generated_header} ${generated_source})
rohr_add_example_runtime(pong)
target_include_directories(pong PRIVATE ${generated_dir})
target_link_libraries(pong PRIVATE rohr_engine)
rohr_stage_example_assets(pong engine-core/pong)
