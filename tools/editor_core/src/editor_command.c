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
    char values[128];
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
        default: return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "unknown editor command");
    }
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
