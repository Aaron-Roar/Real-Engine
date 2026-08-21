/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_project.h"
#include "editor_array.h"

#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool editor_name_c_keyword(const char *name) {
    static const char *keywords[] = {
        "auto", "break", "case", "char", "const", "continue", "default",
        "do", "double", "else", "enum", "extern", "float", "for", "goto",
        "if", "inline", "int", "long", "register", "restrict", "return",
        "short", "signed", "sizeof", "static", "struct", "switch", "typedef",
        "union", "unsigned", "void", "volatile", "while", "_alignas",
        "_alignof", "_atomic", "_bool", "_complex", "_generic", "_imaginary",
        "_noreturn", "_static_assert", "_thread_local"
    };
    for(size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i += 1) {
        if(strcmp(name, keywords[i]) == 0) return true;
    }
    return false;
}

static bool editor_name_word_start(const char *input, size_t index) {
    unsigned char current = (unsigned char)input[index];
    unsigned char previous = index == 0 ? 0 : (unsigned char)input[index - 1];
    unsigned char next = (unsigned char)input[index + 1];
    if(!isalnum(current)) return false;
    if(index == 0 || !isalnum(previous)) return true;
    if(isupper(current) && (islower(previous) || isdigit(previous))) return true;
    return isupper(current) && isupper(previous) && islower(next);
}

void editor_project_object_name_format(char *output, size_t capacity,
    const char *input) {
    char formatted[EDITOR_OBJECT_NAME_MAX] = {0};
    size_t written = 0;
    bool word = false;
    if(output == NULL || capacity == 0) return;
    if(input != NULL) {
        for(size_t i = 0; input[i] != '\0' && written + 1 < sizeof(formatted); i += 1) {
            unsigned char character = (unsigned char)input[i];
            bool start = editor_name_word_start(input, i);
            if(!isalnum(character)) {
                word = false;
                continue;
            }
            if(start || !word) {
                formatted[written++] = (char)toupper(character);
                word = true;
            } else {
                formatted[written++] = (char)tolower(character);
            }
        }
    }
    if(written == 0) snprintf(formatted, sizeof(formatted), "Object");
    if(isdigit((unsigned char)formatted[0])) {
        char prefixed[EDITOR_OBJECT_NAME_MAX];
        snprintf(prefixed, sizeof(prefixed), "Object%s", formatted);
        snprintf(formatted, sizeof(formatted), "%s", prefixed);
    }
    snprintf(output, capacity, "%s", formatted);
}

void editor_project_property_name_format(char *output, size_t capacity,
    const char *input) {
    char formatted[EDITOR_OBJECT_NAME_MAX] = {0};
    size_t written = 0;
    bool have_word = false;
    if(output == NULL || capacity == 0) return;
    if(input != NULL) {
        for(size_t i = 0; input[i] != '\0' && written + 1 < sizeof(formatted); i += 1) {
            unsigned char character = (unsigned char)input[i];
            bool start = editor_name_word_start(input, i);
            if(!isalnum(character)) continue;
            if(start && have_word && written + 2 < sizeof(formatted)) {
                formatted[written++] = '_';
            }
            formatted[written++] = (char)tolower(character);
            have_word = true;
        }
    }
    if(written == 0) snprintf(formatted, sizeof(formatted), "item");
    if(isdigit((unsigned char)formatted[0]) || editor_name_c_keyword(formatted)) {
        char prefixed[EDITOR_OBJECT_NAME_MAX];
        snprintf(prefixed, sizeof(prefixed), "item_%s", formatted);
        snprintf(formatted, sizeof(formatted), "%s", prefixed);
    }
    snprintf(output, capacity, "%s", formatted);
}

static uint32_t editor_vertex_count_clamp(uint32_t vertex_count) {
    if(vertex_count < EDITOR_HITBOX_VERTEX_MIN) {
        return EDITOR_HITBOX_VERTEX_MIN;
    }
    return vertex_count;
}

static bool editor_hitbox_vertices_reserve(EditorHitbox *hitbox,
        size_t required) {
    EditorVertex *vertices;
    char (*line_names)[EDITOR_OBJECT_NAME_MAX];
    size_t capacity;
    if(hitbox == NULL) return false;
    if(required <= hitbox->vertex_capacity) return true;
    capacity = hitbox->vertex_capacity == 0 ? 4 : hitbox->vertex_capacity;
    while(capacity < required) capacity *= 2;
    vertices = realloc(hitbox->vertices, capacity * sizeof(*vertices));
    if(vertices == NULL) return false;
    hitbox->vertices = vertices;
    line_names = realloc(hitbox->line_names, capacity * sizeof(*line_names));
    if(line_names == NULL) return false;
    hitbox->line_names = line_names;
    hitbox->vertex_capacity = capacity;
    return true;
}

static void editor_hitbox_regular_set(EditorProject *project, EditorHitbox *hitbox,
    uint32_t vertex_count) {
    const float radius = 70.0f;

    if(project == NULL || hitbox == NULL) return;
    vertex_count = editor_vertex_count_clamp(vertex_count);
    if(!editor_hitbox_vertices_reserve(hitbox, vertex_count)) return;
    hitbox->visible = true;
    hitbox->vertex_count = vertex_count;
    for(uint32_t i = 0; i < vertex_count; i += 1) {
        float angle = -1.57079632679f + 6.28318530718f *
            (float)i / (float)vertex_count;
        hitbox->vertices[i] = (EditorVertex){
            .id = project->next_vertex_id++,
            .position = {
                cosf(angle) * radius,
                sinf(angle) * radius
            }
        };
        snprintf(hitbox->vertices[i].name, sizeof(hitbox->vertices[i].name),
            "vertex_%u", i + 1);
        snprintf(hitbox->line_names[i], sizeof(hitbox->line_names[i]),
            "line_%u", i + 1);
    }
}

void editor_project_init(EditorProject *project) {
    if(project == NULL) return;
    *project = (EditorProject){
        .viewport_camera_zoom = 1.0f,
        .next_id = 1,
        .next_vertex_id = 1,
        .next_rigid_body_id = 1,
        .next_hitbox_id = 1,
        .next_joint_id = 1,
        .next_anchor_id = 1,
        .next_soft_body_id = 1,
        .next_soft_node_id = 1,
        .next_soft_beam_id = 1,
        .next_soft_area_id = 1,
        .next_sprite_id = 1,
        .next_animated_sprite_id = 1
    };
    if(EDITOR_ARRAY_RESERVE(project->collision_masks,
            project->collision_mask_capacity, EDITOR_COLLISION_MASK_MAX)) {
        project->collision_masks[0] = (EditorCollisionMask){0};
        snprintf(project->collision_masks[0].name,
            sizeof(project->collision_masks[0].name), "default");
        project->collision_mask_count = 1;
    }
    (void)EDITOR_ARRAY_RESERVE(project->objects, project->object_capacity,
        EDITOR_OBJECT_MAX);
}

void editor_project_animated_sprite_destroy(EditorAnimatedSprite *sprite) {
    if(sprite == NULL) return;
    free(sprite->frames);
    *sprite = (EditorAnimatedSprite){0};
}

static bool editor_project_animated_sprite_clone(EditorAnimatedSprite *destination,
        const EditorAnimatedSprite *source) {
    if(destination == NULL || source == NULL) return false;
    *destination = *source;
    destination->frames = NULL;
    destination->frame_capacity = 0;
    if(!EDITOR_ARRAY_RESERVE(destination->frames, destination->frame_capacity,
            source->frame_count)) return false;
    if(source->frame_count > 0) memcpy(destination->frames, source->frames,
        source->frame_count * sizeof(*source->frames));
    return true;
}

static bool editor_project_animated_sprite_copy_set(
        EditorAnimatedSprite *destination, const EditorAnimatedSprite *source) {
    EditorAnimationFrame *frames;
    size_t capacity;
    if(destination == NULL || source == NULL ||
            !EDITOR_ARRAY_RESERVE(destination->frames,
                destination->frame_capacity, source->frame_count)) return false;
    frames = destination->frames;
    capacity = destination->frame_capacity;
    *destination = *source;
    destination->frames = frames;
    destination->frame_capacity = capacity;
    if(source->frame_count > 0) memcpy(destination->frames, source->frames,
        source->frame_count * sizeof(*source->frames));
    return true;
}

static void editor_project_hitbox_destroy(EditorHitbox *hitbox) {
    if(hitbox == NULL) return;
    free(hitbox->vertices);
    free(hitbox->line_names);
    *hitbox = (EditorHitbox){0};
}

static bool editor_project_hitbox_clone(EditorHitbox *destination,
        const EditorHitbox *source) {
    if(destination == NULL || source == NULL) return false;
    *destination = *source;
    destination->vertices = NULL;
    destination->line_names = NULL;
    destination->vertex_capacity = 0;
    if(source->vertex_count == 0) return true;
    if(!EDITOR_ARRAY_RESERVE(destination->vertices,
            destination->vertex_capacity, source->vertex_count)) return false;
    destination->line_names = calloc(destination->vertex_capacity,
        sizeof(*destination->line_names));
    if(destination->line_names == NULL) {
        editor_project_hitbox_destroy(destination);
        return false;
    }
    memcpy(destination->vertices, source->vertices,
        source->vertex_count * sizeof(*source->vertices));
    memcpy(destination->line_names, source->line_names,
        source->vertex_count * sizeof(*source->line_names));
    return true;
}

static bool editor_project_hitbox_copy_set(EditorHitbox *destination,
        const EditorHitbox *source) {
    EditorVertex *vertices;
    char (*line_names)[EDITOR_OBJECT_NAME_MAX];
    size_t capacity;
    if(destination == NULL || source == NULL) return false;
    vertices = destination->vertices;
    line_names = destination->line_names;
    capacity = destination->vertex_capacity;
    destination->vertices = vertices;
    destination->line_names = line_names;
    destination->vertex_capacity = capacity;
    if(!editor_hitbox_vertices_reserve(destination, source->vertex_count))
        return false;
    vertices = destination->vertices;
    line_names = destination->line_names;
    capacity = destination->vertex_capacity;
    *destination = *source;
    destination->vertices = vertices;
    destination->line_names = line_names;
    destination->vertex_capacity = capacity;
    if(source->vertex_count > 0) {
        memcpy(destination->vertices, source->vertices,
            source->vertex_count * sizeof(*source->vertices));
        memcpy(destination->line_names, source->line_names,
            source->vertex_count * sizeof(*source->line_names));
    }
    return true;
}

void editor_project_rigid_body_destroy(EditorRigidBody *body) {
    if(body == NULL) return;
    for(size_t i = 0; i < body->hitbox_count; i += 1)
        editor_project_hitbox_destroy(&body->hitboxes[i]);
    free(body->hitboxes);
    free(body->hitbox_animation_bindings);
    *body = (EditorRigidBody){0};
}

bool editor_project_rigid_body_clone(EditorRigidBody *destination,
        const EditorRigidBody *source) {
    if(destination == NULL || source == NULL) return false;
    *destination = *source;
    destination->hitboxes = NULL;
    destination->hitbox_count = 0;
    destination->hitbox_capacity = 0;
    destination->hitbox_animation_bindings = NULL;
    destination->hitbox_animation_binding_count = 0;
    destination->hitbox_animation_binding_capacity = 0;
    if(!EDITOR_ARRAY_RESERVE(destination->hitboxes,
            destination->hitbox_capacity, source->hitbox_count)) return false;
    for(size_t i = 0; i < source->hitbox_count; i += 1) {
        if(!editor_project_hitbox_clone(&destination->hitboxes[i],
                &source->hitboxes[i])) {
            destination->hitbox_count = i;
            editor_project_rigid_body_destroy(destination);
            return false;
        }
        destination->hitbox_count += 1;
    }
    if(!EDITOR_ARRAY_RESERVE(destination->hitbox_animation_bindings,
            destination->hitbox_animation_binding_capacity,
            source->hitbox_animation_binding_count)) {
        editor_project_rigid_body_destroy(destination);
        return false;
    }
    memcpy(destination->hitbox_animation_bindings,
        source->hitbox_animation_bindings,
        source->hitbox_animation_binding_count *
            sizeof(*source->hitbox_animation_bindings));
    destination->hitbox_animation_binding_count =
        source->hitbox_animation_binding_count;
    return true;
}

