#ifndef ROHR_EDITOR_COMMAND_H
#define ROHR_EDITOR_COMMAND_H

#include "editor_object_commands.h"

typedef enum EditorCommandType {
    EDITOR_COMMAND_OBJECT_ADD,
    EDITOR_COMMAND_OBJECT_RENAME,
    EDITOR_COMMAND_OBJECT_REMOVE
} EditorCommandType;

typedef struct EditorCommand {
    EditorCommandType type;
    union {
        struct {
            char name[EDITOR_OBJECT_NAME_MAX];
            Position position;
        } object_add;
        struct {
            EditorObjectId object;
            char name[EDITOR_OBJECT_NAME_MAX];
        } object_rename;
        struct {
            EditorObjectId object;
        } object_remove;
    } data;
} EditorCommand;

typedef struct EditorCommandResult {
    ErrorResultKind kind;
    union {
        EditorObjectId object;
        EditorError error;
    } result;
} EditorCommandResult;

EditorCommandResult editor_command_execute(EditorProject *project,
    const EditorCommand *command);
EditorResult editor_command_cli_parse(int count, char **arguments,
    const char **document_path, EditorCommand *command);
EditorResult editor_command_cli_write(const EditorCommand *command,
    const char *document_path, char *output, size_t output_capacity);

#endif
