add_executable(rohr_editor
    src/editor_project.c
    src/editor_project_json.c
    src/editor_viewport.c
    src/main.c
)

target_include_directories(rohr_editor PRIVATE src ${PROJECT_SOURCE_DIR}/lib)

target_link_libraries(rohr_editor PRIVATE rohr_engine)

set_target_properties(rohr_editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/editor
)

add_custom_command(TARGET rohr_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:rohr_editor>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/JetBrainsMono-BoldItalic.ttf
        $<TARGET_FILE_DIR:rohr_editor>/assets/JetBrainsMono-BoldItalic.ttf
    VERBATIM
)

if(ROHR_BUILD_TESTS)
    add_executable(editor_project_test
        tests/editor_project.c
        src/editor_project.c
        src/editor_project_json.c
    )
    target_include_directories(editor_project_test PRIVATE src ${PROJECT_SOURCE_DIR}/lib)
    target_link_libraries(editor_project_test PRIVATE rohr_engine)
    set_target_properties(editor_project_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
    add_test(NAME editor_project COMMAND editor_project_test)
endif()
