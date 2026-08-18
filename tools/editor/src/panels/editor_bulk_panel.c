#include "editor_bulk_panel.h"
#include "editor_command.h"
#include "editor_navigation.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum EditorBulkValueKind {
    EDITOR_BULK_FLOAT,
    EDITOR_BULK_CHECKBOX,
    EDITOR_BULK_MOTION_DROPDOWN,
    EDITOR_BULK_ROTATION_DROPDOWN,
    EDITOR_BULK_COLOR,
    EDITOR_BULK_JOINT_KIND_DROPDOWN,
    EDITOR_BULK_VISUAL_SIZE_SLIDER
} EditorBulkValueKind;

typedef enum EditorBulkTargetKind {
    EDITOR_BULK_PROPERTY,
    EDITOR_BULK_VISIBILITY,
    EDITOR_BULK_POSITION_X,
    EDITOR_BULK_POSITION_Y,
    EDITOR_BULK_ROTATION
} EditorBulkTargetKind;

typedef struct EditorBulkProperty {
    const char *name;
    EditorBulkTargetKind target;
    EditorBulkValueKind value;
    EditorPropertyKind property;
} EditorBulkProperty;

static const EditorBulkProperty object_properties[] = {
    {"Visible", EDITOR_BULK_VISIBILITY, EDITOR_BULK_CHECKBOX, 0},
    {"X", EDITOR_BULK_POSITION_X, EDITOR_BULK_FLOAT, 0},
    {"Y", EDITOR_BULK_POSITION_Y, EDITOR_BULK_FLOAT, 0}
};
static const EditorBulkProperty rigid_body_properties[] = {
    {"Visible", EDITOR_BULK_VISIBILITY, EDITOR_BULK_CHECKBOX, 0},
    {"X", EDITOR_BULK_POSITION_X, EDITOR_BULK_FLOAT, 0},
    {"Y", EDITOR_BULK_POSITION_Y, EDITOR_BULK_FLOAT, 0},
    {"Rotation", EDITOR_BULK_ROTATION, EDITOR_BULK_FLOAT, 0},
    {"Mass", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT, EDITOR_PROPERTY_MASS},
    {"Friction", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT, EDITOR_PROPERTY_FRICTION},
    {"Restitution", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT, EDITOR_PROPERTY_RESTITUTION},
    {"Gravity", EDITOR_BULK_PROPERTY, EDITOR_BULK_CHECKBOX, EDITOR_PROPERTY_GRAVITY},
    {"Motion", EDITOR_BULK_PROPERTY, EDITOR_BULK_MOTION_DROPDOWN, EDITOR_PROPERTY_STATIC},
    {"Rotation", EDITOR_BULK_PROPERTY, EDITOR_BULK_ROTATION_DROPDOWN,
        EDITOR_PROPERTY_ROTATION_LOCKED},
    {"Collision", EDITOR_BULK_PROPERTY, EDITOR_BULK_CHECKBOX, EDITOR_PROPERTY_COLLISION},
    {"Particle", EDITOR_BULK_PROPERTY, EDITOR_BULK_CHECKBOX, EDITOR_PROPERTY_PARTICLE},
    {"Particle Radius", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT,
        EDITOR_PROPERTY_PARTICLE_RADIUS},
    {"Particle Auto Fit", EDITOR_BULK_PROPERTY, EDITOR_BULK_CHECKBOX,
        EDITOR_PROPERTY_PARTICLE_AUTO_FIT},
    {"Outline Color", EDITOR_BULK_PROPERTY, EDITOR_BULK_COLOR,
        EDITOR_PROPERTY_OUTLINE_COLOR},
    {"Surface Color", EDITOR_BULK_PROPERTY, EDITOR_BULK_COLOR,
        EDITOR_PROPERTY_SURFACE_COLOR},
    {"Particle Ring Color", EDITOR_BULK_PROPERTY, EDITOR_BULK_COLOR,
        EDITOR_PROPERTY_PARTICLE_RING_COLOR},
    {"Particle Fill Color", EDITOR_BULK_PROPERTY, EDITOR_BULK_COLOR,
        EDITOR_PROPERTY_PARTICLE_FILL_COLOR}
};
static const EditorBulkProperty visible_properties[] = {
    {"Visible", EDITOR_BULK_VISIBILITY, EDITOR_BULK_CHECKBOX, 0}
};
static const EditorBulkProperty joint_properties[] = {
    {"Visible", EDITOR_BULK_VISIBILITY, EDITOR_BULK_CHECKBOX, 0},
    {"Kind", EDITOR_BULK_PROPERTY, EDITOR_BULK_JOINT_KIND_DROPDOWN,
        EDITOR_PROPERTY_JOINT_KIND},
    {"Visual Size", EDITOR_BULK_PROPERTY, EDITOR_BULK_VISUAL_SIZE_SLIDER,
        EDITOR_PROPERTY_VISUAL_SIZE},
    {"Rest Length", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT,
        EDITOR_PROPERTY_REST_LENGTH},
    {"Stiffness", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT,
        EDITOR_PROPERTY_STIFFNESS},
    {"Damping", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT,
        EDITOR_PROPERTY_DAMPING}
};
static const EditorBulkProperty anchor_properties[] = {
    {"Visible", EDITOR_BULK_VISIBILITY, EDITOR_BULK_CHECKBOX, 0},
    {"X", EDITOR_BULK_POSITION_X, EDITOR_BULK_FLOAT, 0},
    {"Y", EDITOR_BULK_POSITION_Y, EDITOR_BULK_FLOAT, 0},
    {"Rotation", EDITOR_BULK_ROTATION, EDITOR_BULK_FLOAT, 0},
    {"Position Follows Body", EDITOR_BULK_PROPERTY, EDITOR_BULK_CHECKBOX,
        EDITOR_PROPERTY_POSITION_FOLLOWS_BODY},
    {"Rotation Follows Body", EDITOR_BULK_PROPERTY, EDITOR_BULK_CHECKBOX,
        EDITOR_PROPERTY_ROTATION_FOLLOWS_BODY}
};
static const EditorBulkProperty soft_body_properties[] = {
    {"Visible", EDITOR_BULK_VISIBILITY, EDITOR_BULK_CHECKBOX, 0},
    {"X", EDITOR_BULK_POSITION_X, EDITOR_BULK_FLOAT, 0},
    {"Y", EDITOR_BULK_POSITION_Y, EDITOR_BULK_FLOAT, 0},
    {"Rotation", EDITOR_BULK_ROTATION, EDITOR_BULK_FLOAT, 0},
    {"Node Color", EDITOR_BULK_PROPERTY, EDITOR_BULK_COLOR,
        EDITOR_PROPERTY_NODE_COLOR},
    {"Beam Color", EDITOR_BULK_PROPERTY, EDITOR_BULK_COLOR,
        EDITOR_PROPERTY_BEAM_COLOR},
    {"Area Color", EDITOR_BULK_PROPERTY, EDITOR_BULK_COLOR,
        EDITOR_PROPERTY_AREA_COLOR}
};
static const EditorBulkProperty soft_node_properties[] = {
    {"Visible", EDITOR_BULK_VISIBILITY, EDITOR_BULK_CHECKBOX, 0},
    {"X", EDITOR_BULK_POSITION_X, EDITOR_BULK_FLOAT, 0},
    {"Y", EDITOR_BULK_POSITION_Y, EDITOR_BULK_FLOAT, 0},
    {"Mass", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT, EDITOR_PROPERTY_MASS},
    {"Friction", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT, EDITOR_PROPERTY_FRICTION},
    {"Restitution", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT,
        EDITOR_PROPERTY_RESTITUTION},
    {"Gravity", EDITOR_BULK_PROPERTY, EDITOR_BULK_CHECKBOX, EDITOR_PROPERTY_GRAVITY},
    {"Collision", EDITOR_BULK_PROPERTY, EDITOR_BULK_CHECKBOX, EDITOR_PROPERTY_COLLISION},
    {"Radius", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT,
        EDITOR_PROPERTY_NODE_RADIUS},
    {"Color", EDITOR_BULK_PROPERTY, EDITOR_BULK_COLOR, EDITOR_PROPERTY_COLOR}
};
static const EditorBulkProperty soft_beam_properties[] = {
    {"Visible", EDITOR_BULK_VISIBILITY, EDITOR_BULK_CHECKBOX, 0},
    {"Stiffness", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT,
        EDITOR_PROPERTY_STIFFNESS},
    {"Damping", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT,
        EDITOR_PROPERTY_DAMPING},
    {"Color", EDITOR_BULK_PROPERTY, EDITOR_BULK_COLOR, EDITOR_PROPERTY_COLOR}
};
static const EditorBulkProperty soft_area_properties[] = {
    {"Visible", EDITOR_BULK_VISIBILITY, EDITOR_BULK_CHECKBOX, 0},
    {"Color", EDITOR_BULK_PROPERTY, EDITOR_BULK_COLOR, EDITOR_PROPERTY_COLOR}
};
static const EditorBulkProperty vertex_properties[] = {
    {"X", EDITOR_BULK_POSITION_X, EDITOR_BULK_FLOAT, 0},
    {"Y", EDITOR_BULK_POSITION_Y, EDITOR_BULK_FLOAT, 0},
    {"Position Locked", EDITOR_BULK_PROPERTY, EDITOR_BULK_CHECKBOX,
        EDITOR_PROPERTY_POSITION_LOCKED}
};
static const EditorBulkProperty line_properties[] = {
    {"Length", EDITOR_BULK_PROPERTY, EDITOR_BULK_FLOAT,
        EDITOR_PROPERTY_LINE_LENGTH}
};
static const EditorBulkProperty origin_properties[] = {
    {"X", EDITOR_BULK_POSITION_X, EDITOR_BULK_FLOAT, 0},
    {"Y", EDITOR_BULK_POSITION_Y, EDITOR_BULK_FLOAT, 0}
};
static const EditorBulkProperty mixed_position_properties[] = {
    {"X", EDITOR_BULK_POSITION_X, EDITOR_BULK_FLOAT, 0},
    {"Y", EDITOR_BULK_POSITION_Y, EDITOR_BULK_FLOAT, 0}
};
static const EditorBulkProperty mixed_transform_properties[] = {
    {"X", EDITOR_BULK_POSITION_X, EDITOR_BULK_FLOAT, 0},
    {"Y", EDITOR_BULK_POSITION_Y, EDITOR_BULK_FLOAT, 0},
    {"Rotation", EDITOR_BULK_ROTATION, EDITOR_BULK_FLOAT, 0}
};

