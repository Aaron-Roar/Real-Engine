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
    EDITOR_COMMAND_SOFT_BODY_ORIGIN,
    EDITOR_COMMAND_VIEWPORT_CAMERA,
    EDITOR_COMMAND_VIEWPORT_COORDINATES,
    EDITOR_COMMAND_VISIBILITY,
    EDITOR_COMMAND_NAVIGATION_SET,
    EDITOR_COMMAND_ITEM_ADD,
    EDITOR_COMMAND_ITEM_REMOVE,
    EDITOR_COMMAND_ITEM_RENAME,
    EDITOR_COMMAND_PROPERTY_SET,
    EDITOR_COMMAND_RELATIONSHIP_SET,
    EDITOR_COMMAND_COLLISION_MASK_ADD,
    EDITOR_COMMAND_COLLISION_FILTER_SET
} EditorCommandType;

typedef enum EditorItemKind {
    EDITOR_ITEM_OBJECT,
    EDITOR_ITEM_RIGID_BODY,
    EDITOR_ITEM_HITBOX,
    EDITOR_ITEM_JOINT,
    EDITOR_ITEM_ANCHOR,
    EDITOR_ITEM_SOFT_BODY,
    EDITOR_ITEM_SOFT_NODE,
    EDITOR_ITEM_SOFT_BEAM,
    EDITOR_ITEM_VERTEX,
    EDITOR_ITEM_LINE
} EditorItemKind;

typedef struct EditorItemAddCommand {
    EditorItemKind kind;
    EditorObjectId object;
    uint32_t parent;
    uint32_t first;
    uint32_t second;
    uint32_t index;
    uint32_t option;
    Position position;
    char name[EDITOR_OBJECT_NAME_MAX];
} EditorItemAddCommand;

typedef struct EditorItemRemoveCommand {
    EditorItemKind kind;
    EditorObjectId object;
    uint32_t parent;
    uint32_t item;
    uint32_t index;
} EditorItemRemoveCommand;

typedef struct EditorItemRenameCommand {
    EditorItemKind kind;
    EditorObjectId object;
    uint32_t parent;
    uint32_t item;
    uint32_t index;
    char name[EDITOR_OBJECT_NAME_MAX];
} EditorItemRenameCommand;

typedef enum EditorPropertyKind {
    EDITOR_PROPERTY_MASS,
    EDITOR_PROPERTY_FRICTION,
    EDITOR_PROPERTY_RESTITUTION,
    EDITOR_PROPERTY_GRAVITY,
    EDITOR_PROPERTY_STATIC,
    EDITOR_PROPERTY_ROTATION_LOCKED,
    EDITOR_PROPERTY_COLLISION,
    EDITOR_PROPERTY_PARTICLE,
    EDITOR_PROPERTY_POSITION_LOCKED,
    EDITOR_PROPERTY_VISUAL_SIZE,
    EDITOR_PROPERTY_JOINT_KIND,
    EDITOR_PROPERTY_REST_LENGTH,
    EDITOR_PROPERTY_STIFFNESS,
    EDITOR_PROPERTY_DAMPING,
    EDITOR_PROPERTY_POSITION_FOLLOWS_BODY,
    EDITOR_PROPERTY_ROTATION_FOLLOWS_BODY,
    EDITOR_PROPERTY_LINE_LENGTH
} EditorPropertyKind;

typedef enum EditorPropertyValueKind {
    EDITOR_PROPERTY_VALUE_FLOAT,
    EDITOR_PROPERTY_VALUE_BOOL,
    EDITOR_PROPERTY_VALUE_UINT
} EditorPropertyValueKind;

typedef struct EditorPropertySetCommand {
    EditorItemKind kind;
    EditorObjectId object;
    uint32_t parent;
    uint32_t item;
    uint32_t index;
    EditorPropertyKind property;
    EditorPropertyValueKind value_kind;
    union {
        float number;
        bool boolean;
        uint32_t integer;
    } value;
} EditorPropertySetCommand;

typedef enum EditorRelationshipKind {
    EDITOR_RELATIONSHIP_JOINT_ANCHOR,
    EDITOR_RELATIONSHIP_ANCHOR_RIGID_BODY,
    EDITOR_RELATIONSHIP_SOFT_BEAM_NODE
} EditorRelationshipKind;

typedef struct EditorRelationshipSetCommand {
    EditorRelationshipKind kind;
    EditorObjectId object;
    uint32_t parent;
    uint32_t item;
    uint32_t endpoint;
    uint32_t target;
} EditorRelationshipSetCommand;

typedef enum EditorCollisionFilterKind {
    EDITOR_COLLISION_FILTER_CATEGORY,
    EDITOR_COLLISION_FILTER_COLLIDE_WITH
} EditorCollisionFilterKind;

typedef struct EditorCollisionFilterSetCommand {
    EditorItemKind kind;
    EditorObjectId object;
    uint32_t parent;
    uint32_t item;
    EditorCollisionFilterKind filter;
    char mask[EDITOR_OBJECT_NAME_MAX];
    bool enabled;
} EditorCollisionFilterSetCommand;

typedef enum EditorVisibilityKind {
    EDITOR_VISIBILITY_OBJECT,
    EDITOR_VISIBILITY_RIGID_BODY,
    EDITOR_VISIBILITY_HITBOX,
    EDITOR_VISIBILITY_JOINT,
    EDITOR_VISIBILITY_ANCHOR,
    EDITOR_VISIBILITY_SOFT_BODY,
    EDITOR_VISIBILITY_SOFT_NODE,
    EDITOR_VISIBILITY_SOFT_BEAM
} EditorVisibilityKind;

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
        struct {
            Vec2D offset;
            float zoom;
        } viewport_camera;
        struct {
            bool local;
        } viewport_coordinates;
        struct {
            EditorVisibilityKind kind;
            EditorObjectId object;
            uint32_t parent;
            uint32_t item;
            bool visible;
        } visibility;
        EditorNavigationState navigation;
        EditorItemAddCommand item_add;
        EditorItemRemoveCommand item_remove;
        EditorItemRenameCommand item_rename;
        EditorPropertySetCommand property_set;
        EditorRelationshipSetCommand relationship_set;
        struct {
            char name[EDITOR_OBJECT_NAME_MAX];
        } collision_mask_add;
        EditorCollisionFilterSetCommand collision_filter_set;
    } data;
} EditorCommand;

typedef struct EditorCommandResult {
    ErrorResultKind kind;
    union {
        EditorObjectId object;
        EditorError error;
    } result;
    struct {
        bool valid;
        EditorItemKind kind;
        EditorObjectId object;
        uint32_t parent;
        uint32_t container;
        uint32_t item;
        char name[EDITOR_OBJECT_NAME_MAX];
    } created;
} EditorCommandResult;

typedef void (*EditorCommandExecuted)(const EditorCommand *command,
    const EditorCommandResult *result, void *context);

EditorCommandResult editor_command_execute(EditorProject *project,
    const EditorCommand *command);
void editor_command_executed_callback_set(EditorCommandExecuted callback,
    void *context);
EditorResult editor_command_cli_parse(int count, char **arguments,
    const char **document_path, EditorCommand *command);
EditorResult editor_command_cli_write(const EditorCommand *command,
    const char *document_path, char *output, size_t output_capacity);
EditorResult editor_command_cli_named_parse(const EditorProject *project,
    int count, char **arguments, const char **document_path,
    EditorCommand *command);
EditorResult editor_command_cli_named_write(const EditorProject *project,
    const EditorCommand *command, const char *document_path,
    char *output, size_t output_capacity);

#endif
