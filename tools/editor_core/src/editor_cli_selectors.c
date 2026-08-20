/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_command.h"

#include <stdio.h>
#include <string.h>

#define EDITOR_CLI_ARGUMENT_MAX 32
#define EDITOR_CLI_TOKEN_MAX 1024

typedef struct EditorCliSelector {
    const char *name;
    uint32_t id;
    bool id_set;
} EditorCliSelector;

typedef struct EditorCliSelectors {
    EditorCliSelector object;
    EditorCliSelector body;
    EditorCliSelector hitbox;
    EditorCliSelector joint;
    EditorCliSelector anchor;
    EditorCliSelector soft_body;
    EditorCliSelector node;
    EditorCliSelector beam;
    EditorCliSelector area;
    EditorCliSelector vertex;
    EditorCliSelector line;
    EditorCliSelector node_a;
    EditorCliSelector node_b;
    EditorCliSelector sprite;
    EditorCliSelector animated_sprite;
    EditorCliSelector frame;
} EditorCliSelectors;

static bool editor_cli_uint_parse(const char *text, uint32_t *value) {
    unsigned long parsed;
    char tail;
    if(text == NULL || value == NULL || sscanf(text, "%lu%c", &parsed, &tail) != 1 ||
            parsed > UINT32_MAX) return false;
    *value = (uint32_t)parsed;
    return true;
}

static EditorResult editor_cli_selector_set(EditorCliSelector *selector,
        const char *flag, const char *value, bool id) {
    if(selector == NULL || value == NULL || selector->name != NULL || selector->id_set)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Selector %s was provided more than once", flag);
    if(id) {
        if(!editor_cli_uint_parse(value, &selector->id))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "Selector %s requires an unsigned integer ID", flag);
        selector->id_set = true;
    } else {
        if(value[0] == '\0') return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Selector %s requires a non-empty name", flag);
        selector->name = value;
    }
    return editor_result_value(true);
}

static EditorCliSelector *editor_cli_selector_flag_get(EditorCliSelectors *selectors,
        const char *flag, bool *id) {
    struct Entry { const char *name; EditorCliSelector *selector; bool id; } entries[] = {
        {"--object", &selectors->object, false},
        {"--object-id", &selectors->object, true},
        {"--body", &selectors->body, false},
        {"--body-id", &selectors->body, true},
        {"--hitbox", &selectors->hitbox, false},
        {"--hitbox-id", &selectors->hitbox, true},
        {"--joint", &selectors->joint, false},
        {"--joint-id", &selectors->joint, true},
        {"--anchor", &selectors->anchor, false},
        {"--anchor-id", &selectors->anchor, true},
        {"--soft-body", &selectors->soft_body, false},
        {"--soft-body-id", &selectors->soft_body, true},
        {"--node", &selectors->node, false},
        {"--node-id", &selectors->node, true},
        {"--beam", &selectors->beam, false},
        {"--beam-id", &selectors->beam, true},
        {"--area", &selectors->area, false},
        {"--area-id", &selectors->area, true},
        {"--vertex", &selectors->vertex, false},
        {"--vertex-id", &selectors->vertex, true},
        {"--line", &selectors->line, false},
        {"--line-index", &selectors->line, true},
        {"--node-a", &selectors->node_a, false},
        {"--node-a-id", &selectors->node_a, true},
        {"--node-b", &selectors->node_b, false},
        {"--node-b-id", &selectors->node_b, true}
        ,{"--sprite", &selectors->sprite, false}
        ,{"--sprite-id", &selectors->sprite, true}
        ,{"--animated-sprite", &selectors->animated_sprite, false}
        ,{"--animated-sprite-id", &selectors->animated_sprite, true}
        ,{"--frame-index", &selectors->frame, true}
    };
    for(size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i += 1) {
        if(strcmp(flag, entries[i].name) != 0) continue;
        *id = entries[i].id;
        return entries[i].selector;
    }
    return NULL;
}

static EditorResult editor_cli_selectors_parse(int count, char **arguments,
        EditorCliSelectors *selectors, char **rest, size_t *rest_count,
        bool *found_selector) {
    *selectors = (EditorCliSelectors){0};
    *rest_count = 0;
    *found_selector = false;
    for(int i = 4; i < count; i += 1) {
        bool id = false;
        EditorCliSelector *selector = editor_cli_selector_flag_get(
            selectors, arguments[i], &id);
        if(selector == NULL) {
            if(*rest_count >= EDITOR_CLI_ARGUMENT_MAX)
                return editor_result_error(EDITOR_ERROR_CAPACITY,
                    "Too many CLI arguments");
            rest[(*rest_count)++] = arguments[i];
            continue;
        }
        *found_selector = true;
        if(i + 1 >= count) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Selector %s requires a value", arguments[i]);
        {
            EditorResult result = editor_cli_selector_set(
                selector, arguments[i], arguments[++i], id);
            if(editor_result_check(result)) return result;
        }
    }
    return editor_result_value(true);
}

static bool editor_cli_selector_present(const EditorCliSelector *selector) {
    return selector != NULL && (selector->name != NULL || selector->id_set);
}

static bool editor_cli_name_equal(const char *stored, const char *requested,
        bool object_name) {
    char formatted[EDITOR_OBJECT_NAME_MAX];
    if(object_name) editor_project_object_name_format(
        formatted, sizeof(formatted), requested);
    else editor_project_property_name_format(formatted, sizeof(formatted), requested);
    return strcmp(stored, formatted) == 0;
}

static EditorResult editor_cli_match_result(const char *kind, const char *name,
        const char *id_flag, uint32_t found, size_t matches, uint32_t *output) {
    if(matches == 0) return editor_result_error(EDITOR_ERROR_NOT_FOUND,
        "%s named '%s' was not found", kind, name);
    if(matches > 1) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "%s name '%s' is ambiguous; use %s", kind, name, id_flag);
    *output = found;
    return editor_result_value(true);
}

static EditorResult editor_cli_object_resolve(const EditorProject *project,
        const EditorCliSelector *selector, uint32_t *output) {
    uint32_t found = 0;
    size_t matches = 0;
    if(selector->id_set) { *output = selector->id; return editor_result_value(true); }
    if(selector->name == NULL) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "Expected --object <name> or --object-id <id>");
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(!editor_cli_name_equal(project->objects[i].name, selector->name, true)) continue;
        found = project->objects[i].id;
        matches += 1;
    }
    return editor_cli_match_result("Object", selector->name, "--object-id",
        found, matches, output);
}

static EditorResult editor_cli_sprite_resolve(const EditorObject *object,
        const EditorCliSelector *selector, uint32_t *output) {
    uint32_t found = 0;
    size_t matches = 0;
    if(selector->id_set) { *output = selector->id; return editor_result_value(true); }
    if(selector->name == NULL) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "Expected --sprite <name> or --sprite-id <id>");
    for(size_t i = 0; object != NULL && i < object->sprite_count; i += 1) {
        if(!editor_cli_name_equal(object->sprites[i].name, selector->name, false))
            continue;
        found = object->sprites[i].id;
        matches += 1;
    }
    return editor_cli_match_result("Sprite", selector->name, "--sprite-id",
        found, matches, output);
}