static bool editor_bulk_text_create(FontAsset *font, const char *value,
        TextAsset *text) {
    TextAssetResult result = rohr_graphics_text_create(font, value,
        (Color){230, 234, 242, 255});
    if(rohr_error_check(result)) return false;
    *text = result.result.value;
    return true;
}

static const EditorBulkProperty *editor_bulk_properties_get(
        EditorHierarchySelection kind, size_t *count) {
#define EDITOR_BULK_LIST(items) do { *count = sizeof(items) / sizeof(items[0]); \
    return items; } while(0)
    switch(kind) {
        case EDITOR_SELECTION_OBJECT: EDITOR_BULK_LIST(object_properties);
        case EDITOR_SELECTION_RIGID_BODY:
        case EDITOR_SELECTION_PARTICLE: EDITOR_BULK_LIST(rigid_body_properties);
        case EDITOR_SELECTION_HITBOX: EDITOR_BULK_LIST(visible_properties);
        case EDITOR_SELECTION_JOINT: EDITOR_BULK_LIST(joint_properties);
        case EDITOR_SELECTION_ANCHOR: EDITOR_BULK_LIST(anchor_properties);
        case EDITOR_SELECTION_SOFT_BODY: EDITOR_BULK_LIST(soft_body_properties);
        case EDITOR_SELECTION_SOFT_NODE: EDITOR_BULK_LIST(soft_node_properties);
        case EDITOR_SELECTION_SOFT_BEAM: EDITOR_BULK_LIST(soft_beam_properties);
        case EDITOR_SELECTION_SOFT_AREA: EDITOR_BULK_LIST(soft_area_properties);
        case EDITOR_SELECTION_VERTEX: EDITOR_BULK_LIST(vertex_properties);
        case EDITOR_SELECTION_LINE: EDITOR_BULK_LIST(line_properties);
        case EDITOR_SELECTION_ORIGIN: EDITOR_BULK_LIST(origin_properties);
        default: *count = 0; return NULL;
    }
