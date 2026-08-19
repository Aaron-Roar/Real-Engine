#include "editor_command.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EditorCommandExecuted editor_command_executed_callback;
static void *editor_command_executed_context;
static EditorCommandExecuting editor_command_executing_callback;
static void *editor_command_executing_context;
static EditorCommandFinished editor_command_finished_callback;
static void *editor_command_finished_context;

static EditorCommandResult editor_command_error(EditorError error) {
    return (EditorCommandResult){.kind = ERROR_RESULT_ERROR,
        .result.error = error};
}

static bool editor_command_float_equal(float first, float second) {
    return fabsf(first - second) <= 0.0001f;
}

static bool editor_command_position_equal(Position first, Position second) {
    return editor_command_float_equal(first.x, second.x) &&
        editor_command_float_equal(first.y, second.y);
}

static Position editor_command_position_rotate(Position position,
        Orientation rotation) {
    float cosine = cosf(rotation);
    float sine = sinf(rotation);
    return (Position){position.x * cosine - position.y * sine,
        position.x * sine + position.y * cosine};
}

static const char *editor_auto_shape_kind_name_get(EditorAutoShapeKind kind) {
    switch(kind) {
        case EDITOR_AUTO_SHAPE_TRIANGLE: return "triangle";
        case EDITOR_AUTO_SHAPE_RECTANGLE: return "square";
        case EDITOR_AUTO_SHAPE_CIRCLE: return "circle";
        default: return NULL;
    }
}

static const char *editor_auto_triangle_kind_name_get(EditorAutoTriangleKind kind) {
    switch(kind) {
        case EDITOR_AUTO_TRIANGLE_EQUILATERAL: return "equilateral";
        case EDITOR_AUTO_TRIANGLE_ISOSCELES: return "isosceles";
        case EDITOR_AUTO_TRIANGLE_SCALENE: return "scalene";
        default: return NULL;
    }
}

static bool editor_auto_shape_kind_parse(const char *value,
        EditorAutoShapeKind *kind) {
    if(strcmp(value, "triangle") == 0) *kind = EDITOR_AUTO_SHAPE_TRIANGLE;
    else if(strcmp(value, "rectangle") == 0 || strcmp(value, "square") == 0)
        *kind = EDITOR_AUTO_SHAPE_RECTANGLE;
    else if(strcmp(value, "circle") == 0) *kind = EDITOR_AUTO_SHAPE_CIRCLE;
    else return false;
    return true;
}

static bool editor_auto_triangle_kind_parse(const char *value,
        EditorAutoTriangleKind *kind) {
    if(strcmp(value, "equilateral") == 0) *kind = EDITOR_AUTO_TRIANGLE_EQUILATERAL;
    else if(strcmp(value, "isosceles") == 0) *kind = EDITOR_AUTO_TRIANGLE_ISOSCELES;
    else if(strcmp(value, "scalene") == 0) *kind = EDITOR_AUTO_TRIANGLE_SCALENE;
    else return false;
    return true;
}

static EditorCommandResult editor_command_result_from(EditorResult result) {
    if(editor_result_check(result)) return editor_command_error(result.result.error);
    return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
}

static EditorCommandResult editor_command_not_found(const char *kind, uint32_t id) {
    EditorResult result = editor_result_error(EDITOR_ERROR_NOT_FOUND,
        "%s %u was not found", kind, id);
    return editor_command_error(result.result.error);
}

static EditorSoftBody *editor_command_soft_body_get(EditorObject *object,
        EditorSoftBodyId body) {
    if(object == NULL) return NULL;
    for(size_t i = 0; i < object->soft_body_count; i += 1)
        if(object->soft_body_items[i].id == body) return &object->soft_body_items[i];
    return NULL;
}

static EditorJoint *editor_command_joint_get(EditorObject *object, EditorJointId joint) {
    if(object == NULL) return NULL;
    for(size_t i = 0; i < object->joint_count; i += 1)
        if(object->joint_items[i].id == joint) return &object->joint_items[i];
    return NULL;
}

static EditorSoftNode *editor_command_soft_node_get(EditorSoftBody *body,
        EditorSoftNodeId node) {
    if(body == NULL) return NULL;
    for(size_t i = 0; i < body->node_count; i += 1)
        if(body->nodes[i].id == node) return &body->nodes[i];
    return NULL;
}

static EditorSoftBeam *editor_command_soft_beam_get(EditorSoftBody *body,
        EditorSoftBeamId beam) {
    if(body == NULL) return NULL;
    for(size_t i = 0; i < body->beam_count; i += 1)
        if(body->beams[i].id == beam) return &body->beams[i];
    return NULL;
}

static EditorAnimatedSprite *editor_command_animated_sprite_get(EditorObject *object,
        EditorAnimatedSpriteId id) {
    return editor_project_animated_sprite_get(object, id);
}

static bool editor_command_collision_mask_find(const EditorProject *project,
        const char *name, size_t *index) {
    char formatted[EDITOR_OBJECT_NAME_MAX];
    if(project == NULL || name == NULL || index == NULL) return false;
    editor_project_property_name_format(formatted, sizeof(formatted), name);
    if(formatted[0] == '\0') return false;
    for(size_t i = 0; i < project->collision_mask_count; i += 1) {
        if(strcmp(project->collision_masks[i].name, formatted) != 0) continue;
        *index = i;
        return true;
    }
    return false;
}

