add_executable(rohr_editor
    src/main.c
)

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