#undef EDITOR_BULK_LIST
}

static const EditorBulkProperty *editor_bulk_mixed_properties_get(
        const EditorViewportState *state, size_t *count) {
    bool rotation = true;
    if(state == NULL || count == NULL) return NULL;
    for(size_t i = 0; i < state->selected_item_count; i += 1) {
        switch(state->selected_items[i].kind) {
            case EDITOR_SELECTION_RIGID_BODY:
            case EDITOR_SELECTION_PARTICLE:
            case EDITOR_SELECTION_SOFT_BODY:
            case EDITOR_SELECTION_ANCHOR:
                break;
            case EDITOR_SELECTION_OBJECT:
            case EDITOR_SELECTION_SOFT_NODE:
            case EDITOR_SELECTION_VERTEX:
            case EDITOR_SELECTION_ORIGIN:
                rotation = false;
                break;
            default:
                *count = 0;
                return NULL;
        }
    }
    if(rotation) {
        *count = sizeof(mixed_transform_properties) /
            sizeof(mixed_transform_properties[0]);
        return mixed_transform_properties;
    }
    *count = sizeof(mixed_position_properties) /
        sizeof(mixed_position_properties[0]);
    return mixed_position_properties;
}

float editor_bulk_panel_content_height_get(const EditorViewportState *state) {
    size_t count = 0;
    if(state == NULL || state->selected_item_count < 2) return 0.0f;
    if(editor_viewport_selection_homogeneous_check(state))
        (void)editor_bulk_properties_get(state->selected_items[0].kind, &count);
    else (void)editor_bulk_mixed_properties_get(state, &count);
    return 150.0f + (float)count * 36.0f;
}

static EditorItemKind editor_bulk_item_kind_get(EditorHierarchySelection kind) {
    switch(kind) {
        case EDITOR_SELECTION_RIGID_BODY:
        case EDITOR_SELECTION_PARTICLE: return EDITOR_ITEM_RIGID_BODY;
        case EDITOR_SELECTION_HITBOX: return EDITOR_ITEM_HITBOX;
        case EDITOR_SELECTION_JOINT: return EDITOR_ITEM_JOINT;
        case EDITOR_SELECTION_ANCHOR: return EDITOR_ITEM_ANCHOR;
        case EDITOR_SELECTION_SOFT_BODY: return EDITOR_ITEM_SOFT_BODY;
        case EDITOR_SELECTION_SOFT_NODE: return EDITOR_ITEM_SOFT_NODE;
        case EDITOR_SELECTION_SOFT_BEAM: return EDITOR_ITEM_SOFT_BEAM;
        case EDITOR_SELECTION_SOFT_AREA: return EDITOR_ITEM_SOFT_AREA;
        case EDITOR_SELECTION_VERTEX: return EDITOR_ITEM_VERTEX;
        case EDITOR_SELECTION_LINE: return EDITOR_ITEM_LINE;
        default: return EDITOR_ITEM_OBJECT;
    }
}

static bool editor_bulk_delete_check(const EditorViewportState *state) {
    if(state == NULL) return false;
    for(size_t i = 0; i < state->selected_item_count; i += 1) {
        if(state->selected_items[i].kind == EDITOR_SELECTION_ORIGIN ||
                state->selected_items[i].kind == EDITOR_SELECTION_SOFT_AREA)
            return false;
    }
    return true;
}

