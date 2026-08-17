#include "editor_build_settings_panel.h"

#include <stdio.h>
#include <string.h>

static bool editor_build_settings_text_create(const FontAsset *font,
        const char *value, TextAsset *text) {
    TextAssetResult result = rohr_graphics_text_create(font, value,
        (Color){235, 238, 244, 255});
    if(rohr_error_check(result)) return false;
    *text = result.result.value;
    return true;
}

static EditorResult editor_build_settings_layers_load(EditorConfig *base,
        EditorConfig *effective, const char *project_directory) {
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    EditorResult result;
    editor_config_init(base);
    result = editor_config_sdk_path_get(path, sizeof(path), "editor.lua", true);
    if(editor_result_check(result)) return result;
    result = editor_config_file_merge(base, path, true);
    if(editor_result_check(result)) return result;
    if(snprintf(path, sizeof(path), "%s/editor.lua", project_directory) >=
            (int)sizeof(path)) return editor_result_error(EDITOR_ERROR_CAPACITY,
        "Project editor.lua path is too long");
    result = editor_config_file_merge(base, path, false);
    if(editor_result_check(result)) return result;
    *effective = *base;
    if(snprintf(path, sizeof(path), "%s/.rohr/gui-overrides.lua",
            project_directory) >= (int)sizeof(path)) return editor_result_error(
        EDITOR_ERROR_CAPACITY, "Project GUI override path is too long");
    return editor_config_file_merge(effective, path, false);
}

static EditorConfigCommand editor_build_settings_builtin(bool configure) {
    EditorConfigCommand command = {.set = true};
    static const char *configure_arguments[] = {
        "cmake", "-S", "{project}", "-B", "{build}"};
    static const char *compile_arguments[] = {"cmake", "--build", "{build}"};
    const char *const *arguments = configure ? configure_arguments : compile_arguments;
    command.count = configure ? 5 : 3;
    for(size_t i = 0; i < command.count; i += 1)
        snprintf(command.arguments[i], sizeof(command.arguments[i]), "%s", arguments[i]);
    return command;
}

bool editor_build_settings_panel_create(EditorBuildSettingsPanel *panel,
        const FontAsset *font) {
    if(panel == NULL || font == NULL) return false;
    *panel = (EditorBuildSettingsPanel){0};
    return editor_build_settings_text_create(font, "Build Settings",
            &panel->title_label) &&
        editor_build_settings_text_create(font, "Build configure override",
            &panel->configure_label) &&
        editor_build_settings_text_create(font, "Build compile override",
            &panel->compile_label) &&
        editor_build_settings_text_create(font, "", &panel->configure_field) &&
        editor_build_settings_text_create(font, "", &panel->compile_field) &&
        editor_build_settings_text_create(font, "Apply", &panel->apply_label) &&
        editor_build_settings_text_create(font, "Cancel", &panel->cancel_label) &&
        editor_build_settings_text_create(font, "Reset to inherited",
            &panel->reset_label) &&
        editor_build_settings_text_create(font, "", &panel->error_label);
}