static EditorObject *editor_cli_object_get(const EditorProject *project, uint32_t id) {
    for(size_t i = 0; i < project->object_count; i += 1)
        if(project->objects[i].id == id) return (EditorObject *)&project->objects[i];
    return NULL;
}

static EditorSoftBody *editor_cli_soft_body_get(EditorObject *object, uint32_t id) {
    if(object != NULL) for(size_t i = 0; i < object->soft_body_count; i += 1)
        if(object->soft_body_items[i].id == id) return &object->soft_body_items[i];
    return NULL;
}

#define EDITOR_CLI_RESOLVE_FUNCTION(function_name, type_name, count_field, array_field, \
        kind_text, id_flag_text) \
static EditorResult function_name(type_name *parent, const EditorCliSelector *selector, \
        uint32_t *output) { \
    uint32_t found = 0; size_t matches = 0; \
    if(selector->id_set) { *output = selector->id; return editor_result_value(true); } \
    if(parent == NULL) return editor_result_error(EDITOR_ERROR_NOT_FOUND, \
        "Parent for %s selector was not found", kind_text); \
    if(selector->name == NULL) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT, \
        "Expected a named or ID %s selector", kind_text); \
    for(size_t i = 0; i < parent->count_field; i += 1) { \
        if(!editor_cli_name_equal(parent->array_field[i].name, selector->name, false)) continue; \
        found = parent->array_field[i].id; matches += 1; \
    } \
    return editor_cli_match_result(kind_text, selector->name, id_flag_text, \
        found, matches, output); \
}

EDITOR_CLI_RESOLVE_FUNCTION(editor_cli_body_resolve, EditorObject,
    rigid_body_count, rigid_bodies, "Rigid body", "--body-id")
EDITOR_CLI_RESOLVE_FUNCTION(editor_cli_joint_resolve, EditorObject,
    joint_count, joint_items, "Joint", "--joint-id")
EDITOR_CLI_RESOLVE_FUNCTION(editor_cli_anchor_resolve, EditorObject,
    anchor_count, anchors, "Anchor", "--anchor-id")
EDITOR_CLI_RESOLVE_FUNCTION(editor_cli_soft_body_resolve, EditorObject,
    soft_body_count, soft_body_items, "Soft body", "--soft-body-id")
EDITOR_CLI_RESOLVE_FUNCTION(editor_cli_animated_sprite_resolve, EditorObject,
    animated_sprite_count, animated_sprite_items, "Animated sprite",
    "--animated-sprite-id")
EDITOR_CLI_RESOLVE_FUNCTION(editor_cli_node_resolve, EditorSoftBody,
    node_count, nodes, "Soft node", "--node-id")
EDITOR_CLI_RESOLVE_FUNCTION(editor_cli_beam_resolve, EditorSoftBody,
    beam_count, beams, "Soft beam", "--beam-id")
EDITOR_CLI_RESOLVE_FUNCTION(editor_cli_area_resolve, EditorSoftBody,
    area_count, areas, "Soft area", "--area-id")
#undef EDITOR_CLI_RESOLVE_FUNCTION

static EditorResult editor_cli_hitbox_resolve(EditorRigidBody *body,
        const EditorCliSelector *selector, uint32_t *output) {
    uint32_t found = 0; size_t matches = 0;
    if(selector->id_set) { *output = selector->id; return editor_result_value(true); }
    if(body == NULL) return editor_result_error(EDITOR_ERROR_NOT_FOUND,
        "Parent rigid body for hitbox selector was not found");
    if(selector->name == NULL) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "Expected --hitbox <name> or --hitbox-id <id>");
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        if(!editor_cli_name_equal(body->hitboxes[i].name, selector->name, false)) continue;
        found = body->hitboxes[i].id; matches += 1;
    }
    return editor_cli_match_result("Hitbox", selector->name, "--hitbox-id",
        found, matches, output);
}

static EditorResult editor_cli_vertex_resolve(EditorHitbox *hitbox,
        const EditorCliSelector *selector, uint32_t *output) {
    uint32_t found = 0; size_t matches = 0;
    if(selector->id_set) { *output = selector->id; return editor_result_value(true); }
    if(hitbox == NULL) return editor_result_error(EDITOR_ERROR_NOT_FOUND,
        "Parent hitbox for vertex selector was not found");
    if(selector->name == NULL) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "Expected --vertex <name> or --vertex-id <id>");
    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
        if(!editor_cli_name_equal(hitbox->vertices[i].name, selector->name, false)) continue;
        found = hitbox->vertices[i].id; matches += 1;
    }
    return editor_cli_match_result("Vertex", selector->name, "--vertex-id",
        found, matches, output);
}

static EditorResult editor_cli_line_resolve(EditorHitbox *hitbox,
        const EditorCliSelector *selector, uint32_t *output) {
    uint32_t found = 0; size_t matches = 0;
    if(selector->id_set) {
        if(hitbox == NULL || selector->id >= hitbox->vertex_count)
            return editor_result_error(EDITOR_ERROR_NOT_FOUND,
                "Line index %u was not found", selector->id);
        *output = selector->id;
        return editor_result_value(true);
    }
    if(hitbox == NULL) return editor_result_error(EDITOR_ERROR_NOT_FOUND,
        "Parent hitbox for line selector was not found");
    if(selector->name == NULL) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "Expected --line <name> or --line-index <index>");
    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
        if(!editor_cli_name_equal(hitbox->line_names[i], selector->name, false)) continue;
        found = i; matches += 1;
    }
    return editor_cli_match_result("Line", selector->name, "--line-index",
        found, matches, output);
}

static bool editor_cli_selector_matches(const EditorCliSelector *selector,
        const char *name, uint32_t id, bool object_name) {
    if(!editor_cli_selector_present(selector)) return true;
    if(selector->id_set) return selector->id == id;
    return editor_cli_name_equal(name, selector->name, object_name);
}

static bool editor_cli_hitbox_matches(const EditorHitbox *hitbox,
        const EditorCliSelectors *selectors) {
    if(hitbox == NULL || !editor_cli_selector_matches(&selectors->hitbox,
            hitbox->name, hitbox->id, false)) return false;
    if(editor_cli_selector_present(&selectors->vertex)) {
        bool found = false;
        for(uint32_t i = 0; i < hitbox->vertex_count; i += 1)
            if(editor_cli_selector_matches(&selectors->vertex,
                    hitbox->vertices[i].name, hitbox->vertices[i].id, false)) found = true;
        if(!found) return false;
    }
    if(editor_cli_selector_present(&selectors->line)) {
        bool found = false;
        for(uint32_t i = 0; i < hitbox->vertex_count; i += 1)
            if(editor_cli_selector_matches(&selectors->line,
                    hitbox->line_names[i], i, false)) found = true;
        if(!found) return false;
    }
    return true;
}