static bool editor_bulk_float_parse(const char *text, float *value) {
    char *end;
    float parsed;
    if(text == NULL || text[0] == '\0' || value == NULL) return false;
    errno = 0;
    parsed = strtof(text, &end);
    if(errno != 0 || end == text || *end != '\0' || !isfinite(parsed)) return false;
    *value = parsed;
    return true;
}

static bool editor_bulk_bool_parse(const char *text, bool *value) {
    if(text == NULL || value == NULL) return false;
    if(strcmp(text, "true") == 0 || strcmp(text, "1") == 0 ||
            strcmp(text, "on") == 0) *value = true;
    else if(strcmp(text, "false") == 0 || strcmp(text, "0") == 0 ||
            strcmp(text, "off") == 0) *value = false;
    else return false;
    return true;
}

static bool editor_bulk_color_parse(const char *text, uint32_t *color) {
    const char *digits = text;
    char *end;
    unsigned long parsed;
    size_t length;
    if(text == NULL || color == NULL) return false;
    if(digits[0] == '#') digits += 1;
    length = strlen(digits);
    if(length != 6 && length != 8) return false;
    errno = 0;
    parsed = strtoul(digits, &end, 16);
    if(errno != 0 || *end != '\0' || parsed > UINT32_MAX) return false;
    if(length == 6) parsed = (parsed << 8) | 0xFFu;
    *color = (uint32_t)parsed;
    return true;
}

static bool editor_bulk_joint_kind_parse(const char *text, uint32_t *kind) {
    if(text == NULL || kind == NULL) return false;
    if(strcmp(text, "revolute") == 0 || strcmp(text, "pin") == 0)
        *kind = EDITOR_JOINT_REVOLUTE;
    else if(strcmp(text, "weld") == 0) *kind = EDITOR_JOINT_WELD;
    else if(strcmp(text, "spring") == 0) *kind = EDITOR_JOINT_SPRING;
    else return false;
    return true;
}

static EditorObject *editor_bulk_object_get(EditorProject *project,
        EditorObjectId id) {
    if(project == NULL) return NULL;
    for(size_t i = 0; i < project->object_count; i += 1)
        if(project->objects[i].id == id) return &project->objects[i];
    return NULL;
}