EditorResult editor_build_settings_panel_open(EditorBuildSettingsPanel *panel,
        const char *project_directory) {
    EditorConfig base;
    EditorConfig effective;
    EditorConfig overrides;
    const EditorConfigCommand *command;
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    EditorResult result;
    if(panel == NULL || project_directory == NULL) return editor_result_error(
        EDITOR_ERROR_INVALID_ARGUMENT, "Build settings received an invalid project");
    result = editor_build_settings_layers_load(&base, &effective, project_directory);
    if(editor_result_check(result)) return result;
    command = editor_config_command_get(&base, EDITOR_CONFIG_FRONTEND_GUI,
        EDITOR_CONFIG_OPERATION_CONFIGURE);
    panel->inherited_configure = command == NULL ?
        editor_build_settings_builtin(true) : *command;
    command = editor_config_command_get(&base, EDITOR_CONFIG_FRONTEND_GUI,
        EDITOR_CONFIG_OPERATION_COMPILE);
    panel->inherited_compile = command == NULL ?
        editor_build_settings_builtin(false) : *command;
    command = editor_config_command_get(&effective, EDITOR_CONFIG_FRONTEND_GUI,
        EDITOR_CONFIG_OPERATION_CONFIGURE);
    if(command == NULL) command = &panel->inherited_configure;
    result = editor_config_command_expression_write(command, panel->configure,
        sizeof(panel->configure));
    if(editor_result_check(result)) return result;
    command = editor_config_command_get(&effective, EDITOR_CONFIG_FRONTEND_GUI,
        EDITOR_CONFIG_OPERATION_COMPILE);
    if(command == NULL) command = &panel->inherited_compile;
    result = editor_config_command_expression_write(command, panel->compile,
        sizeof(panel->compile));
    if(editor_result_check(result)) return result;
    editor_config_init(&overrides);
    if(snprintf(path, sizeof(path), "%s/.rohr/gui-overrides.lua",
            project_directory) >= (int)sizeof(path)) return editor_result_error(
        EDITOR_ERROR_CAPACITY, "Project GUI override path is too long");
    result = editor_config_file_merge(&overrides, path, false);
    if(editor_result_check(result)) return result;
    panel->configure_override = overrides.gui_configure.set;
    panel->compile_override = overrides.gui_compile.set;
    panel->error[0] = '\0';
    panel->open = true;
    return editor_result_value(true);
}

static void editor_build_settings_error_set(EditorBuildSettingsPanel *panel,
        EditorResult result) {
    if(!editor_result_check(result)) panel->error[0] = '\0';
    else snprintf(panel->error, sizeof(panel->error), "%s",
        result.result.error.message);
    (void)rohr_graphics_text_value_set(&panel->error_label, panel->error);
}

static EditorResult editor_build_settings_validate(
        const EditorBuildSettingsPanel *panel, EditorConfigCommand *configure,
        EditorConfigCommand *compile) {
    EditorResult result = editor_config_command_expression_parse(
        panel->configure, configure);
    if(editor_result_check(result)) return editor_result_error(
        result.result.error.code, "Configure override: %s",
        result.result.error.message);
    result = editor_config_command_expression_parse(panel->compile, compile);
    if(editor_result_check(result)) return editor_result_error(
        result.result.error.code, "Compile override: %s",
        result.result.error.message);
    return editor_result_value(true);
}

