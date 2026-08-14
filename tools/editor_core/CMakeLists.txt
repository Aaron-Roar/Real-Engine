add_library(rohr_editor_core STATIC
    ${PROJECT_SOURCE_DIR}/tools/editor/src/editor_error.c
    ${PROJECT_SOURCE_DIR}/tools/editor/src/editor_project.c
    ${PROJECT_SOURCE_DIR}/tools/editor/src/editor_project_json.c
    src/editor_document.c
    src/editor_object_commands.c
)

target_include_directories(rohr_editor_core
    PUBLIC
        include
        ${PROJECT_SOURCE_DIR}/tools/editor/src
        ${PROJECT_SOURCE_DIR}/lib
)
target_link_libraries(rohr_editor_core PUBLIC rohr_engine)

if(ROHR_BUILD_TESTS)
    add_executable(editor_core_test tests/editor_core.c)
    target_link_libraries(editor_core_test PRIVATE rohr_editor_core)
    set_target_properties(editor_core_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests)
    add_test(NAME editor_core COMMAND editor_core_test)
endif()