static bool editor_bulk_transform_command_get(EditorProject *project,
        EditorSelectionRef ref, EditorBulkTargetKind target, float value,
        EditorCommand *command) {
    EditorObject *object = editor_bulk_object_get(project, ref.object);
    if(object == NULL || command == NULL) return false;
    if(ref.kind == EDITOR_SELECTION_OBJECT) {
        Position position = object->position;
        if(target == EDITOR_BULK_POSITION_X) position.x = value;
        else if(target == EDITOR_BULK_POSITION_Y) position.y = value;
        else return false;
        *command = (EditorCommand){.type = EDITOR_COMMAND_OBJECT_POSITION,
            .data.object_position = {ref.object, position}};
        return true;
    }
    if(ref.kind == EDITOR_SELECTION_RIGID_BODY ||
            ref.kind == EDITOR_SELECTION_PARTICLE) {
        EditorRigidBody *body = editor_project_rigid_body_get(object, ref.item);
        if(body == NULL) return false;
        Position position = body->position;
        float rotation = body->rotation;
        if(target == EDITOR_BULK_POSITION_X) position.x = value;
        else if(target == EDITOR_BULK_POSITION_Y) position.y = value;
        else if(target == EDITOR_BULK_ROTATION) rotation = value;
        else return false;
        *command = (EditorCommand){.type = EDITOR_COMMAND_RIGID_BODY_TRANSFORM,
            .data.rigid_body_transform = {ref.object, ref.item, position, rotation}};
        return true;
    }
    if(ref.kind == EDITOR_SELECTION_ANCHOR) {
        EditorAnchor *anchor = editor_project_anchor_get(object, ref.item);
        if(anchor == NULL) return false;
        Position position = anchor->position;
        float rotation = anchor->rotation;
        if(target == EDITOR_BULK_POSITION_X) position.x = value;
        else if(target == EDITOR_BULK_POSITION_Y) position.y = value;
        else if(target == EDITOR_BULK_ROTATION) rotation = value;
        else return false;
        *command = (EditorCommand){.type = EDITOR_COMMAND_ANCHOR_TRANSFORM,
            .data.anchor_transform = {ref.object, ref.item, position, rotation}};
        return true;
    }
    if(ref.kind == EDITOR_SELECTION_SOFT_BODY) {
        EditorSoftBody *body = NULL;
        for(size_t i = 0; i < object->soft_body_count; i += 1)
            if(object->soft_body_items[i].id == ref.item)
                body = &object->soft_body_items[i];
        if(body == NULL) return false;
        Position position = body->position;
        float rotation = body->rotation;
        if(target == EDITOR_BULK_POSITION_X) position.x = value;
        else if(target == EDITOR_BULK_POSITION_Y) position.y = value;
        else if(target == EDITOR_BULK_ROTATION) rotation = value;
        else return false;
        *command = (EditorCommand){.type = EDITOR_COMMAND_SOFT_BODY_TRANSFORM,
            .data.soft_body_transform = {ref.object, ref.item, position, rotation}};
        return true;
    }
    if(ref.kind == EDITOR_SELECTION_SOFT_NODE) {
        EditorSoftBody *body = NULL;
        EditorSoftNode *node = NULL;
        for(size_t i = 0; i < object->soft_body_count; i += 1)
            if(object->soft_body_items[i].id == ref.parent)
                body = &object->soft_body_items[i];
        if(body != NULL) for(size_t i = 0; i < body->node_count; i += 1)
            if(body->nodes[i].id == ref.item) node = &body->nodes[i];
        if(node == NULL) return false;
        Position position = node->position;
        if(target == EDITOR_BULK_POSITION_X) position.x = value;
        else if(target == EDITOR_BULK_POSITION_Y) position.y = value;
        else return false;
        *command = (EditorCommand){.type = EDITOR_COMMAND_SOFT_NODE_POSITION,
            .data.soft_node_position = {ref.object, ref.parent, ref.item, position}};
        return true;
    }
    if(ref.kind == EDITOR_SELECTION_VERTEX) {
        EditorRigidBody *body = editor_project_rigid_body_get(object, ref.parent);
        EditorHitbox *hitbox = editor_project_hitbox_get(body, ref.container);
        EditorVertex *vertex = NULL;
        if(hitbox != NULL) for(uint32_t i = 0; i < hitbox->vertex_count; i += 1)
            if(hitbox->vertices[i].id == ref.item) vertex = &hitbox->vertices[i];
        if(vertex == NULL) return false;
        Position position = vertex->position;
        if(target == EDITOR_BULK_POSITION_X) position.x = value;
        else if(target == EDITOR_BULK_POSITION_Y) position.y = value;
        else return false;
        *command = (EditorCommand){.type = EDITOR_COMMAND_VERTEX_POSITION,
            .data.vertex_position = {ref.object, ref.parent, ref.container,
                ref.item, position}};
        return true;
    }
    if(ref.kind == EDITOR_SELECTION_ORIGIN) {
        Position position;
        if(ref.parent == EDITOR_ORIGIN_RIGID_BODY) {
            EditorRigidBody *body = editor_project_rigid_body_get(object, ref.item);
            if(body == NULL) return false;
            position = body->position;
            if(target == EDITOR_BULK_POSITION_X) position.x = value;
            else if(target == EDITOR_BULK_POSITION_Y) position.y = value;
            else return false;
            *command = (EditorCommand){.type = EDITOR_COMMAND_RIGID_BODY_ORIGIN,
                .data.origin = {ref.object, ref.item, position}};
            return true;
        }
        if(ref.parent == EDITOR_ORIGIN_SOFT_BODY) {
            EditorSoftBody *body = NULL;
            for(size_t i = 0; i < object->soft_body_count; i += 1)
                if(object->soft_body_items[i].id == ref.item)
                    body = &object->soft_body_items[i];
            if(body == NULL) return false;
            position = body->position;
            if(target == EDITOR_BULK_POSITION_X) position.x = value;
            else if(target == EDITOR_BULK_POSITION_Y) position.y = value;
            else return false;
            *command = (EditorCommand){.type = EDITOR_COMMAND_SOFT_BODY_ORIGIN,
                .data.origin = {ref.object, ref.item, position}};
            return true;
        }
    }
    return false;
}

static bool editor_bulk_visibility_command_get(EditorSelectionRef ref,
        bool visible, EditorCommand *command) {
    EditorVisibilityKind kind;
    switch(ref.kind) {
        case EDITOR_SELECTION_OBJECT: kind = EDITOR_VISIBILITY_OBJECT; break;
        case EDITOR_SELECTION_RIGID_BODY:
        case EDITOR_SELECTION_PARTICLE: kind = EDITOR_VISIBILITY_RIGID_BODY; break;
        case EDITOR_SELECTION_HITBOX: kind = EDITOR_VISIBILITY_HITBOX; break;
        case EDITOR_SELECTION_JOINT: kind = EDITOR_VISIBILITY_JOINT; break;
        case EDITOR_SELECTION_ANCHOR: kind = EDITOR_VISIBILITY_ANCHOR; break;
        case EDITOR_SELECTION_SOFT_BODY: kind = EDITOR_VISIBILITY_SOFT_BODY; break;
        case EDITOR_SELECTION_SOFT_NODE: kind = EDITOR_VISIBILITY_SOFT_NODE; break;
        case EDITOR_SELECTION_SOFT_BEAM: kind = EDITOR_VISIBILITY_SOFT_BEAM; break;
        case EDITOR_SELECTION_SOFT_AREA: kind = EDITOR_VISIBILITY_SOFT_AREA; break;
        default: return false;
    }
    *command = (EditorCommand){.type = EDITOR_COMMAND_VISIBILITY,
        .data.visibility = {.kind = kind, .object = ref.object,
            .parent = ref.parent, .item = ref.item, .visible = visible}};
    return true;
}