bool editor_project_rigid_body_copy_set(EditorRigidBody *destination,
        const EditorRigidBody *source) {
    EditorHitbox *hitboxes;
    size_t capacity;
    size_t old_count;
    size_t old_capacity;
    EditorHitboxAnimationBinding *bindings;
    size_t binding_capacity;
    if(destination == NULL || source == NULL) return false;
    old_capacity = destination->hitbox_capacity;
    if(!EDITOR_ARRAY_RESERVE(destination->hitboxes,
            destination->hitbox_capacity, source->hitbox_count)) return false;
    if(destination->hitbox_capacity > old_capacity)
        memset(&destination->hitboxes[old_capacity], 0,
            (destination->hitbox_capacity - old_capacity) *
                sizeof(*destination->hitboxes));
    hitboxes = destination->hitboxes;
    capacity = destination->hitbox_capacity;
    old_count = destination->hitbox_count;
    if(!EDITOR_ARRAY_RESERVE(destination->hitbox_animation_bindings,
            destination->hitbox_animation_binding_capacity,
            source->hitbox_animation_binding_count)) return false;
    bindings = destination->hitbox_animation_bindings;
    binding_capacity = destination->hitbox_animation_binding_capacity;
    for(size_t i = 0; i < source->hitbox_count; i += 1)
        if(!editor_project_hitbox_copy_set(&hitboxes[i],
                &source->hitboxes[i])) return false;
    for(size_t i = source->hitbox_count; i < old_count; i += 1)
        editor_project_hitbox_destroy(&hitboxes[i]);
    *destination = *source;
    destination->hitboxes = hitboxes;
    destination->hitbox_capacity = capacity;
    destination->hitbox_animation_bindings = bindings;
    destination->hitbox_animation_binding_capacity = binding_capacity;
    if(source->hitbox_animation_binding_count > 0)
        memcpy(destination->hitbox_animation_bindings,
            source->hitbox_animation_bindings,
            source->hitbox_animation_binding_count *
                sizeof(*source->hitbox_animation_bindings));
    return true;
}

static void editor_project_soft_area_destroy(EditorSoftArea *area) {
    if(area == NULL) return;
    free(area->nodes);
    *area = (EditorSoftArea){0};
}

static bool editor_project_soft_area_clone(EditorSoftArea *destination,
        const EditorSoftArea *source) {
    if(destination == NULL || source == NULL) return false;
    *destination = *source;
    destination->nodes = NULL;
    destination->node_capacity = 0;
    if(!EDITOR_ARRAY_RESERVE(destination->nodes,
            destination->node_capacity, source->node_count)) return false;
    if(source->node_count > 0)
        memcpy(destination->nodes, source->nodes,
            source->node_count * sizeof(*source->nodes));
    return true;
}

static bool editor_project_soft_area_copy_set(EditorSoftArea *destination,
        const EditorSoftArea *source) {
    EditorSoftNodeId *nodes;
    size_t capacity;
    if(destination == NULL || source == NULL ||
            !EDITOR_ARRAY_RESERVE(destination->nodes,
                destination->node_capacity, source->node_count)) return false;
    nodes = destination->nodes;
    capacity = destination->node_capacity;
    *destination = *source;
    destination->nodes = nodes;
    destination->node_capacity = capacity;
    if(source->node_count > 0) memcpy(destination->nodes, source->nodes,
        source->node_count * sizeof(*source->nodes));
    return true;
}

void editor_project_soft_body_destroy(EditorSoftBody *body) {
    if(body == NULL) return;
    for(size_t i = 0; i < body->area_count; i += 1)
        editor_project_soft_area_destroy(&body->areas[i]);
    free(body->nodes);
    free(body->beams);
    free(body->areas);
    free(body->hierarchy);
    *body = (EditorSoftBody){0};
}

bool editor_project_soft_body_clone(EditorSoftBody *destination,
        const EditorSoftBody *source) {
    if(destination == NULL || source == NULL) return false;
    *destination = *source;
    destination->nodes = NULL;
    destination->beams = NULL;
    destination->areas = NULL;
    destination->hierarchy = NULL;
    destination->node_count = 0;
    destination->beam_count = 0;
    destination->area_count = 0;
    destination->hierarchy_count = 0;
    destination->node_capacity = 0;
    destination->beam_capacity = 0;
    destination->area_capacity = 0;
    destination->hierarchy_capacity = 0;
    if(!EDITOR_ARRAY_RESERVE(destination->nodes, destination->node_capacity,
            source->node_count) ||
            !EDITOR_ARRAY_RESERVE(destination->beams,
                destination->beam_capacity, source->beam_count) ||
            !EDITOR_ARRAY_RESERVE(destination->areas,
                destination->area_capacity, source->area_count) ||
            !EDITOR_ARRAY_RESERVE(destination->hierarchy,
                destination->hierarchy_capacity, source->hierarchy_count))
        goto fail;
    if(source->node_count > 0) memcpy(destination->nodes, source->nodes,
        source->node_count * sizeof(*source->nodes));
    if(source->beam_count > 0) memcpy(destination->beams, source->beams,
        source->beam_count * sizeof(*source->beams));
    if(source->hierarchy_count > 0) memcpy(destination->hierarchy,
        source->hierarchy, source->hierarchy_count * sizeof(*source->hierarchy));
    destination->node_count = source->node_count;
    destination->beam_count = source->beam_count;
    destination->hierarchy_count = source->hierarchy_count;
    for(size_t i = 0; i < source->area_count; i += 1) {
        if(!editor_project_soft_area_clone(&destination->areas[i],
                &source->areas[i])) goto fail;
        destination->area_count += 1;
    }
    return true;
fail:
    editor_project_soft_body_destroy(destination);
    return false;
}

bool editor_project_soft_body_copy_set(EditorSoftBody *destination,
        const EditorSoftBody *source) {
    EditorSoftNode *nodes;
    EditorSoftBeam *beams;
    EditorSoftArea *areas;
    EditorSoftHierarchyItem *hierarchy;
    size_t node_capacity, beam_capacity, area_capacity, hierarchy_capacity;
    size_t old_area_count, old_area_capacity;
    if(destination == NULL || source == NULL) return false;
    old_area_capacity = destination->area_capacity;
    if(
            !EDITOR_ARRAY_RESERVE(destination->nodes,
                destination->node_capacity, source->node_count) ||
            !EDITOR_ARRAY_RESERVE(destination->beams,
                destination->beam_capacity, source->beam_count) ||
            !EDITOR_ARRAY_RESERVE(destination->areas,
                destination->area_capacity, source->area_count) ||
            !EDITOR_ARRAY_RESERVE(destination->hierarchy,
                destination->hierarchy_capacity, source->hierarchy_count)) return false;
    if(destination->area_capacity > old_area_capacity)
        memset(&destination->areas[old_area_capacity], 0,
            (destination->area_capacity - old_area_capacity) *
                sizeof(*destination->areas));
    nodes = destination->nodes; beams = destination->beams;
    areas = destination->areas; hierarchy = destination->hierarchy;
    node_capacity = destination->node_capacity;
    beam_capacity = destination->beam_capacity;
    area_capacity = destination->area_capacity;
    hierarchy_capacity = destination->hierarchy_capacity;
    old_area_count = destination->area_count;
    if(source->node_count > 0) memcpy(nodes, source->nodes,
        source->node_count * sizeof(*nodes));
    if(source->beam_count > 0) memcpy(beams, source->beams,
        source->beam_count * sizeof(*beams));
    if(source->hierarchy_count > 0) memcpy(hierarchy, source->hierarchy,
        source->hierarchy_count * sizeof(*hierarchy));
    for(size_t i = 0; i < source->area_count; i += 1)
        if(!editor_project_soft_area_copy_set(&areas[i],
                &source->areas[i])) return false;
    for(size_t i = source->area_count; i < old_area_count; i += 1)
        editor_project_soft_area_destroy(&areas[i]);
    *destination = *source;
    destination->nodes = nodes; destination->beams = beams;
    destination->areas = areas; destination->hierarchy = hierarchy;
    destination->node_capacity = node_capacity;
    destination->beam_capacity = beam_capacity;
    destination->area_capacity = area_capacity;
    destination->hierarchy_capacity = hierarchy_capacity;
    return true;
}

void editor_project_object_destroy(EditorObject *object) {
    if(object == NULL) return;
    for(size_t i = 0; i < object->rigid_body_count; i += 1)
        editor_project_rigid_body_destroy(&object->rigid_bodies[i]);
    for(size_t i = 0; i < object->soft_body_count; i += 1)
        editor_project_soft_body_destroy(&object->soft_body_items[i]);
    for(size_t i = 0; i < object->animated_sprite_count; i += 1)
        editor_project_animated_sprite_destroy(&object->animated_sprite_items[i]);
    free(object->rigid_bodies);
    free(object->joint_items);
    free(object->anchors);
    free(object->soft_body_items);
    free(object->sprites);
    free(object->animated_sprite_items);
    free(object->hierarchy);
    *object = (EditorObject){0};
}

bool editor_project_object_clone(EditorObject *destination,
        const EditorObject *source) {
    if(destination == NULL || source == NULL) return false;
    *destination = *source;
    destination->rigid_bodies = NULL;
    destination->joint_items = NULL;
    destination->anchors = NULL;
    destination->soft_body_items = NULL;
    destination->sprites = NULL;
    destination->animated_sprite_items = NULL;
    destination->hierarchy = NULL;
    destination->rigid_body_count = 0;
    destination->soft_body_count = 0;
    destination->sprite_count = 0;
    destination->animated_sprite_count = 0;
    destination->rigid_body_capacity = 0;
    destination->joint_capacity = 0;
    destination->anchor_capacity = 0;
    destination->soft_body_capacity = 0;
    destination->sprite_capacity = 0;
    destination->animated_sprite_capacity = 0;
    destination->hierarchy_capacity = 0;
    if(!EDITOR_ARRAY_RESERVE(destination->rigid_bodies,
            destination->rigid_body_capacity, source->rigid_body_count) ||
            !EDITOR_ARRAY_RESERVE(destination->joint_items,
                destination->joint_capacity, source->joint_count) ||
            !EDITOR_ARRAY_RESERVE(destination->anchors,
                destination->anchor_capacity, source->anchor_count) ||
            !EDITOR_ARRAY_RESERVE(destination->soft_body_items,
                destination->soft_body_capacity, source->soft_body_count) ||
            !EDITOR_ARRAY_RESERVE(destination->sprites,
                destination->sprite_capacity, source->sprite_count) ||
            !EDITOR_ARRAY_RESERVE(destination->animated_sprite_items,
                destination->animated_sprite_capacity,
                source->animated_sprite_count) ||
            !EDITOR_ARRAY_RESERVE(destination->hierarchy,
                destination->hierarchy_capacity, source->hierarchy_count))
        goto fail;
    if(source->joint_count > 0) memcpy(destination->joint_items,
        source->joint_items, source->joint_count * sizeof(*source->joint_items));
    if(source->anchor_count > 0) memcpy(destination->anchors,
        source->anchors, source->anchor_count * sizeof(*source->anchors));
    if(source->hierarchy_count > 0) memcpy(destination->hierarchy,
        source->hierarchy, source->hierarchy_count * sizeof(*source->hierarchy));
    destination->joint_count = source->joint_count;
    destination->anchor_count = source->anchor_count;
    destination->hierarchy_count = source->hierarchy_count;
    for(size_t i = 0; i < source->rigid_body_count; i += 1) {
        if(!editor_project_rigid_body_clone(&destination->rigid_bodies[i],
                &source->rigid_bodies[i])) goto fail;
        destination->rigid_body_count += 1;
    }
    for(size_t i = 0; i < source->soft_body_count; i += 1) {
        if(!editor_project_soft_body_clone(&destination->soft_body_items[i],
                &source->soft_body_items[i])) goto fail;
        destination->soft_body_count += 1;
    }
    for(size_t i = 0; i < source->animated_sprite_count; i += 1) {
        if(!editor_project_animated_sprite_clone(
                &destination->animated_sprite_items[i],
                &source->animated_sprite_items[i])) goto fail;
        destination->animated_sprite_count += 1;
    }
    if(source->sprite_count > 0) memcpy(destination->sprites, source->sprites,
        source->sprite_count * sizeof(*source->sprites));
    destination->sprite_count = source->sprite_count;
    return true;
fail:
    editor_project_object_destroy(destination);
    return false;
}