static bool editor_cli_body_matches(const EditorRigidBody *body,
        const EditorCliSelectors *selectors) {
    if(body == NULL || !editor_cli_selector_matches(&selectors->body,
            body->name, body->id, false)) return false;
    if(editor_cli_selector_present(&selectors->hitbox) ||
            editor_cli_selector_present(&selectors->vertex) ||
            editor_cli_selector_present(&selectors->line)) {
        for(size_t i = 0; i < body->hitbox_count; i += 1)
            if(editor_cli_hitbox_matches(&body->hitboxes[i], selectors)) return true;
        return false;
    }
    return true;
}

static bool editor_cli_soft_body_matches(const EditorSoftBody *body,
        const EditorCliSelectors *selectors) {
    if(body == NULL || !editor_cli_selector_matches(&selectors->soft_body,
            body->name, body->id, false)) return false;
    if(editor_cli_selector_present(&selectors->node)) {
        bool found = false;
        for(size_t i = 0; i < body->node_count; i += 1)
            if(editor_cli_selector_matches(&selectors->node,
                    body->nodes[i].name, body->nodes[i].id, false)) found = true;
        if(!found) return false;
    }
    if(editor_cli_selector_present(&selectors->beam)) {
        bool found = false;
        for(size_t i = 0; i < body->beam_count; i += 1)
            if(editor_cli_selector_matches(&selectors->beam,
                    body->beams[i].name, body->beams[i].id, false)) found = true;
        if(!found) return false;
    }
    if(editor_cli_selector_present(&selectors->area)) {
        bool found = false;
        for(size_t i = 0; i < body->area_count; i += 1)
            if(editor_cli_selector_matches(&selectors->area,
                    body->areas[i].name, body->areas[i].id, false)) found = true;
        if(!found) return false;
    }
    return true;
}

static bool editor_cli_object_matches(const EditorObject *object,
        const EditorCliSelectors *selectors) {
    if(!editor_cli_selector_matches(&selectors->object,
            object->name, object->id, true)) return false;
    if(editor_cli_selector_present(&selectors->body) ||
            editor_cli_selector_present(&selectors->hitbox) ||
            editor_cli_selector_present(&selectors->vertex) ||
            editor_cli_selector_present(&selectors->line)) {
        bool found = false;
        for(size_t i = 0; i < object->rigid_body_count; i += 1)
            if(editor_cli_body_matches(&object->rigid_bodies[i], selectors)) found = true;
        if(!found) return false;
    }
#define MATCH_OBJECT_CHILD(selector, count, items) do { \
    if(editor_cli_selector_present(&(selector))) { bool found = false; \
        for(size_t i = 0; i < (count); i += 1) \
            if(editor_cli_selector_matches(&(selector), (items)[i].name, \
                    (items)[i].id, false)) found = true; \
        if(!found) return false; \
    } \
} while(0)
    MATCH_OBJECT_CHILD(selectors->joint, object->joint_count, object->joint_items);
    MATCH_OBJECT_CHILD(selectors->anchor, object->anchor_count, object->anchors);
    MATCH_OBJECT_CHILD(selectors->sprite, object->sprite_count, object->sprites);
    MATCH_OBJECT_CHILD(selectors->animated_sprite, object->animated_sprite_count,
        object->animated_sprite_items);
#undef MATCH_OBJECT_CHILD
    if(editor_cli_selector_present(&selectors->soft_body) ||
            editor_cli_selector_present(&selectors->node) ||
            editor_cli_selector_present(&selectors->beam) ||
            editor_cli_selector_present(&selectors->area)) {
        bool found = false;
        for(size_t i = 0; i < object->soft_body_count; i += 1)
            if(editor_cli_soft_body_matches(&object->soft_body_items[i], selectors))
                found = true;
        if(!found) return false;
    }
    return true;
}

static EditorResult editor_cli_global_context_resolve(const EditorProject *project,
        EditorCliSelectors *selectors) {
    uint32_t object_id = 0;
    size_t matches = 0;
    if(!editor_cli_selector_present(&selectors->object)) {
        for(size_t i = 0; i < project->object_count; i += 1)
            if(editor_cli_object_matches(&project->objects[i], selectors)) {
                object_id = project->objects[i].id; matches += 1;
            }
        if(matches == 0) return editor_result_error(EDITOR_ERROR_NOT_FOUND,
            "No object contains the requested selector hierarchy");
        if(matches > 1) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Selector hierarchy is ambiguous; add --object or --object-id");
        selectors->object.id = object_id;
        selectors->object.id_set = true;
    }
    return editor_result_value(true);
}

static bool editor_cli_argument_push(char **normalized, int *count, char *value) {
    if(*count >= EDITOR_CLI_ARGUMENT_MAX) return false;
    normalized[(*count)++] = value;
    return true;
}

static bool editor_cli_id_push(char **normalized, int *count,
        char buffers[][16], size_t *buffer_count, uint32_t id) {
    if(*buffer_count >= 16) return false;
    snprintf(buffers[*buffer_count], 16, "%u", id);
    return editor_cli_argument_push(normalized, count, buffers[(*buffer_count)++]);
}

EditorResult editor_command_cli_named_parse(const EditorProject *project,
        int count, char **arguments, const char **document_path,
        EditorCommand *command) {
    EditorCliSelectors selectors;
    char *rest[EDITOR_CLI_ARGUMENT_MAX];
    size_t rest_count;
    bool found_selector;
    char *normalized[EDITOR_CLI_ARGUMENT_MAX];
    char id_buffers[16][16];
    size_t id_buffer_count = 0;
    int normalized_count = 0;
    uint32_t object_id = 0, body_id = 0, hitbox_id = 0, item_id = 0;
    EditorObject *object = NULL;
    EditorRigidBody *body = NULL;
    EditorHitbox *hitbox = NULL;
    EditorSoftBody *soft_body = NULL;
    EditorResult result;
    const char *domain;
    const char *action;
    bool needs_object = true;

    if(project == NULL) return editor_command_cli_parse(
        count, arguments, document_path, command);
    result = editor_cli_selectors_parse(count, arguments, &selectors,
        rest, &rest_count, &found_selector);
    if(editor_result_check(result) || !found_selector) return editor_result_check(result) ?
        result : editor_command_cli_parse(count, arguments, document_path, command);
    if(count < 4) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "Expected an editor CLI command");
    domain = arguments[1]; action = arguments[2];
    if(strcmp(domain, "viewport") == 0 || strcmp(domain, "navigation") == 0 ||
            strcmp(domain, "collision-mask") == 0)
        needs_object = false;
    normalized[normalized_count++] = arguments[0];
    normalized[normalized_count++] = arguments[1];
    normalized[normalized_count++] = arguments[2];
    normalized[normalized_count++] = arguments[3];
    if(strcmp(domain, "navigation") == 0 && strcmp(action, "set") == 0) {
        uint32_t joint_id = 0, anchor_id = 0, soft_body_id = 0;
        uint32_t node_id = 0, beam_id = 0, line_id = 0, vertex_id = 0;
        if(rest_count != 3) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Navigation set requires mode, selection, and origin kind");
        if(editor_cli_selector_present(&selectors.object)) {
            result = editor_cli_object_resolve(project, &selectors.object, &object_id);
            if(editor_result_check(result)) return result;
            object = editor_cli_object_get(project, object_id);
            if(object == NULL) return editor_result_error(EDITOR_ERROR_NOT_FOUND,
                "Object ID %u was not found", object_id);
        }
        if(editor_cli_selector_present(&selectors.body)) {
            result = editor_cli_body_resolve(object, &selectors.body, &body_id);
            if(editor_result_check(result)) return result;
            body = editor_project_rigid_body_get(object, body_id);
        }
        if(editor_cli_selector_present(&selectors.hitbox)) {
            result = editor_cli_hitbox_resolve(body, &selectors.hitbox, &hitbox_id);
            if(editor_result_check(result)) return result;
            hitbox = editor_project_hitbox_get(body, hitbox_id);
        }
#define RESOLVE_OPTIONAL(selector, call, output) do { \
    if(editor_cli_selector_present(&(selector))) { result = (call); \
        if(editor_result_check(result)) return result; } \
} while(0)
        RESOLVE_OPTIONAL(selectors.joint,
            editor_cli_joint_resolve(object, &selectors.joint, &joint_id), joint_id);
        RESOLVE_OPTIONAL(selectors.anchor,
            editor_cli_anchor_resolve(object, &selectors.anchor, &anchor_id), anchor_id);
        RESOLVE_OPTIONAL(selectors.soft_body,
            editor_cli_soft_body_resolve(object, &selectors.soft_body, &soft_body_id),
            soft_body_id);
        soft_body = editor_cli_soft_body_get(object, soft_body_id);
        RESOLVE_OPTIONAL(selectors.node,
            editor_cli_node_resolve(soft_body, &selectors.node, &node_id), node_id);
        RESOLVE_OPTIONAL(selectors.beam,
            editor_cli_beam_resolve(soft_body, &selectors.beam, &beam_id), beam_id);
        RESOLVE_OPTIONAL(selectors.line,
            editor_cli_line_resolve(hitbox, &selectors.line, &line_id), line_id);
        RESOLVE_OPTIONAL(selectors.vertex,
            editor_cli_vertex_resolve(hitbox, &selectors.vertex, &vertex_id), vertex_id);