static EditorCommandResult editor_command_execute_internal(EditorProject *project,
        const EditorCommand *command) {
    if(project == NULL || command == NULL)
        return editor_command_error((EditorError){EDITOR_ERROR_INVALID_ARGUMENT,
            "command execution requires a project and command"});
    switch(command->type) {
        case EDITOR_COMMAND_OBJECT_ADD: {
            EditorObjectIdResult result = editor_object_command_add(project,
                &(EditorObjectAddArgs){
                    .name = command->data.object_add.name,
                    .position = command->data.object_add.position
                });
            if(result.kind == ERROR_RESULT_ERROR)
                return editor_command_error(result.result.error);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE,
                .result.object = result.result.value};
        }
        case EDITOR_COMMAND_OBJECT_RENAME:
            return editor_command_result_from(editor_object_command_rename(project,
                command->data.object_rename.object,
                command->data.object_rename.name));
        case EDITOR_COMMAND_OBJECT_REMOVE:
            return editor_command_result_from(editor_object_command_remove(project,
                command->data.object_remove.object));
        case EDITOR_COMMAND_OBJECT_POSITION: {
            EditorObject *object = editor_object_query_get(project,
                command->data.object_position.object);
            if(object == NULL) return editor_command_not_found("object",
                command->data.object_position.object);
            if(editor_command_position_equal(object->position,
                    command->data.object_position.position))
                return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
            object->position = command->data.object_position.position;
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_RIGID_BODY_TRANSFORM: {
            EditorObject *object = editor_object_query_get(project,
                command->data.rigid_body_transform.object);
            EditorRigidBody *body = editor_project_rigid_body_get(object,
                command->data.rigid_body_transform.body);
            if(object == NULL) return editor_command_not_found("object",
                command->data.rigid_body_transform.object);
            if(body == NULL) return editor_command_not_found("rigid body",
                command->data.rigid_body_transform.body);
            if(editor_command_position_equal(body->position,
                        command->data.rigid_body_transform.position) &&
                    editor_command_float_equal(body->rotation,
                        command->data.rigid_body_transform.rotation))
                return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
            body->position = command->data.rigid_body_transform.position;
            body->rotation = command->data.rigid_body_transform.rotation;
            editor_project_rigid_body_constraints_apply(object, body->id);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_VERTEX_POSITION: {
            EditorObject *object = editor_object_query_get(project,
                command->data.vertex_position.object);
            EditorRigidBody *body = editor_project_rigid_body_get(object,
                command->data.vertex_position.body);
            EditorHitbox *hitbox = editor_project_hitbox_get(body,
                command->data.vertex_position.hitbox);
            if(object == NULL) return editor_command_not_found("object",
                command->data.vertex_position.object);
            if(body == NULL) return editor_command_not_found("rigid body",
                command->data.vertex_position.body);
            if(hitbox == NULL) return editor_command_not_found("hitbox",
                command->data.vertex_position.hitbox);
            for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
                if(hitbox->vertices[i].id != command->data.vertex_position.vertex) continue;
                if(hitbox->vertices[i].position_locked)
                    return editor_command_error(editor_result_error(
                        EDITOR_ERROR_INVALID_ARGUMENT, "vertex %u is locked",
                        command->data.vertex_position.vertex).result.error);
                if(editor_command_position_equal(hitbox->vertices[i].position,
                        command->data.vertex_position.position))
                    return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
                hitbox->vertices[i].position = command->data.vertex_position.position;
                return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
            }
            return editor_command_not_found("vertex",
                command->data.vertex_position.vertex);
        }
        case EDITOR_COMMAND_ANCHOR_TRANSFORM: {
            EditorObject *object = editor_object_query_get(project,
                command->data.anchor_transform.object);
            EditorAnchor *anchor = editor_project_anchor_get(object,
                command->data.anchor_transform.anchor);
            if(object == NULL) return editor_command_not_found("object",
                command->data.anchor_transform.object);
            if(anchor == NULL) return editor_command_not_found("anchor",
                command->data.anchor_transform.anchor);
            if(editor_command_position_equal(anchor->position,
                        command->data.anchor_transform.position) &&
                    editor_command_float_equal(anchor->rotation,
                        command->data.anchor_transform.rotation))
                return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
            anchor->position = command->data.anchor_transform.position;
            anchor->rotation = command->data.anchor_transform.rotation;
            editor_project_anchor_constraints_apply(object, anchor->id);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_SOFT_BODY_TRANSFORM: {
            EditorObject *object = editor_object_query_get(project,
                command->data.soft_body_transform.object);
            EditorSoftBody *body = editor_command_soft_body_get(object,
                command->data.soft_body_transform.body);
            if(object == NULL) return editor_command_not_found("object",
                command->data.soft_body_transform.object);
            if(body == NULL) return editor_command_not_found("soft body",
                command->data.soft_body_transform.body);
            if(editor_command_position_equal(body->position,
                        command->data.soft_body_transform.position) &&
                    editor_command_float_equal(body->rotation,
                        command->data.soft_body_transform.rotation))
                return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
            body->position = command->data.soft_body_transform.position;
            body->rotation = command->data.soft_body_transform.rotation;
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_SOFT_NODE_POSITION: {
            EditorObject *object = editor_object_query_get(project,
                command->data.soft_node_position.object);
            EditorSoftBody *body = editor_command_soft_body_get(object,
                command->data.soft_node_position.body);
            if(object == NULL) return editor_command_not_found("object",
                command->data.soft_node_position.object);
            if(body == NULL) return editor_command_not_found("soft body",
                command->data.soft_node_position.body);
            for(size_t i = 0; i < body->node_count; i += 1) {
                if(body->nodes[i].id != command->data.soft_node_position.node) continue;
                if(editor_command_position_equal(body->nodes[i].position,
                        command->data.soft_node_position.position))
                    return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
                body->nodes[i].position = command->data.soft_node_position.position;
                return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
            }
            return editor_command_not_found("soft node",
                command->data.soft_node_position.node);
        }
        case EDITOR_COMMAND_AUTO_SHAPE: {
            EditorObject *object = editor_object_query_get(project,
                command->data.auto_shape.object);
            if(object == NULL) return editor_command_not_found("object",
                command->data.auto_shape.object);
            if(command->data.auto_shape.kind == EDITOR_ITEM_HITBOX) {
                EditorRigidBody *body = editor_project_rigid_body_get(object,
                    command->data.auto_shape.parent);
                EditorHitbox *hitbox;
                if(body == NULL) return editor_command_not_found("rigid body",
                    command->data.auto_shape.parent);
                hitbox = editor_project_hitbox_get(body,
                    command->data.auto_shape.item);
                if(hitbox == NULL) return editor_command_not_found("hitbox",
                    command->data.auto_shape.item);
                return editor_command_result_from(
                    command->data.auto_shape.point_count == 0 ?
                    editor_auto_shape_hitbox_apply(hitbox,
                        &command->data.auto_shape.config) :
                    editor_auto_shape_hitbox_points_apply(hitbox,
                        &command->data.auto_shape.config,
                        command->data.auto_shape.points,
                        command->data.auto_shape.point_count));
            }
            if(command->data.auto_shape.kind == EDITOR_ITEM_SOFT_BODY) {
                EditorSoftBody *body = editor_command_soft_body_get(object,
                    command->data.auto_shape.item);
                if(body == NULL) return editor_command_not_found("soft body",
                    command->data.auto_shape.item);
                return editor_command_result_from(
                    command->data.auto_shape.point_count == 0 ?
                    editor_auto_shape_soft_body_apply(body,
                        &command->data.auto_shape.config) :
                    editor_auto_shape_soft_body_points_apply(body,
                        &command->data.auto_shape.config,
                        command->data.auto_shape.points,
                        command->data.auto_shape.point_count));
            }
            return editor_command_error(editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT,
                "auto shape target must be a hitbox or soft body").result.error);
        }
        case EDITOR_COMMAND_RIGID_BODY_ORIGIN: {
            EditorObject *object = editor_object_query_get(project,
                command->data.origin.object);
            EditorRigidBody *body = editor_project_rigid_body_get(object,
                command->data.origin.body);
            if(object == NULL) return editor_command_not_found("object",
                command->data.origin.object);
            if(body == NULL) return editor_command_not_found("rigid body",
                command->data.origin.body);
            if(editor_command_position_equal(body->position,
                    command->data.origin.position))
                return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
            if(!editor_project_rigid_body_origin_set(object, body,
                    command->data.origin.position))
                return editor_command_error(editor_result_error(
                    EDITOR_ERROR_INVALID_ARGUMENT,
                    "rigid body origin could not be changed").result.error);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_SOFT_BODY_ORIGIN: {
            EditorObject *object = editor_object_query_get(project,
                command->data.origin.object);
            EditorSoftBody *body = editor_command_soft_body_get(object,
                command->data.origin.body);
            if(object == NULL) return editor_command_not_found("object",
                command->data.origin.object);
            if(body == NULL) return editor_command_not_found("soft body",
                command->data.origin.body);
            if(editor_command_position_equal(body->position,
                    command->data.origin.position))
                return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
            if(!editor_project_soft_body_origin_set(body,
                    command->data.origin.position))
                return editor_command_error(editor_result_error(
                    EDITOR_ERROR_INVALID_ARGUMENT,
                    "soft body origin could not be changed").result.error);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_VIEWPORT_CAMERA:
            if(command->data.viewport_camera.zoom < 0.1f ||
                    command->data.viewport_camera.zoom > 8.0f)
                return editor_command_error(editor_result_error(
                    EDITOR_ERROR_INVALID_ARGUMENT,
                    "viewport zoom must be between 0.1 and 8").result.error);
            project->viewport_camera_offset = command->data.viewport_camera.offset;
            project->viewport_camera_zoom = command->data.viewport_camera.zoom;
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        case EDITOR_COMMAND_VIEWPORT_COORDINATES:
            project->viewport_local_view = command->data.viewport_coordinates.local;
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        case EDITOR_COMMAND_VISIBILITY: {
            EditorObject *object = editor_object_query_get(project,
                command->data.visibility.object);
            bool *visible = NULL;
            if(object == NULL) return editor_command_not_found("object",
                command->data.visibility.object);
            switch(command->data.visibility.kind) {
                case EDITOR_VISIBILITY_OBJECT:
                    visible = &object->visible;
                    break;
                case EDITOR_VISIBILITY_RIGID_BODY: {
                    EditorRigidBody *body = editor_project_rigid_body_get(object,
                        command->data.visibility.item);
                    if(body != NULL) visible = &body->visible;
                    break;
                }
                case EDITOR_VISIBILITY_HITBOX: {
                    EditorRigidBody *body = editor_project_rigid_body_get(object,
                        command->data.visibility.parent);
                    EditorHitbox *hitbox = editor_project_hitbox_get(body,
                        command->data.visibility.item);
                    if(hitbox != NULL) visible = &hitbox->visible;
                    break;
                }
                case EDITOR_VISIBILITY_JOINT:
                    for(size_t i = 0; i < object->joint_count; i += 1)
                        if(object->joint_items[i].id == command->data.visibility.item)
                            visible = &object->joint_items[i].visible;
                    break;
                case EDITOR_VISIBILITY_ANCHOR: {
                    EditorAnchor *anchor = editor_project_anchor_get(object,
                        command->data.visibility.item);
                    if(anchor != NULL) visible = &anchor->visible;
                    break;
                }
                case EDITOR_VISIBILITY_SOFT_BODY: {
                    EditorSoftBody *body = editor_command_soft_body_get(object,
                        command->data.visibility.item);
                    if(body != NULL) visible = &body->visible;
                    break;
                }
                case EDITOR_VISIBILITY_SOFT_NODE: {
                    EditorSoftBody *body = editor_command_soft_body_get(object,
                        command->data.visibility.parent);
                    if(body != NULL) for(size_t i = 0; i < body->node_count; i += 1)
                        if(body->nodes[i].id == command->data.visibility.item)
                            visible = &body->nodes[i].visible;
                    break;
                }
                case EDITOR_VISIBILITY_SOFT_BEAM: {
                    EditorSoftBody *body = editor_command_soft_body_get(object,
                        command->data.visibility.parent);
                    if(body != NULL) for(size_t i = 0; i < body->beam_count; i += 1)
                        if(body->beams[i].id == command->data.visibility.item)
                            visible = &body->beams[i].visible;
                    break;
                }
                case EDITOR_VISIBILITY_SOFT_AREA: {
                    EditorSoftBody *body = editor_command_soft_body_get(object,
                        command->data.visibility.parent);
                    if(body != NULL) for(size_t i = 0; i < body->area_count; i += 1)
                        if(body->areas[i].id == command->data.visibility.item)
                            visible = &body->areas[i].visible;
                    break;
                }
            }
            if(visible == NULL) return editor_command_not_found("visibility target",
                command->data.visibility.item);
            *visible = command->data.visibility.visible;
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_NAVIGATION_SET:
            if(command->data.navigation.mode > EDITOR_NAVIGATION_MODE_MAX ||
                    command->data.navigation.selection >
                        EDITOR_NAVIGATION_SELECTION_MAX ||
                    command->data.navigation.origin_kind > 2)
                return editor_command_error(editor_result_error(
                    EDITOR_ERROR_INVALID_ARGUMENT,
                    "navigation state contains an invalid mode or selection").result.error);
            if(command->data.navigation.object != 0 &&
                    editor_object_query_get(project,
                        command->data.navigation.object) == NULL)
                return editor_command_not_found("object",
                    command->data.navigation.object);
            project->selected = command->data.navigation.object;
            project->navigation = command->data.navigation;
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        case EDITOR_COMMAND_ITEM_ADD: {
            const EditorItemKind kind = command->data.item_add.kind;
            EditorObject *object;
            uint32_t created = 0;
            const char *created_name = NULL;
            if(kind == EDITOR_ITEM_OBJECT) {
                EditorObjectIdResult result = editor_object_command_add(project,
                    &(EditorObjectAddArgs){command->data.item_add.name,
                        command->data.item_add.position});
                EditorCommandResult command_result;
                EditorObject *created_object;
                if(result.kind == ERROR_RESULT_ERROR)
                    return editor_command_error(result.result.error);
                command_result = (EditorCommandResult){.kind = ERROR_RESULT_VALUE,
                    .result.object = result.result.value,
                    .created = {.valid = true, .kind = EDITOR_ITEM_OBJECT,
                        .object = result.result.value, .item = result.result.value}};
                created_object = editor_object_query_get(project, result.result.value);
                if(created_object != NULL) snprintf(command_result.created.name,
                    sizeof(command_result.created.name), "%s", created_object->name);
                return command_result;
            }
            object = editor_object_query_get(project, command->data.item_add.object);
            if(object == NULL) return editor_command_not_found("object",
                command->data.item_add.object);
            if(kind == EDITOR_ITEM_RIGID_BODY) {
                EditorRigidBody *value = editor_project_rigid_body_add(project, object);
                if(value != NULL) { created = value->id; created_name = value->name; }
            } else if(kind == EDITOR_ITEM_HITBOX) {
                EditorRigidBody *body = editor_project_rigid_body_get(object,
                    command->data.item_add.parent);
                EditorHitbox *value = editor_project_hitbox_add(project, body);
                if(value != NULL) { created = value->id; created_name = value->name; }
            } else if(kind == EDITOR_ITEM_JOINT && command->data.item_add.option <=
                    (uint32_t)EDITOR_JOINT_SPRING) {
                EditorJoint *value = editor_project_joint_add(project, object,
                    (EditorJointKind)command->data.item_add.option);
                if(value != NULL) { created = value->id; created_name = value->name; }
            } else if(kind == EDITOR_ITEM_ANCHOR) {
                EditorAnchor *value = editor_project_anchor_add(project, object,
                    command->data.item_add.position, command->data.item_add.parent);
                if(value != NULL) { created = value->id; created_name = value->name; }
            } else if(kind == EDITOR_ITEM_SOFT_BODY) {
                EditorSoftBody *value = editor_project_soft_body_add(project, object);
                if(value != NULL) { created = value->id; created_name = value->name; }
            } else if(kind == EDITOR_ITEM_SOFT_NODE) {
                EditorSoftBody *body = editor_command_soft_body_get(object,
                    command->data.item_add.parent);
                EditorSoftNode *value = editor_project_soft_node_add(project, body,
                    command->data.item_add.position);
                if(value != NULL) { created = value->id; created_name = value->name; }
            } else if(kind == EDITOR_ITEM_SOFT_BEAM) {
                EditorSoftBody *body = editor_command_soft_body_get(object,
                    command->data.item_add.parent);
                EditorSoftBeam *value = editor_project_soft_beam_add(project, body,
                    command->data.item_add.first, command->data.item_add.second);
                if(value != NULL) { created = value->id; created_name = value->name; }
            } else if(kind == EDITOR_ITEM_VERTEX) {
                EditorRigidBody *body = editor_project_rigid_body_get(object,
                    command->data.item_add.parent);
                EditorHitbox *hitbox = editor_project_hitbox_get(body,
                    command->data.item_add.first);
                if(editor_project_hitbox_vertex_insert(project, hitbox,
                        command->data.item_add.index)) {
                    created = project->next_vertex_id - 1;
                    if(hitbox != NULL) for(uint32_t i = 0; i < hitbox->vertex_count; i += 1)
                        if(hitbox->vertices[i].id == created)
                            created_name = hitbox->vertices[i].name;
                }
            }
            if(created == 0) return editor_command_error(editor_result_error(
                EDITOR_ERROR_CAPACITY, "could not add editor item").result.error);
            if(command->data.item_add.name[0] != '\0') {
                EditorCommand rename = {.type = EDITOR_COMMAND_ITEM_RENAME,
                    .data.item_rename = {.kind = kind,
                        .object = command->data.item_add.object,
                        .parent = command->data.item_add.parent,
                        .item = created,
                        .index = kind == EDITOR_ITEM_VERTEX ? created :
                            command->data.item_add.index}};
                EditorCommandResult renamed;
                if(kind == EDITOR_ITEM_VERTEX || kind == EDITOR_ITEM_LINE)
                    rename.data.item_rename.item = command->data.item_add.first;
                snprintf(rename.data.item_rename.name,
                    sizeof(rename.data.item_rename.name), "%s",
                    command->data.item_add.name);
                renamed = editor_command_execute_internal(project, &rename);
                if(renamed.kind == ERROR_RESULT_ERROR) return renamed;
            }
            {
                EditorCommandResult result = {.kind = ERROR_RESULT_VALUE,
                    .result.object = created,
                    .created = {.valid = true, .kind = kind,
                        .object = command->data.item_add.object,
                        .parent = command->data.item_add.parent,
                        .container = command->data.item_add.first,
                        .item = created}};
                if(created_name != NULL) snprintf(result.created.name,
                    sizeof(result.created.name), "%s", created_name);
                return result;
            }
        }
        case EDITOR_COMMAND_ITEM_REMOVE: {
            const EditorItemKind kind = command->data.item_remove.kind;
            EditorObject *object = editor_object_query_get(project,
                command->data.item_remove.object);
            bool removed = false;
            if(kind == EDITOR_ITEM_OBJECT)
                return editor_command_result_from(editor_object_command_remove(project,
                    command->data.item_remove.object));
            if(object == NULL) return editor_command_not_found("object",
                command->data.item_remove.object);
            if(kind == EDITOR_ITEM_RIGID_BODY)
                removed = editor_project_rigid_body_remove(object,
                    command->data.item_remove.item);
            else if(kind == EDITOR_ITEM_HITBOX) {
                EditorRigidBody *body = editor_project_rigid_body_get(object,
                    command->data.item_remove.parent);
                removed = editor_project_hitbox_remove(body,
                    command->data.item_remove.item);
            } else if(kind == EDITOR_ITEM_JOINT)
                removed = editor_project_joint_remove(object,
                    command->data.item_remove.item);
            else if(kind == EDITOR_ITEM_ANCHOR)
                removed = editor_project_anchor_remove(object,
                    command->data.item_remove.item);
            else if(kind == EDITOR_ITEM_SOFT_BODY)
                removed = editor_project_soft_body_remove(object,
                    command->data.item_remove.item);
            else if(kind == EDITOR_ITEM_SOFT_NODE) {
                EditorSoftBody *body = editor_command_soft_body_get(object,
                    command->data.item_remove.parent);
                removed = editor_project_soft_node_remove(project, body,
                    command->data.item_remove.item);
            } else if(kind == EDITOR_ITEM_SOFT_BEAM) {
                EditorSoftBody *body = editor_command_soft_body_get(object,
                    command->data.item_remove.parent);
                removed = editor_project_soft_beam_remove(project, body,
                    command->data.item_remove.item);
            } else if(kind == EDITOR_ITEM_VERTEX || kind == EDITOR_ITEM_LINE) {
                EditorRigidBody *body = editor_project_rigid_body_get(object,
                    command->data.item_remove.parent);
                EditorHitbox *hitbox = editor_project_hitbox_get(body,
                    command->data.item_remove.item);
                uint32_t index = command->data.item_remove.index;
                if(kind == EDITOR_ITEM_VERTEX && hitbox != NULL) {
                    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1)
                        if(hitbox->vertices[i].id == command->data.item_remove.index)
                            index = i;
                    removed = editor_project_hitbox_vertex_remove(hitbox, index);
                } else if(kind == EDITOR_ITEM_LINE) {
                    removed = editor_project_hitbox_line_remove(hitbox, index);
                }
            }
            if(!removed) return editor_command_not_found("editor item",
                command->data.item_remove.item);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_ITEM_RENAME: {
            const EditorItemKind kind = command->data.item_rename.kind;
            EditorObject *object = editor_object_query_get(project,
                command->data.item_rename.object);
            char *name = NULL;
            char formatted[EDITOR_OBJECT_NAME_MAX];
            if(kind == EDITOR_ITEM_OBJECT)
                return editor_command_result_from(editor_object_command_rename(project,
                    command->data.item_rename.object,
                    command->data.item_rename.name));
            if(object == NULL) return editor_command_not_found("object",
                command->data.item_rename.object);
            if(kind == EDITOR_ITEM_RIGID_BODY) {
                EditorRigidBody *value = editor_project_rigid_body_get(object,
                    command->data.item_rename.item);
                if(value != NULL) name = value->name;
            } else if(kind == EDITOR_ITEM_HITBOX) {
                EditorRigidBody *body = editor_project_rigid_body_get(object,
                    command->data.item_rename.parent);
                EditorHitbox *value = editor_project_hitbox_get(body,
                    command->data.item_rename.item);
                if(value != NULL) name = value->name;
            } else if(kind == EDITOR_ITEM_JOINT) {
                EditorJoint *value = editor_command_joint_get(object,
                    command->data.item_rename.item);
                if(value != NULL) name = value->name;
            } else if(kind == EDITOR_ITEM_ANCHOR) {
                EditorAnchor *value = editor_project_anchor_get(object,
                    command->data.item_rename.item);
                if(value != NULL) name = value->name;
            } else if(kind == EDITOR_ITEM_SOFT_BODY) {
                EditorSoftBody *value = editor_command_soft_body_get(object,
                    command->data.item_rename.item);
                if(value != NULL) name = value->name;
            } else if(kind == EDITOR_ITEM_SOFT_NODE) {
                EditorSoftBody *body = editor_command_soft_body_get(object,
                    command->data.item_rename.parent);
                EditorSoftNode *value = editor_command_soft_node_get(body,
                    command->data.item_rename.item);
                if(value != NULL) name = value->name;
            } else if(kind == EDITOR_ITEM_SOFT_BEAM) {
                EditorSoftBody *body = editor_command_soft_body_get(object,
                    command->data.item_rename.parent);
                EditorSoftBeam *value = editor_command_soft_beam_get(body,
                    command->data.item_rename.item);
                if(value != NULL) name = value->name;
            } else if(kind == EDITOR_ITEM_VERTEX || kind == EDITOR_ITEM_LINE) {
                EditorRigidBody *body = editor_project_rigid_body_get(object,
                    command->data.item_rename.parent);
                EditorHitbox *hitbox = editor_project_hitbox_get(body,
                    command->data.item_rename.item);
                if(hitbox != NULL && kind == EDITOR_ITEM_LINE &&
                        command->data.item_rename.index < hitbox->vertex_count)
                    name = hitbox->line_names[command->data.item_rename.index];
                if(hitbox != NULL && kind == EDITOR_ITEM_VERTEX)
                    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1)
                        if(hitbox->vertices[i].id == command->data.item_rename.index)
                            name = hitbox->vertices[i].name;
            }
            if(name == NULL) return editor_command_not_found("editor item",
                command->data.item_rename.item);
            editor_project_property_name_format(formatted, sizeof(formatted),
                command->data.item_rename.name);
            if(formatted[0] == '\0') return editor_command_error(editor_result_error(
                EDITOR_ERROR_NAME_INVALID,
                "item name does not contain a valid identifier").result.error);
            snprintf(name, EDITOR_OBJECT_NAME_MAX, "%s", formatted);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_PROPERTY_SET: {
            const EditorPropertySetCommand *set = &command->data.property_set;
            EditorObject *object = editor_object_query_get(project, set->object);
            if(object == NULL) return editor_command_not_found("object", set->object);
            if(set->kind == EDITOR_ITEM_RIGID_BODY) {
                EditorRigidBody *body = editor_project_rigid_body_get(object, set->item);
                if(body == NULL) return editor_command_not_found("rigid body", set->item);
                if(set->property == EDITOR_PROPERTY_MASS &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number >= 0.0f) body->mass_value = set->value.number;
                else if(set->property == EDITOR_PROPERTY_FRICTION &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number >= 0.0f) body->friction = set->value.number;
                else if(set->property == EDITOR_PROPERTY_RESTITUTION &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number >= 0.0f && set->value.number <= 1.0f)
                    body->restitution = set->value.number;
                else if(set->property == EDITOR_PROPERTY_GRAVITY &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_BOOL)
                    body->gravity_enabled = set->value.boolean;
                else if(set->property == EDITOR_PROPERTY_STATIC &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_BOOL)
                    body->static_body = set->value.boolean;
                else if(set->property == EDITOR_PROPERTY_ROTATION_LOCKED &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_BOOL)
                    body->rotation_locked = set->value.boolean;
                else if(set->property == EDITOR_PROPERTY_COLLISION &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_BOOL) {
                    body->collision_enabled = set->value.boolean;
                    if(!body->collision_enabled) body->particle = false;
                } else if(set->property == EDITOR_PROPERTY_PARTICLE &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_BOOL &&
                        (!set->value.boolean || body->collision_enabled))
                    body->particle = set->value.boolean;
                else if(set->property == EDITOR_PROPERTY_PARTICLE_RADIUS &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number > 0.0f)
                    body->particle_radius = set->value.number;
                else if(set->property == EDITOR_PROPERTY_PARTICLE_AUTO_FIT &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_BOOL)
                    body->particle_auto_fit = set->value.boolean;
                else if(set->property == EDITOR_PROPERTY_OUTLINE_COLOR &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_UINT)
                    body->border_color = set->value.integer;
                else if(set->property == EDITOR_PROPERTY_SURFACE_COLOR &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_UINT)
                    body->surface_color = set->value.integer;
                else if(set->property == EDITOR_PROPERTY_PARTICLE_RING_COLOR &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_UINT)
                    body->particle_ring_color = set->value.integer;
                else if(set->property == EDITOR_PROPERTY_PARTICLE_FILL_COLOR &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_UINT)
                    body->particle_fill_color = set->value.integer;
                else goto property_invalid;
            } else if(set->kind == EDITOR_ITEM_SOFT_BODY) {
                EditorSoftBody *body = editor_command_soft_body_get(object, set->item);
                if(body == NULL) return editor_command_not_found("soft body", set->item);
                if(set->value_kind != EDITOR_PROPERTY_VALUE_UINT) goto property_invalid;
                if(set->property == EDITOR_PROPERTY_NODE_COLOR)
                    body->node_color = set->value.integer;
                else if(set->property == EDITOR_PROPERTY_BEAM_COLOR)
                    body->beam_color = set->value.integer;
                else if(set->property == EDITOR_PROPERTY_AREA_COLOR)
                    body->area_color = set->value.integer;
                else goto property_invalid;
            } else if(set->kind == EDITOR_ITEM_VERTEX) {
                EditorRigidBody *body = editor_project_rigid_body_get(object, set->parent);
                EditorHitbox *hitbox = editor_project_hitbox_get(body, set->item);
                EditorVertex *vertex = NULL;
                if(hitbox != NULL) for(uint32_t i = 0; i < hitbox->vertex_count; i += 1)
                    if(hitbox->vertices[i].id == set->index) vertex = &hitbox->vertices[i];
                if(vertex == NULL) return editor_command_not_found("vertex", set->index);
                if(set->property != EDITOR_PROPERTY_POSITION_LOCKED ||
                        set->value_kind != EDITOR_PROPERTY_VALUE_BOOL) goto property_invalid;
                vertex->position_locked = set->value.boolean;
            } else if(set->kind == EDITOR_ITEM_LINE) {
                EditorRigidBody *body = editor_project_rigid_body_get(object, set->parent);
                EditorHitbox *hitbox = editor_project_hitbox_get(body, set->item);
                if(set->property != EDITOR_PROPERTY_LINE_LENGTH ||
                        set->value_kind != EDITOR_PROPERTY_VALUE_FLOAT ||
                        set->value.number < 0.0f ||
                        !editor_project_hitbox_line_length_set(hitbox, set->index,
                            set->value.number)) goto property_invalid;
            } else if(set->kind == EDITOR_ITEM_JOINT) {
                EditorJoint *joint = editor_command_joint_get(object, set->item);
                if(joint == NULL) return editor_command_not_found("joint", set->item);
                if(set->property == EDITOR_PROPERTY_VISUAL_SIZE &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number >= 0.25f && set->value.number <= 3.0f)
                    joint->visual_size = set->value.number;
                else if(set->property == EDITOR_PROPERTY_JOINT_KIND &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_UINT &&
                        set->value.integer <= (uint32_t)EDITOR_JOINT_SPRING) {
                    if(!editor_project_joint_kind_set(object, joint,
                            (EditorJointKind)set->value.integer)) goto property_invalid;
                } else if(set->property == EDITOR_PROPERTY_REST_LENGTH &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number >= 0.0f) joint->rest_length = set->value.number;
                else if(set->property == EDITOR_PROPERTY_STIFFNESS &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number >= 0.0f) joint->stiffness = set->value.number;
                else if(set->property == EDITOR_PROPERTY_DAMPING &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number >= 0.0f) joint->damping = set->value.number;
                else goto property_invalid;
            } else if(set->kind == EDITOR_ITEM_ANCHOR) {
                EditorAnchor *anchor = editor_project_anchor_get(object, set->item);
                bool success = false;
                if(anchor == NULL) return editor_command_not_found("anchor", set->item);
                if(set->property == EDITOR_PROPERTY_POSITION_FOLLOWS_BODY &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_BOOL)
                    success = editor_project_anchor_position_lock_set(object, anchor,
                        set->value.boolean);
                else if(set->property == EDITOR_PROPERTY_ROTATION_FOLLOWS_BODY &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_BOOL)
                    success = editor_project_anchor_rotation_lock_set(object, anchor,
                        set->value.boolean);
                if(!success) goto property_invalid;
                editor_project_anchor_constraints_apply(object, anchor->id);
            } else if(set->kind == EDITOR_ITEM_SOFT_NODE) {
                EditorSoftBody *body = editor_command_soft_body_get(object, set->parent);
                EditorSoftNode *node = editor_command_soft_node_get(body, set->item);
                if(node == NULL) return editor_command_not_found("soft node", set->item);
                if(set->property == EDITOR_PROPERTY_MASS &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number >= 0.0f) node->node_mass = set->value.number;
                else if(set->property == EDITOR_PROPERTY_FRICTION &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number >= 0.0f) node->friction = set->value.number;
                else if(set->property == EDITOR_PROPERTY_RESTITUTION &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number >= 0.0f && set->value.number <= 1.0f)
                    node->restitution = set->value.number;
                else if(set->property == EDITOR_PROPERTY_GRAVITY &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_BOOL)
                    node->gravity_enabled = set->value.boolean;
                else if(set->property == EDITOR_PROPERTY_COLLISION &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_BOOL)
                    node->collision_enabled = set->value.boolean;
                else if(set->property == EDITOR_PROPERTY_NODE_RADIUS &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number > 0.0f)
                    node->radius = set->value.number;
                else if(set->property == EDITOR_PROPERTY_COLOR &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_UINT) {
                    node->color = set->value.integer;
                    node->color_overridden = true;
                }
                else goto property_invalid;
            } else if(set->kind == EDITOR_ITEM_SOFT_BEAM) {
                EditorSoftBody *body = editor_command_soft_body_get(object, set->parent);
                EditorSoftBeam *beam = editor_command_soft_beam_get(body, set->item);
                if(beam == NULL) return editor_command_not_found("soft beam", set->item);
                if(set->property == EDITOR_PROPERTY_STIFFNESS &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number >= 0.0f) beam->stiffness = set->value.number;
                else if(set->property == EDITOR_PROPERTY_DAMPING &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT &&
                        set->value.number >= 0.0f) beam->damping = set->value.number;
                else if(set->property == EDITOR_PROPERTY_COLOR &&
                        set->value_kind == EDITOR_PROPERTY_VALUE_UINT) {
                    beam->color = set->value.integer;
                    beam->color_overridden = true;
                }
                else goto property_invalid;
            } else if(set->kind == EDITOR_ITEM_SOFT_AREA) {
                EditorSoftBody *body = editor_command_soft_body_get(object, set->parent);
                EditorSoftArea *area = NULL;
                if(body != NULL) for(size_t i = 0; i < body->area_count; i += 1)
                    if(body->areas[i].id == set->item) area = &body->areas[i];
                if(area == NULL) return editor_command_not_found("soft area", set->item);
                if(set->property != EDITOR_PROPERTY_COLOR ||
                        set->value_kind != EDITOR_PROPERTY_VALUE_UINT)
                    goto property_invalid;
                area->color = set->value.integer;
                area->color_overridden = true;
            } else goto property_invalid;
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
property_invalid:
            return editor_command_error(editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT,
                "property is invalid for the target or value").result.error);
        }
        case EDITOR_COMMAND_RELATIONSHIP_SET: {
            const EditorRelationshipSetCommand *set =
                &command->data.relationship_set;
            EditorObject *object = editor_object_query_get(project, set->object);
            if(object == NULL) return editor_command_not_found("object", set->object);
            if(set->endpoint > 1) return editor_command_error(editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT,
                "relationship endpoint must be 0 or 1").result.error);
            if(set->kind == EDITOR_RELATIONSHIP_JOINT_ANCHOR) {
                EditorJoint *joint = editor_command_joint_get(object, set->item);
                if(joint == NULL) return editor_command_not_found("joint", set->item);
                if(!editor_project_joint_anchor_set(object, joint, set->endpoint,
                        set->target)) return editor_command_error(editor_result_error(
                    EDITOR_ERROR_INVALID_ARGUMENT,
                    "joint anchor relationship is invalid").result.error);
            } else if(set->kind == EDITOR_RELATIONSHIP_ANCHOR_RIGID_BODY) {
                EditorAnchor *anchor = editor_project_anchor_get(object, set->item);
                if(anchor == NULL) return editor_command_not_found("anchor", set->item);
                if(!editor_project_anchor_rigid_body_set(object, anchor, set->target))
                    return editor_command_error(editor_result_error(
                        EDITOR_ERROR_INVALID_ARGUMENT,
                        "anchor rigid-body relationship is invalid").result.error);
                editor_project_anchor_constraints_apply(object, anchor->id);
            } else if(set->kind == EDITOR_RELATIONSHIP_SOFT_BEAM_NODE) {
                EditorSoftBody *body = editor_command_soft_body_get(object, set->parent);
                EditorSoftBeam *beam = editor_command_soft_beam_get(body, set->item);
                if(beam == NULL) return editor_command_not_found("soft beam", set->item);
                if(set->target != 0 &&
                        editor_command_soft_node_get(body, set->target) == NULL)
                    return editor_command_not_found("soft node", set->target);
                if(set->endpoint == 0) beam->node_a = set->target;
                else beam->node_b = set->target;
            } else {
                return editor_command_error(editor_result_error(
                    EDITOR_ERROR_INVALID_ARGUMENT,
                    "unknown relationship kind").result.error);
            }
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_COLLISION_MASK_ADD: {
            size_t index;
            if(editor_command_collision_mask_find(project,
                    command->data.collision_mask_add.name, &index))
                return (EditorCommandResult){.kind = ERROR_RESULT_VALUE,
                    .result.object = (uint32_t)index};
            if(!editor_project_collision_mask_add(project,
                    command->data.collision_mask_add.name, &index))
                return editor_command_error(editor_result_error(
                    EDITOR_ERROR_CAPACITY,
                    "collision mask could not be added").result.error);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE,
                .result.object = (uint32_t)index};
        }
        case EDITOR_COMMAND_COLLISION_FILTER_SET: {
            const EditorCollisionFilterSetCommand *set =
                &command->data.collision_filter_set;
            EditorObject *object = editor_object_query_get(project, set->object);
            RohrCollisionCategoryMask *filter;
            size_t index;
            uint64_t bit;
            if(object == NULL) return editor_command_not_found("object", set->object);
            if(set->filter != EDITOR_COLLISION_FILTER_CATEGORY &&
                    set->filter != EDITOR_COLLISION_FILTER_COLLIDE_WITH)
                return editor_command_error(editor_result_error(
                    EDITOR_ERROR_INVALID_ARGUMENT,
                    "unknown collision filter kind").result.error);
            if(!editor_command_collision_mask_find(project, set->mask, &index))
                return editor_command_error(editor_result_error(
                    EDITOR_ERROR_NOT_FOUND,
                    "collision mask '%s' was not found", set->mask).result.error);
            if(set->kind == EDITOR_ITEM_RIGID_BODY) {
                EditorRigidBody *body = editor_project_rigid_body_get(object, set->item);
                if(body == NULL) return editor_command_not_found("rigid body", set->item);
                filter = set->filter == EDITOR_COLLISION_FILTER_CATEGORY ?
                    &body->collision_category : &body->collision_with;
            } else if(set->kind == EDITOR_ITEM_SOFT_NODE) {
                EditorSoftBody *body = editor_command_soft_body_get(object, set->parent);
                EditorSoftNode *node = editor_command_soft_node_get(body, set->item);
                if(node == NULL) return editor_command_not_found("soft node", set->item);
                filter = set->filter == EDITOR_COLLISION_FILTER_CATEGORY ?
                    &node->collision_category : &node->collision_with;
            } else {
                return editor_command_error(editor_result_error(
                    EDITOR_ERROR_INVALID_ARGUMENT,
                    "collision filters are only supported by rigid bodies and soft nodes")
                        .result.error);
            }
            bit = UINT64_C(1) << index;
            if(set->enabled) *filter |= bit;
            else *filter &= ~bit;
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_SPRITE_ADD: {
            EditorObject *object = editor_object_query_get(project,
                command->data.sprite_add.object);
            EditorSprite *sprite = editor_project_sprite_add(project, object,
                command->data.sprite_add.name, command->data.sprite_add.path);
            if(sprite == NULL) return editor_command_error(editor_result_error(
                EDITOR_ERROR_CAPACITY, "could not add sprite").result.error);
            sprite->size = command->data.sprite_add.size;
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE,
                .result.object = sprite->id};
        }
        case EDITOR_COMMAND_SPRITE_REMOVE:
            if(!editor_project_sprite_remove(editor_object_query_get(project,
                    command->data.sprite_remove.object),
                    command->data.sprite_remove.sprite))
                return editor_command_error(editor_result_error(
                    EDITOR_ERROR_REFERENCE_INVALID,
                    "sprite was not found").result.error);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        case EDITOR_COMMAND_SPRITE_RENAME:
        case EDITOR_COMMAND_SPRITE_PATH_SET:
        case EDITOR_COMMAND_SPRITE_POSITION_SET:
        case EDITOR_COMMAND_SPRITE_ROTATION_SET:
        case EDITOR_COMMAND_SPRITE_SIZE_SET:
        case EDITOR_COMMAND_SPRITE_BODY_SET:
        case EDITOR_COMMAND_SPRITE_FOLLOW_ROTATION_SET:
        case EDITOR_COMMAND_SPRITE_VISIBILITY_SET: {
            EditorSpriteId id = command->type == EDITOR_COMMAND_SPRITE_RENAME ?
                command->data.sprite_rename.sprite :
                command->type == EDITOR_COMMAND_SPRITE_PATH_SET ?
                    command->data.sprite_path_set.sprite :
                command->type == EDITOR_COMMAND_SPRITE_POSITION_SET ?
                    command->data.sprite_position_set.sprite :
                command->type == EDITOR_COMMAND_SPRITE_ROTATION_SET ?
                    command->data.sprite_rotation_set.sprite :
                command->type == EDITOR_COMMAND_SPRITE_SIZE_SET ?
                    command->data.sprite_size_set.sprite :
                command->type == EDITOR_COMMAND_SPRITE_BODY_SET ?
                    command->data.sprite_body_set.sprite :
                command->type == EDITOR_COMMAND_SPRITE_FOLLOW_ROTATION_SET ?
                    command->data.sprite_boolean_set.sprite :
                    command->data.sprite_visibility_set.sprite;
            EditorObjectId object_id = command->type == EDITOR_COMMAND_SPRITE_RENAME ?
                command->data.sprite_rename.object :
                command->type == EDITOR_COMMAND_SPRITE_PATH_SET ?
                    command->data.sprite_path_set.object :
                command->type == EDITOR_COMMAND_SPRITE_POSITION_SET ?
                    command->data.sprite_position_set.object :
                command->type == EDITOR_COMMAND_SPRITE_ROTATION_SET ?
                    command->data.sprite_rotation_set.object :
                command->type == EDITOR_COMMAND_SPRITE_SIZE_SET ?
                    command->data.sprite_size_set.object :
                command->type == EDITOR_COMMAND_SPRITE_BODY_SET ?
                    command->data.sprite_body_set.object :
                command->type == EDITOR_COMMAND_SPRITE_FOLLOW_ROTATION_SET ?
                    command->data.sprite_boolean_set.object :
                    command->data.sprite_visibility_set.object;
            EditorSprite *sprite = editor_project_sprite_get(
                editor_object_query_get(project, object_id), id);
            if(sprite == NULL) return editor_command_not_found("sprite", id);
            if(command->type == EDITOR_COMMAND_SPRITE_RENAME)
                editor_project_property_name_format(sprite->name, sizeof(sprite->name),
                    command->data.sprite_rename.name);
            else if(command->type == EDITOR_COMMAND_SPRITE_PATH_SET) {
                if(command->data.sprite_path_set.path[0] == '\0')
                    return editor_command_error(editor_result_error(
                        EDITOR_ERROR_INVALID_ARGUMENT,
                        "sprite path cannot be empty").result.error);
                snprintf(sprite->path, sizeof(sprite->path), "%s",
                    command->data.sprite_path_set.path);
            } else if(command->type == EDITOR_COMMAND_SPRITE_POSITION_SET) {
                sprite->position = command->data.sprite_position_set.position;
            } else if(command->type == EDITOR_COMMAND_SPRITE_ROTATION_SET) {
                sprite->rotation = command->data.sprite_rotation_set.rotation;
            } else if(command->type == EDITOR_COMMAND_SPRITE_SIZE_SET) {
                if(command->data.sprite_size_set.size.x <= 0.0f ||
                        command->data.sprite_size_set.size.y <= 0.0f)
                    return editor_command_error(editor_result_error(
                        EDITOR_ERROR_INVALID_ARGUMENT,
                        "sprite size must be positive").result.error);
                sprite->size = command->data.sprite_size_set.size;
            } else if(command->type == EDITOR_COMMAND_SPRITE_BODY_SET) {
                EditorObject *object = editor_object_query_get(project, object_id);
                EditorRigidBody *old_body;
                EditorRigidBody *new_body;
                Position object_local;
                Orientation world_rotation;
                if(command->data.sprite_body_set.body != 0 &&
                        editor_project_rigid_body_get(
                            object,
                            command->data.sprite_body_set.body) == NULL)
                    return editor_command_not_found("rigid body",
                        command->data.sprite_body_set.body);
                for(size_t i = 0; object != NULL &&
                        i < object->sprite_count; i += 1)
                    if(object->sprites[i].id != sprite->id &&
                            command->data.sprite_body_set.body != 0 &&
                            object->sprites[i].rigid_body ==
                                command->data.sprite_body_set.body)
                        return editor_command_error(editor_result_error(
                            EDITOR_ERROR_REFERENCE_INVALID,
                            "rigid body already has a static sprite").result.error);
                old_body = editor_project_rigid_body_get(object, sprite->rigid_body);
                new_body = editor_project_rigid_body_get(object,
                    command->data.sprite_body_set.body);
                object_local = sprite->position;
                world_rotation = sprite->rotation;
                if(old_body != NULL) {
                    object_local = editor_command_position_rotate(object_local,
                        old_body->rotation);
                    if(sprite->follow_body_rotation) world_rotation += old_body->rotation;
                    object_local.x += old_body->position.x;
                    object_local.y += old_body->position.y;
                }
                if(new_body != NULL) {
                    object_local.x -= new_body->position.x;
                    object_local.y -= new_body->position.y;
                    object_local = editor_command_position_rotate(object_local,
                        -new_body->rotation);
                    if(sprite->follow_body_rotation) world_rotation -= new_body->rotation;
                }
                sprite->position = object_local;
                sprite->rotation = world_rotation;
                sprite->rigid_body = command->data.sprite_body_set.body;
            } else if(command->type == EDITOR_COMMAND_SPRITE_FOLLOW_ROTATION_SET) {
                bool enabled = command->data.sprite_boolean_set.enabled;
                EditorRigidBody *body = editor_project_rigid_body_get(
                    editor_object_query_get(project, object_id), sprite->rigid_body);
                if(body != NULL && enabled != sprite->follow_body_rotation) {
                    sprite->rotation += enabled ? -body->rotation : body->rotation;
                }
                sprite->follow_body_rotation = enabled;
            }
            else sprite->visible = command->data.sprite_visibility_set.visible;
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_ANIMATED_SPRITE_ADD: {
            EditorObject *object = editor_object_query_get(project,
                command->data.animated_sprite_add.object);
            EditorAnimatedSprite *sprite;
            if(object == NULL) return editor_command_not_found("object",
                command->data.animated_sprite_add.object);
            sprite = editor_project_animated_sprite_add(project, object);
            if(sprite == NULL) return editor_command_error(editor_result_error(
                EDITOR_ERROR_CAPACITY, "could not add animated sprite").result.error);
            if(command->data.animated_sprite_add.name[0] != '\0')
                editor_project_property_name_format(sprite->name, sizeof(sprite->name),
                    command->data.animated_sprite_add.name);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE,
                .result.object = sprite->id};
        }
        case EDITOR_COMMAND_ANIMATED_SPRITE_REMOVE: {
            EditorObject *object = editor_object_query_get(project,
                command->data.animated_sprite_remove.object);
            if(object == NULL || !editor_project_animated_sprite_remove(object,
                    command->data.animated_sprite_remove.sprite))
                return editor_command_not_found("animated sprite",
                    command->data.animated_sprite_remove.sprite);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_ANIMATED_SPRITE_RENAME:
        case EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_POSITION_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_ROTATION_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_SCALE_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_TIMING_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_STARTING_FRAME_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_DIRECTION_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_FOLLOW_ROTATION_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_PLAYING_SET:
        case EDITOR_COMMAND_ANIMATION_FRAME_ADD:
        case EDITOR_COMMAND_ANIMATION_FRAME_REMOVE:
        case EDITOR_COMMAND_ANIMATION_FRAME_RENAME:
        case EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET:
        case EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET: {
            EditorObjectId object_id;
            EditorAnimatedSpriteId sprite_id;
            EditorObject *object;
            EditorAnimatedSprite *sprite;
#define ANIMATED_IDS(member) do { object_id = command->data.member.object; \
    sprite_id = command->data.member.sprite; } while(0)
            if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_RENAME)
                ANIMATED_IDS(animated_sprite_rename);
            else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET)
                ANIMATED_IDS(animated_sprite_body_set);
            else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_POSITION_SET)
                ANIMATED_IDS(animated_sprite_position_set);
            else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_ROTATION_SET)
                ANIMATED_IDS(animated_sprite_rotation_set);
            else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_SCALE_SET)
                ANIMATED_IDS(animated_sprite_scale_set);
            else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_TIMING_SET)
                ANIMATED_IDS(animated_sprite_timing_set);
            else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_STARTING_FRAME_SET)
                ANIMATED_IDS(animated_sprite_starting_frame_set);
            else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_DIRECTION_SET)
                ANIMATED_IDS(animated_sprite_direction_set);
            else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_ADD)
                ANIMATED_IDS(animation_frame_add);
            else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_REMOVE)
                ANIMATED_IDS(animation_frame_remove);
            else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_RENAME)
                ANIMATED_IDS(animation_frame_rename);
            else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET)
                ANIMATED_IDS(animation_frame_path_set);
            else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET)
                ANIMATED_IDS(animation_frame_size_set);
            else ANIMATED_IDS(animated_sprite_boolean_set);