static bool editor_bulk_property_command_get(EditorSelectionRef ref,
        const EditorBulkProperty *property, const char *text,
        EditorCommand *command) {
    EditorPropertySetCommand set = {.kind = editor_bulk_item_kind_get(ref.kind),
        .object = ref.object, .parent = ref.parent,
        .item = ref.kind == EDITOR_SELECTION_VERTEX ||
                ref.kind == EDITOR_SELECTION_LINE ? ref.container : ref.item,
        .index = ref.kind == EDITOR_SELECTION_VERTEX ||
                ref.kind == EDITOR_SELECTION_LINE ? ref.item : 0,
        .property = property->property};
    if(property->value == EDITOR_BULK_FLOAT ||
            property->value == EDITOR_BULK_VISUAL_SIZE_SLIDER) {
        set.value_kind = EDITOR_PROPERTY_VALUE_FLOAT;
        if(!editor_bulk_float_parse(text, &set.value.number)) return false;
    } else if(property->value == EDITOR_BULK_CHECKBOX ||
            property->value == EDITOR_BULK_MOTION_DROPDOWN ||
            property->value == EDITOR_BULK_ROTATION_DROPDOWN) {
        set.value_kind = EDITOR_PROPERTY_VALUE_BOOL;
        if(!editor_bulk_bool_parse(text, &set.value.boolean)) return false;
    } else if(property->value == EDITOR_BULK_COLOR) {
        set.value_kind = EDITOR_PROPERTY_VALUE_UINT;
        if(!editor_bulk_color_parse(text, &set.value.integer)) return false;
    } else {
        set.value_kind = EDITOR_PROPERTY_VALUE_UINT;
        if(!editor_bulk_joint_kind_parse(text, &set.value.integer)) return false;
    }
    *command = (EditorCommand){.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = set};
    return true;
}

bool editor_bulk_property_set(EditorProject *project, EditorViewportState *state,
        EditorHistory *history, const EditorPropertySetCommand *property) {
    if(project == NULL || state == NULL || history == NULL || property == NULL ||
            state->selected_item_count < 2 ||
            !editor_history_transaction_begin(history)) return false;
    for(size_t i = 0; i < state->selected_item_count; i += 1)
        if(!editor_history_transaction_object_track(history,
                state->selected_items[i].object)) {
            editor_history_transaction_cancel(history);
            return false;
        }
    for(size_t i = 0; i < state->selected_item_count; i += 1) {
        EditorSelectionRef ref = state->selected_items[i];
        EditorPropertySetCommand set = *property;
        EditorCommand command;
        EditorCommandResult result;
        set.kind = editor_bulk_item_kind_get(ref.kind);
        set.object = ref.object;
        set.parent = ref.parent;
        set.item = ref.kind == EDITOR_SELECTION_VERTEX ||
                ref.kind == EDITOR_SELECTION_LINE ? ref.container : ref.item;
        set.index = ref.kind == EDITOR_SELECTION_VERTEX ||
                ref.kind == EDITOR_SELECTION_LINE ? ref.item : 0;
        command = (EditorCommand){.type = EDITOR_COMMAND_PROPERTY_SET,
            .data.property_set = set};
        result = editor_command_execute(project, &command);
        if(result.kind == ERROR_RESULT_ERROR) {
            editor_history_transaction_cancel(history);
            return false;
        }
    }
    return editor_history_transaction_end(history);
}

static bool editor_bulk_apply(EditorProject *project, EditorViewportState *state,
        EditorHistory *history, const EditorBulkProperty *property,
        const char *text) {
    float number = 0.0f;
    bool boolean = false;
    if(project == NULL || state == NULL || history == NULL || property == NULL ||
            text == NULL || text[0] == '\0') return false;
    if(property->target == EDITOR_BULK_VISIBILITY &&
            !editor_bulk_bool_parse(text, &boolean)) return false;
    if((property->target == EDITOR_BULK_POSITION_X ||
            property->target == EDITOR_BULK_POSITION_Y ||
            property->target == EDITOR_BULK_ROTATION) &&
            !editor_bulk_float_parse(text, &number)) return false;
    if(!editor_history_transaction_begin(history)) return false;
    for(size_t i = 0; i < state->selected_item_count; i += 1)
        if(!editor_history_transaction_object_track(history,
                state->selected_items[i].object)) {
            editor_history_transaction_cancel(history);
            return false;
        }
    for(size_t i = 0; i < state->selected_item_count; i += 1) {
        EditorCommand command;
        EditorCommandResult result;
        bool built;
        if(property->target == EDITOR_BULK_PROPERTY)
            built = editor_bulk_property_command_get(state->selected_items[i],
                property, text, &command);
        else if(property->target == EDITOR_BULK_VISIBILITY)
            built = editor_bulk_visibility_command_get(state->selected_items[i],
                boolean, &command);
        else built = editor_bulk_transform_command_get(project,
            state->selected_items[i], property->target, number, &command);
        if(!built) {
            editor_history_transaction_cancel(history);
            return false;
        }
        result = editor_command_execute(project, &command);
        if(result.kind == ERROR_RESULT_ERROR) {
            editor_history_transaction_cancel(history);
            return false;
        }
    }
    return editor_history_transaction_end(history);
}

