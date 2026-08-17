#include "editor_config.h"

#include <stdio.h>
#include <string.h>

#ifndef EDITOR_CONFIG_TEST_DATA_DIR
#define EDITOR_CONFIG_TEST_DATA_DIR "."
#endif

static bool path_get(char *path, size_t capacity, const char *name) {
    int count = snprintf(path, capacity, "%s/%s", EDITOR_CONFIG_TEST_DATA_DIR, name);
    return count >= 0 && (size_t)count < capacity;
}

int main(int count, char **program_arguments) {
    EditorConfig config;
    EditorResult result;
    const EditorConfigCommand *command;
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    char arguments[EDITOR_CONFIG_ARGUMENT_MAX][EDITOR_CONFIG_ARGUMENT_LENGTH_MAX];
    const char *output[EDITOR_CONFIG_ARGUMENT_MAX + 1];
    EditorConfigCommand parsed;
    char lua_error[EDITOR_ERROR_MESSAGE_MAX];

    editor_config_init(&config);
    if(!path_get(path, sizeof(path), "config-base.lua")) return 1;
    result = editor_config_file_merge(&config, path, true);
    if(editor_result_check(result) || !config.font_set ||
            strcmp(config.font, "/fonts/base.ttf") != 0) return 1;
    command = editor_config_command_get(&config, EDITOR_CONFIG_FRONTEND_CLI,
        EDITOR_CONFIG_OPERATION_CONFIGURE);
    if(command == NULL || strcmp(command->arguments[0], "base-configure") != 0)
        return 1;
    command = editor_config_command_get(&config, EDITOR_CONFIG_FRONTEND_CLI,
        EDITOR_CONFIG_OPERATION_COMPILE);
    if(command == NULL || strcmp(command->arguments[0], "cli-compile") != 0)
        return 1;

    if(!path_get(path, sizeof(path), "config-project.lua")) return 1;
    result = editor_config_file_merge(&config, path, true);
    if(editor_result_check(result)) return 1;
    command = editor_config_command_get(&config, EDITOR_CONFIG_FRONTEND_CLI,
        EDITOR_CONFIG_OPERATION_CONFIGURE);
    if(command == NULL || strcmp(command->arguments[0], "project-configure") != 0)
        return 1;
    command = editor_config_command_get(&config, EDITOR_CONFIG_FRONTEND_CLI,
        EDITOR_CONFIG_OPERATION_COMPILE);
    if(command == NULL || strcmp(command->arguments[0],
            "project-cli-compile") != 0) return 1;
    result = editor_config_command_expand(command, "/game path", "/game path/build",
        "/sdk path", arguments, output);
    if(editor_result_check(result) || strcmp(output[0], "project-cli-compile") != 0 ||
            strcmp(output[1], "/game path/build") != 0 || output[2] != NULL) return 1;

    if(!path_get(path, sizeof(path), "config-malformed.lua")) return 1;
    result = editor_config_file_merge(&config, path, true);
    if(!editor_result_check(result) ||
            strstr(result.result.error.message, "array of strings") == NULL) return 1;

    result = editor_config_command_expression_parse(
        "{ \"cmake\", \"--build\", \"{build}\" }", &parsed);
    if(editor_result_check(result) || parsed.count != 3 ||
            strcmp(parsed.arguments[2], "{build}") != 0) return 1;
    result = editor_config_command_expression_write(&parsed, path, sizeof(path));
    if(editor_result_check(result) ||
            strcmp(path, "{\"cmake\", \"--build\", \"{build}\"}") != 0) return 1;
    result = editor_config_command_expression_parse("\"not an array\"", &parsed);
    if(!editor_result_check(result)) return 1;
    result = editor_config_command_expression_parse("{ \"tool\", 42 }", &parsed);
    if(!editor_result_check(result)) return 1;
    result = editor_config_command_expression_parse(
        "{ \"tool\", \"{unknown}\" }", &parsed);
    if(!editor_result_check(result)) return 1;
    result = editor_config_command_expression_parse("{ \"\" }", &parsed);
    if(!editor_result_check(result)) return 1;
    result = editor_config_command_expression_parse_detailed("{", &parsed,
        lua_error, sizeof(lua_error));
    if(!editor_result_check(result) || lua_error[0] == '\0') return 1;
    if(count < 2) return 1;
    memset(&parsed, 0, sizeof(parsed));
    parsed.set = true;
    parsed.count = 1;
    snprintf(parsed.arguments[0], sizeof(parsed.arguments[0]), "%s",
        program_arguments[1]);
    result = editor_config_command_executable_check(&parsed, ".");
    if(editor_result_check(result)) return 1;
    snprintf(parsed.arguments[0], sizeof(parsed.arguments[0]), "%s",
        "rohr-editor-definitely-missing-executable");
    result = editor_config_command_executable_check(&parsed, ".");
    if(!editor_result_check(result) ||
            strstr(result.result.error.message, "PATH") == NULL) return 1;

    {
        const char *root = "/tmp/rohr_editor_gui_config_test";
        char override_path[EDITOR_WORKSPACE_PATH_MAX * 2];
        EditorConfig saved;
        (void)SDL_RemovePath("/tmp/rohr_editor_gui_config_test/.rohr/gui-overrides.lua");
        (void)SDL_RemovePath("/tmp/rohr_editor_gui_config_test/.rohr");
        (void)SDL_RemovePath(root);
        if(!SDL_CreateDirectory(root)) return 1;
        result = editor_config_command_expression_parse(
            "{ \"tool\", \"{project}\" }", &parsed);
        if(editor_result_check(result)) return 1;
        result = editor_config_gui_override_save(root, &parsed, NULL);
        if(editor_result_check(result)) return 1;
        snprintf(override_path, sizeof(override_path),
            "%s/.rohr/gui-overrides.lua", root);
        editor_config_init(&saved);
        result = editor_config_file_merge(&saved, override_path, true);
        if(editor_result_check(result) || !saved.gui_configure.set ||
                saved.gui_compile.set ||
                strcmp(saved.gui_configure.arguments[0], "tool") != 0) return 1;
        (void)SDL_RemovePath(override_path);
        snprintf(override_path, sizeof(override_path), "%s/.rohr", root);
        (void)SDL_RemovePath(override_path);
        (void)SDL_RemovePath(root);
    }
    return 0;
}