#undef ANIMATED_IDS
            object = editor_object_query_get(project, object_id);
            sprite = editor_command_animated_sprite_get(object, sprite_id);
            if(sprite == NULL) return editor_command_not_found("animated sprite", sprite_id);
            if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_RENAME)
                editor_project_property_name_format(sprite->name, sizeof(sprite->name),
                    command->data.animated_sprite_rename.name);
            else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET) {
                EditorRigidBodyId body = command->data.animated_sprite_body_set.body;
                EditorRigidBody *old_body;
                EditorRigidBody *new_body;
                Position object_local;
                Orientation world_rotation;
                if(body != 0 && editor_project_rigid_body_get(object, body) == NULL)
                    return editor_command_not_found("rigid body", body);
                for(size_t i = 0; body != 0 && i < object->animated_sprite_count; i += 1)
                    if(object->animated_sprite_items[i].id != sprite->id &&
                            object->animated_sprite_items[i].rigid_body == body)
                        return editor_command_error(editor_result_error(
                            EDITOR_ERROR_REFERENCE_INVALID,
                            "rigid body already has an animated sprite").result.error);
                old_body = editor_project_rigid_body_get(object, sprite->rigid_body);
                new_body = editor_project_rigid_body_get(object, body);
                object_local = sprite->editor_position;
                world_rotation = sprite->editor_rotation;
                if(old_body != NULL) {
                    object_local = editor_command_position_rotate(object_local,
                        old_body->rotation);
                    if(sprite->follow_body_rotation) world_rotation += old_body->rotation;
                    object_local.x += old_body->position.x;
                    object_local.y += old_body->position.y;
                }
                if(new_body != NULL) {
                    object_local.x -= new_body->position.x;
                    object_local.y -= new_body->position.y;
                    object_local = editor_command_position_rotate(object_local,
                        -new_body->rotation);
                    if(sprite->follow_body_rotation) world_rotation -= new_body->rotation;
                }
                sprite->editor_position = object_local;
                sprite->editor_rotation = world_rotation;
                sprite->rigid_body = body;
            } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_POSITION_SET) {
                sprite->editor_position =
                    command->data.animated_sprite_position_set.position;
            } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_ROTATION_SET) {
                sprite->editor_rotation =
                    command->data.animated_sprite_rotation_set.rotation;
            } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_SCALE_SET) {
                Scale scale = command->data.animated_sprite_scale_set.scale;
                if(scale.x <= 0.0f || scale.y <= 0.0f)
                    return editor_command_error(editor_result_error(
                        EDITOR_ERROR_INVALID_ARGUMENT,
                        "animated sprite scale must be positive").result.error);
                sprite->scale = scale;
            } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_TIMING_SET) {
                sprite->ticks_per_frame = command->data.animated_sprite_timing_set.ticks;
                sprite->time_per_frame = command->data.animated_sprite_timing_set.time;
            } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_STARTING_FRAME_SET) {
                uint32_t frame = command->data.animated_sprite_starting_frame_set.frame;
                if(frame >= sprite->frame_count)
                    return editor_command_error(editor_result_error(
                        EDITOR_ERROR_INVALID_ARGUMENT,
                        "starting frame is outside the animation").result.error);
                sprite->starting_frame = frame;
            } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_DIRECTION_SET)
                sprite->direction = command->data.animated_sprite_direction_set.direction;
            else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_FOLLOW_ROTATION_SET) {
                bool enabled = command->data.animated_sprite_boolean_set.enabled;
                EditorRigidBody *body = editor_project_rigid_body_get(object,
                    sprite->rigid_body);
                if(body != NULL && enabled != sprite->follow_body_rotation) {
                    sprite->editor_rotation += enabled ?
                        -body->rotation : body->rotation;
                }
                sprite->follow_body_rotation = enabled;
            }
            else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET)
                sprite->visible = command->data.animated_sprite_boolean_set.enabled;
            else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_PLAYING_SET)
                sprite->playing = command->data.animated_sprite_boolean_set.enabled;
            else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_ADD) {
                if(sprite->frame_count >= MAX_ANIMATIONS_FRAMES ||
                        !editor_project_animation_frame_add(project, sprite,
                            command->data.animation_frame_add.name,
                            command->data.animation_frame_add.path,
                            command->data.animation_frame_add.size))
                    return editor_command_error(editor_result_error(
                        EDITOR_ERROR_CAPACITY,
                        "animation frame is invalid or reached the runtime frame limit")
                            .result.error);
            } else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_REMOVE) {
                if(!editor_project_animation_frame_remove(sprite,
                        command->data.animation_frame_remove.index))
                    return editor_command_error(editor_result_error(
                        EDITOR_ERROR_NOT_FOUND,
                        "animation frame index was not found").result.error);
                if(sprite->frame_count == 0) sprite->starting_frame = 0;
                else if(sprite->starting_frame >= sprite->frame_count)
                    sprite->starting_frame = (uint32_t)sprite->frame_count - 1;
            } else {
                size_t index = command->type == EDITOR_COMMAND_ANIMATION_FRAME_RENAME ?
                    command->data.animation_frame_rename.index :
                    command->type == EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET ?
                        command->data.animation_frame_path_set.index :
                        command->data.animation_frame_size_set.index;
                EditorAnimationFrame *frame;
                if(index >= sprite->frame_count)
                    return editor_command_error(editor_result_error(
                        EDITOR_ERROR_NOT_FOUND,
                        "animation frame index was not found").result.error);
                frame = &sprite->frames[index];
                if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_RENAME) {
                    editor_project_property_name_format(frame->name,
                        sizeof(frame->name), command->data.animation_frame_rename.name);
                } else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET) {
                    if(command->data.animation_frame_path_set.path[0] == '\0')
                        return editor_command_error(editor_result_error(
                            EDITOR_ERROR_INVALID_ARGUMENT,
                            "animation frame path cannot be empty").result.error);
                    snprintf(frame->path, sizeof(frame->path), "%s",
                        command->data.animation_frame_path_set.path);
                } else {
                    Scale size = command->data.animation_frame_size_set.size;
                    if(size.x <= 0.0f || size.y <= 0.0f)
                        return editor_command_error(editor_result_error(
                            EDITOR_ERROR_INVALID_ARGUMENT,
                            "animation frame size must be positive").result.error);
                    frame->size = size;
                }
            }
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
    }
    return editor_command_error((EditorError){EDITOR_ERROR_INVALID_ARGUMENT,
        "unknown editor command"});
}