bool editor_project_object_copy_set(EditorObject *destination,
        const EditorObject *source) {
    EditorRigidBody *rigid_bodies;
    EditorJoint *editor_joints;
    EditorAnchor *anchors;
    EditorSoftBody *editor_soft_bodies;
    EditorSprite *sprites;
    EditorAnimatedSprite *sprite_items;
    EditorHierarchyItem *hierarchy;
    size_t rigid_capacity, joint_capacity, anchor_capacity, soft_capacity,
        animated_sprite_capacity, sprite_capacity;
    size_t hierarchy_capacity, old_rigid_count, old_soft_count,
        old_animated_sprite_count;
    size_t old_rigid_capacity, old_soft_capacity, old_animated_sprite_capacity;
    if(destination == NULL || source == NULL) return false;
    old_rigid_capacity = destination->rigid_body_capacity;
    old_soft_capacity = destination->soft_body_capacity;
    old_animated_sprite_capacity = destination->animated_sprite_capacity;
    if(
            !EDITOR_ARRAY_RESERVE(destination->rigid_bodies,
                destination->rigid_body_capacity, source->rigid_body_count) ||
            !EDITOR_ARRAY_RESERVE(destination->joint_items,
                destination->joint_capacity, source->joint_count) ||
            !EDITOR_ARRAY_RESERVE(destination->anchors,
                destination->anchor_capacity, source->anchor_count) ||
            !EDITOR_ARRAY_RESERVE(destination->soft_body_items,
                destination->soft_body_capacity, source->soft_body_count) ||
            !EDITOR_ARRAY_RESERVE(destination->sprites,
                destination->sprite_capacity, source->sprite_count) ||
            !EDITOR_ARRAY_RESERVE(destination->animated_sprite_items,
                destination->animated_sprite_capacity,
                source->animated_sprite_count) ||
            !EDITOR_ARRAY_RESERVE(destination->hierarchy,
                destination->hierarchy_capacity, source->hierarchy_count)) return false;
    if(destination->rigid_body_capacity > old_rigid_capacity)
        memset(&destination->rigid_bodies[old_rigid_capacity], 0,
            (destination->rigid_body_capacity - old_rigid_capacity) *
                sizeof(*destination->rigid_bodies));
    if(destination->soft_body_capacity > old_soft_capacity)
        memset(&destination->soft_body_items[old_soft_capacity], 0,
            (destination->soft_body_capacity - old_soft_capacity) *
                sizeof(*destination->soft_body_items));
    if(destination->animated_sprite_capacity > old_animated_sprite_capacity)
        memset(&destination->animated_sprite_items[old_animated_sprite_capacity], 0,
            (destination->animated_sprite_capacity - old_animated_sprite_capacity) *
                sizeof(*destination->animated_sprite_items));
    rigid_bodies = destination->rigid_bodies;
    editor_joints = destination->joint_items;
    anchors = destination->anchors;
    editor_soft_bodies = destination->soft_body_items;
    sprites = destination->sprites;
    sprite_items = destination->animated_sprite_items;
    hierarchy = destination->hierarchy;
    rigid_capacity = destination->rigid_body_capacity;
    joint_capacity = destination->joint_capacity;
    anchor_capacity = destination->anchor_capacity;
    soft_capacity = destination->soft_body_capacity;
    sprite_capacity = destination->sprite_capacity;
    animated_sprite_capacity = destination->animated_sprite_capacity;
    hierarchy_capacity = destination->hierarchy_capacity;
    old_rigid_count = destination->rigid_body_count;
    old_soft_count = destination->soft_body_count;
    old_animated_sprite_count = destination->animated_sprite_count;
    for(size_t i = 0; i < source->rigid_body_count; i += 1)
        if(!editor_project_rigid_body_copy_set(&rigid_bodies[i],
                &source->rigid_bodies[i])) return false;
    for(size_t i = source->rigid_body_count; i < old_rigid_count; i += 1)
        editor_project_rigid_body_destroy(&rigid_bodies[i]);
    for(size_t i = 0; i < source->soft_body_count; i += 1)
        if(!editor_project_soft_body_copy_set(&editor_soft_bodies[i],
                &source->soft_body_items[i])) return false;
    for(size_t i = source->soft_body_count; i < old_soft_count; i += 1)
        editor_project_soft_body_destroy(&editor_soft_bodies[i]);
    for(size_t i = 0; i < source->animated_sprite_count; i += 1)
        if(!editor_project_animated_sprite_copy_set(&sprite_items[i],
                &source->animated_sprite_items[i])) return false;
    for(size_t i = source->animated_sprite_count;
            i < old_animated_sprite_count; i += 1)
        editor_project_animated_sprite_destroy(&sprite_items[i]);
    if(source->joint_count > 0) memcpy(editor_joints, source->joint_items,
        source->joint_count * sizeof(*editor_joints));
    if(source->anchor_count > 0) memcpy(anchors, source->anchors,
        source->anchor_count * sizeof(*anchors));
    if(source->sprite_count > 0) memcpy(sprites, source->sprites,
        source->sprite_count * sizeof(*sprites));
    if(source->hierarchy_count > 0) memcpy(hierarchy, source->hierarchy,
        source->hierarchy_count * sizeof(*hierarchy));
    *destination = *source;
    destination->rigid_bodies = rigid_bodies;
    destination->joint_items = editor_joints;
    destination->anchors = anchors;
    destination->soft_body_items = editor_soft_bodies;
    destination->sprites = sprites;
    destination->animated_sprite_items = sprite_items;
    destination->hierarchy = hierarchy;
    destination->rigid_body_capacity = rigid_capacity;
    destination->joint_capacity = joint_capacity;
    destination->anchor_capacity = anchor_capacity;
    destination->soft_body_capacity = soft_capacity;
    destination->sprite_capacity = sprite_capacity;
    destination->animated_sprite_capacity = animated_sprite_capacity;
    destination->hierarchy_capacity = hierarchy_capacity;
    return true;
}

void editor_project_destroy(EditorProject *project) {
    if(project == NULL) return;
    for(size_t i = 0; i < project->object_count; i += 1)
        editor_project_object_destroy(&project->objects[i]);
    free(project->collision_masks);
    free(project->objects);
    *project = (EditorProject){0};
}

bool editor_project_clone(EditorProject *destination,
        const EditorProject *source) {
    if(destination == NULL || source == NULL) return false;
    *destination = *source;
    destination->collision_masks = NULL;
    destination->objects = NULL;
    destination->collision_mask_count = 0;
    destination->object_count = 0;
    destination->collision_mask_capacity = 0;
    destination->object_capacity = 0;
    if(!EDITOR_ARRAY_RESERVE(destination->collision_masks,
            destination->collision_mask_capacity, source->collision_mask_count) ||
            !EDITOR_ARRAY_RESERVE(destination->objects,
                destination->object_capacity, source->object_count)) goto fail;
    if(source->collision_mask_count > 0)
        memcpy(destination->collision_masks, source->collision_masks,
            source->collision_mask_count * sizeof(*source->collision_masks));
    destination->collision_mask_count = source->collision_mask_count;
    for(size_t i = 0; i < source->object_count; i += 1) {
        if(!editor_project_object_clone(&destination->objects[i],
                &source->objects[i])) goto fail;
        destination->object_count += 1;
    }
    return true;
fail:
    editor_project_destroy(destination);
    return false;
}

bool editor_project_collision_mask_add(EditorProject *project, const char *name,
    size_t *index) {
    char formatted[EDITOR_OBJECT_NAME_MAX];

    if(project == NULL || name == NULL || name[0] == '\0' ||
            project->collision_mask_count >= EDITOR_COLLISION_MASK_MAX) return false;
    editor_project_property_name_format(formatted, sizeof(formatted), name);
    if(formatted[0] == '\0') return false;
    for(size_t i = 0; i < project->collision_mask_count; i += 1) {
        if(strcmp(project->collision_masks[i].name, formatted) != 0) continue;
        if(index != NULL) *index = i;
        return false;
    }
    if(index != NULL) *index = project->collision_mask_count;
    if(!EDITOR_ARRAY_RESERVE(project->collision_masks,
            project->collision_mask_capacity,
            project->collision_mask_count + 1)) return false;
    snprintf(project->collision_masks[project->collision_mask_count].name,
        sizeof(project->collision_masks[project->collision_mask_count].name),
        "%s", formatted);
    project->collision_mask_count += 1;
    return true;
}

EditorObject *editor_project_object_add(EditorProject *project, Position position) {
    EditorObject *object;

    if(project == NULL || !EDITOR_ARRAY_RESERVE(project->objects,
            project->object_capacity, project->object_count + 1)) return NULL;
    object = &project->objects[project->object_count++];
    *object = (EditorObject){
        .id = project->next_id++,
        .position = position,
        .visible = true
    };
    if(!EDITOR_ARRAY_RESERVE(object->rigid_bodies,
            object->rigid_body_capacity, EDITOR_RIGID_BODY_MAX) ||
            !EDITOR_ARRAY_RESERVE(object->joint_items,
                object->joint_capacity, EDITOR_JOINT_MAX) ||
            !EDITOR_ARRAY_RESERVE(object->anchors,
                object->anchor_capacity, EDITOR_ANCHOR_MAX) ||
            !EDITOR_ARRAY_RESERVE(object->soft_body_items,
                object->soft_body_capacity, EDITOR_SOFT_BODY_MAX) ||
            !EDITOR_ARRAY_RESERVE(object->sprites, object->sprite_capacity, 8) ||
            !EDITOR_ARRAY_RESERVE(object->animated_sprite_items,
                object->animated_sprite_capacity, 8) ||
            !EDITOR_ARRAY_RESERVE(object->hierarchy,
                object->hierarchy_capacity, EDITOR_OBJECT_HIERARCHY_MAX)) {
        editor_project_object_destroy(object);
        project->object_count -= 1;
        return NULL;
    }
    memset(object->rigid_bodies, 0,
        object->rigid_body_capacity * sizeof(*object->rigid_bodies));
    memset(object->joint_items, 0,
        object->joint_capacity * sizeof(*object->joint_items));
    memset(object->anchors, 0,
        object->anchor_capacity * sizeof(*object->anchors));
    memset(object->soft_body_items, 0,
        object->soft_body_capacity * sizeof(*object->soft_body_items));
    memset(object->sprites, 0,
        object->sprite_capacity * sizeof(*object->sprites));
    memset(object->animated_sprite_items, 0,
        object->animated_sprite_capacity * sizeof(*object->animated_sprite_items));
    memset(object->hierarchy, 0,
        object->hierarchy_capacity * sizeof(*object->hierarchy));
    snprintf(object->name, sizeof(object->name), "Object%u", object->id);
    project->selected = object->id;
    return object;
}

bool editor_project_object_remove(EditorProject *project, EditorObjectId id) {
    size_t index;

    if(project == NULL || id == EDITOR_OBJECT_INVALID) return false;
    for(index = 0; index < project->object_count; index += 1) {
        if(project->objects[index].id == id) break;
    }
    if(index == project->object_count) return false;
    editor_project_object_destroy(&project->objects[index]);
    for(size_t i = index + 1; i < project->object_count; i += 1) {
        project->objects[i - 1] = project->objects[i];
    }
    project->object_count -= 1;
    project->objects[project->object_count] = (EditorObject){0};
    if(project->selected == id) project->selected = EDITOR_OBJECT_INVALID;
    return true;
}

EditorObject *editor_project_selected_get(EditorProject *project) {
    if(project == NULL || project->selected == EDITOR_OBJECT_INVALID) return NULL;
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(project->objects[i].id == project->selected) return &project->objects[i];
    }
    return NULL;
}

bool editor_project_object_select(EditorProject *project, EditorObjectId id) {
    if(project == NULL) return false;
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(project->objects[i].id != id) continue;
        project->selected = id;
        return true;
    }
    return false;
}

void editor_project_selection_clear(EditorProject *project) {
    if(project == NULL) return;
    project->selected = EDITOR_OBJECT_INVALID;
}

