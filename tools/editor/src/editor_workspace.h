#ifndef ROHR_EDITOR_WORKSPACE_H
#define ROHR_EDITOR_WORKSPACE_H

#include "editor_project.h"

#define EDITOR_WORKSPACE_FORMAT_VERSION 1
#define EDITOR_WORKSPACE_PATH_MAX 1024
#define EDITOR_WORKSPACE_NAME_MAX 64

typedef struct EditorWorkspaceConfig {
    uint32_t format_version;
    char name[EDITOR_WORKSPACE_NAME_MAX];
    char source_directory[EDITOR_WORKSPACE_PATH_MAX];
    char generated_directory[EDITOR_WORKSPACE_PATH_MAX];
    char asset_directory[EDITOR_WORKSPACE_PATH_MAX];
    char object_directory[EDITOR_WORKSPACE_PATH_MAX];
    char editor_state_file[EDITOR_WORKSPACE_PATH_MAX];
} EditorWorkspaceConfig;

typedef struct EditorWorkspace {
    EditorWorkspaceConfig config;
    char directory[EDITOR_WORKSPACE_PATH_MAX];
    bool open;
} EditorWorkspace;

typedef enum EditorWorkspaceCommandType {
    EDITOR_WORKSPACE_COMMAND_CREATE,
    EDITOR_WORKSPACE_COMMAND_LOAD,
    EDITOR_WORKSPACE_COMMAND_SAVE,
    EDITOR_WORKSPACE_COMMAND_GENERATE_C
} EditorWorkspaceCommandType;

typedef struct EditorWorkspaceCommand {
    EditorWorkspaceCommandType type;
    char directory[EDITOR_WORKSPACE_PATH_MAX];
} EditorWorkspaceCommand;

EditorWorkspaceConfig editor_workspace_config_default_get(void);
EditorResult editor_workspace_create(EditorWorkspace *workspace, EditorProject *project,
    const char *directory);
EditorResult editor_workspace_load(EditorWorkspace *workspace, EditorProject *project,
    const char *directory);
bool editor_workspace_save(const EditorWorkspace *workspace,
    const EditorProject *project);
bool editor_workspace_c_generate(const EditorWorkspace *workspace,
    const EditorProject *project);
void editor_workspace_close(EditorWorkspace *workspace, EditorProject *project);
EditorResult editor_workspace_command_execute(EditorWorkspace *workspace,
    EditorProject *project, const EditorWorkspaceCommand *command);
EditorResult editor_workspace_command_cli_write(const EditorWorkspaceCommand *command,
    char *output, size_t output_capacity);

#endif