EditorCommandResult editor_command_execute(EditorProject *project,
        const EditorCommand *command) {
    if(editor_command_executing_callback != NULL)
        editor_command_executing_callback(project, command,
            editor_command_executing_context);
    EditorCommandResult result = editor_command_execute_internal(project, command);
    if(result.kind == ERROR_RESULT_VALUE && editor_command_executed_callback != NULL)
        editor_command_executed_callback(command, &result,
            editor_command_executed_context);
    if(editor_command_finished_callback != NULL)
        editor_command_finished_callback(command, &result,
            editor_command_finished_context);
    return result;
}

void editor_command_executed_callback_set(EditorCommandExecuted callback,
        void *context) {
    editor_command_executed_callback = callback;
    editor_command_executed_context = context;
}

void editor_command_executing_callback_set(EditorCommandExecuting callback,
        void *context) {
    editor_command_executing_callback = callback;
    editor_command_executing_context = context;
}

void editor_command_finished_callback_set(EditorCommandFinished callback,
        void *context) {
    editor_command_finished_callback = callback;
    editor_command_finished_context = context;
}

static bool editor_command_uint_parse(const char *text, uint32_t *value) {
    char *end;
    unsigned long parsed;
    if(text == NULL || value == NULL || text[0] == '\0') return false;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if(errno != 0 || *end != '\0' || parsed > UINT32_MAX) return false;
    *value = (uint32_t)parsed;
    return true;
}

static bool editor_command_color_parse(const char *text, uint32_t *value) {
    const char *digits = text;
    char *end;
    unsigned long parsed;
    size_t length;
    if(text == NULL || value == NULL) return false;
    if(*digits == '#') digits += 1;
    length = strlen(digits);
    if(length != 6 && length != 8) return false;
    errno = 0;
    parsed = strtoul(digits, &end, 16);
    if(errno != 0 || *end != '\0' || parsed > UINT32_MAX) return false;
    *value = length == 6 ? ((uint32_t)parsed << 8) | UINT32_C(0xff) :
        (uint32_t)parsed;
    return true;
}

static bool editor_command_property_color_check(EditorPropertyKind property) {
    return property >= EDITOR_PROPERTY_OUTLINE_COLOR &&
        property <= EDITOR_PROPERTY_COLOR;
}

static bool editor_command_float_parse(const char *text, float *value) {
    char *end;
    float parsed;
    if(text == NULL || value == NULL || text[0] == '\0') return false;
    errno = 0;
    parsed = strtof(text, &end);
    if(errno != 0 || *end != '\0') return false;
    *value = parsed;
    return true;
}

static bool editor_command_bool_parse(const char *text, bool *value) {
    if(text == NULL || value == NULL) return false;
    if(strcmp(text, "true") == 0) {
        *value = true;
        return true;
    }
    if(strcmp(text, "false") == 0) {
        *value = false;
        return true;
    }
    return false;
}

static bool editor_command_optional_id_parse(const char *text, uint32_t *value) {
    if(text != NULL && strcmp(text, "none") == 0) {
        *value = 0;
        return true;
    }
    return editor_command_uint_parse(text, value);
}

static const char *editor_command_navigation_modes[] = {
    "hierarchy", "object", "rigid-body", "hitbox", "joint", "anchor",
    "soft-body", "soft-node", "soft-beam", "origin", "line", "vertex"
};

static const char *editor_command_navigation_selections[] = {
    "none", "object", "rigid-body", "hitbox", "joint", "anchor",
    "soft-body", "soft-node", "soft-beam", "origin", "line", "vertex"
};

static const char *editor_command_origin_kinds[] = {
    "none", "rigid-body", "soft-body"
};

static bool editor_command_named_uint_parse(const char *text,
        const char *const *names, size_t count, uint32_t *value) {
    if(text == NULL || names == NULL || value == NULL) return false;
    for(size_t i = 0; i < count; i += 1) {
        if(strcmp(text, names[i]) != 0) continue;
        *value = (uint32_t)i;
        return true;
    }
    return false;
}

static bool editor_command_item_kind_parse(const char *domain, EditorItemKind *kind) {
    if(domain == NULL || kind == NULL) return false;
    if(strcmp(domain, "object") == 0) *kind = EDITOR_ITEM_OBJECT;
    else if(strcmp(domain, "rigid-body") == 0) *kind = EDITOR_ITEM_RIGID_BODY;
    else if(strcmp(domain, "hitbox") == 0) *kind = EDITOR_ITEM_HITBOX;
    else if(strcmp(domain, "joint") == 0) *kind = EDITOR_ITEM_JOINT;
    else if(strcmp(domain, "anchor") == 0) *kind = EDITOR_ITEM_ANCHOR;
    else if(strcmp(domain, "soft-body") == 0) *kind = EDITOR_ITEM_SOFT_BODY;
    else if(strcmp(domain, "soft-node") == 0) *kind = EDITOR_ITEM_SOFT_NODE;
    else if(strcmp(domain, "soft-beam") == 0) *kind = EDITOR_ITEM_SOFT_BEAM;
    else if(strcmp(domain, "soft-area") == 0) *kind = EDITOR_ITEM_SOFT_AREA;
    else if(strcmp(domain, "vertex") == 0) *kind = EDITOR_ITEM_VERTEX;
    else if(strcmp(domain, "line") == 0) *kind = EDITOR_ITEM_LINE;
    else return false;
    return true;
}

static const char *editor_command_item_domain_get(EditorItemKind kind) {
    switch(kind) {
        case EDITOR_ITEM_OBJECT: return "object";
        case EDITOR_ITEM_RIGID_BODY: return "rigid-body";
        case EDITOR_ITEM_HITBOX: return "hitbox";
        case EDITOR_ITEM_JOINT: return "joint";
        case EDITOR_ITEM_ANCHOR: return "anchor";
        case EDITOR_ITEM_SOFT_BODY: return "soft-body";
        case EDITOR_ITEM_SOFT_NODE: return "soft-node";
        case EDITOR_ITEM_SOFT_BEAM: return "soft-beam";
        case EDITOR_ITEM_SOFT_AREA: return "soft-area";
        case EDITOR_ITEM_VERTEX: return "vertex";
        case EDITOR_ITEM_LINE: return "line";
    }
    return NULL;
}

static bool editor_command_property_parse(const char *name,
        EditorPropertyKind *property, EditorPropertyValueKind *value_kind) {
    if(name == NULL || property == NULL || value_kind == NULL) return false;
#define EDITOR_FLOAT_PROPERTY(text, value) \
    if(strcmp(name, text) == 0) { *property = value; \
        *value_kind = EDITOR_PROPERTY_VALUE_FLOAT; return true; }
#define EDITOR_BOOL_PROPERTY(text, value) \
    if(strcmp(name, text) == 0) { *property = value; \
        *value_kind = EDITOR_PROPERTY_VALUE_BOOL; return true; }
    EDITOR_FLOAT_PROPERTY("mass", EDITOR_PROPERTY_MASS)
    EDITOR_FLOAT_PROPERTY("friction", EDITOR_PROPERTY_FRICTION)
    EDITOR_FLOAT_PROPERTY("restitution", EDITOR_PROPERTY_RESTITUTION)
    EDITOR_FLOAT_PROPERTY("visual-size", EDITOR_PROPERTY_VISUAL_SIZE)
    EDITOR_FLOAT_PROPERTY("rest-length", EDITOR_PROPERTY_REST_LENGTH)
    EDITOR_FLOAT_PROPERTY("stiffness", EDITOR_PROPERTY_STIFFNESS)
    EDITOR_FLOAT_PROPERTY("damping", EDITOR_PROPERTY_DAMPING)
    EDITOR_FLOAT_PROPERTY("length", EDITOR_PROPERTY_LINE_LENGTH)
    EDITOR_FLOAT_PROPERTY("particle-radius", EDITOR_PROPERTY_PARTICLE_RADIUS)
    EDITOR_FLOAT_PROPERTY("node-radius", EDITOR_PROPERTY_NODE_RADIUS)
    EDITOR_BOOL_PROPERTY("gravity", EDITOR_PROPERTY_GRAVITY)
    EDITOR_BOOL_PROPERTY("static", EDITOR_PROPERTY_STATIC)
    EDITOR_BOOL_PROPERTY("rotation-locked", EDITOR_PROPERTY_ROTATION_LOCKED)
    EDITOR_BOOL_PROPERTY("collision", EDITOR_PROPERTY_COLLISION)
    EDITOR_BOOL_PROPERTY("particle", EDITOR_PROPERTY_PARTICLE)
    EDITOR_BOOL_PROPERTY("particle-auto-fit", EDITOR_PROPERTY_PARTICLE_AUTO_FIT)
    EDITOR_BOOL_PROPERTY("position-locked", EDITOR_PROPERTY_POSITION_LOCKED)
    EDITOR_BOOL_PROPERTY("position-follows-body", EDITOR_PROPERTY_POSITION_FOLLOWS_BODY)
    EDITOR_BOOL_PROPERTY("rotation-follows-body", EDITOR_PROPERTY_ROTATION_FOLLOWS_BODY)
#define EDITOR_COLOR_PROPERTY(text, value) \
    if(strcmp(name, text) == 0) { *property = value; \
        *value_kind = EDITOR_PROPERTY_VALUE_UINT; return true; }
    EDITOR_COLOR_PROPERTY("outline-color", EDITOR_PROPERTY_OUTLINE_COLOR)
    EDITOR_COLOR_PROPERTY("surface-color", EDITOR_PROPERTY_SURFACE_COLOR)
    EDITOR_COLOR_PROPERTY("particle-ring-color", EDITOR_PROPERTY_PARTICLE_RING_COLOR)
    EDITOR_COLOR_PROPERTY("particle-fill-color", EDITOR_PROPERTY_PARTICLE_FILL_COLOR)
    EDITOR_COLOR_PROPERTY("node-color", EDITOR_PROPERTY_NODE_COLOR)
    EDITOR_COLOR_PROPERTY("beam-color", EDITOR_PROPERTY_BEAM_COLOR)
    EDITOR_COLOR_PROPERTY("area-color", EDITOR_PROPERTY_AREA_COLOR)
    EDITOR_COLOR_PROPERTY("color", EDITOR_PROPERTY_COLOR)
#undef EDITOR_COLOR_PROPERTY
#undef EDITOR_FLOAT_PROPERTY
#undef EDITOR_BOOL_PROPERTY
    if(strcmp(name, "kind") == 0) {
        *property = EDITOR_PROPERTY_JOINT_KIND;
        *value_kind = EDITOR_PROPERTY_VALUE_UINT;
        return true;
    }
    return false;
}

static const char *editor_command_property_name_get(EditorPropertyKind property) {
    switch(property) {
        case EDITOR_PROPERTY_MASS: return "mass";
        case EDITOR_PROPERTY_FRICTION: return "friction";
        case EDITOR_PROPERTY_RESTITUTION: return "restitution";
        case EDITOR_PROPERTY_GRAVITY: return "gravity";
        case EDITOR_PROPERTY_STATIC: return "static";
        case EDITOR_PROPERTY_ROTATION_LOCKED: return "rotation-locked";
        case EDITOR_PROPERTY_COLLISION: return "collision";
        case EDITOR_PROPERTY_PARTICLE: return "particle";
        case EDITOR_PROPERTY_PARTICLE_RADIUS: return "particle-radius";
        case EDITOR_PROPERTY_PARTICLE_AUTO_FIT: return "particle-auto-fit";
        case EDITOR_PROPERTY_NODE_RADIUS: return "node-radius";
        case EDITOR_PROPERTY_POSITION_LOCKED: return "position-locked";
        case EDITOR_PROPERTY_VISUAL_SIZE: return "visual-size";
        case EDITOR_PROPERTY_JOINT_KIND: return "kind";
        case EDITOR_PROPERTY_REST_LENGTH: return "rest-length";
        case EDITOR_PROPERTY_STIFFNESS: return "stiffness";
        case EDITOR_PROPERTY_DAMPING: return "damping";
        case EDITOR_PROPERTY_POSITION_FOLLOWS_BODY: return "position-follows-body";
        case EDITOR_PROPERTY_ROTATION_FOLLOWS_BODY: return "rotation-follows-body";
        case EDITOR_PROPERTY_LINE_LENGTH: return "length";
        case EDITOR_PROPERTY_OUTLINE_COLOR: return "outline-color";
        case EDITOR_PROPERTY_SURFACE_COLOR: return "surface-color";
        case EDITOR_PROPERTY_PARTICLE_RING_COLOR: return "particle-ring-color";
        case EDITOR_PROPERTY_PARTICLE_FILL_COLOR: return "particle-fill-color";
        case EDITOR_PROPERTY_NODE_COLOR: return "node-color";
        case EDITOR_PROPERTY_BEAM_COLOR: return "beam-color";
        case EDITOR_PROPERTY_AREA_COLOR: return "area-color";
        case EDITOR_PROPERTY_COLOR: return "color";
    }
    return NULL;
}