EditorRigidBody editor_project_rigid_body_default_get(void) {
    return (EditorRigidBody){
        .mass_value = 1.0f,
        .friction = 0.5f,
        .restitution = 0.0f,
        .gravity_enabled = false,
        .collision_enabled = true,
        .particle_auto_fit = true,
        .particle_ring_color = UINT32_C(0x4a925fff),
        .particle_fill_color = UINT32_C(0x4a925f40),
        .border_color = UINT32_C(0xe6ebf4ff),
        .surface_color = UINT32_C(0x66758c80),
        .collision_category = UINT64_C(1),
        .collision_with = UINT64_C(1),
        .particle_auto_fit = true,
        .particle_radius = 30.0f,
        .particle_ring_color = UINT32_C(0x4a90e2ff),
        .particle_fill_color = UINT32_C(0x4a90e240),
        .border_color = UINT32_C(0xffffffff),
        .surface_color = UINT32_C(0x808080ff),
        .visible = true
    };
}

static bool editor_project_hierarchy_item_exists(const EditorObject *object,
        EditorHierarchyItem item) {
    if(object == NULL || item.id == 0) return false;
    if(item.kind == EDITOR_HIERARCHY_RIGID_BODY) {
        for(size_t i = 0; i < object->rigid_body_count; i += 1)
            if(object->rigid_bodies[i].id == item.id) return true;
    } else if(item.kind == EDITOR_HIERARCHY_JOINT) {
        for(size_t i = 0; i < object->joint_count; i += 1)
            if(object->joint_items[i].id == item.id) return true;
    } else if(item.kind == EDITOR_HIERARCHY_SOFT_BODY) {
        for(size_t i = 0; i < object->soft_body_count; i += 1)
            if(object->soft_body_items[i].id == item.id) return true;
    } else if(item.kind == EDITOR_HIERARCHY_SPRITE) {
        for(size_t i = 0; i < object->sprite_count; i += 1)
            if(object->sprites[i].id == item.id) return true;
    } else if(item.kind == EDITOR_HIERARCHY_ANIMATED_SPRITE) {
        for(size_t i = 0; i < object->animated_sprite_count; i += 1)
            if(object->animated_sprite_items[i].id == item.id) return true;
    }
    return false;
}

static void editor_project_hierarchy_item_add(EditorObject *object,
        EditorHierarchyItemKind kind, uint32_t id) {
    if(object == NULL || id == 0 || !EDITOR_ARRAY_RESERVE(object->hierarchy,
            object->hierarchy_capacity, object->hierarchy_count + 1)) return;
    object->hierarchy[object->hierarchy_count++] = (EditorHierarchyItem){kind, id};
}

void editor_project_object_hierarchy_sync(EditorObject *object) {
    size_t output = 0;
    if(object == NULL) return;
    for(size_t i = 0; i < object->hierarchy_count; i += 1) {
        bool duplicate = false;
        if(!editor_project_hierarchy_item_exists(object, object->hierarchy[i])) continue;
        for(size_t j = 0; j < output; j += 1)
            if(object->hierarchy[j].kind == object->hierarchy[i].kind &&
                    object->hierarchy[j].id == object->hierarchy[i].id)
                duplicate = true;
        if(duplicate) continue;
        object->hierarchy[output++] = object->hierarchy[i];
    }
    object->hierarchy_count = output;
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        EditorHierarchyItem item = {EDITOR_HIERARCHY_RIGID_BODY,
            object->rigid_bodies[i].id};
        bool found = false;
        for(size_t j = 0; j < output; j += 1)
            if(object->hierarchy[j].kind == item.kind &&
                    object->hierarchy[j].id == item.id) found = true;
        if(!found) editor_project_hierarchy_item_add(object, item.kind, item.id);
    }
    for(size_t i = 0; i < object->joint_count; i += 1) {
        EditorHierarchyItem item = {EDITOR_HIERARCHY_JOINT, object->joint_items[i].id};
        bool found = false;
        for(size_t j = 0; j < object->hierarchy_count; j += 1)
            if(object->hierarchy[j].kind == item.kind &&
                    object->hierarchy[j].id == item.id) found = true;
        if(!found) editor_project_hierarchy_item_add(object, item.kind, item.id);
    }
    for(size_t i = 0; i < object->soft_body_count; i += 1) {
        EditorHierarchyItem item = {EDITOR_HIERARCHY_SOFT_BODY,
            object->soft_body_items[i].id};
        bool found = false;
        for(size_t j = 0; j < object->hierarchy_count; j += 1)
            if(object->hierarchy[j].kind == item.kind &&
                    object->hierarchy[j].id == item.id) found = true;
        if(!found) editor_project_hierarchy_item_add(object, item.kind, item.id);
    }
    for(size_t i = 0; i < object->animated_sprite_count; i += 1) {
        EditorHierarchyItem item = {EDITOR_HIERARCHY_ANIMATED_SPRITE,
            object->animated_sprite_items[i].id};
        bool found = false;
        for(size_t j = 0; j < object->hierarchy_count; j += 1)
            if(object->hierarchy[j].kind == item.kind &&
                    object->hierarchy[j].id == item.id) found = true;
        if(!found) editor_project_hierarchy_item_add(object, item.kind, item.id);
    }
    for(size_t i = 0; i < object->sprite_count; i += 1) {
        EditorHierarchyItem item = {EDITOR_HIERARCHY_SPRITE, object->sprites[i].id};
        bool found = false;
        for(size_t j = 0; j < object->hierarchy_count; j += 1)
            if(object->hierarchy[j].kind == item.kind &&
                    object->hierarchy[j].id == item.id) found = true;
        if(!found) editor_project_hierarchy_item_add(object, item.kind, item.id);
    }
}

size_t editor_project_object_hierarchy_index_get(const EditorObject *object,
        EditorHierarchyItemKind kind, uint32_t id) {
    if(object == NULL) return SIZE_MAX;
    for(size_t i = 0; i < object->hierarchy_count; i += 1)
        if(object->hierarchy[i].kind == kind && object->hierarchy[i].id == id) return i;
    return SIZE_MAX;
}

Position editor_project_particle_center_get(const EditorRigidBody *body) {
    const EditorHitbox *hitbox;
    float twice_area = 0.0f;
    Position weighted = {0};

    if(body == NULL) return (Position){0};
    if(body->particle) return body->particle_origin;
    if(body->hitbox_count == 0) return (Position){0};
    hitbox = &body->hitboxes[body->active_hitbox_index < body->hitbox_count ?
        body->active_hitbox_index : 0];
    if(hitbox->vertex_count == 0) return (Position){0};
    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
        Position a = hitbox->vertices[i].position;
        Position b = hitbox->vertices[(i + 1) % hitbox->vertex_count].position;
        float cross = a.x * b.y - b.x * a.y;
        twice_area += cross;
        weighted.x += (a.x + b.x) * cross;
        weighted.y += (a.y + b.y) * cross;
    }
    if(fabsf(twice_area) > 0.0001f) {
        return (Position){weighted.x / (3.0f * twice_area),
            weighted.y / (3.0f * twice_area)};
    }
    weighted = (Position){0};
    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
        weighted.x += hitbox->vertices[i].position.x;
        weighted.y += hitbox->vertices[i].position.y;
    }
    return (Position){weighted.x / (float)hitbox->vertex_count,
        weighted.y / (float)hitbox->vertex_count};
}

float editor_project_particle_auto_radius_get(const EditorRigidBody *body) {
    Position center = editor_project_particle_center_get(body);
    float radius = 0.0f;

    if(body == NULL || body->hitbox_count == 0) return radius;
    {
    const EditorHitbox *active = &body->hitboxes[
        body->active_hitbox_index < body->hitbox_count ?
            body->active_hitbox_index : 0];
    for(uint32_t i = 0; i < active->vertex_count; i += 1) {
        Position point = active->vertices[i].position;
        radius = fmaxf(radius, hypotf(point.x - center.x, point.y - center.y));
    }
    }
    return radius;
}

void editor_project_particle_auto_fit_update(EditorProject *project) {
    if(project == NULL) return;
    for(size_t object_index = 0; object_index < project->object_count; object_index += 1) {
        EditorObject *object = &project->objects[object_index];
        for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
            EditorRigidBody *body = &object->rigid_bodies[body_index];
            if(body->particle && body->particle_auto_fit)
                body->particle_radius = editor_project_particle_auto_radius_get(body);
        }
    }
}

EditorJoint editor_project_joint_default_get(EditorJointKind kind) {
    return (EditorJoint){
        .kind = kind,
        .rest_length = 0.0f,
        .stiffness = 100.0f,
        .damping = kind == EDITOR_JOINT_SPRING ? 10.0f : 0.0f,
        .visual_size = 1.0f,
        .visible = true
    };
}

EditorRigidBody *editor_project_rigid_body_add(EditorProject *project,
    EditorObject *object) {
    EditorRigidBody *body;

    if(project == NULL || object == NULL ||
            !EDITOR_ARRAY_RESERVE(object->rigid_bodies,
                object->rigid_body_capacity,
                object->rigid_body_count + 1)) return NULL;
    body = &object->rigid_bodies[object->rigid_body_count++];
    *body = editor_project_rigid_body_default_get();
    if(!EDITOR_ARRAY_RESERVE(body->hitboxes, body->hitbox_capacity,
            EDITOR_BODY_HITBOX_MAX)) {
        object->rigid_body_count -= 1;
        return NULL;
    }
    memset(body->hitboxes, 0,
        body->hitbox_capacity * sizeof(*body->hitboxes));
    body->id = project->next_rigid_body_id++;
    snprintf(body->name, sizeof(body->name), "rigid_body_%u", body->id);
    if(editor_project_hitbox_add(project, body) == NULL) {
        object->rigid_body_count -= 1;
        object->rigid_bodies[object->rigid_body_count] = (EditorRigidBody){0};
        return NULL;
    }
    editor_project_hierarchy_item_add(object, EDITOR_HIERARCHY_RIGID_BODY, body->id);
    return body;
}

EditorRigidBody *editor_project_rigid_body_get(EditorObject *object,
    EditorRigidBodyId id) {
    if(object == NULL || id == 0) return NULL;
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        if(object->rigid_bodies[i].id == id) return &object->rigid_bodies[i];
    }
    return NULL;
}

bool editor_project_rigid_body_remove(EditorObject *object, EditorRigidBodyId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        if(object->rigid_bodies[i].id != id) continue;
        for(size_t j = 0; j < object->anchor_count; j += 1) {
            EditorAnchor *anchor = &object->anchors[j];
            if(anchor->rigid_body != id) continue;
            if(anchor->position_follows_body) {
                (void)editor_project_anchor_position_lock_set(object, anchor, false);
            }
            if(anchor->rotation_follows_body) {
                (void)editor_project_anchor_rotation_lock_set(object, anchor, false);
            }
            anchor->rigid_body = 0;
        }
        for(size_t j = 0; j < object->animated_sprite_count; j += 1)
            if(object->animated_sprite_items[j].rigid_body == id)
                object->animated_sprite_items[j].rigid_body = 0;
        for(size_t j = 0; j < object->sprite_count; j += 1)
            if(object->sprites[j].rigid_body == id)
                object->sprites[j].rigid_body = 0;
        editor_project_rigid_body_destroy(&object->rigid_bodies[i]);
        for(size_t j = i + 1; j < object->rigid_body_count; j += 1) {
            object->rigid_bodies[j - 1] = object->rigid_bodies[j];
        }
        object->rigid_body_count -= 1;
        object->rigid_bodies[object->rigid_body_count] = (EditorRigidBody){0};
        editor_project_object_hierarchy_sync(object);
        return true;
    }
    return false;
}

bool editor_project_rigid_body_origin_set(EditorObject *object, EditorRigidBody *body,
    Position position) {
    Vec2D world_delta;
    Vec2D local_delta;
    float cosine;
    float sine;

    if(object == NULL || body == NULL) return false;
    world_delta = (Vec2D){position.x - body->position.x,
        position.y - body->position.y};
    cosine = cosf(-body->rotation);
    sine = sinf(-body->rotation);
    local_delta = (Vec2D){world_delta.x * cosine - world_delta.y * sine,
        world_delta.x * sine + world_delta.y * cosine};
    body->position = position;
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        for(uint32_t vertex = 0; vertex < body->hitboxes[i].vertex_count; vertex += 1) {
            body->hitboxes[i].vertices[vertex].position.x -= local_delta.x;
            body->hitboxes[i].vertices[vertex].position.y -= local_delta.y;
        }
    }
    for(size_t i = 0; i < object->anchor_count; i += 1) {
        EditorAnchor *anchor = &object->anchors[i];
        if(anchor->rigid_body != body->id || !anchor->position_follows_body) continue;
        anchor->position.x -= local_delta.x;
        anchor->position.y -= local_delta.y;
    }
    return true;
}

