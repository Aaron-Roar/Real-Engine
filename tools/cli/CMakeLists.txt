add_executable(rohr_editor_cli src/main.c)
target_link_libraries(rohr_editor_cli PRIVATE rohr_editor_core)
target_compile_definitions(rohr_editor_cli PRIVATE
    ROHR_DEVELOPMENT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_target_properties(rohr_editor_cli PROPERTIES
    OUTPUT_NAME editor-cli
    RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tools/editor-cli
)
install(TARGETS rohr_editor_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME editor_cli_help_general COMMAND rohr_editor_cli --help)
    set_tests_properties(editor_cli_help_general PROPERTIES
        PASS_REGULAR_EXPRESSION "General examples")
    add_test(NAME editor_cli_help_context COMMAND rohr_editor_cli
        --object car --body chassis --property mass)
    set_tests_properties(editor_cli_help_context PROPERTIES
        PASS_REGULAR_EXPRESSION "Selected depth: rigid body")
    add_test(NAME editor_cli_help_prevents_execution COMMAND rohr_editor_cli
        --project /this/project/does/not/exist/project.rohr.json
        --object car --property visibility true --help)
    set_tests_properties(editor_cli_help_prevents_execution PROPERTIES
        PASS_REGULAR_EXPRESSION "Selected depth: object")
endif()
