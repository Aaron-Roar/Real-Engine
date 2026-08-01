set(generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(generated_header ${generated_dir}/game_components.h)
set(generated_source ${generated_dir}/game_components.c)

add_executable(pong_component_generator src/generate_components.c)
target_link_libraries(pong_component_generator PRIVATE rohr_editor)

add_custom_command(
    OUTPUT ${generated_header} ${generated_source}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${generated_dir}
    COMMAND $<TARGET_FILE:pong_component_generator> ${generated_header} ${generated_source}
    DEPENDS pong_component_generator
    COMMENT "Generating Pong app components"
    VERBATIM
)

add_executable(pong src/main.c ${generated_header} ${generated_source})
target_include_directories(pong PRIVATE ${generated_dir})
target_link_libraries(pong PRIVATE rohr_editor)
rohr_stage_example_assets(pong editor/pong)