EditorAnchor *editor_project_anchor_get(EditorObject *object, EditorAnchorId id) {
    if(object == NULL || id == 0) return NULL;
    for(size_t i = 0; i < object->anchor_count; i += 1) {
        if(object->anchors[i].id == id) return &object->anchors[i];
    }
    return NULL;
}

EditorAnchor *editor_project_anchor_add(EditorProject *project, EditorObject *object,
    Position position, EditorRigidBodyId rigid_body) {
    EditorAnchor *anchor;

    if(project == NULL || object == NULL ||
            (rigid_body != 0 && editor_project_rigid_body_get(object, rigid_body) == NULL)) {
        return NULL;
    }
    if(!EDITOR_ARRAY_RESERVE(object->anchors, object->anchor_capacity,
            object->anchor_count + 1)) return NULL;
    anchor = &object->anchors[object->anchor_count++];
    *anchor = (EditorAnchor){.id = project->next_anchor_id++, .position = position,
        .rigid_body = rigid_body, .position_follows_body = rigid_body != 0,
        .rotation_follows_body = rigid_body != 0, .visible = true};
    snprintf(anchor->name, sizeof(anchor->name), "anchor_%u", anchor->id);
    return anchor;
}

static Position editor_position_rotate(Position position, float rotation) {
    float cosine = cosf(rotation);
    float sine = sinf(rotation);
    return (Position){position.x * cosine - position.y * sine,
        position.x * sine + position.y * cosine};
}

bool editor_project_anchor_position_lock_set(EditorObject *object, EditorAnchor *anchor,
    bool locked) {
    EditorRigidBody *body;
    Position position;

    if(object == NULL || anchor == NULL) return false;
    if(anchor->position_follows_body == locked) return true;
    body = editor_project_rigid_body_get(object, anchor->rigid_body);
    if(body == NULL) return false;
    if(locked) {
        position = (Position){anchor->position.x - body->position.x,
            anchor->position.y - body->position.y};
        anchor->position = editor_position_rotate(position, -body->rotation);
    } else {
        position = editor_position_rotate(anchor->position, body->rotation);
        anchor->position = (Position){body->position.x + position.x,
            body->position.y + position.y};
    }
    anchor->position_follows_body = locked;
    return true;
}

bool editor_project_anchor_rotation_lock_set(EditorObject *object, EditorAnchor *anchor,
    bool locked) {
    EditorRigidBody *body;

    if(object == NULL || anchor == NULL) return false;
    if(anchor->rotation_follows_body == locked) return true;
    body = editor_project_rigid_body_get(object, anchor->rigid_body);
    if(body == NULL) return false;
    anchor->rotation += locked ? -body->rotation : body->rotation;
    anchor->rotation_follows_body = locked;
    return true;
}

bool editor_project_anchor_rigid_body_set(EditorObject *object, EditorAnchor *anchor,
    EditorRigidBodyId rigid_body) {
    bool position_locked;
    bool rotation_locked;

    if(object == NULL || anchor == NULL || (rigid_body != 0 &&
            editor_project_rigid_body_get(object, rigid_body) == NULL)) return false;
    if(anchor->rigid_body == rigid_body) return true;
    position_locked = anchor->position_follows_body;
    rotation_locked = anchor->rotation_follows_body;
    if(position_locked && !editor_project_anchor_position_lock_set(object, anchor, false)) {
        return false;
    }
    if(rotation_locked && !editor_project_anchor_rotation_lock_set(object, anchor, false)) {
        return false;
    }
    anchor->rigid_body = rigid_body;
    if(rigid_body != 0) {
        if(position_locked) (void)editor_project_anchor_position_lock_set(object, anchor, true);
        if(rotation_locked) (void)editor_project_anchor_rotation_lock_set(object, anchor, true);
    }
    return true;
}

bool editor_project_anchor_remove(EditorObject *object, EditorAnchorId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->joint_count; i += 1) {
        EditorJoint *joint = &object->joint_items[i];
        if(joint->anchor_a == id) joint->anchor_a = 0;
        if(joint->anchor_b == id) joint->anchor_b = 0;
    }
    for(size_t i = 0; i < object->anchor_count; i += 1) {
        if(object->anchors[i].id != id) continue;
        for(size_t j = i + 1; j < object->anchor_count; j += 1) {
            object->anchors[j - 1] = object->anchors[j];
        }
        object->anchor_count -= 1;
        object->anchors[object->anchor_count] = (EditorAnchor){0};
        return true;
    }
    return false;
}

EditorHitbox *editor_project_hitbox_add(EditorProject *project, EditorRigidBody *body) {
    EditorHitbox *hitbox;

    if(project == NULL || body == NULL ||
            !EDITOR_ARRAY_RESERVE(body->hitboxes, body->hitbox_capacity,
                body->hitbox_count + 1)) {
        return NULL;
    }
    hitbox = &body->hitboxes[body->hitbox_count++];
    *hitbox = (EditorHitbox){.id = project->next_hitbox_id++, .visible = true};
    if(!editor_hitbox_vertices_reserve(hitbox, EDITOR_HITBOX_VERTEX_MAX)) {
        body->hitbox_count -= 1;
        return NULL;
    }
    snprintf(hitbox->name, sizeof(hitbox->name), "hitbox_%u", hitbox->id);
    editor_hitbox_regular_set(project, hitbox, EDITOR_HITBOX_VERTEX_MIN);
    return hitbox;
}

EditorHitbox *editor_project_hitbox_get(EditorRigidBody *body, EditorHitboxId id) {
    if(body == NULL || id == 0) return NULL;
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        if(body->hitboxes[i].id == id) return &body->hitboxes[i];
    }
    return NULL;
}

bool editor_project_hitbox_remove(EditorRigidBody *body, EditorHitboxId id) {
    if(body == NULL || id == 0) return false;
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        if(body->hitboxes[i].id != id) continue;
        for(size_t binding = 0;
                binding < body->hitbox_animation_binding_count;) {
            if(body->hitbox_animation_bindings[binding].hitbox != id) {
                binding += 1;
                continue;
            }
            for(size_t j = binding + 1;
                    j < body->hitbox_animation_binding_count; j += 1)
                body->hitbox_animation_bindings[j - 1] =
                    body->hitbox_animation_bindings[j];
            body->hitbox_animation_binding_count -= 1;
        }
        editor_project_hitbox_destroy(&body->hitboxes[i]);
        for(size_t j = i + 1; j < body->hitbox_count; j += 1) {
            body->hitboxes[j - 1] = body->hitboxes[j];
        }
        body->hitbox_count -= 1;
        if(i < body->active_hitbox_index) body->active_hitbox_index -= 1;
        else if(i == body->active_hitbox_index &&
                body->active_hitbox_index >= body->hitbox_count &&
                body->hitbox_count > 0)
            body->active_hitbox_index = body->hitbox_count - 1;
        if(body->hitbox_count == 0) body->active_hitbox_index = 0;
        body->hitboxes[body->hitbox_count] = (EditorHitbox){0};
        return true;
    }
    return false;
}

bool editor_project_hitbox_animation_binding_check(const EditorRigidBody *body,
        EditorAnimatedSpriteId animation, EditorSpriteId frame,
        EditorHitboxId hitbox) {
    if(body == NULL) return false;
    for(size_t i = 0; i < body->hitbox_animation_binding_count; i += 1) {
        const EditorHitboxAnimationBinding *binding =
            &body->hitbox_animation_bindings[i];
        if(binding->animation == animation && binding->frame == frame &&
                binding->hitbox == hitbox) return true;
    }
    return false;
}

bool editor_project_hitbox_animation_binding_set(EditorRigidBody *body,
        EditorAnimatedSpriteId animation, EditorSpriteId frame,
        EditorHitboxId hitbox, bool enabled) {
    if(body == NULL || animation == 0 || frame == 0 || hitbox == 0) return false;
    for(size_t i = 0; i < body->hitbox_animation_binding_count; i += 1) {
        EditorHitboxAnimationBinding *binding =
            &body->hitbox_animation_bindings[i];
        if(binding->animation != animation || binding->frame != frame) continue;
        if(enabled) {
            binding->hitbox = hitbox;
        } else {
            for(size_t j = i + 1; j < body->hitbox_animation_binding_count; j += 1)
                body->hitbox_animation_bindings[j - 1] =
                    body->hitbox_animation_bindings[j];
            body->hitbox_animation_binding_count -= 1;
        }
        return true;
    }
    if(!enabled) return true;
    if(!EDITOR_ARRAY_RESERVE(body->hitbox_animation_bindings,
            body->hitbox_animation_binding_capacity,
            body->hitbox_animation_binding_count + 1)) return false;
    body->hitbox_animation_bindings[body->hitbox_animation_binding_count++] =
        (EditorHitboxAnimationBinding){animation, frame, hitbox};
    return true;
}

bool editor_project_hitbox_vertex_remove(EditorHitbox *hitbox, uint32_t vertex_index) {
    if(hitbox == NULL) return false;
    if(hitbox->vertex_count <= EDITOR_HITBOX_VERTEX_MIN ||
            vertex_index >= hitbox->vertex_count) return false;
    for(uint32_t i = vertex_index + 1; i < hitbox->vertex_count; i += 1) {
        hitbox->vertices[i - 1] = hitbox->vertices[i];
        snprintf(hitbox->line_names[i - 1], sizeof(hitbox->line_names[i - 1]),
            "%s", hitbox->line_names[i]);
    }
    hitbox->vertex_count -= 1;
    hitbox->vertices[hitbox->vertex_count] = (EditorVertex){0};
    hitbox->line_names[hitbox->vertex_count][0] = '\0';
    return true;
}

bool editor_project_hitbox_line_remove(EditorHitbox *hitbox, uint32_t line_index) {
    if(hitbox == NULL || line_index >= hitbox->vertex_count) return false;
    return editor_project_hitbox_vertex_remove(hitbox,
        (line_index + 1) % hitbox->vertex_count);
}

bool editor_project_hitbox_vertex_insert(EditorProject *project, EditorHitbox *hitbox,
    uint32_t line_index) {
    uint32_t second;
    EditorVertex inserted;

    if(project == NULL || hitbox == NULL) return false;
    if(line_index >= hitbox->vertex_count ||
            !editor_hitbox_vertices_reserve(hitbox,
                hitbox->vertex_count + 1)) return false;
    second = (line_index + 1) % hitbox->vertex_count;
    inserted = (EditorVertex){
        .id = project->next_vertex_id++,
        .position = {
            (hitbox->vertices[line_index].position.x +
                hitbox->vertices[second].position.x) * 0.5f,
            (hitbox->vertices[line_index].position.y +
                hitbox->vertices[second].position.y) * 0.5f
        }
    };
    snprintf(inserted.name, sizeof(inserted.name), "vertex_%u",
        hitbox->vertex_count + 1);
    if(second == 0) {
        hitbox->vertices[hitbox->vertex_count] = inserted;
        snprintf(hitbox->line_names[hitbox->vertex_count],
            sizeof(hitbox->line_names[hitbox->vertex_count]), "line_%u",
            hitbox->vertex_count + 1);
    } else {
        for(uint32_t i = hitbox->vertex_count; i > second; i -= 1) {
            hitbox->vertices[i] = hitbox->vertices[i - 1];
            snprintf(hitbox->line_names[i], sizeof(hitbox->line_names[i]),
                "%s", hitbox->line_names[i - 1]);
        }
        hitbox->vertices[second] = inserted;
        snprintf(hitbox->line_names[second], sizeof(hitbox->line_names[second]),
            "line_%u", hitbox->vertex_count + 1);
    }
    hitbox->vertex_count += 1;
    return true;
}

float editor_project_hitbox_line_length_get(const EditorHitbox *hitbox,
    uint32_t line_index) {
    uint32_t second;
    Vec2D delta;

    if(hitbox == NULL || line_index >= hitbox->vertex_count) return 0.0f;
    second = (line_index + 1) % hitbox->vertex_count;
    delta = (Vec2D){
        hitbox->vertices[second].position.x - hitbox->vertices[line_index].position.x,
        hitbox->vertices[second].position.y - hitbox->vertices[line_index].position.y
    };
    return sqrtf(delta.x * delta.x + delta.y * delta.y);
}