bool editor_bulk_panel_create(EditorBulkPanel *panel, FontAsset *font) {
    if(panel == NULL || font == NULL) return false;
    memset(panel, 0, sizeof(*panel));
    if(!editor_bulk_text_create(font, "Multiple Selection", &panel->title))
        return false;
    if(!editor_bulk_text_create(font, "Delete Selected", &panel->delete_label)) {
        editor_bulk_panel_destroy(panel);
        return false;
    }
    if(!editor_bulk_text_create(font, "—", &panel->unset_label) ||
            !editor_bulk_text_create(font, "Dynamic", &panel->dynamic_label) ||
            !editor_bulk_text_create(font, "Static", &panel->static_label) ||
            !editor_bulk_text_create(font, "Rotation: Unlocked",
                &panel->rotation_unlocked_label) ||
            !editor_bulk_text_create(font, "Rotation: Locked",
                &panel->rotation_locked_label) ||
            !editor_bulk_text_create(font, "Revolute", &panel->revolute_label) ||
            !editor_bulk_text_create(font, "Weld", &panel->weld_label) ||
            !editor_bulk_text_create(font, "Spring", &panel->spring_label)) {
        editor_bulk_panel_destroy(panel);
        return false;
    }
    for(size_t i = 0; i < EDITOR_BULK_PROPERTY_MAX; i += 1) {
        if(!editor_bulk_text_create(font, "", &panel->labels[i]) ||
                !editor_bulk_text_create(font, "", &panel->fields[i])) {
            editor_bulk_panel_destroy(panel);
            return false;
        }
    }
    return true;
}

void editor_bulk_panel_destroy(EditorBulkPanel *panel) {
    if(panel == NULL) return;
    rohr_graphics_text_destroy(&panel->title);
    rohr_graphics_text_destroy(&panel->delete_label);
    rohr_graphics_text_destroy(&panel->unset_label);
    rohr_graphics_text_destroy(&panel->dynamic_label);
    rohr_graphics_text_destroy(&panel->static_label);
    rohr_graphics_text_destroy(&panel->rotation_unlocked_label);
    rohr_graphics_text_destroy(&panel->rotation_locked_label);
    rohr_graphics_text_destroy(&panel->revolute_label);
    rohr_graphics_text_destroy(&panel->weld_label);
    rohr_graphics_text_destroy(&panel->spring_label);
    for(size_t i = 0; i < EDITOR_BULK_PROPERTY_MAX; i += 1) {
        rohr_graphics_text_destroy(&panel->labels[i]);
        rohr_graphics_text_destroy(&panel->fields[i]);
    }
    memset(panel, 0, sizeof(*panel));
}

static bool editor_bulk_checkbox_draw(const char *id, UIRect bounds,
        bool assigned, bool *checked) {
    UIButtonResult interaction = rohr_ui_interaction(id, bounds);
    UIRect box = {bounds.x + 4.0f, bounds.y + 4.0f,
        bounds.height - 8.0f, bounds.height - 8.0f};
    Color background = interaction.pressed ? (Color){58, 65, 78, 255} :
        interaction.hovered || interaction.focused ? (Color){67, 75, 90, 255} :
        (Color){48, 54, 66, 255};
    rohr_ui_surface(bounds, background);
    rohr_ui_surface(box, (Color){22, 25, 31, 255});
    rohr_ui_border(box, 2.0f, (Color){8, 9, 12, 255});
    if(assigned && *checked)
        rohr_ui_surface((UIRect){box.x + 5.0f, box.y + 5.0f,
            box.width - 10.0f, box.height - 10.0f},
            (Color){225, 230, 240, 255});
    else if(!assigned)
        rohr_ui_surface((UIRect){box.x + 4.0f, box.y + box.height * 0.5f - 1.0f,
            box.width - 8.0f, 2.0f}, (Color){160, 166, 178, 255});
    if(interaction.clicked) *checked = assigned ? !*checked : true;
    return interaction.clicked;
}