#undef RESOLVE_OPTIONAL
        if(!editor_cli_argument_push(normalized, &normalized_count, rest[0]) ||
                !editor_cli_argument_push(normalized, &normalized_count, rest[1]) ||
                !editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, object_id) ||
                !editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, body_id) ||
                !editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, hitbox_id) ||
                !editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, joint_id) ||
                !editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, anchor_id) ||
                !editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, soft_body_id) ||
                !editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, node_id) ||
                !editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, beam_id) ||
                !editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, line_id) ||
                !editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, vertex_id) ||
                !editor_cli_argument_push(normalized, &normalized_count, rest[2]))
            goto capacity_error;
        return editor_command_cli_parse(normalized_count, normalized,
            document_path, command);
    }
    if(needs_object) {
        result = editor_cli_global_context_resolve(project, &selectors);
        if(editor_result_check(result)) return result;
        result = editor_cli_object_resolve(project, &selectors.object, &object_id);
        if(editor_result_check(result)) return result;
        object = editor_cli_object_get(project, object_id);
        if(object == NULL) return editor_result_error(EDITOR_ERROR_NOT_FOUND,
            "Object ID %u was not found", object_id);
        if(!editor_cli_selector_present(&selectors.body) &&
                (editor_cli_selector_present(&selectors.hitbox) ||
                 editor_cli_selector_present(&selectors.vertex) ||
                 editor_cli_selector_present(&selectors.line))) {
            size_t matches = 0;
            for(size_t i = 0; i < object->rigid_body_count; i += 1)
                if(editor_cli_body_matches(&object->rigid_bodies[i], &selectors)) {
                    selectors.body.id = object->rigid_bodies[i].id; matches += 1;
                }
            if(matches != 1) return editor_result_error(matches == 0 ?
                EDITOR_ERROR_NOT_FOUND : EDITOR_ERROR_INVALID_ARGUMENT,
                matches == 0 ? "No rigid body contains the requested selector" :
                    "Selector is ambiguous; add --body or --body-id");
            selectors.body.id_set = true;
        }
        if(!editor_cli_selector_present(&selectors.soft_body) &&
                (editor_cli_selector_present(&selectors.node) ||
                 editor_cli_selector_present(&selectors.beam) ||
                 editor_cli_selector_present(&selectors.area))) {
            size_t matches = 0;
            for(size_t i = 0; i < object->soft_body_count; i += 1)
                if(editor_cli_soft_body_matches(&object->soft_body_items[i], &selectors)) {
                    selectors.soft_body.id = object->soft_body_items[i].id; matches += 1;
                }
            if(matches != 1) return editor_result_error(matches == 0 ?
                EDITOR_ERROR_NOT_FOUND : EDITOR_ERROR_INVALID_ARGUMENT,
                matches == 0 ? "No soft body contains the requested selector" :
                    "Selector is ambiguous; add --soft-body or --soft-body-id");
            selectors.soft_body.id_set = true;
        }
        if(!editor_cli_id_push(normalized, &normalized_count, id_buffers,
                &id_buffer_count, object_id)) goto capacity_error;
    }