bool editor_project_hitbox_line_length_set(EditorHitbox *hitbox,
    uint32_t line_index, float length) {
    EditorVertex *first;
    EditorVertex *second;
    Vec2D direction;
    float current;

    if(hitbox == NULL || length <= 0.001f ||
            line_index >= hitbox->vertex_count) return false;
    first = &hitbox->vertices[line_index];
    second = &hitbox->vertices[(line_index + 1) % hitbox->vertex_count];
    if(first->position_locked && second->position_locked) return false;
    direction = (Vec2D){second->position.x - first->position.x,
        second->position.y - first->position.y};
    current = sqrtf(direction.x * direction.x + direction.y * direction.y);
    if(current <= 0.001f) return false;
    direction.x /= current;
    direction.y /= current;
    if(first->position_locked) {
        second->position.x = first->position.x + direction.x * length;
        second->position.y = first->position.y + direction.y * length;
    } else if(second->position_locked) {
        first->position.x = second->position.x - direction.x * length;
        first->position.y = second->position.y - direction.y * length;
    } else {
        Position midpoint = {(first->position.x + second->position.x) * 0.5f,
            (first->position.y + second->position.y) * 0.5f};
        first->position = (Position){midpoint.x - direction.x * length * 0.5f,
            midpoint.y - direction.y * length * 0.5f};
        second->position = (Position){midpoint.x + direction.x * length * 0.5f,
            midpoint.y + direction.y * length * 0.5f};
    }
    return true;
}

static Position editor_anchor_world_position_get(EditorObject *object,
    EditorAnchor *anchor) {
    EditorRigidBody *body = editor_project_rigid_body_get(object, anchor->rigid_body);
    if(body != NULL && anchor->position_follows_body) {
        Position offset = editor_position_rotate(anchor->position, body->rotation);
        return (Position){body->position.x + offset.x, body->position.y + offset.y};
    }
    return anchor->position;
}

static bool editor_project_joint_constraint_from_endpoint_apply(
    EditorObject *object,
    EditorJoint *joint,
    uint32_t driver_endpoint
) {
    EditorAnchor *driver;
    EditorAnchor *driven;
    EditorRigidBody *driver_body;
    EditorRigidBody *driven_body;
    Position driver_world;
    Position driven_world;

    if(object == NULL || joint == NULL || driver_endpoint > 1 ||
            joint->kind == EDITOR_JOINT_SPRING) return false;
    driver = editor_project_anchor_get(object,
        driver_endpoint == 0 ? joint->anchor_a : joint->anchor_b);
    driven = editor_project_anchor_get(object,
        driver_endpoint == 0 ? joint->anchor_b : joint->anchor_a);
    if(driver == NULL || driven == NULL) return false;
    driver_body = editor_project_rigid_body_get(object, driver->rigid_body);
    driven_body = editor_project_rigid_body_get(object, driven->rigid_body);
    if(driven_body == driver_body && driven_body != NULL) return true;

    if(joint->kind == EDITOR_JOINT_WELD && driven_body != NULL) {
        if(driver_body != NULL) {
            driven_body->rotation = driver_endpoint == 0 ?
                driver_body->rotation + joint->rest_angle :
                driver_body->rotation - joint->rest_angle;
        }
    }
    driver_world = editor_anchor_world_position_get(object, driver);
    driven_world = editor_anchor_world_position_get(object, driven);
    if(driven_body != NULL && driven->position_follows_body) {
        driven_body->position.x += driver_world.x - driven_world.x;
        driven_body->position.y += driver_world.y - driven_world.y;
    } else {
        driven->position = driver_world;
    }
    return true;
}

bool editor_project_joint_constraints_apply(EditorObject *object, EditorJoint *joint) {
    if(object == NULL || joint == NULL) return false;
    if(joint->kind == EDITOR_JOINT_SPRING) return true;
    if(editor_project_anchor_get(object, joint->anchor_a) == NULL ||
            editor_project_anchor_get(object, joint->anchor_b) == NULL) return true;
    return editor_project_joint_constraint_from_endpoint_apply(object, joint, 0);
}

bool editor_project_joint_kind_set(EditorObject *object, EditorJoint *joint,
    EditorJointKind kind) {
    if(object == NULL || joint == NULL || kind < EDITOR_JOINT_REVOLUTE ||
            kind > EDITOR_JOINT_SPRING) return false;
    joint->kind = kind;
    if(kind == EDITOR_JOINT_WELD && joint->anchor_a != 0 && joint->anchor_b != 0) {
        EditorAnchor *a = editor_project_anchor_get(object, joint->anchor_a);
        EditorAnchor *b = editor_project_anchor_get(object, joint->anchor_b);
        EditorRigidBody *body_a = a == NULL ? NULL :
            editor_project_rigid_body_get(object, a->rigid_body);
        EditorRigidBody *body_b = b == NULL ? NULL :
            editor_project_rigid_body_get(object, b->rigid_body);

        if(body_a != NULL && body_b != NULL) {
            joint->rest_angle = body_b->rotation - body_a->rotation;
        }
    }
    return editor_project_joint_constraints_apply(object, joint);
}

void editor_project_anchor_constraints_apply(EditorObject *object, EditorAnchorId anchor) {
    if(object == NULL || anchor == 0) return;
    for(size_t i = 0; i < object->joint_count; i += 1) {
        EditorJoint *joint = &object->joint_items[i];
        if(joint->anchor_a == anchor || joint->anchor_b == anchor) {
            if(joint->kind != EDITOR_JOINT_SPRING && joint->anchor_a != 0 &&
                    joint->anchor_b != 0) {
                (void)editor_project_joint_constraint_from_endpoint_apply(
                    object, joint, joint->anchor_b == anchor ? 1u : 0u);
            }
        }
    }
}

void editor_project_rigid_body_constraints_apply(EditorObject *object,
    EditorRigidBodyId rigid_body) {
    size_t queue_begin = 0;
    size_t queue_end = 0;
    EditorRigidBody *body;
    bool *resolved;
    EditorRigidBodyId *queue;

    if(object == NULL || rigid_body == 0) return;
    body = editor_project_rigid_body_get(object, rigid_body);
    if(body == NULL) return;
    resolved = calloc(object->rigid_body_count, sizeof(*resolved));
    queue = malloc(object->rigid_body_count * sizeof(*queue));
    if(resolved == NULL || queue == NULL) {
        free(resolved);
        free(queue);
        return;
    }
    resolved[(size_t)(body - object->rigid_bodies)] = true;
    queue[queue_end++] = rigid_body;
    while(queue_begin < queue_end) {
        EditorRigidBodyId driver_id = queue[queue_begin++];

        for(size_t i = 0; i < object->joint_count; i += 1) {
            EditorJoint *joint = &object->joint_items[i];
            EditorAnchor *a;
            EditorAnchor *b;
            EditorRigidBody *driven_body;
            uint32_t driver_endpoint;
            size_t driven_index;

            if(joint->kind == EDITOR_JOINT_SPRING) continue;
            a = editor_project_anchor_get(object, joint->anchor_a);
            b = editor_project_anchor_get(object, joint->anchor_b);
            if(a == NULL || b == NULL) continue;
            if(a->rigid_body == driver_id) driver_endpoint = 0;
            else if(b->rigid_body == driver_id) driver_endpoint = 1;
            else continue;
            driven_body = editor_project_rigid_body_get(object,
                driver_endpoint == 0 ? b->rigid_body : a->rigid_body);
            if(driven_body == NULL) {
                (void)editor_project_joint_constraint_from_endpoint_apply(
                    object, joint, driver_endpoint == 0 ? 1 : 0);
                continue;
            }
            driven_index = (size_t)(driven_body - object->rigid_bodies);
            if(resolved[driven_index]) continue;
            (void)editor_project_joint_constraint_from_endpoint_apply(
                object, joint, driver_endpoint);
            resolved[driven_index] = true;
            queue[queue_end++] = driven_body->id;
        }
    }
    free(resolved);
    free(queue);
}

EditorJoint *editor_project_joint_add(EditorProject *project, EditorObject *object,
    EditorJointKind kind) {
    EditorJoint *joint;

    if(project == NULL || object == NULL ||
            !EDITOR_ARRAY_RESERVE(object->joint_items, object->joint_capacity,
                object->joint_count + 1)) {
        return NULL;
    }
    joint = &object->joint_items[object->joint_count++];
    *joint = editor_project_joint_default_get(kind);
    joint->id = project->next_joint_id++;
    snprintf(joint->name, sizeof(joint->name), "joint_%u", joint->id);
    editor_project_hierarchy_item_add(object, EDITOR_HIERARCHY_JOINT, joint->id);
    return joint;
}

bool editor_project_joint_remove(EditorObject *object, EditorJointId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->joint_count; i += 1) {
        if(object->joint_items[i].id != id) continue;
        for(size_t j = i + 1; j < object->joint_count; j += 1) {
            object->joint_items[j - 1] = object->joint_items[j];
        }
        object->joint_count -= 1;
        object->joint_items[object->joint_count] = (EditorJoint){0};
        editor_project_object_hierarchy_sync(object);
        return true;
    }
    return false;
}

bool editor_project_joint_anchor_set(EditorObject *object, EditorJoint *joint,
    uint32_t endpoint, EditorAnchorId anchor) {
    if(object == NULL || joint == NULL || endpoint > 1 ||
            (anchor != 0 && editor_project_anchor_get(object, anchor) == NULL)) return false;
    if(endpoint == 0) joint->anchor_a = anchor;
    else joint->anchor_b = anchor;
    if(joint->kind == EDITOR_JOINT_SPRING && joint->anchor_a != 0 &&
            joint->anchor_b != 0) {
        EditorAnchor *a = editor_project_anchor_get(object, joint->anchor_a);
        EditorAnchor *b = editor_project_anchor_get(object, joint->anchor_b);
        Position world_a = editor_anchor_world_position_get(object, a);
        Position world_b = editor_anchor_world_position_get(object, b);
        joint->rest_length = hypotf(world_b.x - world_a.x, world_b.y - world_a.y);
    } else if(joint->kind == EDITOR_JOINT_WELD && joint->anchor_a != 0 &&
            joint->anchor_b != 0) {
        EditorAnchor *a = editor_project_anchor_get(object, joint->anchor_a);
        EditorAnchor *b = editor_project_anchor_get(object, joint->anchor_b);
        EditorRigidBody *body_a = editor_project_rigid_body_get(object, a->rigid_body);
        EditorRigidBody *body_b = editor_project_rigid_body_get(object, b->rigid_body);

        if(body_a != NULL && body_b != NULL) {
            joint->rest_angle = body_b->rotation - body_a->rotation;
        }
    }
    return editor_project_joint_constraints_apply(object, joint);
}

EditorSoftBody *editor_project_soft_body_add(EditorProject *project, EditorObject *object) {
    EditorSoftBody *body;

    if(project == NULL || object == NULL ||
            !EDITOR_ARRAY_RESERVE(object->soft_body_items,
                object->soft_body_capacity,
                object->soft_body_count + 1)) return NULL;
    body = &object->soft_body_items[object->soft_body_count++];
    *body = (EditorSoftBody){
        .id = project->next_soft_body_id++,
        .node_color = UINT32_C(0xffaa46ff),
        .beam_color = UINT32_C(0xebf0f5ff),
        .area_color = UINT32_C(0x505a78ff),
        .visible = true
    };
    if(!EDITOR_ARRAY_RESERVE(body->nodes, body->node_capacity,
            EDITOR_SOFT_NODE_MAX) || !EDITOR_ARRAY_RESERVE(body->beams,
                body->beam_capacity, EDITOR_SOFT_BEAM_MAX) ||
            !EDITOR_ARRAY_RESERVE(body->areas, body->area_capacity,
                EDITOR_SOFT_AREA_MAX) || !EDITOR_ARRAY_RESERVE(body->hierarchy,
                body->hierarchy_capacity, EDITOR_SOFT_BODY_HIERARCHY_MAX)) {
        editor_project_soft_body_destroy(body);
        object->soft_body_count -= 1;
        return NULL;
    }
    memset(body->nodes, 0, body->node_capacity * sizeof(*body->nodes));
    memset(body->beams, 0, body->beam_capacity * sizeof(*body->beams));
    memset(body->areas, 0, body->area_capacity * sizeof(*body->areas));
    memset(body->hierarchy, 0,
        body->hierarchy_capacity * sizeof(*body->hierarchy));
    snprintf(body->name, sizeof(body->name), "soft_body_%u", body->id);
    editor_project_hierarchy_item_add(object, EDITOR_HIERARCHY_SOFT_BODY, body->id);
    return body;
}