EditorResult editor_command_cli_parse(int count, char **arguments,
        const char **document_path, EditorCommand *command) {
    const char *domain;
    const char *action;
    if(arguments == NULL || document_path == NULL || command == NULL || count < 4)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "expected an editor mutation command");
    domain = arguments[1];
    action = arguments[2];
    if(strcmp(domain, "sprite") == 0) {
        uint32_t object, id;
        *document_path = arguments[3];
        if(count < 6 || !editor_command_uint_parse(arguments[4], &object))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "sprite command requires an object");
        if(strcmp(action, "add") == 0 && count == 9 &&
                editor_command_float_parse(arguments[7],
                    &command->data.sprite_add.size.x) &&
                editor_command_float_parse(arguments[8],
                    &command->data.sprite_add.size.y)) {
            command->type = EDITOR_COMMAND_SPRITE_ADD;
            command->data.sprite_add.object = object;
            snprintf(command->data.sprite_add.name,
                sizeof(command->data.sprite_add.name), "%s", arguments[5]);
            snprintf(command->data.sprite_add.path,
                sizeof(command->data.sprite_add.path), "%s", arguments[6]);
            return editor_result_value(true);
        }
        if(count >= 6 && editor_command_uint_parse(arguments[5], &id)) {
            if(strcmp(action, "delete") == 0 && count == 6) {
                command->type = EDITOR_COMMAND_SPRITE_REMOVE;
                command->data.sprite_remove.object = object;
                command->data.sprite_remove.sprite = id;
                return editor_result_value(true);
            }
            if(strcmp(action, "rename") == 0 && count == 7) {
                command->type = EDITOR_COMMAND_SPRITE_RENAME;
                command->data.sprite_rename.object = object;
                command->data.sprite_rename.sprite = id;
                snprintf(command->data.sprite_rename.name,
                    sizeof(command->data.sprite_rename.name), "%s", arguments[6]);
                return editor_result_value(true);
            }
            if(strcmp(action, "set") == 0 && count == 8 &&
                    strcmp(arguments[6], "path") == 0) {
                command->type = EDITOR_COMMAND_SPRITE_PATH_SET;
                command->data.sprite_path_set.object = object;
                command->data.sprite_path_set.sprite = id;
                snprintf(command->data.sprite_path_set.path,
                    sizeof(command->data.sprite_path_set.path), "%s", arguments[7]);
                return editor_result_value(true);
            }
            if(strcmp(action, "set") == 0 && count == 9 &&
                    strcmp(arguments[6], "position") == 0 &&
                    editor_command_float_parse(arguments[7],
                        &command->data.sprite_position_set.position.x) &&
                    editor_command_float_parse(arguments[8],
                        &command->data.sprite_position_set.position.y)) {
                command->type = EDITOR_COMMAND_SPRITE_POSITION_SET;
                command->data.sprite_position_set.object = object;
                command->data.sprite_position_set.sprite = id;
                return editor_result_value(true);
            }
            if(strcmp(action, "set") == 0 && count == 8 &&
                    strcmp(arguments[6], "rotation") == 0 &&
                    editor_command_float_parse(arguments[7],
                        &command->data.sprite_rotation_set.rotation)) {
                command->type = EDITOR_COMMAND_SPRITE_ROTATION_SET;
                command->data.sprite_rotation_set.object = object;
                command->data.sprite_rotation_set.sprite = id;
                return editor_result_value(true);
            }
            if(strcmp(action, "set") == 0 && count == 9 &&
                    strcmp(arguments[6], "size") == 0 &&
                    editor_command_float_parse(arguments[7],
                        &command->data.sprite_size_set.size.x) &&
                    editor_command_float_parse(arguments[8],
                        &command->data.sprite_size_set.size.y)) {
                command->type = EDITOR_COMMAND_SPRITE_SIZE_SET;
                command->data.sprite_size_set.object = object;
                command->data.sprite_size_set.sprite = id;
                return editor_result_value(true);
            }
            if(strcmp(action, "connect") == 0 && count == 8 &&
                    strcmp(arguments[6], "body") == 0 &&
                    editor_command_uint_parse(arguments[7],
                        &command->data.sprite_body_set.body)) {
                command->type = EDITOR_COMMAND_SPRITE_BODY_SET;
                command->data.sprite_body_set.object = object;
                command->data.sprite_body_set.sprite = id;
                return editor_result_value(true);
            }
            if(strcmp(action, "set") == 0 && count == 8 &&
                    strcmp(arguments[6], "follow-body-rotation") == 0 &&
                    editor_command_bool_parse(arguments[7],
                        &command->data.sprite_boolean_set.enabled)) {
                command->type = EDITOR_COMMAND_SPRITE_FOLLOW_ROTATION_SET;
                command->data.sprite_boolean_set.object = object;
                command->data.sprite_boolean_set.sprite = id;
                return editor_result_value(true);
            }
            if(strcmp(action, "set") == 0 && count == 8 &&
                    strcmp(arguments[6], "visibility") == 0 &&
                    editor_command_bool_parse(arguments[7],
                        &command->data.sprite_visibility_set.visible)) {
                command->type = EDITOR_COMMAND_SPRITE_VISIBILITY_SET;
                command->data.sprite_visibility_set.object = object;
                command->data.sprite_visibility_set.sprite = id;
                return editor_result_value(true);
            }
        }
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "invalid sprite command");
    }
    if(strcmp(domain, "animated-sprite") == 0) {
        uint32_t object, id, value;
        *document_path = arguments[3];
        if(count < 6 || !editor_command_uint_parse(arguments[4], &object))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "animated-sprite command requires an object");
        if(strcmp(action, "add") == 0 && count == 6) {
            command->type = EDITOR_COMMAND_ANIMATED_SPRITE_ADD;
            command->data.animated_sprite_add.object = object;
            snprintf(command->data.animated_sprite_add.name,
                sizeof(command->data.animated_sprite_add.name), "%s", arguments[5]);
            return editor_result_value(true);
        }
        if(!editor_command_uint_parse(arguments[5], &id))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "animated-sprite ID must be an integer");
        if(strcmp(action, "delete") == 0 && count == 6) {
            command->type = EDITOR_COMMAND_ANIMATED_SPRITE_REMOVE;
            command->data.animated_sprite_remove.object = object;
            command->data.animated_sprite_remove.sprite = id;
            return editor_result_value(true);
        }
        if(strcmp(action, "rename") == 0 && count == 7) {
            command->type = EDITOR_COMMAND_ANIMATED_SPRITE_RENAME;
            command->data.animated_sprite_rename.object = object;
            command->data.animated_sprite_rename.sprite = id;
            snprintf(command->data.animated_sprite_rename.name,
                sizeof(command->data.animated_sprite_rename.name), "%s", arguments[6]);
            return editor_result_value(true);
        }
        if(strcmp(action, "frame-add") == 0 && count == 10) {
            char *end_x;
            char *end_y;
            double width = strtod(arguments[8], &end_x);
            double height = strtod(arguments[9], &end_y);
            if(end_x == arguments[8] || *end_x != '\0' || end_y == arguments[9] ||
                    *end_y != '\0' || width <= 0.0 || height <= 0.0)
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "animation frame size must be positive numbers");
            command->type = EDITOR_COMMAND_ANIMATION_FRAME_ADD;
            command->data.animation_frame_add.object = object;
            command->data.animation_frame_add.sprite = id;
            snprintf(command->data.animation_frame_add.name,
                sizeof(command->data.animation_frame_add.name), "%s", arguments[6]);
            snprintf(command->data.animation_frame_add.path,
                sizeof(command->data.animation_frame_add.path), "%s", arguments[7]);
            command->data.animation_frame_add.size = (Scale){(float)width, (float)height};
            return editor_result_value(true);
        }
        if(strcmp(action, "frame-delete") == 0 && count == 7 &&
                editor_command_uint_parse(arguments[6], &value)) {
            command->type = EDITOR_COMMAND_ANIMATION_FRAME_REMOVE;
            command->data.animation_frame_remove.object = object;
            command->data.animation_frame_remove.sprite = id;
            command->data.animation_frame_remove.index = value;
            return editor_result_value(true);
        }
        if(strcmp(action, "frame-rename") == 0 && count == 8 &&
                editor_command_uint_parse(arguments[6], &value)) {
            command->type = EDITOR_COMMAND_ANIMATION_FRAME_RENAME;
            command->data.animation_frame_rename.object = object;
            command->data.animation_frame_rename.sprite = id;
            command->data.animation_frame_rename.index = value;
            snprintf(command->data.animation_frame_rename.name,
                sizeof(command->data.animation_frame_rename.name), "%s", arguments[7]);
            return editor_result_value(true);
        }
        if(strcmp(action, "frame-path-set") == 0 && count == 8 &&
                editor_command_uint_parse(arguments[6], &value)) {
            command->type = EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET;
            command->data.animation_frame_path_set.object = object;
            command->data.animation_frame_path_set.sprite = id;
            command->data.animation_frame_path_set.index = value;
            snprintf(command->data.animation_frame_path_set.path,
                sizeof(command->data.animation_frame_path_set.path), "%s", arguments[7]);
            return editor_result_value(true);
        }
        if(strcmp(action, "frame-size-set") == 0 && count == 9 &&
                editor_command_uint_parse(arguments[6], &value)) {
            char *end_x;
            char *end_y;
            double width = strtod(arguments[7], &end_x);
            double height = strtod(arguments[8], &end_y);
            if(end_x == arguments[7] || *end_x != '\0' || end_y == arguments[8] ||
                    *end_y != '\0' || width <= 0.0 || height <= 0.0)
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "animation frame size must be positive numbers");
            command->type = EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET;
            command->data.animation_frame_size_set.object = object;
            command->data.animation_frame_size_set.sprite = id;
            command->data.animation_frame_size_set.index = value;
            command->data.animation_frame_size_set.size =
                (Scale){(float)width, (float)height};
            return editor_result_value(true);
        }
        if(strcmp(action, "connect") == 0 && count == 8 &&
                strcmp(arguments[6], "body") == 0 &&
                editor_command_optional_id_parse(arguments[7], &value)) {
            command->type = EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET;
            command->data.animated_sprite_body_set.object = object;
            command->data.animated_sprite_body_set.sprite = id;
            command->data.animated_sprite_body_set.body = value;
            return editor_result_value(true);
        }
        if(strcmp(action, "set") == 0 && count >= 8) {
            const char *property = arguments[6];
            if(strcmp(property, "body") == 0 && count == 8 &&
                    editor_command_optional_id_parse(arguments[7], &value)) {
                command->type = EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET;
                command->data.animated_sprite_body_set.object = object;
                command->data.animated_sprite_body_set.sprite = id;
                command->data.animated_sprite_body_set.body = value;
                return editor_result_value(true);
            }
            if(strcmp(property, "scale") == 0 && count == 9 &&
                    editor_command_float_parse(arguments[7],
                        &command->data.animated_sprite_scale_set.scale.x) &&
                    editor_command_float_parse(arguments[8],
                        &command->data.animated_sprite_scale_set.scale.y)) {
                command->type = EDITOR_COMMAND_ANIMATED_SPRITE_SCALE_SET;
                command->data.animated_sprite_scale_set.object = object;
                command->data.animated_sprite_scale_set.sprite = id;
                return editor_result_value(true);
            }
            if(strcmp(property, "position") == 0 && count == 9 &&
                    editor_command_float_parse(arguments[7],
                        &command->data.animated_sprite_position_set.position.x) &&
                    editor_command_float_parse(arguments[8],
                        &command->data.animated_sprite_position_set.position.y)) {
                command->type = EDITOR_COMMAND_ANIMATED_SPRITE_POSITION_SET;
                command->data.animated_sprite_position_set.object = object;
                command->data.animated_sprite_position_set.sprite = id;
                return editor_result_value(true);
            }
            if(strcmp(property, "rotation") == 0 && count == 8 &&
                    editor_command_float_parse(arguments[7],
                        &command->data.animated_sprite_rotation_set.rotation)) {
                command->type = EDITOR_COMMAND_ANIMATED_SPRITE_ROTATION_SET;
                command->data.animated_sprite_rotation_set.object = object;
                command->data.animated_sprite_rotation_set.sprite = id;
                return editor_result_value(true);
            }
            if(strcmp(property, "timing") == 0 && count == 9) {
                char *tick_end, *time_end;
                unsigned long long ticks = strtoull(arguments[7], &tick_end, 10);
                double time = strtod(arguments[8], &time_end);
                if(*tick_end != '\0' || *time_end != '\0' || time < 0.0)
                    return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                        "timing requires non-negative ticks and seconds");
                command->type = EDITOR_COMMAND_ANIMATED_SPRITE_TIMING_SET;
                command->data.animated_sprite_timing_set.object = object;
                command->data.animated_sprite_timing_set.sprite = id;
                command->data.animated_sprite_timing_set.ticks = (Tick)ticks;
                command->data.animated_sprite_timing_set.time = (Time)time;
                return editor_result_value(true);
            }
            if(strcmp(property, "starting-frame") == 0 && count == 8 &&
                    editor_command_uint_parse(arguments[7], &value)) {
                command->type = EDITOR_COMMAND_ANIMATED_SPRITE_STARTING_FRAME_SET;
                command->data.animated_sprite_starting_frame_set.object = object;
                command->data.animated_sprite_starting_frame_set.sprite = id;
                command->data.animated_sprite_starting_frame_set.frame = value;
                return editor_result_value(true);
            }
            if(strcmp(property, "direction") == 0 && count == 8 &&
                    (strcmp(arguments[7], "left") == 0 ||
                        strcmp(arguments[7], "right") == 0)) {
                command->type = EDITOR_COMMAND_ANIMATED_SPRITE_DIRECTION_SET;
                command->data.animated_sprite_direction_set.object = object;
                command->data.animated_sprite_direction_set.sprite = id;
                command->data.animated_sprite_direction_set.direction =
                    strcmp(arguments[7], "left") == 0 ? DIRECTION_LEFT :
                        DIRECTION_RIGHT;
                return editor_result_value(true);
            }
            if((strcmp(property, "follow-body-rotation") == 0 ||
                    strcmp(property, "visibility") == 0 ||
                    strcmp(property, "playing") == 0) && count == 8) {
                bool enabled;
                if(!editor_command_bool_parse(arguments[7], &enabled))
                    return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                        "%s requires true or false", property);
                command->type = strcmp(property, "visibility") == 0 ?
                    EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET :
                    strcmp(property, "playing") == 0 ?
                        EDITOR_COMMAND_ANIMATED_SPRITE_PLAYING_SET :
                        EDITOR_COMMAND_ANIMATED_SPRITE_FOLLOW_ROTATION_SET;
                command->data.animated_sprite_boolean_set.object = object;
                command->data.animated_sprite_boolean_set.sprite = id;
                command->data.animated_sprite_boolean_set.enabled = enabled;
                return editor_result_value(true);
            }
        }
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "invalid animated-sprite command");
    }
    *document_path = arguments[3];
    memset(command, 0, sizeof(*command));
    if(strcmp(domain, "navigation") == 0 && strcmp(action, "set") == 0) {
        EditorNavigationState *navigation = &command->data.navigation;
        if(count != 17 || !editor_command_named_uint_parse(arguments[4],
                    editor_command_navigation_modes,
                    sizeof(editor_command_navigation_modes) /
                        sizeof(editor_command_navigation_modes[0]), &navigation->mode) ||
                !editor_command_named_uint_parse(arguments[5],
                    editor_command_navigation_selections,
                    sizeof(editor_command_navigation_selections) /
                        sizeof(editor_command_navigation_selections[0]),
                    &navigation->selection) ||
                !editor_command_uint_parse(arguments[6], &navigation->object) ||
                !editor_command_uint_parse(arguments[7], &navigation->rigid_body) ||
                !editor_command_uint_parse(arguments[8], &navigation->hitbox) ||
                !editor_command_uint_parse(arguments[9], &navigation->joint) ||
                !editor_command_uint_parse(arguments[10], &navigation->anchor) ||
                !editor_command_uint_parse(arguments[11], &navigation->soft_body) ||
                !editor_command_uint_parse(arguments[12], &navigation->soft_node) ||
                !editor_command_uint_parse(arguments[13], &navigation->soft_beam) ||
                !editor_command_uint_parse(arguments[14], &navigation->selected_line) ||
                !editor_command_uint_parse(arguments[15], &navigation->selected_vertex) ||
                !editor_command_named_uint_parse(arguments[16],
                    editor_command_origin_kinds,
                    sizeof(editor_command_origin_kinds) /
                        sizeof(editor_command_origin_kinds[0]),
                    &navigation->origin_kind))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "invalid navigation set command");
        command->type = EDITOR_COMMAND_NAVIGATION_SET;
        return editor_result_value(true);
    }
    if(strcmp(domain, "collision-mask") == 0 && strcmp(action, "add") == 0) {
        if(count != 5 || arguments[4][0] == '\0')
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "invalid collision-mask add command");
        command->type = EDITOR_COMMAND_COLLISION_MASK_ADD;
        snprintf(command->data.collision_mask_add.name,
            sizeof(command->data.collision_mask_add.name), "%s", arguments[4]);
        return editor_result_value(true);
    }
    if(strcmp(action, "filter") == 0) {
        EditorCollisionFilterSetCommand *set = &command->data.collision_filter_set;
        int filter_index;
        int mask_index;
        int enabled_index;
        if(strcmp(domain, "rigid-body") == 0 && count == 9) {
            set->kind = EDITOR_ITEM_RIGID_BODY;
            if(!editor_command_uint_parse(arguments[4], &set->object) ||
                    !editor_command_uint_parse(arguments[5], &set->item))
                goto collision_filter_invalid;
            filter_index = 6;
        } else if(strcmp(domain, "soft-node") == 0 && count == 10) {
            set->kind = EDITOR_ITEM_SOFT_NODE;
            if(!editor_command_uint_parse(arguments[4], &set->object) ||
                    !editor_command_uint_parse(arguments[5], &set->parent) ||
                    !editor_command_uint_parse(arguments[6], &set->item))
                goto collision_filter_invalid;
            filter_index = 7;
        } else goto collision_filter_invalid;
        mask_index = filter_index + 1;
        enabled_index = filter_index + 2;
        if(strcmp(arguments[filter_index], "category") == 0)
            set->filter = EDITOR_COLLISION_FILTER_CATEGORY;
        else if(strcmp(arguments[filter_index], "collide-with") == 0)
            set->filter = EDITOR_COLLISION_FILTER_COLLIDE_WITH;
        else goto collision_filter_invalid;
        if(arguments[mask_index][0] == '\0' ||
                !editor_command_bool_parse(arguments[enabled_index], &set->enabled))
            goto collision_filter_invalid;
        snprintf(set->mask, sizeof(set->mask), "%s", arguments[mask_index]);
        command->type = EDITOR_COMMAND_COLLISION_FILTER_SET;
        return editor_result_value(true);
collision_filter_invalid:
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "invalid %s collision-filter command", domain);
    }
    if(strcmp(action, "connect") == 0) {
        EditorRelationshipSetCommand *set = &command->data.relationship_set;
        if(strcmp(domain, "joint") == 0 && count == 8 &&
                editor_command_uint_parse(arguments[4], &set->object) &&
                editor_command_uint_parse(arguments[5], &set->item) &&
                (strcmp(arguments[6], "anchor-a") == 0 ||
                    strcmp(arguments[6], "anchor-b") == 0) &&
                editor_command_optional_id_parse(arguments[7], &set->target)) {
            set->kind = EDITOR_RELATIONSHIP_JOINT_ANCHOR;
            set->endpoint = strcmp(arguments[6], "anchor-b") == 0;
        } else if(strcmp(domain, "anchor") == 0 && count == 8 &&
                editor_command_uint_parse(arguments[4], &set->object) &&
                editor_command_uint_parse(arguments[5], &set->item) &&
                strcmp(arguments[6], "rigid-body") == 0 &&
                editor_command_optional_id_parse(arguments[7], &set->target)) {
            set->kind = EDITOR_RELATIONSHIP_ANCHOR_RIGID_BODY;
        } else if(strcmp(domain, "soft-beam") == 0 && count == 9 &&
                editor_command_uint_parse(arguments[4], &set->object) &&
                editor_command_uint_parse(arguments[5], &set->parent) &&
                editor_command_uint_parse(arguments[6], &set->item) &&
                (strcmp(arguments[7], "node-a") == 0 ||
                    strcmp(arguments[7], "node-b") == 0) &&
                editor_command_optional_id_parse(arguments[8], &set->target)) {
            set->kind = EDITOR_RELATIONSHIP_SOFT_BEAM_NODE;
            set->endpoint = strcmp(arguments[7], "node-b") == 0;
        } else {
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "invalid %s relationship command", domain);
        }
        command->type = EDITOR_COMMAND_RELATIONSHIP_SET;
        return editor_result_value(true);
    }
    if(strcmp(action, "set") == 0) {
        EditorPropertySetCommand *set = &command->data.property_set;
        bool nested;
        bool indexed;
        int property_index;
        int value_index;
        if(!editor_command_item_kind_parse(domain, &set->kind)) goto property_parse_invalid;
        nested = set->kind == EDITOR_ITEM_SOFT_NODE ||
            set->kind == EDITOR_ITEM_SOFT_BEAM || set->kind == EDITOR_ITEM_SOFT_AREA;
        indexed = set->kind == EDITOR_ITEM_VERTEX || set->kind == EDITOR_ITEM_LINE;
        if((nested && count != 9) || (indexed && count != 10) ||
                (!nested && !indexed && count != 8) ||
                !editor_command_uint_parse(arguments[4], &set->object))
            goto property_parse_invalid;
        if(nested || indexed) {
            if(!editor_command_uint_parse(arguments[5], &set->parent) ||
                    !editor_command_uint_parse(arguments[6], &set->item))
                goto property_parse_invalid;
        } else if(!editor_command_uint_parse(arguments[5], &set->item)) {
            goto property_parse_invalid;
        }
        if(indexed && !editor_command_uint_parse(arguments[7], &set->index))
            goto property_parse_invalid;
        property_index = indexed ? 8 : nested ? 7 : 6;
        value_index = property_index + 1;
        if(!editor_command_property_parse(arguments[property_index],
                &set->property, &set->value_kind)) goto property_parse_invalid;
        if(set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT) {
            if(!editor_command_float_parse(arguments[value_index], &set->value.number))
                goto property_parse_invalid;
        } else if(set->value_kind == EDITOR_PROPERTY_VALUE_BOOL) {
            if(!editor_command_bool_parse(arguments[value_index], &set->value.boolean))
                goto property_parse_invalid;
        } else {
            if(editor_command_property_color_check(set->property)) {
                if(!editor_command_color_parse(arguments[value_index],
                        &set->value.integer)) goto property_parse_invalid;
            } else {
                const char *kinds[] = {"revolute", "weld", "spring"};
                if(!editor_command_named_uint_parse(arguments[value_index], kinds, 3,
                        &set->value.integer)) goto property_parse_invalid;
            }
        }
        command->type = EDITOR_COMMAND_PROPERTY_SET;
        return editor_result_value(true);
property_parse_invalid:
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "invalid %s property command", domain);
    }
    if(strcmp(domain, "object") != 0 && (strcmp(action, "add") == 0 ||
            strcmp(action, "delete") == 0 || strcmp(action, "rename") == 0)) {
        EditorItemKind kind;
        if(!editor_command_item_kind_parse(domain, &kind)) goto item_invalid;
        if(strcmp(action, "add") == 0) {
            command->type = EDITOR_COMMAND_ITEM_ADD;
            command->data.item_add.kind = kind;
            if(kind == EDITOR_ITEM_RIGID_BODY || kind == EDITOR_ITEM_SOFT_BODY) {
                if(count == 5 && editor_command_uint_parse(arguments[4],
                        &command->data.item_add.object)) return editor_result_value(true);
            } else if(kind == EDITOR_ITEM_HITBOX) {
                if(count == 6 && editor_command_uint_parse(arguments[4],
                            &command->data.item_add.object) &&
                        editor_command_uint_parse(arguments[5],
                            &command->data.item_add.parent)) return editor_result_value(true);
            } else if(kind == EDITOR_ITEM_JOINT) {
                const char *kinds[] = {"revolute", "weld", "spring"};
                if(count == 6 && editor_command_uint_parse(arguments[4],
                            &command->data.item_add.object) &&
                        editor_command_named_uint_parse(arguments[5], kinds, 3,
                            &command->data.item_add.option)) return editor_result_value(true);
            } else if(kind == EDITOR_ITEM_ANCHOR) {
                if(count == 8 && editor_command_uint_parse(arguments[4],
                            &command->data.item_add.object) &&
                        editor_command_uint_parse(arguments[5],
                            &command->data.item_add.parent) &&
                        editor_command_float_parse(arguments[6],
                            &command->data.item_add.position.x) &&
                        editor_command_float_parse(arguments[7],
                            &command->data.item_add.position.y)) return editor_result_value(true);
            } else if(kind == EDITOR_ITEM_SOFT_NODE) {
                if(count == 8 && editor_command_uint_parse(arguments[4],
                            &command->data.item_add.object) &&
                        editor_command_uint_parse(arguments[5],
                            &command->data.item_add.parent) &&
                        editor_command_float_parse(arguments[6],
                            &command->data.item_add.position.x) &&
                        editor_command_float_parse(arguments[7],
                            &command->data.item_add.position.y)) return editor_result_value(true);
            } else if(kind == EDITOR_ITEM_SOFT_BEAM) {
                if(count == 8 && editor_command_uint_parse(arguments[4],
                            &command->data.item_add.object) &&
                        editor_command_uint_parse(arguments[5],
                            &command->data.item_add.parent) &&
                        editor_command_uint_parse(arguments[6],
                            &command->data.item_add.first) &&
                        editor_command_uint_parse(arguments[7],
                            &command->data.item_add.second)) return editor_result_value(true);
            } else if(kind == EDITOR_ITEM_VERTEX) {
                if(count == 8 && editor_command_uint_parse(arguments[4],
                            &command->data.item_add.object) &&
                        editor_command_uint_parse(arguments[5],
                            &command->data.item_add.parent) &&
                        editor_command_uint_parse(arguments[6],
                            &command->data.item_add.first) &&
                        editor_command_uint_parse(arguments[7],
                            &command->data.item_add.index)) return editor_result_value(true);
            }
        } else {
            bool rename = strcmp(action, "rename") == 0;
            int base_count = kind == EDITOR_ITEM_HITBOX || kind == EDITOR_ITEM_SOFT_NODE ||
                    kind == EDITOR_ITEM_SOFT_BEAM ? 7 :
                kind == EDITOR_ITEM_VERTEX || kind == EDITOR_ITEM_LINE ? 8 : 6;
            if(count == base_count + (rename ? 1 : 0)) {
                uint32_t object;
                uint32_t parent = 0;
                uint32_t item;
                uint32_t index = 0;
                bool nested = kind == EDITOR_ITEM_HITBOX ||
                    kind == EDITOR_ITEM_SOFT_NODE || kind == EDITOR_ITEM_SOFT_BEAM;
                bool indexed = kind == EDITOR_ITEM_VERTEX || kind == EDITOR_ITEM_LINE;
                if(editor_command_uint_parse(arguments[4], &object) &&
                        (!nested && !indexed || editor_command_uint_parse(arguments[5],
                            &parent)) &&
                        editor_command_uint_parse(arguments[nested || indexed ? 6 : 5],
                            &item) &&
                        (!indexed || editor_command_uint_parse(arguments[7], &index))) {
                    if(rename) {
                        command->type = EDITOR_COMMAND_ITEM_RENAME;
                        command->data.item_rename.kind = kind;
                        command->data.item_rename.object = object;
                        command->data.item_rename.parent = parent;
                        command->data.item_rename.item = item;
                        command->data.item_rename.index = index;
                        snprintf(command->data.item_rename.name,
                            sizeof(command->data.item_rename.name), "%s",
                            arguments[base_count]);
                    } else {
                        command->type = EDITOR_COMMAND_ITEM_REMOVE;
                        command->data.item_remove.kind = kind;
                        command->data.item_remove.object = object;
                        command->data.item_remove.parent = parent;
                        command->data.item_remove.item = item;
                        command->data.item_remove.index = index;
                    }
                    return editor_result_value(true);
                }
            }
        }
item_invalid:
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "invalid %s %s command", domain, action);
    }
    if(strcmp(action, "visibility") == 0) {
        EditorVisibilityKind kind;
        bool has_parent = false;
        if(strcmp(domain, "sprite") == 0 && count == 7 &&
                editor_command_uint_parse(arguments[4],
                    &command->data.sprite_visibility_set.object) &&
                editor_command_uint_parse(arguments[5],
                    &command->data.sprite_visibility_set.sprite) &&
                editor_command_bool_parse(arguments[6],
                    &command->data.sprite_visibility_set.visible)) {
            command->type = EDITOR_COMMAND_SPRITE_VISIBILITY_SET;
            return editor_result_value(true);
        }
        if(strcmp(domain, "animated-sprite") == 0 && count == 7 &&
                editor_command_uint_parse(arguments[4],
                    &command->data.animated_sprite_boolean_set.object) &&
                editor_command_uint_parse(arguments[5],
                    &command->data.animated_sprite_boolean_set.sprite) &&
                editor_command_bool_parse(arguments[6],
                    &command->data.animated_sprite_boolean_set.enabled)) {
            command->type = EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET;
            return editor_result_value(true);
        }
        if(strcmp(domain, "object") == 0) kind = EDITOR_VISIBILITY_OBJECT;
        else if(strcmp(domain, "rigid-body") == 0) kind = EDITOR_VISIBILITY_RIGID_BODY;
        else if(strcmp(domain, "hitbox") == 0) {
            kind = EDITOR_VISIBILITY_HITBOX;
            has_parent = true;
        } else if(strcmp(domain, "joint") == 0) kind = EDITOR_VISIBILITY_JOINT;
        else if(strcmp(domain, "anchor") == 0) kind = EDITOR_VISIBILITY_ANCHOR;
        else if(strcmp(domain, "soft-body") == 0) kind = EDITOR_VISIBILITY_SOFT_BODY;
        else if(strcmp(domain, "soft-node") == 0) {
            kind = EDITOR_VISIBILITY_SOFT_NODE;
            has_parent = true;
        } else if(strcmp(domain, "soft-beam") == 0) {
            kind = EDITOR_VISIBILITY_SOFT_BEAM;
            has_parent = true;
        } else if(strcmp(domain, "soft-area") == 0) {
            kind = EDITOR_VISIBILITY_SOFT_AREA;
            has_parent = true;
        } else goto visibility_invalid;
        if(kind == EDITOR_VISIBILITY_OBJECT && count == 6) {
            command->type = EDITOR_COMMAND_VISIBILITY;
            command->data.visibility.kind = kind;
            if(editor_command_uint_parse(arguments[4], &command->data.visibility.object) &&
                    editor_command_bool_parse(arguments[5],
                        &command->data.visibility.visible)) return editor_result_value(true);
        } else if((has_parent && count == 8) || (!has_parent && count == 7)) {
            command->type = EDITOR_COMMAND_VISIBILITY;
            command->data.visibility.kind = kind;
            if(editor_command_uint_parse(arguments[4], &command->data.visibility.object) &&
                    (!has_parent || editor_command_uint_parse(arguments[5],
                        &command->data.visibility.parent)) &&
                    editor_command_uint_parse(arguments[has_parent ? 6 : 5],
                        &command->data.visibility.item) &&
                    editor_command_bool_parse(arguments[has_parent ? 7 : 6],
                        &command->data.visibility.visible)) {
                return editor_result_value(true);
            }
        }
visibility_invalid:
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "invalid %s visibility command", domain);
    }
    if(strcmp(domain, "object") == 0 && strcmp(action, "add") == 0) {
        if(count != 5 && count != 7)
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "object add expects <file> <name> [x y]");
        command->type = EDITOR_COMMAND_OBJECT_ADD;
        snprintf(command->data.object_add.name,
            sizeof(command->data.object_add.name), "%s", arguments[4]);
        if(count == 7 && (!editor_command_float_parse(arguments[5],
                    &command->data.object_add.position.x) ||
                !editor_command_float_parse(arguments[6],
                    &command->data.object_add.position.y)))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "object position must contain valid numbers");
        return editor_result_value(true);
    }
    if(strcmp(domain, "object") == 0 && strcmp(action, "rename") == 0) {
        if(count != 6 || !editor_command_uint_parse(arguments[4],
                &command->data.object_rename.object))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "object rename expects <file> <id> <name>");
        command->type = EDITOR_COMMAND_OBJECT_RENAME;
        snprintf(command->data.object_rename.name,
            sizeof(command->data.object_rename.name), "%s", arguments[5]);
        return editor_result_value(true);
    }
    if(strcmp(domain, "object") == 0 && strcmp(action, "delete") == 0) {
        if(count != 5 || !editor_command_uint_parse(arguments[4],
                &command->data.object_remove.object))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "object delete expects <file> <id>");
        command->type = EDITOR_COMMAND_OBJECT_REMOVE;
        return editor_result_value(true);
    }
    if(strcmp(domain, "object") == 0 && strcmp(action, "position") == 0 &&
            count == 7 && editor_command_uint_parse(arguments[4],
                &command->data.object_position.object) &&
            editor_command_float_parse(arguments[5],
                &command->data.object_position.position.x) &&
            editor_command_float_parse(arguments[6],
                &command->data.object_position.position.y)) {
        command->type = EDITOR_COMMAND_OBJECT_POSITION;
        return editor_result_value(true);
    }
    if(strcmp(domain, "rigid-body") == 0 && strcmp(action, "transform") == 0 &&
            count == 9 && editor_command_uint_parse(arguments[4],
                &command->data.rigid_body_transform.object) &&
            editor_command_uint_parse(arguments[5],
                &command->data.rigid_body_transform.body) &&
            editor_command_float_parse(arguments[6],
                &command->data.rigid_body_transform.position.x) &&
            editor_command_float_parse(arguments[7],
                &command->data.rigid_body_transform.position.y) &&
            editor_command_float_parse(arguments[8],
                &command->data.rigid_body_transform.rotation)) {
        command->type = EDITOR_COMMAND_RIGID_BODY_TRANSFORM;
        return editor_result_value(true);
    }
    if(strcmp(domain, "vertex") == 0 && strcmp(action, "position") == 0 &&
            count == 10 && editor_command_uint_parse(arguments[4],
                &command->data.vertex_position.object) &&
            editor_command_uint_parse(arguments[5],
                &command->data.vertex_position.body) &&
            editor_command_uint_parse(arguments[6],
                &command->data.vertex_position.hitbox) &&
            editor_command_uint_parse(arguments[7],
                &command->data.vertex_position.vertex) &&
            editor_command_float_parse(arguments[8],
                &command->data.vertex_position.position.x) &&
            editor_command_float_parse(arguments[9],
                &command->data.vertex_position.position.y)) {
        command->type = EDITOR_COMMAND_VERTEX_POSITION;
        return editor_result_value(true);
    }
    if(strcmp(domain, "anchor") == 0 && strcmp(action, "transform") == 0 &&
            count == 9 && editor_command_uint_parse(arguments[4],
                &command->data.anchor_transform.object) &&
            editor_command_uint_parse(arguments[5],
                &command->data.anchor_transform.anchor) &&
            editor_command_float_parse(arguments[6],
                &command->data.anchor_transform.position.x) &&
            editor_command_float_parse(arguments[7],
                &command->data.anchor_transform.position.y) &&
            editor_command_float_parse(arguments[8],
                &command->data.anchor_transform.rotation)) {
        command->type = EDITOR_COMMAND_ANCHOR_TRANSFORM;
        return editor_result_value(true);
    }
    if(strcmp(domain, "soft-body") == 0 && strcmp(action, "transform") == 0 &&
            count == 9 && editor_command_uint_parse(arguments[4],
                &command->data.soft_body_transform.object) &&
            editor_command_uint_parse(arguments[5],
                &command->data.soft_body_transform.body) &&
            editor_command_float_parse(arguments[6],
                &command->data.soft_body_transform.position.x) &&
            editor_command_float_parse(arguments[7],
                &command->data.soft_body_transform.position.y) &&
            editor_command_float_parse(arguments[8],
                &command->data.soft_body_transform.rotation)) {
        command->type = EDITOR_COMMAND_SOFT_BODY_TRANSFORM;
        return editor_result_value(true);
    }
    if(strcmp(domain, "soft-node") == 0 && strcmp(action, "position") == 0 &&
            count == 9 && editor_command_uint_parse(arguments[4],
                &command->data.soft_node_position.object) &&
            editor_command_uint_parse(arguments[5],
                &command->data.soft_node_position.body) &&
            editor_command_uint_parse(arguments[6],
                &command->data.soft_node_position.node) &&
            editor_command_float_parse(arguments[7],
                &command->data.soft_node_position.position.x) &&
            editor_command_float_parse(arguments[8],
                &command->data.soft_node_position.position.y)) {
        command->type = EDITOR_COMMAND_SOFT_NODE_POSITION;
        return editor_result_value(true);
    }
    if((strcmp(domain, "hitbox") == 0 || strcmp(domain, "soft-body") == 0) &&
            strcmp(action, "auto-shape") == 0) {
        bool hitbox = strcmp(domain, "hitbox") == 0;
        int base = hitbox ? 7 : 6;
        if(count < base + 6 ||
                !editor_command_uint_parse(arguments[4],
                    &command->data.auto_shape.object) ||
                !editor_command_uint_parse(arguments[5],
                    &command->data.auto_shape.parent) ||
                (hitbox && !editor_command_uint_parse(arguments[6],
                    &command->data.auto_shape.item)) ||
                !editor_auto_shape_kind_parse(arguments[base],
                    &command->data.auto_shape.config.kind) ||
                !editor_auto_triangle_kind_parse(arguments[base + 1],
                    &command->data.auto_shape.config.triangle_kind) ||
                !editor_command_float_parse(arguments[base + 2],
                    &command->data.auto_shape.config.width) ||
                !editor_command_float_parse(arguments[base + 3],
                    &command->data.auto_shape.config.height) ||
                !editor_command_float_parse(arguments[base + 4],
                    &command->data.auto_shape.config.radius) ||
                !editor_command_float_parse(arguments[base + 5],
                    &command->data.auto_shape.config.apex_offset))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "invalid %s auto-shape command", domain);
        if(count > base + 6) {
            if(strcmp(arguments[base + 6], "points") != 0 ||
                    count == base + 7 ||
                    (size_t)(count - base - 7) > EDITOR_SOFT_NODE_MAX)
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "invalid %s auto-shape point selection", domain);
            for(int i = base + 7; i < count; i += 1) {
                uint32_t point;
                if(!editor_command_uint_parse(arguments[i], &point))
                    return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                        "auto-shape point identifiers must be integers");
                command->data.auto_shape.points[
                    command->data.auto_shape.point_count++] = point;
            }
        }
        command->type = EDITOR_COMMAND_AUTO_SHAPE;
        command->data.auto_shape.kind = hitbox ? EDITOR_ITEM_HITBOX :
            EDITOR_ITEM_SOFT_BODY;
        if(hitbox) {
            uint32_t body = command->data.auto_shape.parent;
            command->data.auto_shape.parent = body;
        } else {
            command->data.auto_shape.item = command->data.auto_shape.parent;
            command->data.auto_shape.parent = 0;
        }
        return editor_result_value(true);
    }
    if((strcmp(domain, "rigid-body") == 0 || strcmp(domain, "soft-body") == 0) &&
            strcmp(action, "origin") == 0 && count == 8 &&
            editor_command_uint_parse(arguments[4], &command->data.origin.object) &&
            editor_command_uint_parse(arguments[5], &command->data.origin.body) &&
            editor_command_float_parse(arguments[6], &command->data.origin.position.x) &&
            editor_command_float_parse(arguments[7], &command->data.origin.position.y)) {
        command->type = strcmp(domain, "rigid-body") == 0 ?
            EDITOR_COMMAND_RIGID_BODY_ORIGIN : EDITOR_COMMAND_SOFT_BODY_ORIGIN;
        return editor_result_value(true);
    }
    if(strcmp(domain, "viewport") == 0 && strcmp(action, "camera") == 0 &&
            count == 7 && editor_command_float_parse(arguments[4],
                &command->data.viewport_camera.offset.x) &&
            editor_command_float_parse(arguments[5],
                &command->data.viewport_camera.offset.y) &&
            editor_command_float_parse(arguments[6],
                &command->data.viewport_camera.zoom)) {
        command->type = EDITOR_COMMAND_VIEWPORT_CAMERA;
        return editor_result_value(true);
    }
    if(strcmp(domain, "viewport") == 0 && strcmp(action, "coordinates") == 0 &&
            count == 5 && (strcmp(arguments[4], "local") == 0 ||
                strcmp(arguments[4], "world") == 0)) {
        command->type = EDITOR_COMMAND_VIEWPORT_COORDINATES;
        command->data.viewport_coordinates.local = strcmp(arguments[4], "local") == 0;
        return editor_result_value(true);
    }
    return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "invalid or unknown %s %s command", domain, action);
}

