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

int main(void) {
    EditorConfig config;
    EditorResult result;
    const EditorConfigCommand *command;
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    char arguments[EDITOR_CONFIG_ARGUMENT_MAX][EDITOR_CONFIG_ARGUMENT_LENGTH_MAX];
    const char *output[EDITOR_CONFIG_ARGUMENT_MAX + 1];

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
    return 0;
}