#define RESOLVE_AND_PUSH(call) do { result = (call); if(editor_result_check(result)) \
    return result; if(!editor_cli_id_push(normalized, &normalized_count, id_buffers, \
        &id_buffer_count, item_id)) goto capacity_error; } while(0)
    if(strcmp(domain, "sprite") == 0) {
        if(strcmp(action, "add") != 0)
            RESOLVE_AND_PUSH(editor_cli_sprite_resolve(object,
                &selectors.sprite, &item_id));
        if(strcmp(action, "connect") == 0) {
            if(rest_count != 2 || strcmp(rest[0], "body") != 0)
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "sprite connect requires body");
            if(!editor_cli_argument_push(normalized, &normalized_count, rest[0]))
                goto capacity_error;
            if(strcmp(rest[1], "none") == 0) item_id = 0;
            else {
                selectors.body.name = rest[1];
                result = editor_cli_body_resolve(object, &selectors.body, &item_id);
                if(editor_result_check(result)) return result;
            }
            if(!editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, item_id)) goto capacity_error;
            rest_count = 0;
        }
    } else if(strcmp(domain, "object") == 0) {
        if(strcmp(action, "add") == 0 || strcmp(action, "list") == 0)
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "Object %s does not accept selectors", action);
    } else if(strcmp(domain, "rigid-body") == 0) {
        if(strcmp(action, "add") != 0) {
            RESOLVE_AND_PUSH(editor_cli_body_resolve(object, &selectors.body, &item_id));
        }
    } else if(strcmp(domain, "hitbox") == 0) {
        result = editor_cli_body_resolve(object, &selectors.body, &body_id);
        if(editor_result_check(result)) return result;
        body = editor_project_rigid_body_get(object, body_id);
        if(!editor_cli_id_push(normalized, &normalized_count, id_buffers,
                &id_buffer_count, body_id)) goto capacity_error;
        if(strcmp(action, "add") != 0)
            RESOLVE_AND_PUSH(editor_cli_hitbox_resolve(body, &selectors.hitbox, &item_id));
    } else if(strcmp(domain, "vertex") == 0 || strcmp(domain, "line") == 0) {
        result = editor_cli_body_resolve(object, &selectors.body, &body_id);
        if(editor_result_check(result)) return result;
        body = editor_project_rigid_body_get(object, body_id);
        if(!editor_cli_selector_present(&selectors.hitbox) &&
                (editor_cli_selector_present(&selectors.vertex) ||
                 editor_cli_selector_present(&selectors.line))) {
            size_t matches = 0;
            for(size_t i = 0; body != NULL && i < body->hitbox_count; i += 1)
                if(editor_cli_hitbox_matches(&body->hitboxes[i], &selectors)) {
                    selectors.hitbox.id = body->hitboxes[i].id; matches += 1;
                }
            if(matches != 1) return editor_result_error(matches == 0 ?
                EDITOR_ERROR_NOT_FOUND : EDITOR_ERROR_INVALID_ARGUMENT,
                matches == 0 ? "No hitbox contains the requested selector" :
                    "Selector is ambiguous; add --hitbox or --hitbox-id");
            selectors.hitbox.id_set = true;
        }
        result = editor_cli_hitbox_resolve(body, &selectors.hitbox, &hitbox_id);
        if(editor_result_check(result)) return result;
        hitbox = editor_project_hitbox_get(body, hitbox_id);
        if(!editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, body_id) ||
                !editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, hitbox_id)) goto capacity_error;
        if(strcmp(domain, "line") == 0 || strcmp(action, "add") == 0)
            RESOLVE_AND_PUSH(editor_cli_line_resolve(hitbox, &selectors.line, &item_id));
        else RESOLVE_AND_PUSH(editor_cli_vertex_resolve(
            hitbox, &selectors.vertex, &item_id));
    } else if(strcmp(domain, "joint") == 0) {
        if(strcmp(action, "add") != 0)
            RESOLVE_AND_PUSH(editor_cli_joint_resolve(object, &selectors.joint, &item_id));
        if(strcmp(action, "connect") == 0) {
            if(rest_count != 1) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "Joint connect requires anchor-a or anchor-b");
            if(!editor_cli_argument_push(normalized, &normalized_count, rest[0]))
                goto capacity_error;
            RESOLVE_AND_PUSH(editor_cli_anchor_resolve(object, &selectors.anchor, &item_id));
            rest_count = 0;
        }
    } else if(strcmp(domain, "anchor") == 0) {
        if(strcmp(action, "add") == 0) {
            RESOLVE_AND_PUSH(editor_cli_body_resolve(object, &selectors.body, &item_id));
        } else {
            RESOLVE_AND_PUSH(editor_cli_anchor_resolve(object, &selectors.anchor, &item_id));
            if(strcmp(action, "connect") == 0) {
                if(rest_count != 1) return editor_result_error(
                    EDITOR_ERROR_INVALID_ARGUMENT,
                    "Anchor connect requires the rigid-body relationship name");
                if(!editor_cli_argument_push(normalized, &normalized_count, rest[0]))
                    goto capacity_error;
                RESOLVE_AND_PUSH(editor_cli_body_resolve(object, &selectors.body, &item_id));
                rest_count = 0;
            }
        }
    } else if(strcmp(domain, "soft-body") == 0) {
        if(strcmp(action, "add") != 0)
            RESOLVE_AND_PUSH(editor_cli_soft_body_resolve(
                object, &selectors.soft_body, &item_id));
    } else if(strcmp(domain, "animated-sprite") == 0) {
        if(strcmp(action, "add") != 0)
            RESOLVE_AND_PUSH(editor_cli_animated_sprite_resolve(object,
                &selectors.animated_sprite, &item_id));
        if(strcmp(action, "frame-add") == 0) {
            if(rest_count != 4) return editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT,
                "frame-add requires <name> <path> <width> <height>");
            for(size_t i = 0; i < rest_count; i += 1)
                if(!editor_cli_argument_push(normalized, &normalized_count, rest[i]))
                    goto capacity_error;
            rest_count = 0;
        } else if(strcmp(action, "frame-delete") == 0 ||
                strcmp(action, "frame-rename") == 0 ||
                strcmp(action, "frame-path-set") == 0 ||
                strcmp(action, "frame-size-set") == 0) {
            if(!selectors.frame.id_set) return editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT,
                "frame operation requires --frame-index <index>");
            if(!editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, selectors.frame.id)) goto capacity_error;
            for(size_t i = 0; i < rest_count; i += 1)
                if(!editor_cli_argument_push(normalized, &normalized_count, rest[i]))
                    goto capacity_error;
            rest_count = 0;
        } else if(strcmp(action, "connect") == 0) {
            if(rest_count != 2 || strcmp(rest[0], "body") != 0)
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "animated-sprite connect requires body");
            if(!editor_cli_argument_push(normalized, &normalized_count, rest[0]))
                goto capacity_error;
            if(strcmp(rest[1], "none") == 0) item_id = 0;
            else {
                selectors.body.name = rest[1];
                result = editor_cli_body_resolve(object, &selectors.body, &item_id);
                if(editor_result_check(result)) return result;
            }
            if(!editor_cli_id_push(normalized, &normalized_count, id_buffers,
                    &id_buffer_count, item_id)) goto capacity_error;
            rest_count = 0;
        }
    } else if(strcmp(domain, "soft-node") == 0 || strcmp(domain, "soft-beam") == 0 ||
            strcmp(domain, "soft-area") == 0) {
        result = editor_cli_soft_body_resolve(object, &selectors.soft_body, &body_id);
        if(editor_result_check(result)) return result;
        soft_body = editor_cli_soft_body_get(object, body_id);
        if(!editor_cli_id_push(normalized, &normalized_count, id_buffers,
                &id_buffer_count, body_id)) goto capacity_error;
        if(strcmp(domain, "soft-node") == 0) {
            if(strcmp(action, "add") != 0)
                RESOLVE_AND_PUSH(editor_cli_node_resolve(
                    soft_body, &selectors.node, &item_id));
        } else if(strcmp(domain, "soft-area") == 0) {
            RESOLVE_AND_PUSH(editor_cli_area_resolve(
                soft_body, &selectors.area, &item_id));
        } else if(strcmp(action, "add") == 0) {
            RESOLVE_AND_PUSH(editor_cli_node_resolve(
                soft_body, &selectors.node_a, &item_id));
            RESOLVE_AND_PUSH(editor_cli_node_resolve(
                soft_body, &selectors.node_b, &item_id));
        } else {
            RESOLVE_AND_PUSH(editor_cli_beam_resolve(
                soft_body, &selectors.beam, &item_id));
            if(strcmp(action, "connect") == 0) {
                if(rest_count != 1) return editor_result_error(
                    EDITOR_ERROR_INVALID_ARGUMENT,
                    "Soft-beam connect requires node-a or node-b");
                if(!editor_cli_argument_push(normalized, &normalized_count, rest[0]))
                    goto capacity_error;
                RESOLVE_AND_PUSH(editor_cli_node_resolve(
                    soft_body, &selectors.node, &item_id));
                rest_count = 0;
            }
        }
    } else {
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Named selectors are not supported for domain '%s'", domain);
    }
