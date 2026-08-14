#include "editor_command.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EditorCommandExecuted editor_command_executed_callback;
static void *editor_command_executed_context;

static EditorCommandResult editor_command_error(EditorError error) {
    return (EditorCommandResult){.kind = ERROR_RESULT_ERROR,
        .result.error = error};
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
                body->nodes[i].position = command->data.soft_node_position.position;
                return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
            }
            return editor_command_not_found("soft node",
                command->data.soft_node_position.node);
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
            }
            if(visible == NULL) return editor_command_not_found("visibility target",
                command->data.visibility.item);
            *visible = command->data.visibility.visible;
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
        }
        case EDITOR_COMMAND_NAVIGATION_SET:
            if(command->data.navigation.mode > 11 ||
                    command->data.navigation.selection > 11 ||
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
            if(kind == EDITOR_ITEM_OBJECT) {
                EditorObjectIdResult result = editor_object_command_add(project,
                    &(EditorObjectAddArgs){command->data.item_add.name,
                        command->data.item_add.position});
                if(result.kind == ERROR_RESULT_ERROR)
                    return editor_command_error(result.result.error);
                return (EditorCommandResult){.kind = ERROR_RESULT_VALUE,
                    .result.object = result.result.value};
            }
            object = editor_object_query_get(project, command->data.item_add.object);
            if(object == NULL) return editor_command_not_found("object",
                command->data.item_add.object);
            if(kind == EDITOR_ITEM_RIGID_BODY) {
                EditorRigidBody *value = editor_project_rigid_body_add(project, object);
                if(value != NULL) created = value->id;
            } else if(kind == EDITOR_ITEM_HITBOX) {
                EditorRigidBody *body = editor_project_rigid_body_get(object,
                    command->data.item_add.parent);
                EditorHitbox *value = editor_project_hitbox_add(project, body);
                if(value != NULL) created = value->id;
            } else if(kind == EDITOR_ITEM_JOINT && command->data.item_add.option <=
                    (uint32_t)EDITOR_JOINT_SPRING) {
                EditorJoint *value = editor_project_joint_add(project, object,
                    (EditorJointKind)command->data.item_add.option);
                if(value != NULL) created = value->id;
            } else if(kind == EDITOR_ITEM_ANCHOR) {
                EditorAnchor *value = editor_project_anchor_add(project, object,
                    command->data.item_add.position, command->data.item_add.parent);
                if(value != NULL) created = value->id;
            } else if(kind == EDITOR_ITEM_SOFT_BODY) {
                EditorSoftBody *value = editor_project_soft_body_add(project, object);
                if(value != NULL) created = value->id;
            } else if(kind == EDITOR_ITEM_SOFT_NODE) {
                EditorSoftBody *body = editor_command_soft_body_get(object,
                    command->data.item_add.parent);
                EditorSoftNode *value = editor_project_soft_node_add(project, body,
                    command->data.item_add.position);
                if(value != NULL) created = value->id;
            } else if(kind == EDITOR_ITEM_SOFT_BEAM) {
                EditorSoftBody *body = editor_command_soft_body_get(object,
                    command->data.item_add.parent);
                EditorSoftBeam *value = editor_project_soft_beam_add(project, body,
                    command->data.item_add.first, command->data.item_add.second);
                if(value != NULL) created = value->id;
            } else if(kind == EDITOR_ITEM_VERTEX) {
                EditorRigidBody *body = editor_project_rigid_body_get(object,
                    command->data.item_add.parent);
                EditorHitbox *hitbox = editor_project_hitbox_get(body,
                    command->data.item_add.first);
                if(editor_project_hitbox_vertex_insert(project, hitbox,
                        command->data.item_add.index)) created = project->next_vertex_id - 1;
            }
            if(created == 0) return editor_command_error(editor_result_error(
                EDITOR_ERROR_CAPACITY, "could not add editor item").result.error);
            return (EditorCommandResult){.kind = ERROR_RESULT_VALUE,
                .result.object = created};
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
                removed = editor_project_soft_node_remove(body,
                    command->data.item_remove.item);
            } else if(kind == EDITOR_ITEM_SOFT_BEAM) {
                EditorSoftBody *body = editor_command_soft_body_get(object,
                    command->data.item_remove.parent);
                removed = editor_project_soft_beam_remove(body,
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
    }
    return editor_command_error((EditorError){EDITOR_ERROR_INVALID_ARGUMENT,
        "unknown editor command"});
}

EditorCommandResult editor_command_execute(EditorProject *project,
        const EditorCommand *command) {
    EditorCommandResult result = editor_command_execute_internal(project, command);
    if(result.kind == ERROR_RESULT_VALUE && editor_command_executed_callback != NULL)
        editor_command_executed_callback(command, editor_command_executed_context);
    return result;
}

void editor_command_executed_callback_set(EditorCommandExecuted callback,
        void *context) {
    editor_command_executed_callback = callback;
    editor_command_executed_context = context;
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
        case EDITOR_ITEM_VERTEX: return "vertex";
        case EDITOR_ITEM_LINE: return "line";
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
    char values[512];
    size_t used = 0;
    if(command == NULL || document_path == NULL || output == NULL ||
            output_capacity == 0)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "command serialization requires a command, path, and output buffer");
    output[0] = '\0';
    switch(command->type) {
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
                    command->data.visibility.kind == EDITOR_VISIBILITY_SOFT_BEAM) {
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