bool editor_project_soft_body_remove(EditorObject *object, EditorSoftBodyId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->soft_body_count; i += 1) {
        if(object->soft_body_items[i].id != id) continue;
        editor_project_soft_body_destroy(&object->soft_body_items[i]);
        for(size_t j = i + 1; j < object->soft_body_count; j += 1) {
            object->soft_body_items[j - 1] = object->soft_body_items[j];
        }
        object->soft_body_count -= 1;
        object->soft_body_items[object->soft_body_count] = (EditorSoftBody){0};
        editor_project_object_hierarchy_sync(object);
        return true;
    }
    return false;
}

bool editor_project_soft_body_origin_set(EditorSoftBody *body, Position position) {
    Vec2D world_delta;
    Vec2D local_delta;
    float cosine;
    float sine;

    if(body == NULL) return false;
    world_delta = (Vec2D){position.x - body->position.x,
        position.y - body->position.y};
    cosine = cosf(-body->rotation);
    sine = sinf(-body->rotation);
    local_delta = (Vec2D){world_delta.x * cosine - world_delta.y * sine,
        world_delta.x * sine + world_delta.y * cosine};
    body->position = position;
    for(size_t i = 0; i < body->node_count; i += 1) {
        body->nodes[i].position.x -= local_delta.x;
        body->nodes[i].position.y -= local_delta.y;
    }
    return true;
}

EditorSoftNode *editor_project_soft_node_add(EditorProject *project, EditorSoftBody *body,
    Position position) {
    EditorSoftNode *node;

    if(project == NULL || body == NULL ||
            !EDITOR_ARRAY_RESERVE(body->nodes, body->node_capacity,
                body->node_count + 1) ||
            !EDITOR_ARRAY_RESERVE(body->hierarchy, body->hierarchy_capacity,
                body->hierarchy_count + 1)) {
        return NULL;
    }
    node = &body->nodes[body->node_count++];
    *node = (EditorSoftNode){
        .id = project->next_soft_node_id++,
        .position = position,
        .node_mass = 1.0f,
        .radius = 4.0f,
        .friction = 0.0f,
        .restitution = 0.25f,
        .gravity_enabled = false,
        .collision_enabled = true,
        .collision_category = UINT64_C(1),
        .collision_with = UINT64_C(1),
        .color = UINT32_C(0xffaa46ff),
        .visible = true
    };
    snprintf(node->name, sizeof(node->name), "node_%u", node->id);
    body->hierarchy[body->hierarchy_count++] = (EditorSoftHierarchyItem){
        EDITOR_SOFT_HIERARCHY_NODE, node->id};
    return node;
}

static bool editor_project_soft_hierarchy_item_exists(const EditorSoftBody *body,
        EditorSoftHierarchyItem item) {
    if(item.kind == EDITOR_SOFT_HIERARCHY_NODE) {
        for(size_t i = 0; i < body->node_count; i += 1)
            if(body->nodes[i].id == item.id) return true;
    } else if(item.kind == EDITOR_SOFT_HIERARCHY_BEAM) {
        for(size_t i = 0; i < body->beam_count; i += 1)
            if(body->beams[i].id == item.id) return true;
    } else if(item.kind == EDITOR_SOFT_HIERARCHY_AREA) {
        for(size_t i = 0; i < body->area_count; i += 1)
            if(body->areas[i].id == item.id) return true;
    }
    return false;
}

static void editor_project_soft_hierarchy_item_add(EditorSoftBody *body,
        EditorSoftHierarchyItemKind kind, uint32_t id) {
    if(body == NULL || id == 0 || !EDITOR_ARRAY_RESERVE(body->hierarchy,
            body->hierarchy_capacity, body->hierarchy_count + 1)) return;
    body->hierarchy[body->hierarchy_count++] = (EditorSoftHierarchyItem){kind, id};
}

void editor_project_soft_body_hierarchy_sync(EditorSoftBody *body) {
    size_t output = 0;
    if(body == NULL) return;
    for(size_t i = 0; i < body->hierarchy_count; i += 1) {
        bool duplicate = false;
        if(!editor_project_soft_hierarchy_item_exists(body, body->hierarchy[i])) continue;
        for(size_t j = 0; j < output; j += 1)
            if(body->hierarchy[j].kind == body->hierarchy[i].kind &&
                    body->hierarchy[j].id == body->hierarchy[i].id) duplicate = true;
        if(!duplicate) body->hierarchy[output++] = body->hierarchy[i];
    }
    body->hierarchy_count = output;
    for(size_t i = 0; i < body->node_count; i += 1) {
        EditorSoftHierarchyItem item = {EDITOR_SOFT_HIERARCHY_NODE, body->nodes[i].id};
        bool found = false;
        for(size_t j = 0; j < body->hierarchy_count; j += 1)
            if(body->hierarchy[j].kind == item.kind && body->hierarchy[j].id == item.id)
                found = true;
        if(!found) editor_project_soft_hierarchy_item_add(body, item.kind, item.id);
    }
    for(size_t i = 0; i < body->beam_count; i += 1) {
        EditorSoftHierarchyItem item = {EDITOR_SOFT_HIERARCHY_BEAM, body->beams[i].id};
        bool found = false;
        for(size_t j = 0; j < body->hierarchy_count; j += 1)
            if(body->hierarchy[j].kind == item.kind && body->hierarchy[j].id == item.id)
                found = true;
        if(!found) editor_project_soft_hierarchy_item_add(body, item.kind, item.id);
    }
    for(size_t i = 0; i < body->area_count; i += 1) {
        EditorSoftHierarchyItem item = {EDITOR_SOFT_HIERARCHY_AREA, body->areas[i].id};
        bool found = false;
        for(size_t j = 0; j < body->hierarchy_count; j += 1)
            if(body->hierarchy[j].kind == item.kind && body->hierarchy[j].id == item.id)
                found = true;
        if(!found) editor_project_soft_hierarchy_item_add(body, item.kind, item.id);
    }
}

size_t editor_project_soft_body_hierarchy_index_get(const EditorSoftBody *body,
        EditorSoftHierarchyItemKind kind, uint32_t id) {
    if(body == NULL) return SIZE_MAX;
    for(size_t i = 0; i < body->hierarchy_count; i += 1)
        if(body->hierarchy[i].kind == kind && body->hierarchy[i].id == id) return i;
    return SIZE_MAX;
}

bool editor_project_soft_node_remove(EditorProject *project, EditorSoftBody *body,
        EditorSoftNodeId id) {
    if(project == NULL || body == NULL || id == 0) return false;
    for(size_t i = 0; i < body->node_count; i += 1) {
        if(body->nodes[i].id != id) continue;
        for(size_t j = 0; j < body->beam_count; j += 1) {
            EditorSoftBeam *beam = &body->beams[j];
            if(beam->node_a == id) beam->node_a = 0;
            if(beam->node_b == id) beam->node_b = 0;
        }
        for(size_t j = i + 1; j < body->node_count; j += 1) {
            body->nodes[j - 1] = body->nodes[j];
        }
        body->node_count -= 1;
        body->nodes[body->node_count] = (EditorSoftNode){0};
        editor_project_soft_areas_sync(project, body);
        editor_project_soft_body_hierarchy_sync(body);
        return true;
    }
    return false;
}

EditorSoftBeam *editor_project_soft_beam_add(EditorProject *project, EditorSoftBody *body,
    EditorSoftNodeId node_a, EditorSoftNodeId node_b) {
    EditorSoftBeam *beam;
    bool found_a = node_a == 0;
    bool found_b = node_b == 0;

    if(project == NULL || body == NULL || (node_a != 0 && node_a == node_b) ||
            !EDITOR_ARRAY_RESERVE(body->beams, body->beam_capacity,
                body->beam_count + 1)) return NULL;
    for(size_t i = 0; i < body->node_count; i += 1) {
        if(body->nodes[i].id == node_a) found_a = true;
        if(body->nodes[i].id == node_b) found_b = true;
    }
    if(!found_a || !found_b) return NULL;
    beam = &body->beams[body->beam_count++];
    *beam = (EditorSoftBeam){
        .id = project->next_soft_beam_id++,
        .node_a = node_a,
        .node_b = node_b,
        .stiffness = 1.0f,
        .damping = 0.0f,
        .color = UINT32_C(0xebf0f5ff),
        .visible = true
    };
    snprintf(beam->name, sizeof(beam->name), "beam_%u", beam->id);
    editor_project_soft_hierarchy_item_add(body, EDITOR_SOFT_HIERARCHY_BEAM, beam->id);
    editor_project_soft_areas_sync(project, body);
    return beam;
}

bool editor_project_soft_beam_remove(EditorProject *project, EditorSoftBody *body,
        EditorSoftBeamId id) {
    if(project == NULL || body == NULL || id == 0) return false;
    for(size_t i = 0; i < body->beam_count; i += 1) {
        if(body->beams[i].id != id) continue;
        for(size_t j = i + 1; j < body->beam_count; j += 1) {
            body->beams[j - 1] = body->beams[j];
        }
        body->beam_count -= 1;
        body->beams[body->beam_count] = (EditorSoftBeam){0};
        editor_project_soft_areas_sync(project, body);
        editor_project_soft_body_hierarchy_sync(body);
        return true;
    }
    return false;
}

static const EditorSoftNode *editor_soft_node_by_id_get(const EditorSoftBody *body,
        EditorSoftNodeId id) {
    if(body == NULL || id == 0) return NULL;
    for(size_t i = 0; i < body->node_count; i += 1)
        if(body->nodes[i].id == id) return &body->nodes[i];
    return NULL;
}

static bool editor_soft_area_boundary_equal(const EditorSoftArea *area,
        const EditorSoftNodeId *nodes, size_t count) {
    if(area == NULL || nodes == NULL || area->node_count != count) return false;
    for(size_t start = 0; start < count; start += 1) {
        if(area->nodes[start] != nodes[0]) continue;
        for(size_t direction = 0; direction < 2; direction += 1) {
            bool equal = true;
            for(size_t i = 0; i < count; i += 1) {
                size_t index = direction == 0 ? (start + i) % count :
                    (start + count - i) % count;
                if(area->nodes[index] != nodes[i]) equal = false;
            }
            if(equal) return true;
        }
    }
    return false;
}

static float editor_soft_area_signed_twice_get(const EditorSoftBody *body,
        const EditorSoftNodeId *nodes, size_t count) {
    float area = 0.0f;
    for(size_t i = 0; i < count; i += 1) {
        const EditorSoftNode *a = editor_soft_node_by_id_get(body, nodes[i]);
        const EditorSoftNode *b = editor_soft_node_by_id_get(body, nodes[(i + 1) % count]);
        if(a == NULL || b == NULL) return 0.0f;
        area += a->position.x * b->position.y - b->position.x * a->position.y;
    }
    return area;
}