#undef RESOLVE_AND_PUSH
    for(size_t i = 0; i < rest_count; i += 1)
        if(!editor_cli_argument_push(normalized, &normalized_count, rest[i]))
            goto capacity_error;
    return editor_command_cli_parse(normalized_count, normalized,
        document_path, command);
capacity_error:
    return editor_result_error(EDITOR_ERROR_CAPACITY,
        "Too many normalized CLI arguments");
}

static bool editor_cli_token_safe(const char *text) {
    if(text == NULL || text[0] == '\0') return false;
    for(const unsigned char *at = (const unsigned char *)text; *at != '\0'; at += 1)
        if(!( (*at >= 'a' && *at <= 'z') || (*at >= 'A' && *at <= 'Z') ||
                (*at >= '0' && *at <= '9') || *at == '_' || *at == '-' ||
                *at == '.' || *at == '/' || *at == ':')) return false;
    return true;
}

static bool editor_cli_text_append(char *output, size_t capacity,
        size_t *used, const char *text) {
    size_t length = strlen(text);
    if(*used >= capacity || length >= capacity - *used) return false;
    memcpy(output + *used, text, length + 1);
    *used += length;
    return true;
}

static bool editor_cli_token_append(char *output, size_t capacity,
        size_t *used, const char *token) {
    if(*used != 0 && !editor_cli_text_append(output, capacity, used, " ")) return false;
    if(editor_cli_token_safe(token))
        return editor_cli_text_append(output, capacity, used, token);
    if(!editor_cli_text_append(output, capacity, used, "'")) return false;
    for(const char *at = token; *at != '\0'; at += 1) {
        char character[2] = {*at, '\0'};
        if(!editor_cli_text_append(output, capacity, used,
                *at == '\'' ? "'\\''" : character)) return false;
    }
    return editor_cli_text_append(output, capacity, used, "'");
}

static size_t editor_cli_tokens_parse(const char *text,
        char tokens[][EDITOR_CLI_TOKEN_MAX], size_t capacity) {
    size_t count = 0;
    while(*text != '\0') {
        size_t used = 0;
        bool quoted = false;
        while(*text == ' ') text += 1;
        if(*text == '\0' || count >= capacity) break;
        while(*text != '\0' && (quoted || *text != ' ')) {
            if(*text == '\'') { quoted = !quoted; text += 1; continue; }
            if(*text == '\\' && !quoted && text[1] != '\0') text += 1;
            if(used + 1 < EDITOR_CLI_TOKEN_MAX) tokens[count][used++] = *text;
            text += 1;
        }
        tokens[count][used] = '\0';
        count += 1;
    }
    return count;
}

static bool editor_cli_selector_write(char *output, size_t capacity, size_t *used,
        const char *name_flag, const char *id_flag, const char *name,
        uint32_t id, size_t name_matches) {
    char id_text[16];
    if(name != NULL && name[0] != '\0' && name_matches == 1)
        return editor_cli_token_append(output, capacity, used, name_flag) &&
            editor_cli_token_append(output, capacity, used, name);
    snprintf(id_text, sizeof(id_text), "%u", id);
    return editor_cli_token_append(output, capacity, used, id_flag) &&
        editor_cli_token_append(output, capacity, used, id_text);
}

static bool editor_cli_id_selector_write(char *output, size_t capacity,
        size_t *used, const char *id_flag, uint32_t id) {
    char id_text[16];
    snprintf(id_text, sizeof(id_text), "%u", id);
    return editor_cli_token_append(output, capacity, used, id_flag) &&
        editor_cli_token_append(output, capacity, used, id_text);
}

static const char *editor_cli_object_name_get(const EditorProject *project,
        uint32_t id, size_t *matches) {
    const char *name = NULL;
    *matches = 0;
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(project->objects[i].id == id) name = project->objects[i].name;
    }
    if(name != NULL) for(size_t i = 0; i < project->object_count; i += 1)
        if(strcmp(project->objects[i].name, name) == 0) *matches += 1;
    return name;
}

static const char *editor_cli_child_name_get(EditorObject *object,
        const char *kind, uint32_t id, size_t *matches) {
    const char *name = NULL;
    *matches = 0;
#define FIND_CHILD(count, items) do { \
    for(size_t i = 0; i < (count); i += 1) if((items)[i].id == id) name = (items)[i].name; \
    if(name != NULL) for(size_t i = 0; i < (count); i += 1) \
        if(strcmp((items)[i].name, name) == 0) *matches += 1; \
} while(0)
    if(object != NULL && strcmp(kind, "body") == 0)
        FIND_CHILD(object->rigid_body_count, object->rigid_bodies);
    else if(object != NULL && strcmp(kind, "joint") == 0)
        FIND_CHILD(object->joint_count, object->joint_items);
    else if(object != NULL && strcmp(kind, "anchor") == 0)
        FIND_CHILD(object->anchor_count, object->anchors);
    else if(object != NULL && strcmp(kind, "soft-body") == 0)
        FIND_CHILD(object->soft_body_count, object->soft_body_items);
#undef FIND_CHILD
    return name;
}

static bool editor_cli_hierarchy_selector_write(const EditorProject *project,
        char *output, size_t capacity, size_t *used, const char *kind,
        uint32_t object_id, uint32_t parent_id, uint32_t id) {
    EditorObject *object = editor_cli_object_get(project, object_id);
    EditorRigidBody *body;
    EditorSoftBody *soft_body;
    const char *name = NULL;
    const char *name_flag = NULL;
    const char *id_flag = NULL;
    size_t matches = 0;
    if(strcmp(kind, "object") == 0) {
        name = editor_cli_object_name_get(project, id, &matches);
        name_flag = "--object"; id_flag = "--object-id";
    } else if(strcmp(kind, "body") == 0 || strcmp(kind, "joint") == 0 ||
            strcmp(kind, "anchor") == 0 || strcmp(kind, "soft-body") == 0) {
        name = editor_cli_child_name_get(object, kind, id, &matches);
        name_flag = strcmp(kind, "body") == 0 ? "--body" :
            strcmp(kind, "soft-body") == 0 ? "--soft-body" :
            strcmp(kind, "joint") == 0 ? "--joint" : "--anchor";
        id_flag = strcmp(kind, "body") == 0 ? "--body-id" :
            strcmp(kind, "soft-body") == 0 ? "--soft-body-id" :
            strcmp(kind, "joint") == 0 ? "--joint-id" : "--anchor-id";
    } else if(strcmp(kind, "hitbox") == 0) {
        body = editor_project_rigid_body_get(object, parent_id);
        if(body != NULL) for(size_t i = 0; i < body->hitbox_count; i += 1)
            if(body->hitboxes[i].id == id) name = body->hitboxes[i].name;
        if(name != NULL) for(size_t i = 0; i < body->hitbox_count; i += 1)
            if(strcmp(body->hitboxes[i].name, name) == 0) matches += 1;
        name_flag = "--hitbox"; id_flag = "--hitbox-id";
    } else {
        soft_body = editor_cli_soft_body_get(object, parent_id);
        if(soft_body != NULL && strcmp(kind, "node") == 0)
            for(size_t i = 0; i < soft_body->node_count; i += 1)
                if(soft_body->nodes[i].id == id) name = soft_body->nodes[i].name;
        else if(soft_body != NULL && strcmp(kind, "area") == 0)
            for(size_t i = 0; i < soft_body->area_count; i += 1)
                if(soft_body->areas[i].id == id) name = soft_body->areas[i].name;
        else if(soft_body != NULL) for(size_t i = 0; i < soft_body->beam_count; i += 1)
            if(soft_body->beams[i].id == id) name = soft_body->beams[i].name;
        if(name != NULL && soft_body != NULL) {
            size_t count = strcmp(kind, "node") == 0 ? soft_body->node_count :
                strcmp(kind, "area") == 0 ? soft_body->area_count : soft_body->beam_count;
            for(size_t i = 0; i < count; i += 1) {
                const char *candidate = strcmp(kind, "node") == 0 ?
                    soft_body->nodes[i].name : strcmp(kind, "area") == 0 ?
                    soft_body->areas[i].name : soft_body->beams[i].name;
                if(strcmp(candidate, name) == 0) matches += 1;
            }
        }
        name_flag = strcmp(kind, "node") == 0 ? "--node" :
            strcmp(kind, "area") == 0 ? "--area" : "--beam";
        id_flag = strcmp(kind, "node") == 0 ? "--node-id" :
            strcmp(kind, "area") == 0 ? "--area-id" : "--beam-id";
    }
    return editor_cli_selector_write(output, capacity, used,
        name_flag, id_flag, name, id, matches);
}

