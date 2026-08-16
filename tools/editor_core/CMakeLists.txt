add_library(rohr_editor_core STATIC
    ${PROJECT_SOURCE_DIR}/tools/editor/src/editor_error.c
    ${PROJECT_SOURCE_DIR}/tools/editor/src/editor_project.c
    ${PROJECT_SOURCE_DIR}/tools/editor/src/editor_project_json.c
    ${PROJECT_SOURCE_DIR}/tools/editor/src/editor_workspace.c
    src/editor_command.c
    src/editor_auto_shape.c
    src/editor_cli_selectors.c
    src/editor_cli_standard.c
    src/editor_document.c
    src/editor_config.c
    src/editor_object_commands.c
)

target_compile_definitions(rohr_editor_core PRIVATE
    ROHR_DEVELOPMENT_SHARE_DIR="${PROJECT_SOURCE_DIR}/config")

target_include_directories(rohr_editor_core
    PUBLIC
        include
        ${PROJECT_SOURCE_DIR}/tools/editor/src
        ${PROJECT_SOURCE_DIR}/lib
)
target_link_libraries(rohr_editor_core PUBLIC rohr_engine PRIVATE rohr_lua)

if(ROHR_BUILD_TESTS)
    add_executable(editor_core_test tests/editor_core.c)
    target_link_libraries(editor_core_test PRIVATE rohr_editor_core)
    set_target_properties(editor_core_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME editor_core COMMAND editor_core_test)

    add_executable(editor_config_test tests/editor_config.c)
    target_link_libraries(editor_config_test PRIVATE rohr_editor_core)
    target_compile_definitions(editor_config_test PRIVATE
        EDITOR_CONFIG_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    set_target_properties(editor_config_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME editor_config COMMAND editor_config_test)
endif()
