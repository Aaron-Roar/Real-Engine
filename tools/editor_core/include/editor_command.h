#ifndef ROHR_EDITOR_COMMAND_H
#define ROHR_EDITOR_COMMAND_H

#include "editor_auto_shape.h"
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
    EDITOR_COMMAND_AUTO_SHAPE,
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
    EDITOR_COMMAND_COLLISION_FILTER_SET,
    EDITOR_COMMAND_SPRITE_ADD,
    EDITOR_COMMAND_SPRITE_REMOVE,
    EDITOR_COMMAND_SPRITE_RENAME,
    EDITOR_COMMAND_SPRITE_PATH_SET,
    EDITOR_COMMAND_SPRITE_POSITION_SET,
    EDITOR_COMMAND_SPRITE_SIZE_SET,
    EDITOR_COMMAND_SPRITE_BODY_SET,
    EDITOR_COMMAND_SPRITE_FOLLOW_ROTATION_SET,
    EDITOR_COMMAND_SPRITE_VISIBILITY_SET,
    EDITOR_COMMAND_ANIMATED_SPRITE_ADD,
    EDITOR_COMMAND_ANIMATED_SPRITE_REMOVE,
    EDITOR_COMMAND_ANIMATED_SPRITE_RENAME,
    EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET,
    EDITOR_COMMAND_ANIMATED_SPRITE_POSITION_SET,
    EDITOR_COMMAND_ANIMATED_SPRITE_SCALE_SET,
    EDITOR_COMMAND_ANIMATED_SPRITE_TIMING_SET,
    EDITOR_COMMAND_ANIMATED_SPRITE_STARTING_FRAME_SET,
    EDITOR_COMMAND_ANIMATED_SPRITE_DIRECTION_SET,
    EDITOR_COMMAND_ANIMATED_SPRITE_FOLLOW_ROTATION_SET,
    EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET,
    EDITOR_COMMAND_ANIMATED_SPRITE_PLAYING_SET,
    EDITOR_COMMAND_ANIMATION_FRAME_ADD,
    EDITOR_COMMAND_ANIMATION_FRAME_REMOVE,
    EDITOR_COMMAND_ANIMATION_FRAME_RENAME,
    EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET,
    EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET
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
    EDITOR_ITEM_LINE,
    EDITOR_ITEM_SOFT_AREA
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
    EDITOR_PROPERTY_PARTICLE_RADIUS,
    EDITOR_PROPERTY_PARTICLE_AUTO_FIT,
    EDITOR_PROPERTY_NODE_RADIUS,
    EDITOR_PROPERTY_POSITION_LOCKED,
    EDITOR_PROPERTY_VISUAL_SIZE,
    EDITOR_PROPERTY_JOINT_KIND,
    EDITOR_PROPERTY_REST_LENGTH,
    EDITOR_PROPERTY_STIFFNESS,
    EDITOR_PROPERTY_DAMPING,
    EDITOR_PROPERTY_POSITION_FOLLOWS_BODY,
    EDITOR_PROPERTY_ROTATION_FOLLOWS_BODY,
    EDITOR_PROPERTY_LINE_LENGTH,
    EDITOR_PROPERTY_OUTLINE_COLOR,
    EDITOR_PROPERTY_SURFACE_COLOR,
    EDITOR_PROPERTY_PARTICLE_RING_COLOR,
    EDITOR_PROPERTY_PARTICLE_FILL_COLOR,
    EDITOR_PROPERTY_NODE_COLOR,
    EDITOR_PROPERTY_BEAM_COLOR,
    EDITOR_PROPERTY_AREA_COLOR,
    EDITOR_PROPERTY_COLOR
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
    EDITOR_VISIBILITY_SOFT_BEAM,
    EDITOR_VISIBILITY_SOFT_AREA
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
            EditorItemKind kind;
            EditorObjectId object;
            uint32_t parent;
            uint32_t item;
            EditorAutoShapeConfig config;
            uint32_t points[EDITOR_SOFT_NODE_MAX];
            size_t point_count;
        } auto_shape;
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
        struct {
            EditorObjectId object;
            char name[EDITOR_OBJECT_NAME_MAX];
            char path[EDITOR_ASSET_PATH_MAX];
            Scale size;
        } sprite_add;
        struct { EditorObjectId object; EditorSpriteId sprite; } sprite_remove;
        struct { EditorObjectId object; EditorSpriteId sprite;
            char name[EDITOR_OBJECT_NAME_MAX]; } sprite_rename;
        struct { EditorObjectId object; EditorSpriteId sprite;
            char path[EDITOR_ASSET_PATH_MAX]; } sprite_path_set;
        struct { EditorObjectId object; EditorSpriteId sprite; Position position; }
            sprite_position_set;
        struct { EditorObjectId object; EditorSpriteId sprite; Scale size; }
            sprite_size_set;
        struct { EditorObjectId object; EditorSpriteId sprite;
            EditorRigidBodyId body; } sprite_body_set;
        struct { EditorObjectId object; EditorSpriteId sprite; bool visible; }
            sprite_visibility_set;
        struct { EditorObjectId object; EditorSpriteId sprite; bool enabled; }
            sprite_boolean_set;
        struct { EditorObjectId object; char name[EDITOR_OBJECT_NAME_MAX]; }
            animated_sprite_add;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite; }
            animated_sprite_remove;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite;
            char name[EDITOR_OBJECT_NAME_MAX]; } animated_sprite_rename;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite;
            EditorRigidBodyId body; } animated_sprite_body_set;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite;
            Position position; } animated_sprite_position_set;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite; Scale scale; }
            animated_sprite_scale_set;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite;
            Tick ticks; Time time; } animated_sprite_timing_set;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite; uint32_t frame; }
            animated_sprite_starting_frame_set;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite;
            Direction direction; } animated_sprite_direction_set;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite; bool enabled; }
            animated_sprite_boolean_set;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite;
            char name[EDITOR_OBJECT_NAME_MAX]; char path[EDITOR_ASSET_PATH_MAX];
            Scale size; } animation_frame_add;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite; size_t index; }
            animation_frame_remove;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite; size_t index;
            char name[EDITOR_OBJECT_NAME_MAX]; } animation_frame_rename;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite; size_t index;
            char path[EDITOR_ASSET_PATH_MAX]; } animation_frame_path_set;
        struct { EditorObjectId object; EditorAnimatedSpriteId sprite; size_t index;
            Scale size; } animation_frame_size_set;
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
typedef void (*EditorCommandExecuting)(const EditorProject *project,
    const EditorCommand *command, void *context);
typedef void (*EditorCommandFinished)(const EditorCommand *command,
    const EditorCommandResult *result, void *context);

EditorCommandResult editor_command_execute(EditorProject *project,
    const EditorCommand *command);
void editor_command_executed_callback_set(EditorCommandExecuted callback,
    void *context);
void editor_command_executing_callback_set(EditorCommandExecuting callback,
    void *context);
void editor_command_finished_callback_set(EditorCommandFinished callback,
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
EditorResult editor_command_cli_standard_parse(const EditorProject *project,
    int count, char **arguments, const char **document_path,
    EditorCommand *command);
EditorResult editor_command_cli_standard_write(const EditorProject *project,
    const EditorCommand *command, const EditorCommandResult *result,
    const char *document_path, char *output, size_t output_capacity);

#endif