void editor_build_settings_panel_draw(EditorBuildSettingsPanel *panel,
        const char *project_directory, UIRect bounds) {
    UIFieldResult configure_result;
    UIFieldResult compile_result;
    EditorConfigCommand configure;
    EditorConfigCommand compile;
    EditorResult result;
    bool valid;
    bool apply_clicked = false;
    if(panel == NULL || !panel->open) return;
    rohr_ui_modal_controls_begin();
    rohr_ui_surface(bounds, (Color){37, 42, 52, 255});
    rohr_ui_border(bounds, 2.0f, (Color){5, 6, 8, 255});
    rohr_ui_label(&panel->title_label,
        (UIRect){bounds.x + 20.0f, bounds.y + 14.0f, bounds.width - 40.0f, 32.0f});
    rohr_ui_label(&panel->configure_label,
        (UIRect){bounds.x + 24.0f, bounds.y + 60.0f, bounds.width - 48.0f, 24.0f});
    configure_result = rohr_ui_multiline_field("editor.settings.build.configure",
        (UIFieldBinding){UI_FIELD_STRING, panel->configure,
            sizeof(panel->configure), NULL}, &panel->configure_field,
        (UIRect){bounds.x + 30.0f, bounds.y + 88.0f,
            bounds.width - 60.0f, 84.0f}, NULL);
    if(configure_result.changed) panel->configure_override = true;
    if(rohr_ui_button("editor.settings.build.configure.reset", &panel->reset_label,
            (UIRect){bounds.x + bounds.width - 190.0f, bounds.y + 178.0f,
                160.0f, 30.0f}, NULL).clicked) {
        (void)editor_config_command_expression_write(&panel->inherited_configure,
            panel->configure, sizeof(panel->configure));
        panel->configure_override = false;
    }
    rohr_ui_label(&panel->compile_label,
        (UIRect){bounds.x + 24.0f, bounds.y + 218.0f, bounds.width - 48.0f, 24.0f});
    compile_result = rohr_ui_multiline_field("editor.settings.build.compile",
        (UIFieldBinding){UI_FIELD_STRING, panel->compile,
            sizeof(panel->compile), NULL}, &panel->compile_field,
        (UIRect){bounds.x + 30.0f, bounds.y + 246.0f,
            bounds.width - 60.0f, 84.0f}, NULL);
    if(compile_result.changed) panel->compile_override = true;
    if(rohr_ui_button("editor.settings.build.compile.reset", &panel->reset_label,
            (UIRect){bounds.x + bounds.width - 190.0f, bounds.y + 336.0f,
                160.0f, 30.0f}, NULL).clicked) {
        (void)editor_config_command_expression_write(&panel->inherited_compile,
            panel->compile, sizeof(panel->compile));
        panel->compile_override = false;
    }
    result = editor_build_settings_validate(panel, &configure, &compile);
    valid = !editor_result_check(result);
    if(valid) {
        snprintf(panel->error, sizeof(panel->error),
            "Valid Lua build command arrays");
    } else {
        snprintf(panel->error, sizeof(panel->error), "%s",
            result.result.error.message);
    }
    (void)rohr_graphics_text_value_set(&panel->error_label, panel->error);
    rohr_ui_label(&panel->error_label,
        (UIRect){bounds.x + 30.0f, bounds.y + bounds.height - 100.0f,
            bounds.width - 60.0f, 34.0f});
    if(valid) {
        apply_clicked = rohr_ui_button("editor.settings.build.apply",
            &panel->apply_label, (UIRect){bounds.x + bounds.width - 250.0f,
                bounds.y + bounds.height - 54.0f, 100.0f, 34.0f}, NULL).clicked;
    } else {
        rohr_ui_button_disabled((UIRect){bounds.x + bounds.width - 250.0f,
            bounds.y + bounds.height - 54.0f, 100.0f, 34.0f}, NULL);
    }
    if(apply_clicked) {
        result = editor_config_gui_override_save(
            project_directory, panel->configure_override ? &configure : NULL,
            panel->compile_override ? &compile : NULL);
        editor_build_settings_error_set(panel, result);
        if(!editor_result_check(result)) panel->open = false;
    }
    if(rohr_ui_button("editor.settings.build.cancel", &panel->cancel_label,
            (UIRect){bounds.x + bounds.width - 140.0f,
                bounds.y + bounds.height - 54.0f, 100.0f, 34.0f}, NULL).clicked)
        panel->open = false;
    rohr_ui_modal_controls_end();
}

void editor_build_settings_panel_destroy(EditorBuildSettingsPanel *panel) {
    if(panel == NULL) return;
    rohr_graphics_text_destroy(&panel->title_label);
    rohr_graphics_text_destroy(&panel->configure_label);
    rohr_graphics_text_destroy(&panel->compile_label);
    rohr_graphics_text_destroy(&panel->configure_field);
    rohr_graphics_text_destroy(&panel->compile_field);
    rohr_graphics_text_destroy(&panel->apply_label);
    rohr_graphics_text_destroy(&panel->cancel_label);
    rohr_graphics_text_destroy(&panel->reset_label);
    rohr_graphics_text_destroy(&panel->error_label);
    *panel = (EditorBuildSettingsPanel){0};
}