static bool editor_cli_hitbox_item_selector_write(const EditorProject *project,
        char *output, size_t capacity, size_t *used, const char *kind,
        uint32_t object_id, uint32_t body_id, uint32_t hitbox_id, uint32_t id) {
    EditorObject *object = editor_cli_object_get(project, object_id);
    EditorRigidBody *body = editor_project_rigid_body_get(object, body_id);
    EditorHitbox *hitbox = editor_project_hitbox_get(body, hitbox_id);
    const char *name = NULL;
    size_t matches = 0;
    bool line = strcmp(kind, "line") == 0;
    if(hitbox != NULL && line && id < hitbox->vertex_count) name = hitbox->line_names[id];
    if(hitbox != NULL && !line) for(uint32_t i = 0; i < hitbox->vertex_count; i += 1)
        if(hitbox->vertices[i].id == id) name = hitbox->vertices[i].name;
    if(name != NULL && hitbox != NULL) for(uint32_t i = 0;
            i < hitbox->vertex_count; i += 1) {
        const char *candidate = line ? hitbox->line_names[i] : hitbox->vertices[i].name;
        if(strcmp(candidate, name) == 0) matches += 1;
    }
    return editor_cli_selector_write(output, capacity, used,
        line ? "--line" : "--vertex", line ? "--line-index" : "--vertex-id",
        name, id, matches);
}

static bool editor_cli_node_endpoint_selector_write(const EditorProject *project,
        char *output, size_t capacity, size_t *used, const char *slot,
        uint32_t object_id, uint32_t soft_body_id, uint32_t node_id) {
    EditorObject *object = editor_cli_object_get(project, object_id);
    EditorSoftBody *body = editor_cli_soft_body_get(object, soft_body_id);
    const char *name = NULL;
    size_t matches = 0;
    char name_flag[16];
    char id_flag[20];
    if(body != NULL) for(size_t i = 0; i < body->node_count; i += 1)
        if(body->nodes[i].id == node_id) name = body->nodes[i].name;
    if(name != NULL && body != NULL) for(size_t i = 0; i < body->node_count; i += 1)
        if(strcmp(body->nodes[i].name, name) == 0) matches += 1;
    snprintf(name_flag, sizeof(name_flag), "--%s", slot);
    snprintf(id_flag, sizeof(id_flag), "--%s-id", slot);
    return editor_cli_selector_write(output, capacity, used,
        name_flag, id_flag, name, node_id, matches);
}

static bool editor_cli_token_id_get(char tokens[][EDITOR_CLI_TOKEN_MAX],
        size_t count, size_t index, uint32_t *id) {
    return index < count && editor_cli_uint_parse(tokens[index], id);
}

EditorResult editor_command_cli_named_write(const EditorProject *project,
        const EditorCommand *command, const char *document_path,
        char *output, size_t output_capacity) {
    char legacy[3072];
    char tokens[32][EDITOR_CLI_TOKEN_MAX];
    bool skip[32] = {0};
    size_t token_count;
    size_t used = 0;
    uint32_t object = 0, parent = 0, hitbox = 0, item = 0;
    const char *domain;
    const char *action;
    EditorResult result;
    if(project == NULL) return editor_command_cli_write(command, document_path,
        output, output_capacity);
    result = editor_command_cli_write(command, document_path, legacy, sizeof(legacy));
    if(editor_result_check(result)) return result;
    token_count = editor_cli_tokens_parse(legacy, tokens, 32);
    if(token_count < 4) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "Could not parse serialized editor command");
    domain = tokens[1]; action = tokens[2];
    for(size_t i = 0; i < 4; i += 1)
        if(!editor_cli_token_append(output, output_capacity, &used, tokens[i]))
            goto capacity_error;
    if(strcmp(domain, "navigation") == 0 && strcmp(action, "set") == 0) {
        uint32_t values[10] = {0};
        if(token_count != 17) return editor_result_error(
            EDITOR_ERROR_INVALID_ARGUMENT, "Invalid serialized navigation command");
        for(size_t i = 0; i < 10; i += 1)
            if(!editor_cli_uint_parse(tokens[6 + i], &values[i]))
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "Invalid serialized navigation selector");
        if(!editor_cli_token_append(output, output_capacity, &used, tokens[4]) ||
                !editor_cli_token_append(output, output_capacity, &used, tokens[5]) ||
                !editor_cli_token_append(output, output_capacity, &used, tokens[16]))
            goto capacity_error;
        object = values[0];
        if(object != 0) {
            if(!editor_cli_hierarchy_selector_write(project, output, output_capacity,
                    &used, "object", object, 0, object)) goto capacity_error;
        }
        if(values[1] != 0 && !editor_cli_hierarchy_selector_write(project, output,
                output_capacity, &used, "body", object, 0, values[1]))
            goto capacity_error;
        if(values[2] != 0 && !editor_cli_hierarchy_selector_write(project, output,
                output_capacity, &used, "hitbox", object, values[1], values[2]))
            goto capacity_error;
        if(values[3] != 0 && !editor_cli_hierarchy_selector_write(project, output,
                output_capacity, &used, "joint", object, 0, values[3]))
            goto capacity_error;
        if(values[4] != 0 && !editor_cli_hierarchy_selector_write(project, output,
                output_capacity, &used, "anchor", object, 0, values[4]))
            goto capacity_error;
        if(values[5] != 0 && !editor_cli_hierarchy_selector_write(project, output,
                output_capacity, &used, "soft-body", object, 0, values[5]))
            goto capacity_error;
        if(values[6] != 0 && !editor_cli_hierarchy_selector_write(project, output,
                output_capacity, &used, "node", object, values[5], values[6]))
            goto capacity_error;
        if(values[7] != 0 && !editor_cli_hierarchy_selector_write(project, output,
                output_capacity, &used, "beam", object, values[5], values[7]))
            goto capacity_error;
        if(strcmp(tokens[5], "line") == 0 &&
                !editor_cli_hitbox_item_selector_write(project, output,
                    output_capacity, &used, "line", object, values[1], values[2],
                    values[8])) goto capacity_error;
        if(strcmp(tokens[5], "vertex") == 0 &&
                !editor_cli_hitbox_item_selector_write(project, output,
                    output_capacity, &used, "vertex", object, values[1], values[2],
                    values[9])) goto capacity_error;
        return editor_result_value(true);
    }

