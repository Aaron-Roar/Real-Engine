add_executable(rohr_editor_gui
    src/browser/editor_file_browser.c
    src/editor_history.c
    src/editor_shortcuts.c
    src/editor_navigation.c
    src/editor_viewport.c
    src/editors/multi/editor_bulk_panel.c
    src/panels/editor_build_settings_panel.c
    src/panels/editor_build_notifications.c
    src/panels/editor_generation_report.c
    src/panels/editor_notification_panel.c
    src/panels/editor_notification_store.c
    src/editors/geometry/editor_origin_panel.c
    src/panels/editor_terminal_panel.c
    src/panels/editor_visual_settings_panel.c
    src/panels/editor_error_notifications.c
    src/states/editor_app_state.c
    src/states/editor_project_launcher_state.c
    src/editors/editor_mode_controls.c
    src/editors/geometry/editor_vertex.c
    src/editors/geometry/editor_line.c
    src/editors/geometry/editor_auto_shape.c
    src/editors/geometry/editor_hitbox.c
    src/editors/physics/editor_particle.c
    src/editors/physics/editor_anchor.c
    src/editors/physics/editor_rigid_body.c
    src/editors/physics/editor_joint.c
    src/editors/soft_body/editor_soft_beam.c
    src/editors/soft_body/editor_soft_node.c
    src/editors/soft_body/editor_soft_area.c
    src/editors/soft_body/editor_soft_body.c
    src/editors/render/editor_animation_frame.c
    src/editors/render/editor_sprite.c
    src/editors/render/editor_animated_sprite.c
    src/editors/object/editor_object.c
    src/editors/object/editor_hierarchy.c
    src/viewport/controls/editor_rotation_control.c
    src/viewport/controls/editor_coordinate_toggle.c
    src/viewport/menus/editor_viewport_context_menu.c
    src/main.c
)

target_include_directories(rohr_editor_gui PRIVATE src ${PROJECT_SOURCE_DIR}/lib)
target_compile_definitions(rohr_editor_gui PRIVATE
    ROHR_DEVELOPMENT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(rohr_editor_gui PRIVATE rohr_engine rohr_terminal rohr_editor_core)

set_target_properties(rohr_editor_gui PROPERTIES
    OUTPUT_NAME rohr-gui
    RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tools/rohr-gui
)

add_custom_command(TARGET rohr_editor_gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:rohr_editor_gui>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/jetbrains_mono_bold_italic.ttf
        $<TARGET_FILE_DIR:rohr_editor_gui>/assets/jetbrains_mono_bold_italic.ttf
    VERBATIM
)
install(TARGETS rohr_editor_gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets/ DESTINATION ${CMAKE_INSTALL_BINDIR}/assets)

if(ROHR_BUILD_TESTS)
    add_executable(editor_app_state_test
        tests/editor_app_state.c
        src/states/editor_app_state.c
    )
    target_include_directories(editor_app_state_test PRIVATE src)
    set_target_properties(editor_app_state_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests
    )
    add_test(NAME editor_app_state COMMAND editor_app_state_test)

    add_executable(editor_project_test
        tests/editor_project.c
    )
    target_include_directories(editor_project_test PRIVATE src ${PROJECT_SOURCE_DIR}/lib)
    target_link_libraries(editor_project_test PRIVATE rohr_engine rohr_editor_core)
    set_target_properties(editor_project_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests
    )
    add_test(NAME editor_project COMMAND editor_project_test)

    add_executable(editor_navigation_test
        tests/editor_navigation.c
        src/browser/editor_file_browser.c
        src/editor_history.c
        src/editor_navigation.c
        src/editor_viewport.c
        src/viewport/controls/editor_rotation_control.c
        src/editors/multi/editor_bulk_panel.c
    )
    target_include_directories(editor_navigation_test PRIVATE
        src ${PROJECT_SOURCE_DIR}/lib)
    target_link_libraries(editor_navigation_test PRIVATE rohr_engine rohr_editor_core)
    set_target_properties(editor_navigation_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests
    )
    add_test(NAME editor_navigation COMMAND editor_navigation_test)

    add_executable(editor_history_test
        tests/editor_history.c
        src/editor_history.c
        src/editor_shortcuts.c
        src/editor_viewport.c
        src/viewport/controls/editor_rotation_control.c
    )
    target_include_directories(editor_history_test PRIVATE
        src ${PROJECT_SOURCE_DIR}/lib)
    target_link_libraries(editor_history_test PRIVATE rohr_engine rohr_editor_core)
    set_target_properties(editor_history_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests
    )
    add_test(NAME editor_history COMMAND editor_history_test)

    add_executable(editor_notification_store_test
        tests/editor_notification_store.c
        src/panels/editor_notification_store.c
    )
    target_include_directories(editor_notification_store_test PRIVATE src)
    set_target_properties(editor_notification_store_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ROHR_BUILD_OUTPUT_DIRECTORY}/tests
    )
    add_test(NAME editor_notification_store COMMAND editor_notification_store_test)
endif()
