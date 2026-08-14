#ifndef ROHR_EDITOR_COMMAND_H
#define ROHR_EDITOR_COMMAND_H

#include "editor_object_commands.h"

typedef enum EditorCommandType {
    EDITOR_COMMAND_OBJECT_ADD,
    EDITOR_COMMAND_OBJECT_RENAME,
    EDITOR_COMMAND_OBJECT_REMOVE,
    EDITOR_COMMAND_OBJECT_POSITION,
    EDITOR_COMMAND_RIGID_BODY_TRANSFORM,
    EDITOR_COMMAND_VERTEX_POSITION,
    EDITOR_COMMAND_ANCHOR_TRANSFORM,
    EDITOR_COMMAND_SOFT_BODY_TRANSFORM,
    EDITOR_COMMAND_SOFT_NODE_POSITION,
    EDITOR_COMMAND_RIGID_BODY_ORIGIN,
    EDITOR_COMMAND_SOFT_BODY_ORIGIN
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
        struct {
            EditorObjectId object;
            Position position;
        } object_position;
        struct {
            EditorObjectId object;
            EditorRigidBodyId body;
            Position position;
            float rotation;
        } rigid_body_transform;
        struct {
            EditorObjectId object;
            EditorRigidBodyId body;
            EditorHitboxId hitbox;
            EditorVertexId vertex;
            Position position;
        } vertex_position;
        struct {
            EditorObjectId object;
            EditorAnchorId anchor;
            Position position;
            float rotation;
        } anchor_transform;
        struct {
            EditorObjectId object;
            EditorSoftBodyId body;
            Position position;
            float rotation;
        } soft_body_transform;
        struct {
            EditorObjectId object;
            EditorSoftBodyId body;
            EditorSoftNodeId node;
            Position position;
        } soft_node_position;
        struct {
            EditorObjectId object;
            uint32_t body;
            Position position;
        } origin;
    } data;
} EditorCommand;

typedef struct EditorCommandResult {
    ErrorResultKind kind;
    union {
        EditorObjectId object;
        EditorError error;
    } result;
} EditorCommandResult;

typedef void (*EditorCommandExecuted)(const EditorCommand *command, void *context);

EditorCommandResult editor_command_execute(EditorProject *project,
    const EditorCommand *command);
void editor_command_executed_callback_set(EditorCommandExecuted callback,
    void *context);
EditorResult editor_command_cli_parse(int count, char **arguments,
    const char **document_path, EditorCommand *command);
EditorResult editor_command_cli_write(const EditorCommand *command,
    const char *document_path, char *output, size_t output_capacity);

#endif
