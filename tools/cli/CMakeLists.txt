add_executable(rohr_editor_cli src/main.c)
target_link_libraries(rohr_editor_cli PRIVATE rohr_editor_core)
set_target_properties(rohr_editor_cli PROPERTIES
    OUTPUT_NAME editor-cli
    RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tools/editor-cli
)