static bool editor_command_text_append(char *output, size_t capacity,
        size_t *used, const char *text) {
    size_t length = strlen(text);
    if(*used >= capacity || length >= capacity - *used) return false;
    memcpy(output + *used, text, length);
    *used += length;
    output[*used] = '\0';
    return true;
}

static bool editor_command_shell_text_append(char *output, size_t capacity,
        size_t *used, const char *text) {
    if(!editor_command_text_append(output, capacity, used, "'")) return false;
    for(const char *at = text; *at != '\0'; at += 1) {
        const char *part = *at == '\'' ? "'\\''" : NULL;
        char character[2] = {*at, '\0'};
        if(!editor_command_text_append(output, capacity, used,
                part != NULL ? part : character)) return false;
    }
    return editor_command_text_append(output, capacity, used, "'");
}

EditorResult editor_command_cli_write(const EditorCommand *command,
        const char *document_path, char *output, size_t output_capacity) {
    const char *domain;
    char values[2048];
    size_t used = 0;
    if(command == NULL || document_path == NULL || output == NULL ||
            output_capacity == 0)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "command serialization requires a command, path, and output buffer");
    output[0] = '\0';
    if(command->type >= EDITOR_COMMAND_SPRITE_ADD &&
            command->type <= EDITOR_COMMAND_SPRITE_SIZE_SET) {
        if(!editor_command_text_append(output, output_capacity, &used,
                "editor-cli sprite ")) goto capacity_error;
    } else if(command->type >= EDITOR_COMMAND_ANIMATED_SPRITE_ADD &&
            command->type <= EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET) {
        if(!editor_command_text_append(output, output_capacity, &used,
                "editor-cli animated-sprite ")) goto capacity_error;
    }
    switch(command->type) {
        case EDITOR_COMMAND_SPRITE_ADD:
            if(!editor_command_text_append(output, output_capacity, &used, "add ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u ", command->data.sprite_add.object);
            if(!editor_command_text_append(output, output_capacity, &used, values) ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        command->data.sprite_add.name) ||
                    !editor_command_text_append(output, output_capacity, &used, " ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        command->data.sprite_add.path)) goto capacity_error;
            snprintf(values, sizeof(values), " %.9g %.9g",
                command->data.sprite_add.size.x, command->data.sprite_add.size.y);
            if(!editor_command_text_append(output, output_capacity, &used, values))
                goto capacity_error;
            return editor_result_value(true);
        case EDITOR_COMMAND_SPRITE_REMOVE:
        case EDITOR_COMMAND_SPRITE_RENAME:
        case EDITOR_COMMAND_SPRITE_PATH_SET:
        case EDITOR_COMMAND_SPRITE_SIZE_SET: {
            EditorSpriteId id = command->type == EDITOR_COMMAND_SPRITE_REMOVE ?
                command->data.sprite_remove.sprite :
                command->type == EDITOR_COMMAND_SPRITE_RENAME ?
                    command->data.sprite_rename.sprite :
                command->type == EDITOR_COMMAND_SPRITE_PATH_SET ?
                    command->data.sprite_path_set.sprite :
                    command->data.sprite_size_set.sprite;
            const char *action = command->type == EDITOR_COMMAND_SPRITE_REMOVE ?
                "delete" : command->type == EDITOR_COMMAND_SPRITE_RENAME ? "rename" : "set";
            EditorObjectId object = command->type == EDITOR_COMMAND_SPRITE_REMOVE ?
                command->data.sprite_remove.object :
                command->type == EDITOR_COMMAND_SPRITE_RENAME ?
                    command->data.sprite_rename.object :
                command->type == EDITOR_COMMAND_SPRITE_PATH_SET ?
                    command->data.sprite_path_set.object :
                    command->data.sprite_size_set.object;
            snprintf(values, sizeof(values), "%s ", action);
            if(!editor_command_text_append(output, output_capacity, &used, values) ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u %u", object, id);
            if(!editor_command_text_append(output, output_capacity, &used, values))
                goto capacity_error;
            if(command->type == EDITOR_COMMAND_SPRITE_RENAME) {
                if(!editor_command_text_append(output, output_capacity, &used, " ") ||
                        !editor_command_shell_text_append(output, output_capacity, &used,
                            command->data.sprite_rename.name)) goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_SPRITE_PATH_SET) {
                if(!editor_command_text_append(output, output_capacity, &used, " path ") ||
                        !editor_command_shell_text_append(output, output_capacity, &used,
                            command->data.sprite_path_set.path)) goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_SPRITE_SIZE_SET) {
                snprintf(values, sizeof(values), " size %.9g %.9g",
                    command->data.sprite_size_set.size.x,
                    command->data.sprite_size_set.size.y);
                if(!editor_command_text_append(output, output_capacity, &used, values))
                    goto capacity_error;
            }
            return editor_result_value(true);
        }
        case EDITOR_COMMAND_ANIMATED_SPRITE_ADD:
            snprintf(values, sizeof(values), "add ");
            if(!editor_command_text_append(output, output_capacity, &used, values) ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u ",
                command->data.animated_sprite_add.object);
            if(!editor_command_text_append(output, output_capacity, &used, values) ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        command->data.animated_sprite_add.name)) goto capacity_error;
            return editor_result_value(true);
        case EDITOR_COMMAND_ANIMATED_SPRITE_REMOVE:
        case EDITOR_COMMAND_ANIMATED_SPRITE_RENAME:
        case EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_SCALE_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_TIMING_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_STARTING_FRAME_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_DIRECTION_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_FOLLOW_ROTATION_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_PLAYING_SET:
        case EDITOR_COMMAND_ANIMATION_FRAME_ADD:
        case EDITOR_COMMAND_ANIMATION_FRAME_REMOVE:
        case EDITOR_COMMAND_ANIMATION_FRAME_RENAME:
        case EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET:
        case EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET: {
            EditorObjectId object = command->data.animated_sprite_remove.object;
            EditorAnimatedSpriteId id = command->data.animated_sprite_remove.sprite;
            const char *action = command->type == EDITOR_COMMAND_ANIMATED_SPRITE_REMOVE ?
                "delete" : command->type == EDITOR_COMMAND_ANIMATED_SPRITE_RENAME ?
                "rename" : command->type == EDITOR_COMMAND_ANIMATION_FRAME_ADD ?
                "frame-add" : command->type == EDITOR_COMMAND_ANIMATION_FRAME_REMOVE ?
                "frame-delete" : command->type == EDITOR_COMMAND_ANIMATION_FRAME_RENAME ?
                "frame-rename" : command->type == EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET ?
                "frame-path-set" : command->type == EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET ?
                "frame-size-set" : command->type == EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET ?
                "connect" : "set";
            snprintf(values, sizeof(values), "%s ", action);
            if(!editor_command_text_append(output, output_capacity, &used, values) ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u %u", object, id);
            if(!editor_command_text_append(output, output_capacity, &used, values))
                goto capacity_error;
            if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_RENAME) {
                if(!editor_command_text_append(output, output_capacity, &used, " ") ||
                        !editor_command_shell_text_append(output, output_capacity, &used,
                            command->data.animated_sprite_rename.name)) goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET) {
                snprintf(values, sizeof(values), " body %u",
                    command->data.animated_sprite_body_set.body);
                if(!editor_command_text_append(output, output_capacity, &used, values)) goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_SCALE_SET) {
                snprintf(values, sizeof(values), " scale %.9g %.9g",
                    command->data.animated_sprite_scale_set.scale.x,
                    command->data.animated_sprite_scale_set.scale.y);
                if(!editor_command_text_append(output, output_capacity, &used, values)) goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_TIMING_SET) {
                snprintf(values, sizeof(values), " timing %llu %.17g",
                    (unsigned long long)command->data.animated_sprite_timing_set.ticks,
                    (double)command->data.animated_sprite_timing_set.time);
                if(!editor_command_text_append(output, output_capacity, &used, values)) goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_STARTING_FRAME_SET) {
                snprintf(values, sizeof(values), " starting-frame %u",
                    command->data.animated_sprite_starting_frame_set.frame);
                if(!editor_command_text_append(output, output_capacity, &used, values)) goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_DIRECTION_SET) {
                snprintf(values, sizeof(values), " direction %s",
                    command->data.animated_sprite_direction_set.direction == DIRECTION_LEFT ?
                        "left" : "right");
                if(!editor_command_text_append(output, output_capacity, &used, values)) goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_FOLLOW_ROTATION_SET ||
                    command->type == EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET ||
                    command->type == EDITOR_COMMAND_ANIMATED_SPRITE_PLAYING_SET) {
                snprintf(values, sizeof(values), " %s %s",
                    command->type == EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET ?
                        "visibility" : command->type ==
                            EDITOR_COMMAND_ANIMATED_SPRITE_PLAYING_SET ?
                            "playing" : "follow-body-rotation",
                    command->data.animated_sprite_boolean_set.enabled ? "true" : "false");
                if(!editor_command_text_append(output, output_capacity, &used, values)) goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_ADD) {
                if(!editor_command_text_append(output, output_capacity, &used, " ") ||
                        !editor_command_shell_text_append(output, output_capacity, &used,
                            command->data.animation_frame_add.name) ||
                        !editor_command_text_append(output, output_capacity, &used, " ") ||
                        !editor_command_shell_text_append(output, output_capacity, &used,
                            command->data.animation_frame_add.path)) goto capacity_error;
                snprintf(values, sizeof(values), " %.9g %.9g",
                    command->data.animation_frame_add.size.x,
                    command->data.animation_frame_add.size.y);
                if(!editor_command_text_append(output, output_capacity, &used, values))
                    goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_REMOVE) {
                snprintf(values, sizeof(values), " %zu",
                    command->data.animation_frame_remove.index);
                if(!editor_command_text_append(output, output_capacity, &used, values)) goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_RENAME) {
                snprintf(values, sizeof(values), " %zu ",
                    command->data.animation_frame_rename.index);
                if(!editor_command_text_append(output, output_capacity, &used, values) ||
                        !editor_command_shell_text_append(output, output_capacity, &used,
                            command->data.animation_frame_rename.name)) goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET) {
                snprintf(values, sizeof(values), " %zu ",
                    command->data.animation_frame_path_set.index);
                if(!editor_command_text_append(output, output_capacity, &used, values) ||
                        !editor_command_shell_text_append(output, output_capacity, &used,
                            command->data.animation_frame_path_set.path)) goto capacity_error;
            } else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET) {
                snprintf(values, sizeof(values), " %zu %.9g %.9g",
                    command->data.animation_frame_size_set.index,
                    command->data.animation_frame_size_set.size.x,
                    command->data.animation_frame_size_set.size.y);
                if(!editor_command_text_append(output, output_capacity, &used, values))
                    goto capacity_error;
            }
            return editor_result_value(true);
        }
        case EDITOR_COMMAND_OBJECT_ADD:
        case EDITOR_COMMAND_OBJECT_RENAME:
        case EDITOR_COMMAND_OBJECT_REMOVE:
        case EDITOR_COMMAND_OBJECT_POSITION: domain = "object"; break;
        case EDITOR_COMMAND_RIGID_BODY_TRANSFORM:
        case EDITOR_COMMAND_RIGID_BODY_ORIGIN: domain = "rigid-body"; break;
        case EDITOR_COMMAND_VERTEX_POSITION: domain = "vertex"; break;
        case EDITOR_COMMAND_ANCHOR_TRANSFORM: domain = "anchor"; break;
        case EDITOR_COMMAND_SOFT_BODY_TRANSFORM:
        case EDITOR_COMMAND_SOFT_BODY_ORIGIN: domain = "soft-body"; break;
        case EDITOR_COMMAND_SOFT_NODE_POSITION: domain = "soft-node"; break;
        case EDITOR_COMMAND_AUTO_SHAPE:
            domain = command->data.auto_shape.kind == EDITOR_ITEM_HITBOX ?
                "hitbox" : "soft-body";
            break;
        case EDITOR_COMMAND_VIEWPORT_CAMERA:
        case EDITOR_COMMAND_VIEWPORT_COORDINATES: domain = "viewport"; break;
        case EDITOR_COMMAND_VISIBILITY:
            switch(command->data.visibility.kind) {
                case EDITOR_VISIBILITY_OBJECT: domain = "object"; break;
                case EDITOR_VISIBILITY_RIGID_BODY: domain = "rigid-body"; break;
                case EDITOR_VISIBILITY_HITBOX: domain = "hitbox"; break;
                case EDITOR_VISIBILITY_JOINT: domain = "joint"; break;
                case EDITOR_VISIBILITY_ANCHOR: domain = "anchor"; break;
                case EDITOR_VISIBILITY_SOFT_BODY: domain = "soft-body"; break;
                case EDITOR_VISIBILITY_SOFT_NODE: domain = "soft-node"; break;
                case EDITOR_VISIBILITY_SOFT_BEAM: domain = "soft-beam"; break;
                case EDITOR_VISIBILITY_SOFT_AREA: domain = "soft-area"; break;
                default: return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "unknown visibility target");
            }
            break;
        case EDITOR_COMMAND_NAVIGATION_SET: domain = "navigation"; break;
        case EDITOR_COMMAND_ITEM_ADD:
            domain = editor_command_item_domain_get(command->data.item_add.kind);
            break;
        case EDITOR_COMMAND_ITEM_REMOVE:
            domain = editor_command_item_domain_get(command->data.item_remove.kind);
            break;
        case EDITOR_COMMAND_ITEM_RENAME:
            domain = editor_command_item_domain_get(command->data.item_rename.kind);
            break;
        case EDITOR_COMMAND_PROPERTY_SET:
            domain = editor_command_item_domain_get(command->data.property_set.kind);
            break;
        case EDITOR_COMMAND_RELATIONSHIP_SET:
            if(command->data.relationship_set.kind ==
                    EDITOR_RELATIONSHIP_JOINT_ANCHOR) domain = "joint";
            else if(command->data.relationship_set.kind ==
                    EDITOR_RELATIONSHIP_ANCHOR_RIGID_BODY) domain = "anchor";
            else if(command->data.relationship_set.kind ==
                    EDITOR_RELATIONSHIP_SOFT_BEAM_NODE) domain = "soft-beam";
            else return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "unknown relationship target");
            break;
        case EDITOR_COMMAND_COLLISION_MASK_ADD: domain = "collision-mask"; break;
        case EDITOR_COMMAND_COLLISION_FILTER_SET:
            domain = editor_command_item_domain_get(
                command->data.collision_filter_set.kind);
            break;
        default: return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "unknown editor command");
    }
    if(domain == NULL) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "unknown editor item target");
    if(!editor_command_text_append(output, output_capacity, &used, "editor-cli ") ||
            !editor_command_text_append(output, output_capacity, &used, domain) ||
            !editor_command_text_append(output, output_capacity, &used, " "))
        goto capacity_error;
    switch(command->type) {
        case EDITOR_COMMAND_OBJECT_ADD:
            if(!editor_command_text_append(output, output_capacity, &used, "add ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path) ||
                    !editor_command_text_append(output, output_capacity, &used, " ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        command->data.object_add.name)) goto capacity_error;
            snprintf(values, sizeof(values), " %.9g %.9g",
                command->data.object_add.position.x,
                command->data.object_add.position.y);
            if(!editor_command_text_append(output, output_capacity, &used, values))
                goto capacity_error;
            return editor_result_value(true);
        case EDITOR_COMMAND_OBJECT_RENAME:
            snprintf(values, sizeof(values), "rename ");
            if(!editor_command_text_append(output, output_capacity, &used, values) ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u ",
                command->data.object_rename.object);
            if(!editor_command_text_append(output, output_capacity, &used, values) ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        command->data.object_rename.name)) goto capacity_error;
            return editor_result_value(true);
        case EDITOR_COMMAND_OBJECT_REMOVE:
            if(!editor_command_text_append(output, output_capacity, &used, "delete ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u",
                command->data.object_remove.object);
            if(!editor_command_text_append(output, output_capacity, &used, values))
                goto capacity_error;
            return editor_result_value(true);
        case EDITOR_COMMAND_OBJECT_POSITION:
            snprintf(values, sizeof(values), "position ");
            if(!editor_command_text_append(output, output_capacity, &used, values) ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u %.9g %.9g",
                command->data.object_position.object,
                command->data.object_position.position.x,
                command->data.object_position.position.y);
            break;
        case EDITOR_COMMAND_RIGID_BODY_TRANSFORM:
            if(!editor_command_text_append(output, output_capacity, &used, "transform ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u %u %.9g %.9g %.9g",
                command->data.rigid_body_transform.object,
                command->data.rigid_body_transform.body,
                command->data.rigid_body_transform.position.x,
                command->data.rigid_body_transform.position.y,
                command->data.rigid_body_transform.rotation);
            break;
        case EDITOR_COMMAND_VERTEX_POSITION:
            if(!editor_command_text_append(output, output_capacity, &used, "position ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u %u %u %u %.9g %.9g",
                command->data.vertex_position.object,
                command->data.vertex_position.body,
                command->data.vertex_position.hitbox,
                command->data.vertex_position.vertex,
                command->data.vertex_position.position.x,
                command->data.vertex_position.position.y);
            break;
        case EDITOR_COMMAND_ANCHOR_TRANSFORM:
            if(!editor_command_text_append(output, output_capacity, &used, "transform ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u %u %.9g %.9g %.9g",
                command->data.anchor_transform.object,
                command->data.anchor_transform.anchor,
                command->data.anchor_transform.position.x,
                command->data.anchor_transform.position.y,
                command->data.anchor_transform.rotation);
            break;
        case EDITOR_COMMAND_SOFT_BODY_TRANSFORM:
            if(!editor_command_text_append(output, output_capacity, &used, "transform ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u %u %.9g %.9g %.9g",
                command->data.soft_body_transform.object,
                command->data.soft_body_transform.body,
                command->data.soft_body_transform.position.x,
                command->data.soft_body_transform.position.y,
                command->data.soft_body_transform.rotation);
            break;
        case EDITOR_COMMAND_SOFT_NODE_POSITION:
            if(!editor_command_text_append(output, output_capacity, &used, "position ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u %u %u %.9g %.9g",
                command->data.soft_node_position.object,
                command->data.soft_node_position.body,
                command->data.soft_node_position.node,
                command->data.soft_node_position.position.x,
                command->data.soft_node_position.position.y);
            break;
        case EDITOR_COMMAND_AUTO_SHAPE: {
            const char *shape = editor_auto_shape_kind_name_get(
                command->data.auto_shape.config.kind);
            const char *triangle = editor_auto_triangle_kind_name_get(
                command->data.auto_shape.config.triangle_kind);
            if(shape == NULL || triangle == NULL ||
                    !editor_command_text_append(output, output_capacity, &used,
                        "auto-shape ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            if(command->data.auto_shape.kind == EDITOR_ITEM_HITBOX)
                snprintf(values, sizeof(values),
                    " %u %u %u %s %s %.9g %.9g %.9g %.9g",
                    command->data.auto_shape.object,
                    command->data.auto_shape.parent,
                    command->data.auto_shape.item, shape, triangle,
                    command->data.auto_shape.config.width,
                    command->data.auto_shape.config.height,
                    command->data.auto_shape.config.radius,
                    command->data.auto_shape.config.apex_offset);
            else snprintf(values, sizeof(values),
                " %u %u %s %s %.9g %.9g %.9g %.9g",
                command->data.auto_shape.object,
                command->data.auto_shape.item, shape, triangle,
                command->data.auto_shape.config.width,
                command->data.auto_shape.config.height,
                command->data.auto_shape.config.radius,
                command->data.auto_shape.config.apex_offset);
            if(command->data.auto_shape.point_count > 0) {
                size_t value_used = strlen(values);
                int written = snprintf(values + value_used,
                    sizeof(values) - value_used, " points");
                if(written < 0 || (size_t)written >= sizeof(values) - value_used)
                    goto capacity_error;
                value_used += (size_t)written;
                for(size_t i = 0; i < command->data.auto_shape.point_count; i += 1) {
                    written = snprintf(values + value_used,
                        sizeof(values) - value_used, " %u",
                        command->data.auto_shape.points[i]);
                    if(written < 0 ||
                            (size_t)written >= sizeof(values) - value_used)
                        goto capacity_error;
                    value_used += (size_t)written;
                }
            }
            break;
        }
        case EDITOR_COMMAND_RIGID_BODY_ORIGIN:
        case EDITOR_COMMAND_SOFT_BODY_ORIGIN:
            if(!editor_command_text_append(output, output_capacity, &used, "origin ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %u %u %.9g %.9g",
                command->data.origin.object, command->data.origin.body,
                command->data.origin.position.x, command->data.origin.position.y);
            break;
        case EDITOR_COMMAND_VIEWPORT_CAMERA:
            if(!editor_command_text_append(output, output_capacity, &used, "camera ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %.9g %.9g %.9g",
                command->data.viewport_camera.offset.x,
                command->data.viewport_camera.offset.y,
                command->data.viewport_camera.zoom);
            break;
        case EDITOR_COMMAND_VIEWPORT_COORDINATES:
            if(!editor_command_text_append(output, output_capacity, &used,
                    "coordinates ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values), " %s",
                command->data.viewport_coordinates.local ? "local" : "world");
            break;
        case EDITOR_COMMAND_VISIBILITY:
            if(!editor_command_text_append(output, output_capacity, &used,
                    "visibility ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            if(command->data.visibility.kind == EDITOR_VISIBILITY_OBJECT) {
                snprintf(values, sizeof(values), " %u %s",
                    command->data.visibility.object,
                    command->data.visibility.visible ? "true" : "false");
            } else if(command->data.visibility.kind == EDITOR_VISIBILITY_HITBOX ||
                    command->data.visibility.kind == EDITOR_VISIBILITY_SOFT_NODE ||
                    command->data.visibility.kind == EDITOR_VISIBILITY_SOFT_BEAM ||
                    command->data.visibility.kind == EDITOR_VISIBILITY_SOFT_AREA) {
                snprintf(values, sizeof(values), " %u %u %u %s",
                    command->data.visibility.object,
                    command->data.visibility.parent,
                    command->data.visibility.item,
                    command->data.visibility.visible ? "true" : "false");
            } else {
                snprintf(values, sizeof(values), " %u %u %s",
                    command->data.visibility.object,
                    command->data.visibility.item,
                    command->data.visibility.visible ? "true" : "false");
            }
            break;
        case EDITOR_COMMAND_NAVIGATION_SET: {
            const EditorNavigationState *navigation = &command->data.navigation;
            if(navigation->mode >= sizeof(editor_command_navigation_modes) /
                        sizeof(editor_command_navigation_modes[0]) ||
                    navigation->selection >= sizeof(editor_command_navigation_selections) /
                        sizeof(editor_command_navigation_selections[0]) ||
                    navigation->origin_kind >= sizeof(editor_command_origin_kinds) /
                        sizeof(editor_command_origin_kinds[0]) ||
                    !editor_command_text_append(output, output_capacity, &used, "set ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            snprintf(values, sizeof(values),
                " %s %s %u %u %u %u %u %u %u %u %u %u %s",
                editor_command_navigation_modes[navigation->mode],
                editor_command_navigation_selections[navigation->selection],
                navigation->object, navigation->rigid_body, navigation->hitbox,
                navigation->joint, navigation->anchor, navigation->soft_body,
                navigation->soft_node, navigation->soft_beam,
                navigation->selected_line, navigation->selected_vertex,
                editor_command_origin_kinds[navigation->origin_kind]);
            break;
        }
        case EDITOR_COMMAND_ITEM_ADD: {
            const EditorItemAddCommand *item = &command->data.item_add;
            if(!editor_command_text_append(output, output_capacity, &used, "add ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            if(item->kind == EDITOR_ITEM_OBJECT) {
                if(!editor_command_text_append(output, output_capacity, &used, " ") ||
                        !editor_command_shell_text_append(output, output_capacity, &used,
                            item->name)) goto capacity_error;
                snprintf(values, sizeof(values), " %.9g %.9g",
                    item->position.x, item->position.y);
            } else if(item->kind == EDITOR_ITEM_RIGID_BODY ||
                    item->kind == EDITOR_ITEM_SOFT_BODY) {
                snprintf(values, sizeof(values), " %u", item->object);
            } else if(item->kind == EDITOR_ITEM_HITBOX) {
                snprintf(values, sizeof(values), " %u %u", item->object, item->parent);
            } else if(item->kind == EDITOR_ITEM_JOINT) {
                const char *kinds[] = {"revolute", "weld", "spring"};
                if(item->option > (uint32_t)EDITOR_JOINT_SPRING) goto capacity_error;
                snprintf(values, sizeof(values), " %u %s", item->object,
                    kinds[item->option]);
            } else if(item->kind == EDITOR_ITEM_ANCHOR ||
                    item->kind == EDITOR_ITEM_SOFT_NODE) {
                snprintf(values, sizeof(values), " %u %u %.9g %.9g", item->object,
                    item->parent, item->position.x, item->position.y);
            } else if(item->kind == EDITOR_ITEM_SOFT_BEAM) {
                snprintf(values, sizeof(values), " %u %u %u %u", item->object,
                    item->parent, item->first, item->second);
            } else if(item->kind == EDITOR_ITEM_VERTEX) {
                snprintf(values, sizeof(values), " %u %u %u %u", item->object,
                    item->parent, item->first, item->index);
            } else goto capacity_error;
            break;
        }
        case EDITOR_COMMAND_ITEM_REMOVE: {
            const EditorItemRemoveCommand *item = &command->data.item_remove;
            if(!editor_command_text_append(output, output_capacity, &used, "delete ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            if(item->kind == EDITOR_ITEM_OBJECT)
                snprintf(values, sizeof(values), " %u", item->object);
            else if(item->kind == EDITOR_ITEM_HITBOX ||
                    item->kind == EDITOR_ITEM_SOFT_NODE ||
                    item->kind == EDITOR_ITEM_SOFT_BEAM)
                snprintf(values, sizeof(values), " %u %u %u", item->object,
                    item->parent, item->item);
            else if(item->kind == EDITOR_ITEM_VERTEX || item->kind == EDITOR_ITEM_LINE)
                snprintf(values, sizeof(values), " %u %u %u %u", item->object,
                    item->parent, item->item, item->index);
            else snprintf(values, sizeof(values), " %u %u", item->object, item->item);
            break;
        }
        case EDITOR_COMMAND_ITEM_RENAME: {
            const EditorItemRenameCommand *item = &command->data.item_rename;
            if(!editor_command_text_append(output, output_capacity, &used, "rename ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            if(item->kind == EDITOR_ITEM_OBJECT) {
                snprintf(values, sizeof(values), " %u ", item->object);
                if(!editor_command_text_append(output, output_capacity, &used, values) ||
                        !editor_command_shell_text_append(output, output_capacity, &used,
                            item->name)) goto capacity_error;
                return editor_result_value(true);
            } else if(item->kind == EDITOR_ITEM_HITBOX ||
                    item->kind == EDITOR_ITEM_SOFT_NODE ||
                    item->kind == EDITOR_ITEM_SOFT_BEAM)
                snprintf(values, sizeof(values), " %u %u %u %s", item->object,
                    item->parent, item->item, item->name);
            else if(item->kind == EDITOR_ITEM_VERTEX || item->kind == EDITOR_ITEM_LINE)
                snprintf(values, sizeof(values), " %u %u %u %u %s", item->object,
                    item->parent, item->item, item->index, item->name);
            else snprintf(values, sizeof(values), " %u %u %s", item->object,
                item->item, item->name);
            break;
        }
        case EDITOR_COMMAND_PROPERTY_SET: {
            const EditorPropertySetCommand *set = &command->data.property_set;
            const char *property = editor_command_property_name_get(set->property);
            const char *boolean;
            char value[64];
            if(property == NULL ||
                    !editor_command_text_append(output, output_capacity, &used, "set ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            if(set->kind == EDITOR_ITEM_SOFT_NODE || set->kind == EDITOR_ITEM_SOFT_BEAM ||
                    set->kind == EDITOR_ITEM_SOFT_AREA)
                snprintf(values, sizeof(values), " %u %u %u", set->object,
                    set->parent, set->item);
            else if(set->kind == EDITOR_ITEM_VERTEX || set->kind == EDITOR_ITEM_LINE)
                snprintf(values, sizeof(values), " %u %u %u %u", set->object,
                    set->parent, set->item, set->index);
            else snprintf(values, sizeof(values), " %u %u", set->object, set->item);
            if(!editor_command_text_append(output, output_capacity, &used, values) ||
                    !editor_command_text_append(output, output_capacity, &used, " ") ||
                    !editor_command_text_append(output, output_capacity, &used, property) ||
                    !editor_command_text_append(output, output_capacity, &used, " "))
                goto capacity_error;
            if(set->value_kind == EDITOR_PROPERTY_VALUE_FLOAT)
                snprintf(value, sizeof(value), "%.9g", set->value.number);
            else if(set->value_kind == EDITOR_PROPERTY_VALUE_BOOL) {
                boolean = set->value.boolean ? "true" : "false";
                snprintf(value, sizeof(value), "%s", boolean);
            } else {
                if(editor_command_property_color_check(set->property))
                    snprintf(value, sizeof(value), "#%08X", set->value.integer);
                else {
                    const char *kinds[] = {"revolute", "weld", "spring"};
                    if(set->value.integer > (uint32_t)EDITOR_JOINT_SPRING)
                        goto capacity_error;
                    snprintf(value, sizeof(value), "%s", kinds[set->value.integer]);
                }
            }
            if(!editor_command_text_append(output, output_capacity, &used, value))
                goto capacity_error;
            return editor_result_value(true);
        }
        case EDITOR_COMMAND_RELATIONSHIP_SET: {
            const EditorRelationshipSetCommand *set =
                &command->data.relationship_set;
            const char *slot;
            char target[32];
            if(!editor_command_text_append(output, output_capacity, &used, "connect ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            if(set->target == 0) snprintf(target, sizeof(target), "none");
            else snprintf(target, sizeof(target), "%u", set->target);
            if(set->kind == EDITOR_RELATIONSHIP_JOINT_ANCHOR) {
                slot = set->endpoint == 0 ? "anchor-a" : "anchor-b";
                snprintf(values, sizeof(values), " %u %u %s %s", set->object,
                    set->item, slot, target);
            } else if(set->kind == EDITOR_RELATIONSHIP_ANCHOR_RIGID_BODY) {
                snprintf(values, sizeof(values), " %u %u rigid-body %s", set->object,
                    set->item, target);
            } else if(set->kind == EDITOR_RELATIONSHIP_SOFT_BEAM_NODE) {
                slot = set->endpoint == 0 ? "node-a" : "node-b";
                snprintf(values, sizeof(values), " %u %u %u %s %s", set->object,
                    set->parent, set->item, slot, target);
            } else goto capacity_error;
            break;
        }
        case EDITOR_COMMAND_COLLISION_MASK_ADD:
            if(!editor_command_text_append(output, output_capacity, &used, "add ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path) ||
                    !editor_command_text_append(output, output_capacity, &used, " ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        command->data.collision_mask_add.name)) goto capacity_error;
            return editor_result_value(true);
        case EDITOR_COMMAND_COLLISION_FILTER_SET: {
            const EditorCollisionFilterSetCommand *set =
                &command->data.collision_filter_set;
            const char *filter = set->filter == EDITOR_COLLISION_FILTER_CATEGORY ?
                "category" : "collide-with";
            const char *enabled = set->enabled ? "true" : "false";
            if((set->kind != EDITOR_ITEM_RIGID_BODY &&
                        set->kind != EDITOR_ITEM_SOFT_NODE) ||
                    (set->filter != EDITOR_COLLISION_FILTER_CATEGORY &&
                        set->filter != EDITOR_COLLISION_FILTER_COLLIDE_WITH) ||
                    !editor_command_text_append(output, output_capacity, &used, "filter ") ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        document_path)) goto capacity_error;
            if(set->kind == EDITOR_ITEM_RIGID_BODY)
                snprintf(values, sizeof(values), " %u %u %s ", set->object,
                    set->item, filter);
            else snprintf(values, sizeof(values), " %u %u %u %s ", set->object,
                set->parent, set->item, filter);
            if(!editor_command_text_append(output, output_capacity, &used, values) ||
                    !editor_command_shell_text_append(output, output_capacity, &used,
                        set->mask) ||
                    !editor_command_text_append(output, output_capacity, &used, " ") ||
                    !editor_command_text_append(output, output_capacity, &used, enabled))
                goto capacity_error;
            return editor_result_value(true);
        }
    }
    if(!editor_command_text_append(output, output_capacity, &used, values))
        goto capacity_error;
    return editor_result_value(true);
    return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "unknown editor command");

capacity_error:
    return editor_result_error(EDITOR_ERROR_CAPACITY,
        "CLI command output buffer is too small");
}