void editor_project_soft_areas_sync(EditorProject *project, EditorSoftBody *body) {
    EditorSoftArea *previous;
    bool *visited;
    EditorSoftNodeId *nodes;
    size_t previous_count;

    if(project == NULL || body == NULL) return;
    previous = body->areas;
    previous_count = body->area_count;
    body->areas = NULL;
    body->area_count = 0;
    body->area_capacity = 0;
    visited = calloc(body->beam_count * 2, sizeof(*visited));
    nodes = malloc((body->beam_count + 1) * sizeof(*nodes));
    if((body->beam_count > 0 && visited == NULL) || nodes == NULL) goto finish;
    for(size_t start_beam = 0; start_beam < body->beam_count; start_beam += 1) {
        for(size_t start_direction = 0; start_direction < 2; start_direction += 1) {
            size_t node_count = 0;
            size_t beam = start_beam;
            size_t direction = start_direction;
            EditorSoftNodeId start = start_direction == 0 ?
                body->beams[start_beam].node_a : body->beams[start_beam].node_b;
            bool closed = false;
            if(visited[start_beam * 2 + start_direction] || start == 0) continue;
            nodes[node_count++] = start;
            for(size_t step = 0; step < body->beam_count * 2; step += 1) {
                EditorSoftNodeId from = direction == 0 ?
                    body->beams[beam].node_a : body->beams[beam].node_b;
                EditorSoftNodeId to = direction == 0 ?
                    body->beams[beam].node_b : body->beams[beam].node_a;
                const EditorSoftNode *from_node = editor_soft_node_by_id_get(body, from);
                const EditorSoftNode *to_node = editor_soft_node_by_id_get(body, to);
                float incoming_angle;
                float best_turn = INFINITY;
                size_t next_beam = SIZE_MAX;
                size_t next_direction = 0;
                visited[beam * 2 + direction] = true;
                if(from_node == NULL || to_node == NULL || to == 0) break;
                if(to == start && node_count >= 3) {
                    closed = true;
                    break;
                }
                if(node_count >= body->beam_count + 1) break;
                nodes[node_count++] = to;
                incoming_angle = atan2f(from_node->position.y - to_node->position.y,
                    from_node->position.x - to_node->position.x);
                for(size_t candidate = 0; candidate < body->beam_count; candidate += 1) {
                    EditorSoftNodeId other = 0;
                    size_t candidate_direction = 0;
                    const EditorSoftNode *other_node;
                    float angle;
                    float turn;
                    if(candidate == beam) continue;
                    if(body->beams[candidate].node_a == to) {
                        other = body->beams[candidate].node_b;
                        candidate_direction = 0;
                    } else if(body->beams[candidate].node_b == to) {
                        other = body->beams[candidate].node_a;
                        candidate_direction = 1;
                    } else continue;
                    other_node = editor_soft_node_by_id_get(body, other);
                    if(other_node == NULL) continue;
                    angle = atan2f(other_node->position.y - to_node->position.y,
                        other_node->position.x - to_node->position.x);
                    turn = incoming_angle - angle;
                    while(turn <= 0.0f) turn += 2.0f * PI_F;
                    if(turn < best_turn) {
                        best_turn = turn;
                        next_beam = candidate;
                        next_direction = candidate_direction;
                    }
                }
                if(next_beam == SIZE_MAX) break;
                beam = next_beam;
                direction = next_direction;
            }
            if(closed && node_count >= 3 &&
                    editor_soft_area_signed_twice_get(body, nodes, node_count) > 0.0001f) {
                EditorSoftArea area = {0};
                for(size_t i = 0; i < previous_count; i += 1) {
                    if(editor_soft_area_boundary_equal(&previous[i], nodes, node_count)) {
                        area = previous[i];
                        previous[i] = (EditorSoftArea){0};
                        break;
                    }
                }
                if(area.id == 0) {
                    area.id = project->next_soft_area_id++;
                    area.color = body->area_color;
                    area.visible = true;
                    snprintf(area.name, sizeof(area.name), "area_%u", area.id);
                }
                if(!EDITOR_ARRAY_RESERVE(area.nodes, area.node_capacity,
                        node_count) || !EDITOR_ARRAY_RESERVE(body->areas,
                            body->area_capacity, body->area_count + 1)) {
                    editor_project_soft_area_destroy(&area);
                    goto finish;
                }
                memcpy(area.nodes, nodes, node_count * sizeof(*nodes));
                area.node_count = node_count;
                body->areas[body->area_count++] = area;
            }
        }
    }
finish:
    for(size_t i = 0; i < previous_count; i += 1)
        editor_project_soft_area_destroy(&previous[i]);
    free(previous);
    free(visited);
    free(nodes);
    editor_project_soft_body_hierarchy_sync(body);
}

static float editor_soft_triangle_cross(Position a, Position b, Position c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static bool editor_soft_point_in_triangle(Position point, Position a, Position b,
        Position c) {
    float ab = editor_soft_triangle_cross(a, b, point);
    float bc = editor_soft_triangle_cross(b, c, point);
    float ca = editor_soft_triangle_cross(c, a, point);
    return ab >= -0.0001f && bc >= -0.0001f && ca >= -0.0001f;
}

size_t editor_project_soft_area_triangulate(const EditorSoftBody *body,
        const EditorSoftArea *area, uint32_t triangles[][3], size_t capacity) {
    uint32_t *remaining;
    size_t count;
    size_t triangle_count = 0;
    if(body == NULL || area == NULL || triangles == NULL || area->node_count < 3) return 0;
    count = area->node_count;
    remaining = malloc(count * sizeof(*remaining));
    if(remaining == NULL) return 0;
    for(size_t i = 0; i < count; i += 1) remaining[i] = (uint32_t)i;
    while(count > 3 && triangle_count < capacity) {
        bool clipped = false;
        for(size_t i = 0; i < count; i += 1) {
            uint32_t previous = remaining[(i + count - 1) % count];
            uint32_t current = remaining[i];
            uint32_t next = remaining[(i + 1) % count];
            const EditorSoftNode *a = editor_soft_node_by_id_get(body, area->nodes[previous]);
            const EditorSoftNode *b = editor_soft_node_by_id_get(body, area->nodes[current]);
            const EditorSoftNode *c = editor_soft_node_by_id_get(body, area->nodes[next]);
            bool contains = false;
            if(a == NULL || b == NULL || c == NULL ||
                    editor_soft_triangle_cross(a->position, b->position, c->position) <=
                        0.0001f) continue;
            for(size_t j = 0; j < count; j += 1) {
                uint32_t candidate = remaining[j];
                const EditorSoftNode *node;
                if(candidate == previous || candidate == current || candidate == next) continue;
                node = editor_soft_node_by_id_get(body, area->nodes[candidate]);
                if(node != NULL && editor_soft_point_in_triangle(node->position,
                        a->position, b->position, c->position)) contains = true;
            }
            if(contains) continue;
            triangles[triangle_count][0] = previous;
            triangles[triangle_count][1] = current;
            triangles[triangle_count][2] = next;
            triangle_count += 1;
            for(size_t j = i + 1; j < count; j += 1) remaining[j - 1] = remaining[j];
            count -= 1;
            clipped = true;
            break;
        }
        if(!clipped) {
            free(remaining);
            return 0;
        }
    }
    if(count == 3 && triangle_count < capacity) {
        triangles[triangle_count][0] = remaining[0];
        triangles[triangle_count][1] = remaining[1];
        triangles[triangle_count][2] = remaining[2];
        triangle_count += 1;
    }
    free(remaining);
    return triangle_count;
}

EditorSprite *editor_project_sprite_add(EditorProject *project, EditorObject *object,
        const char *name, const char *path) {
    EditorSprite *sprite;
    if(project == NULL || object == NULL || name == NULL || path == NULL ||
            path[0] == '\0' || !EDITOR_ARRAY_RESERVE(object->sprites,
                object->sprite_capacity, object->sprite_count + 1)) return NULL;
    sprite = &object->sprites[object->sprite_count++];
    *sprite = (EditorSprite){.id = project->next_sprite_id++,
        .size = {64.0f, 64.0f}, .follow_body_rotation = true, .visible = true};
    editor_project_property_name_format(sprite->name, sizeof(sprite->name), name);
    snprintf(sprite->path, sizeof(sprite->path), "%s", path);
    editor_project_hierarchy_item_add(object, EDITOR_HIERARCHY_SPRITE, sprite->id);
    return sprite;
}

EditorSprite *editor_project_sprite_get(EditorObject *object, EditorSpriteId id) {
    if(object == NULL || id == 0) return NULL;
    for(size_t i = 0; i < object->sprite_count; i += 1)
        if(object->sprites[i].id == id) return &object->sprites[i];
    return NULL;
}

bool editor_project_sprite_remove(EditorObject *object, EditorSpriteId id) {
    size_t index;
    if(object == NULL || id == 0) return false;
    for(index = 0; index < object->sprite_count; index += 1)
        if(object->sprites[index].id == id) break;
    if(index == object->sprite_count) return false;
    for(size_t i = index + 1; i < object->sprite_count; i += 1)
        object->sprites[i - 1] = object->sprites[i];
    object->sprite_count -= 1;
    editor_project_object_hierarchy_sync(object);
    return true;
}

EditorAnimatedSprite *editor_project_animated_sprite_add(EditorProject *project,
        EditorObject *object) {
    EditorAnimatedSprite *sprite;
    if(project == NULL || object == NULL ||
            !EDITOR_ARRAY_RESERVE(object->animated_sprite_items,
                object->animated_sprite_capacity,
                object->animated_sprite_count + 1)) return NULL;
    sprite = &object->animated_sprite_items[object->animated_sprite_count++];
    *sprite = (EditorAnimatedSprite){
        .id = project->next_animated_sprite_id++,
        .scale = {1.0f, 1.0f},
        .direction = DIRECTION_RIGHT,
        .follow_body_rotation = true,
        .visible = true
    };
    snprintf(sprite->name, sizeof(sprite->name), "animated_sprite_%u", sprite->id);
    if(!EDITOR_ARRAY_RESERVE(sprite->frames, sprite->frame_capacity, 4)) {
        object->animated_sprite_count -= 1;
        return NULL;
    }
    editor_project_hierarchy_item_add(object,
        EDITOR_HIERARCHY_ANIMATED_SPRITE, sprite->id);
    return sprite;
}

EditorAnimatedSprite *editor_project_animated_sprite_get(EditorObject *object,
        EditorAnimatedSpriteId id) {
    if(object == NULL || id == 0) return NULL;
    for(size_t i = 0; i < object->animated_sprite_count; i += 1)
        if(object->animated_sprite_items[i].id == id) return &object->animated_sprite_items[i];
    return NULL;
}

bool editor_project_animated_sprite_remove(EditorObject *object,
        EditorAnimatedSpriteId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->animated_sprite_count; i += 1) {
        if(object->animated_sprite_items[i].id != id) continue;
        for(size_t body_index = 0; body_index < object->rigid_body_count;
                body_index += 1) {
            EditorRigidBody *body = &object->rigid_bodies[body_index];
            for(size_t binding = 0;
                    binding < body->hitbox_animation_binding_count;) {
                if(body->hitbox_animation_bindings[binding].animation != id) {
                    binding += 1;
                    continue;
                }
                for(size_t j = binding + 1;
                        j < body->hitbox_animation_binding_count; j += 1)
                    body->hitbox_animation_bindings[j - 1] =
                        body->hitbox_animation_bindings[j];
                body->hitbox_animation_binding_count -= 1;
            }
        }
        editor_project_animated_sprite_destroy(&object->animated_sprite_items[i]);
        for(size_t j = i + 1; j < object->animated_sprite_count; j += 1)
            object->animated_sprite_items[j - 1] = object->animated_sprite_items[j];
        object->animated_sprite_count -= 1;
        object->animated_sprite_items[object->animated_sprite_count] =
            (EditorAnimatedSprite){0};
        editor_project_object_hierarchy_sync(object);
        return true;
    }
    return false;
}

bool editor_project_animation_frame_add(EditorProject *project,
        EditorAnimatedSprite *animated_sprite, const char *name, const char *path,
        Scale size) {
    EditorAnimationFrame *frame;
    if(project == NULL || animated_sprite == NULL || name == NULL || path == NULL ||
            name[0] == '\0' || path[0] == '\0' || size.x <= 0.0f || size.y <= 0.0f ||
            !EDITOR_ARRAY_RESERVE(animated_sprite->frames,
                animated_sprite->frame_capacity,
                animated_sprite->frame_count + 1)) return false;
    frame = &animated_sprite->frames[animated_sprite->frame_count++];
    *frame = (EditorAnimationFrame){.id = project->next_sprite_id++, .size = size};
    editor_project_property_name_format(frame->name, sizeof(frame->name), name);
    snprintf(frame->path, sizeof(frame->path), "%s", path);
    return true;
}

bool editor_project_animation_frame_remove(EditorAnimatedSprite *animated_sprite,
        size_t index) {
    if(animated_sprite == NULL || index >= animated_sprite->frame_count) return false;
    for(size_t i = index + 1; i < animated_sprite->frame_count; i += 1)
        animated_sprite->frames[i - 1] = animated_sprite->frames[i];
    animated_sprite->frame_count -= 1;
    return true;
}
