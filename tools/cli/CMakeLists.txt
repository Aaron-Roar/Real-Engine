add_executable(rohr_cli src/main.c)
target_link_libraries(rohr_cli PRIVATE rohr_editor_core)
set_target_properties(rohr_cli PROPERTIES
    OUTPUT_NAME rohr
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)
