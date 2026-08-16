#ifndef ROHR_EDITOR_CONFIG_H
#define ROHR_EDITOR_CONFIG_H

#include "editor_error.h"
#include "editor_workspace.h"

#include <stdbool.h>
#include <stddef.h>

#define EDITOR_CONFIG_ARGUMENT_MAX 32
#define EDITOR_CONFIG_ARGUMENT_LENGTH_MAX (EDITOR_WORKSPACE_PATH_MAX * 2)

typedef enum EditorConfigFrontend {
    EDITOR_CONFIG_FRONTEND_CLI,
    EDITOR_CONFIG_FRONTEND_GUI
} EditorConfigFrontend;

typedef enum EditorConfigOperation {
    EDITOR_CONFIG_OPERATION_CONFIGURE,
    EDITOR_CONFIG_OPERATION_COMPILE
} EditorConfigOperation;

typedef struct EditorConfigCommand {
    char arguments[EDITOR_CONFIG_ARGUMENT_MAX][EDITOR_CONFIG_ARGUMENT_LENGTH_MAX];
    size_t count;
    bool set;
} EditorConfigCommand;

typedef struct EditorConfig {
    char font[EDITOR_WORKSPACE_PATH_MAX * 2];
    bool font_set;
    EditorConfigCommand project_configure;
    EditorConfigCommand project_compile;
    EditorConfigCommand cli_configure;
    EditorConfigCommand cli_compile;
    EditorConfigCommand gui_configure;
    EditorConfigCommand gui_compile;
} EditorConfig;

void editor_config_init(EditorConfig *config);
EditorResult editor_config_file_merge(EditorConfig *config, const char *path,
    bool required);
const EditorConfigCommand *editor_config_command_get(const EditorConfig *config,
    EditorConfigFrontend frontend, EditorConfigOperation operation);
EditorResult editor_config_command_expand(const EditorConfigCommand *command,
    const char *project, const char *build, const char *sdk,
    char arguments[EDITOR_CONFIG_ARGUMENT_MAX][EDITOR_CONFIG_ARGUMENT_LENGTH_MAX],
    const char *output[EDITOR_CONFIG_ARGUMENT_MAX + 1]);
EditorResult editor_config_sdk_path_get(char *output, size_t capacity,
    const char *name, bool required);

#endif