#define READ_ID(index, target) do { if(!editor_cli_token_id_get(tokens, token_count, \
    (index), &(target))) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT, \
    "Serialized command is missing selector ID"); skip[(index)] = true; } while(0)
#define WRITE_SELECTOR(kind, object_id, parent_id, id) do { \
    if(!editor_cli_hierarchy_selector_write(project, output, output_capacity, &used, \
        (kind), (object_id), (parent_id), (id))) goto capacity_error; \
} while(0)
    if(strcmp(domain, "viewport") != 0 && strcmp(domain, "navigation") != 0 &&
            strcmp(domain, "collision-mask") != 0 &&
            !(strcmp(domain, "object") == 0 && strcmp(action, "add") == 0)) {
        READ_ID(4, object);
        if(strcmp(domain, "object") == 0 && strcmp(action, "rename") == 0) {
            if(!editor_cli_id_selector_write(output, output_capacity, &used,
                    "--object-id", object)) goto capacity_error;
        } else WRITE_SELECTOR("object", object, 0, object);
    }
    if(strcmp(domain, "rigid-body") == 0 && strcmp(action, "add") != 0) {
        READ_ID(5, item);
        if(strcmp(action, "rename") == 0) {
            if(!editor_cli_id_selector_write(output, output_capacity, &used,
                    "--body-id", item)) goto capacity_error;
        } else WRITE_SELECTOR("body", object, 0, item);
    } else if(strcmp(domain, "hitbox") == 0) {
        READ_ID(5, parent); WRITE_SELECTOR("body", object, 0, parent);
        if(strcmp(action, "add") != 0) {
            READ_ID(6, item);
            if(strcmp(action, "rename") == 0) {
                if(!editor_cli_id_selector_write(output, output_capacity, &used,
                        "--hitbox-id", item)) goto capacity_error;
            } else WRITE_SELECTOR("hitbox", object, parent, item);
        }
    } else if(strcmp(domain, "vertex") == 0 || strcmp(domain, "line") == 0) {
        READ_ID(5, parent); WRITE_SELECTOR("body", object, 0, parent);
        READ_ID(6, hitbox); WRITE_SELECTOR("hitbox", object, parent, hitbox);
        READ_ID(7, item);
        if(strcmp(action, "rename") == 0) {
            if(!editor_cli_id_selector_write(output, output_capacity, &used,
                    strcmp(domain, "line") == 0 ? "--line-index" : "--vertex-id",
                    item)) goto capacity_error;
        } else if(!editor_cli_hitbox_item_selector_write(project, output,
                output_capacity, &used, domain, object, parent, hitbox, item))
            goto capacity_error;
    } else if(strcmp(domain, "joint") == 0 && strcmp(action, "add") != 0) {
        READ_ID(5, item);
        if(strcmp(action, "rename") == 0) {
            if(!editor_cli_id_selector_write(output, output_capacity, &used,
                    "--joint-id", item)) goto capacity_error;
        } else WRITE_SELECTOR("joint", object, 0, item);
        if(strcmp(action, "connect") == 0) {
            if(strcmp(tokens[7], "none") == 0) { item = 0; skip[7] = true; }
            else READ_ID(7, item);
            WRITE_SELECTOR("anchor", object, 0, item);
        }
    } else if(strcmp(domain, "anchor") == 0) {
        if(strcmp(action, "add") == 0) {
            READ_ID(5, item); WRITE_SELECTOR("body", object, 0, item);
        } else {
            READ_ID(5, item);
            if(strcmp(action, "rename") == 0) {
                if(!editor_cli_id_selector_write(output, output_capacity, &used,
                        "--anchor-id", item)) goto capacity_error;
            } else WRITE_SELECTOR("anchor", object, 0, item);
            if(strcmp(action, "connect") == 0) {
                if(strcmp(tokens[7], "none") == 0) { item = 0; skip[7] = true; }
                else READ_ID(7, item);
                WRITE_SELECTOR("body", object, 0, item);
            }
        }
    } else if(strcmp(domain, "soft-body") == 0 && strcmp(action, "add") != 0) {
        READ_ID(5, item);
        if(strcmp(action, "rename") == 0) {
            if(!editor_cli_id_selector_write(output, output_capacity, &used,
                    "--soft-body-id", item)) goto capacity_error;
        } else WRITE_SELECTOR("soft-body", object, 0, item);
    } else if(strcmp(domain, "soft-node") == 0 || strcmp(domain, "soft-beam") == 0 ||
            strcmp(domain, "soft-area") == 0) {
        READ_ID(5, parent); WRITE_SELECTOR("soft-body", object, 0, parent);
        if(strcmp(domain, "soft-node") == 0) {
            if(strcmp(action, "add") != 0) {
                READ_ID(6, item);
                if(strcmp(action, "rename") == 0) {
                    if(!editor_cli_id_selector_write(output, output_capacity, &used,
                            "--node-id", item)) goto capacity_error;
                } else WRITE_SELECTOR("node", object, parent, item);
            }
        } else if(strcmp(domain, "soft-area") == 0) {
            READ_ID(6, item);
            WRITE_SELECTOR("area", object, parent, item);
        } else if(strcmp(action, "add") == 0) {
            READ_ID(6, item);
            if(!editor_cli_node_endpoint_selector_write(project, output,
                    output_capacity, &used, "node-a", object, parent, item))
                goto capacity_error;
            READ_ID(7, item);
            if(!editor_cli_node_endpoint_selector_write(project, output,
                    output_capacity, &used, "node-b", object, parent, item))
                goto capacity_error;
        } else {
            READ_ID(6, item);
            if(strcmp(action, "rename") == 0) {
                if(!editor_cli_id_selector_write(output, output_capacity, &used,
                        "--beam-id", item)) goto capacity_error;
            } else WRITE_SELECTOR("beam", object, parent, item);
            if(strcmp(action, "connect") == 0) {
                if(strcmp(tokens[8], "none") == 0) { item = 0; skip[8] = true; }
                else READ_ID(8, item);
                WRITE_SELECTOR("node", object, parent, item);
            }
        }
    }
#undef READ_ID
#undef WRITE_SELECTOR
    for(size_t i = 4; i < token_count; i += 1)
        if(!skip[i] && !editor_cli_token_append(output, output_capacity, &used, tokens[i]))
            goto capacity_error;
    return editor_result_value(true);
capacity_error:
    return editor_result_error(EDITOR_ERROR_CAPACITY,
        "Named editor CLI command exceeds output capacity");
}