bool editor_bulk_panel_draw(EditorBulkPanel *panel, EditorProject *project,
        EditorViewportState *state, EditorHistory *history, float x, float width,
        EditorBulkColorOpen color_open, void *color_context) {
    const EditorBulkProperty *properties;
    size_t property_count;
    bool editing = false;
    if(panel == NULL || project == NULL || state == NULL || history == NULL ||
            state->selected_item_count < 2) return false;
    properties = editor_viewport_selection_homogeneous_check(state) ?
        editor_bulk_properties_get(state->selected_items[0].kind,
            &property_count) : editor_bulk_mixed_properties_get(state,
                &property_count);
    if(property_count > EDITOR_BULK_PROPERTY_MAX) return false;
    if(panel->kind != (editor_viewport_selection_homogeneous_check(state) ?
            state->selected_items[0].kind : EDITOR_SELECTION_NONE) ||
            panel->property_count != property_count) {
        panel->kind = editor_viewport_selection_homogeneous_check(state) ?
            state->selected_items[0].kind : EDITOR_SELECTION_NONE;
        panel->property_count = property_count;
        memset(panel->values, 0, sizeof(panel->values));
        memset(panel->assigned, 0, sizeof(panel->assigned));
        memset(panel->booleans, 0, sizeof(panel->booleans));
        memset(panel->dropdown_indices, 0, sizeof(panel->dropdown_indices));
        for(size_t i = 0; i < EDITOR_BULK_PROPERTY_MAX; i += 1)
            panel->slider_values[i] = 1.0f;
        for(size_t i = 0; i < EDITOR_BULK_PROPERTY_MAX; i += 1)
            panel->colors[i] = 0xffffffffu;
        for(size_t i = 0; i < property_count; i += 1)
            (void)rohr_graphics_text_value_set(&panel->labels[i], properties[i].name);
    }
    {
        char title[64];
        snprintf(title, sizeof(title), "%zu Selected", state->selected_item_count);
        (void)rohr_graphics_text_value_set(&panel->title, title);
    }
    rohr_ui_label(&panel->title, (UIRect){x + 10.0f, 42.0f, width - 20.0f, 30.0f});
    for(size_t i = 0; i < property_count; i += 1) {
        char id[64];
        float y = 84.0f + (float)i * 36.0f;
        UIRect control = {x + 126.0f, y, width - 136.0f, 26.0f};
        snprintf(id, sizeof(id), "editor.bulk.%u.%zu", (unsigned)panel->kind, i);
        rohr_ui_label(&panel->labels[i],
            (UIRect){x + 10.0f, y, 112.0f, 26.0f});
        if(properties[i].value == EDITOR_BULK_CHECKBOX) {
            if(editor_bulk_checkbox_draw(id, control, panel->assigned[i],
                    &panel->booleans[i])) {
                panel->assigned[i] = true;
                (void)editor_bulk_apply(project, state, history, &properties[i],
                    panel->booleans[i] ? "true" : "false");
            }
        } else if(properties[i].value == EDITOR_BULK_MOTION_DROPDOWN ||
                properties[i].value == EDITOR_BULK_ROTATION_DROPDOWN ||
                properties[i].value == EDITOR_BULK_JOINT_KIND_DROPDOWN) {
            const TextAsset *motion[] = {&panel->unset_label,
                &panel->dynamic_label, &panel->static_label};
            const TextAsset *rotation[] = {&panel->unset_label,
                &panel->rotation_unlocked_label, &panel->rotation_locked_label};
            const TextAsset *joint[] = {&panel->unset_label,
                &panel->revolute_label, &panel->weld_label, &panel->spring_label};
            const TextAsset *const *options = motion;
            size_t count = 3;
            UIDropdownResult result;
            if(properties[i].value == EDITOR_BULK_ROTATION_DROPDOWN)
                options = rotation;
            else if(properties[i].value == EDITOR_BULK_JOINT_KIND_DROPDOWN) {
                options = joint;
                count = 4;
            }
            result = rohr_ui_dropdown(id, options, count,
                panel->dropdown_indices[i], control, NULL);
            if(result.changed && result.selected_index > 0) {
                const char *value = result.selected_index == 1 ?
                    (properties[i].value == EDITOR_BULK_JOINT_KIND_DROPDOWN ?
                        "revolute" : "false") :
                    result.selected_index == 2 ?
                        (properties[i].value == EDITOR_BULK_JOINT_KIND_DROPDOWN ?
                            "weld" : "true") : "spring";
                panel->dropdown_indices[i] = result.selected_index;
                panel->assigned[i] = true;
                (void)editor_bulk_apply(project, state, history,
                    &properties[i], value);
            }
        } else if(properties[i].value == EDITOR_BULK_VISUAL_SIZE_SLIDER) {
            UISliderConfig slider = rohr_ui_slider_config_default_get();
            UISliderResult result;
            slider.center = (Position){control.x + control.width * 0.5f,
                control.y + control.height * 0.5f};
            slider.length = control.width;
            slider.min_value = 0.25f;
            slider.max_value = 3.0f;
            result = rohr_ui_slider(id, panel->slider_values[i], &slider);
            if(result.changed) {
                char value[32];
                panel->slider_values[i] = result.value;
                panel->assigned[i] = true;
                snprintf(value, sizeof(value), "%.6g", (double)result.value);
                (void)editor_bulk_apply(project, state, history,
                    &properties[i], value);
            }
        } else if(properties[i].value == EDITOR_BULK_COLOR) {
            UIButtonStyle style = rohr_ui_button_style_default_get();
            Color displayed = panel->assigned[i] ?
                rohr_graphics_color_hex_create(panel->colors[i]) :
                (Color){70, 72, 78, 255};
            UIButtonResult result;
            style.idle = displayed;
            style.hovered = (Color){displayed.red, displayed.green,
                displayed.blue, 220};
            style.pressed = displayed;
            result = rohr_ui_button(id, NULL, control, &style);
            rohr_ui_border(control, 2.0f, (Color){8, 9, 12, 255});
            if(result.clicked && color_open != NULL) {
                panel->assigned[i] = true;
                color_open(color_context, &panel->colors[i],
                    properties[i].property);
            }
        } else {
            UIFieldResult result = rohr_ui_field(id,
                (UIFieldBinding){.kind = UI_FIELD_STRING,
                    .string = panel->values[i],
                    .string_capacity = sizeof(panel->values[i])},
                &panel->fields[i], control, NULL);
            editing = editing || result.active;
            if(result.submitted && panel->values[i][0] != '\0' &&
                    editor_bulk_apply(project, state, history,
                        &properties[i], panel->values[i])) {
                panel->assigned[i] = true;
                panel->values[i][0] = '\0';
                (void)rohr_graphics_text_value_set(&panel->fields[i], "");
            }
        }
    }
    if(editor_bulk_delete_check(state)) {
        UIButtonStyle style = rohr_ui_button_style_default_get();
        float y = 96.0f + (float)property_count * 36.0f;
        style.idle = (Color){125, 42, 48, 255};
        style.hovered = (Color){165, 52, 60, 255};
        if(rohr_ui_button("editor.bulk.delete", &panel->delete_label,
                (UIRect){x + 10.0f, y, width - 20.0f, 32.0f}, &style).clicked)
            (void)editor_navigation_multi_selection_delete(project, state, history);
    }
    return editing;
}
