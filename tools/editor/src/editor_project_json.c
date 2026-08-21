/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_project.h"
#include "editor_array.h"

#include "yyjson.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static yyjson_mut_val *editor_json_position_write(yyjson_mut_doc *document,
    Position position) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_obj_add_real(document, value, "x", position.x);
    yyjson_mut_obj_add_real(document, value, "y", position.y);
    return value;
}

static bool editor_json_position_read(yyjson_val *value, Position *position) {
    yyjson_val *x;
    yyjson_val *y;
    if(!yyjson_is_obj(value) || position == NULL) return false;
    x = yyjson_obj_get(value, "x");
    y = yyjson_obj_get(value, "y");
    if(!yyjson_is_num(x) || !yyjson_is_num(y)) return false;
    *position = (Position){(float)yyjson_get_num(x), (float)yyjson_get_num(y)};
    return true;
}

static bool editor_json_uint(yyjson_val *object, const char *key, uint32_t *value) {
    yyjson_val *item = yyjson_obj_get(object, key);
    if(!yyjson_is_uint(item) || yyjson_get_uint(item) > UINT32_MAX || value == NULL) {
        return false;
    }
    *value = (uint32_t)yyjson_get_uint(item);
    return true;
}

static bool editor_json_uint64(yyjson_val *object, const char *key, uint64_t *value) {
    yyjson_val *item = yyjson_obj_get(object, key);
    if(!yyjson_is_uint(item) || value == NULL) return false;
    *value = yyjson_get_uint(item);
    return true;
}

static bool editor_json_real(yyjson_val *object, const char *key, float *value) {
    yyjson_val *item = yyjson_obj_get(object, key);
    if(!yyjson_is_num(item) || value == NULL) return false;
    *value = (float)yyjson_get_num(item);
    return true;
}

static bool editor_json_optional_real(yyjson_val *object, const char *key,
        float *value, float fallback) {
    yyjson_val *item;
    if(object == NULL || key == NULL || value == NULL) return false;
    item = yyjson_obj_get(object, key);
    if(item == NULL) {
        *value = fallback;
        return true;
    }
    if(!yyjson_is_num(item)) return false;
    *value = (float)yyjson_get_num(item);
    return true;
}

static bool editor_json_bool(yyjson_val *object, const char *key, bool *value) {
    yyjson_val *item = yyjson_obj_get(object, key);
    if(!yyjson_is_bool(item) || value == NULL) return false;
    *value = yyjson_get_bool(item);
    return true;
}

static bool editor_json_name(yyjson_val *object, char name[EDITOR_OBJECT_NAME_MAX]) {
    yyjson_val *item = yyjson_obj_get(object, "name");
    size_t length;
    if(!yyjson_is_str(item) || name == NULL) return false;
    length = yyjson_get_len(item);
    if(length == 0 || length >= EDITOR_OBJECT_NAME_MAX) return false;
    memcpy(name, yyjson_get_str(item), length + 1);
    return true;
}

static yyjson_mut_val *editor_json_hitbox_write(yyjson_mut_doc *document,
    const EditorHitbox *hitbox) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_val *vertices = yyjson_mut_arr(document);
    yyjson_mut_val *lines = yyjson_mut_arr(document);
    yyjson_mut_obj_add_uint(document, value, "id", hitbox->id);
    yyjson_mut_obj_add_strcpy(document, value, "name", hitbox->name);
    yyjson_mut_obj_add_bool(document, value, "visible", hitbox->visible);
    for(size_t i = 0; i < hitbox->vertex_count; i += 1) {
        yyjson_mut_val *vertex = yyjson_mut_obj(document);
        yyjson_mut_obj_add_uint(document, vertex, "id", hitbox->vertices[i].id);
        yyjson_mut_obj_add_strcpy(document, vertex, "name", hitbox->vertices[i].name);
        yyjson_mut_obj_add_val(document, vertex, "position",
            editor_json_position_write(document, hitbox->vertices[i].position));
        yyjson_mut_obj_add_bool(document, vertex, "position_locked",
            hitbox->vertices[i].position_locked);
        yyjson_mut_arr_add_val(vertices, vertex);
        yyjson_mut_arr_add_strcpy(document, lines, hitbox->line_names[i]);
    }
    yyjson_mut_obj_add_val(document, value, "vertices", vertices);
    yyjson_mut_obj_add_val(document, value, "lines", lines);
    return value;
}

static yyjson_mut_val *editor_json_body_write(yyjson_mut_doc *document,
    const EditorRigidBody *body) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_val *hitboxes = yyjson_mut_arr(document);
    yyjson_mut_val *bindings = yyjson_mut_arr(document);
    float particle_radius = body->particle_auto_fit ?
        editor_project_particle_auto_radius_get(body) : body->particle_radius;
    yyjson_mut_obj_add_uint(document, value, "id", body->id);
    yyjson_mut_obj_add_strcpy(document, value, "name", body->name);
    yyjson_mut_obj_add_val(document, value, "position",
        editor_json_position_write(document, body->position));
    yyjson_mut_obj_add_real(document, value, "rotation", body->rotation);
    yyjson_mut_obj_add_real(document, value, "mass", body->mass_value);
    yyjson_mut_obj_add_real(document, value, "friction", body->friction);
    yyjson_mut_obj_add_real(document, value, "restitution", body->restitution);
    yyjson_mut_obj_add_bool(document, value, "static", body->static_body);
    yyjson_mut_obj_add_bool(document, value, "rotation_locked", body->rotation_locked);
    yyjson_mut_obj_add_bool(document, value, "gravity_enabled", body->gravity_enabled);
    yyjson_mut_obj_add_bool(document, value, "collision_enabled", body->collision_enabled);
    yyjson_mut_obj_add_bool(document, value, "particle", body->particle);
    yyjson_mut_obj_add_bool(document, value, "particle_auto_fit",
        body->particle_auto_fit);
    yyjson_mut_obj_add_real(document, value, "particle_radius", particle_radius);
    yyjson_mut_obj_add_val(document, value, "particle_origin",
        editor_json_position_write(document, body->particle_origin));
    yyjson_mut_obj_add_uint(document, value, "particle_ring_color",
        body->particle_ring_color);
    yyjson_mut_obj_add_uint(document, value, "particle_fill_color",
        body->particle_fill_color);
    yyjson_mut_obj_add_uint(document, value, "collision_category",
        body->collision_category);
    yyjson_mut_obj_add_uint(document, value, "collision_with", body->collision_with);
    yyjson_mut_obj_add_bool(document, value, "visible", body->visible);
    yyjson_mut_obj_add_uint(document, value, "border_color", body->border_color);
    yyjson_mut_obj_add_uint(document, value, "surface_color", body->surface_color);
    yyjson_mut_obj_add_uint(document, value, "active_hitbox_index",
        body->active_hitbox_index);
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        yyjson_mut_arr_add_val(hitboxes, editor_json_hitbox_write(document,
            &body->hitboxes[i]));
    }
    yyjson_mut_obj_add_val(document, value, "hitboxes", hitboxes);
    for(size_t i = 0; i < body->hitbox_animation_binding_count; i += 1) {
        const EditorHitboxAnimationBinding *binding =
            &body->hitbox_animation_bindings[i];
        yyjson_mut_val *item = yyjson_mut_obj(document);
        yyjson_mut_obj_add_uint(document, item, "animation", binding->animation);
        yyjson_mut_obj_add_uint(document, item, "frame", binding->frame);
        yyjson_mut_obj_add_uint(document, item, "hitbox", binding->hitbox);
        yyjson_mut_arr_add_val(bindings, item);
    }
    yyjson_mut_obj_add_val(document, value, "hitbox_animation_bindings", bindings);
    return value;
}

static yyjson_mut_val *editor_json_anchor_write(yyjson_mut_doc *document,
    const EditorAnchor *anchor) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_obj_add_uint(document, value, "id", anchor->id);
    yyjson_mut_obj_add_strcpy(document, value, "name", anchor->name);
    yyjson_mut_obj_add_val(document, value, "position",
        editor_json_position_write(document, anchor->position));
    yyjson_mut_obj_add_real(document, value, "rotation", anchor->rotation);
    yyjson_mut_obj_add_uint(document, value, "rigid_body", anchor->rigid_body);
    yyjson_mut_obj_add_bool(document, value, "position_follows_body",
        anchor->position_follows_body);
    yyjson_mut_obj_add_bool(document, value, "rotation_follows_body",
        anchor->rotation_follows_body);
    yyjson_mut_obj_add_bool(document, value, "visible", anchor->visible);
    return value;
}

static yyjson_mut_val *editor_json_joint_write(yyjson_mut_doc *document,
    const EditorJoint *joint) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_obj_add_uint(document, value, "id", joint->id);
    yyjson_mut_obj_add_strcpy(document, value, "name", joint->name);
    yyjson_mut_obj_add_uint(document, value, "kind", (uint32_t)joint->kind);
    yyjson_mut_obj_add_uint(document, value, "anchor_a", joint->anchor_a);
    yyjson_mut_obj_add_uint(document, value, "anchor_b", joint->anchor_b);
    yyjson_mut_obj_add_real(document, value, "rest_length", joint->rest_length);
    yyjson_mut_obj_add_real(document, value, "stiffness", joint->stiffness);
    yyjson_mut_obj_add_real(document, value, "damping", joint->damping);
    yyjson_mut_obj_add_real(document, value, "rest_angle", joint->rest_angle);
    yyjson_mut_obj_add_real(document, value, "visual_size", joint->visual_size);
    yyjson_mut_obj_add_bool(document, value, "visible", joint->visible);
    return value;
}

static yyjson_mut_val *editor_json_soft_body_write(yyjson_mut_doc *document,
    const EditorSoftBody *body) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_val *nodes = yyjson_mut_arr(document);
    yyjson_mut_val *beams = yyjson_mut_arr(document);
    yyjson_mut_val *areas = yyjson_mut_arr(document);
    yyjson_mut_val *hierarchy = yyjson_mut_arr(document);
    yyjson_mut_obj_add_uint(document, value, "id", body->id);
    yyjson_mut_obj_add_strcpy(document, value, "name", body->name);
    yyjson_mut_obj_add_val(document, value, "position",
        editor_json_position_write(document, body->position));
    yyjson_mut_obj_add_real(document, value, "rotation", body->rotation);
    yyjson_mut_obj_add_bool(document, value, "visible", body->visible);
    yyjson_mut_obj_add_uint(document, value, "node_color", body->node_color);
    yyjson_mut_obj_add_uint(document, value, "beam_color", body->beam_color);
    yyjson_mut_obj_add_uint(document, value, "area_color", body->area_color);
    for(size_t i = 0; i < body->node_count; i += 1) {
        const EditorSoftNode *node = &body->nodes[i];
        yyjson_mut_val *item = yyjson_mut_obj(document);
        yyjson_mut_obj_add_uint(document, item, "id", node->id);
        yyjson_mut_obj_add_strcpy(document, item, "name", node->name);
        yyjson_mut_obj_add_val(document, item, "position",
            editor_json_position_write(document, node->position));
        yyjson_mut_obj_add_real(document, item, "mass", node->node_mass);
        yyjson_mut_obj_add_real(document, item, "radius", node->radius);
        yyjson_mut_obj_add_real(document, item, "friction", node->friction);
        yyjson_mut_obj_add_real(document, item, "restitution", node->restitution);
        yyjson_mut_obj_add_bool(document, item, "gravity_enabled", node->gravity_enabled);
        yyjson_mut_obj_add_bool(document, item, "collision_enabled", node->collision_enabled);
        yyjson_mut_obj_add_uint(document, item, "collision_category", node->collision_category);
        yyjson_mut_obj_add_uint(document, item, "collision_with", node->collision_with);
        yyjson_mut_obj_add_bool(document, item, "visible", node->visible);
        yyjson_mut_obj_add_uint(document, item, "color", node->color);
        yyjson_mut_obj_add_bool(document, item, "color_overridden", node->color_overridden);
        yyjson_mut_arr_add_val(nodes, item);
    }
    for(size_t i = 0; i < body->beam_count; i += 1) {
        const EditorSoftBeam *beam = &body->beams[i];
        yyjson_mut_val *item = yyjson_mut_obj(document);
        yyjson_mut_obj_add_uint(document, item, "id", beam->id);
        yyjson_mut_obj_add_strcpy(document, item, "name", beam->name);
        yyjson_mut_obj_add_uint(document, item, "node_a", beam->node_a);
        yyjson_mut_obj_add_uint(document, item, "node_b", beam->node_b);
        yyjson_mut_obj_add_real(document, item, "stiffness", beam->stiffness);
        yyjson_mut_obj_add_real(document, item, "damping", beam->damping);
        yyjson_mut_obj_add_bool(document, item, "visible", beam->visible);
        yyjson_mut_obj_add_uint(document, item, "color", beam->color);
        yyjson_mut_obj_add_bool(document, item, "color_overridden", beam->color_overridden);
        yyjson_mut_arr_add_val(beams, item);
    }
    for(size_t i = 0; i < body->area_count; i += 1) {
        const EditorSoftArea *area = &body->areas[i];
        yyjson_mut_val *item = yyjson_mut_obj(document);
        yyjson_mut_val *area_nodes = yyjson_mut_arr(document);
        yyjson_mut_obj_add_uint(document, item, "id", area->id);
        yyjson_mut_obj_add_strcpy(document, item, "name", area->name);
        for(size_t node_index = 0; node_index < area->node_count; node_index += 1)
            yyjson_mut_arr_add_uint(document, area_nodes, area->nodes[node_index]);
        yyjson_mut_obj_add_val(document, item, "nodes", area_nodes);
        yyjson_mut_obj_add_uint(document, item, "color", area->color);
        yyjson_mut_obj_add_bool(document, item, "color_overridden", area->color_overridden);
        yyjson_mut_obj_add_bool(document, item, "visible", area->visible);
        yyjson_mut_arr_add_val(areas, item);
    }
    for(size_t i = 0; i < body->hierarchy_count; i += 1) {
        yyjson_mut_val *item = yyjson_mut_obj(document);
        yyjson_mut_obj_add_uint(document, item, "kind", body->hierarchy[i].kind);
        yyjson_mut_obj_add_uint(document, item, "id", body->hierarchy[i].id);
        yyjson_mut_arr_add_val(hierarchy, item);
    }
    yyjson_mut_obj_add_val(document, value, "nodes", nodes);
    yyjson_mut_obj_add_val(document, value, "beams", beams);
    yyjson_mut_obj_add_val(document, value, "areas", areas);
    yyjson_mut_obj_add_val(document, value, "hierarchy", hierarchy);
    return value;
}

static yyjson_mut_val *editor_json_animated_sprite_write(yyjson_mut_doc *document,
        const EditorAnimatedSprite *sprite) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_val *frames = yyjson_mut_arr(document);
    yyjson_mut_obj_add_uint(document, value, "id", sprite->id);
    yyjson_mut_obj_add_strcpy(document, value, "name", sprite->name);
    yyjson_mut_obj_add_uint(document, value, "rigid_body", sprite->rigid_body);
    yyjson_mut_obj_add_val(document, value, "editor_position",
        editor_json_position_write(document, sprite->editor_position));
    yyjson_mut_obj_add_real(document, value, "editor_rotation",
        sprite->editor_rotation);
    for(size_t i = 0; i < sprite->frame_count; i += 1) {
        const EditorAnimationFrame *frame = &sprite->frames[i];
        yyjson_mut_val *item = yyjson_mut_obj(document);
        yyjson_mut_obj_add_uint(document, item, "id", frame->id);
        yyjson_mut_obj_add_strcpy(document, item, "name", frame->name);
        yyjson_mut_obj_add_strcpy(document, item, "path", frame->path);
        yyjson_mut_obj_add_real(document, item, "width", frame->size.x);
        yyjson_mut_obj_add_real(document, item, "height", frame->size.y);
        yyjson_mut_arr_add_val(frames, item);
    }
    yyjson_mut_obj_add_val(document, value, "frames", frames);
    yyjson_mut_obj_add_uint(document, value, "ticks_per_frame",
        sprite->ticks_per_frame);
    yyjson_mut_obj_add_real(document, value, "time_per_frame",
        sprite->time_per_frame);
    yyjson_mut_obj_add_uint(document, value, "starting_frame",
        sprite->starting_frame);
    yyjson_mut_obj_add_real(document, value, "scale_x", sprite->scale.x);
    yyjson_mut_obj_add_real(document, value, "scale_y", sprite->scale.y);
    yyjson_mut_obj_add_uint(document, value, "direction", sprite->direction);
    yyjson_mut_obj_add_bool(document, value, "follow_body_rotation",
        sprite->follow_body_rotation);
    yyjson_mut_obj_add_bool(document, value, "visible", sprite->visible);
    yyjson_mut_obj_add_bool(document, value, "playing", sprite->playing);
    return value;
}

bool editor_project_save(const EditorProject *project, const char *path) {
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *objects;
    yyjson_mut_val *collision_masks;
    bool success;
    if(project == NULL || path == NULL || path[0] == '\0') return false;
    document = yyjson_mut_doc_new(NULL);
    if(document == NULL) return false;
    root = yyjson_mut_obj(document);
    objects = yyjson_mut_arr(document);
    collision_masks = yyjson_mut_arr(document);
    yyjson_mut_doc_set_root(document, root);
    yyjson_mut_obj_add_uint(document, root, "format_version", EDITOR_PROJECT_FORMAT_VERSION);
    yyjson_mut_obj_add_val(document, root, "viewport_camera_offset",
        editor_json_position_write(document,
            (Position){project->viewport_camera_offset.x,
                project->viewport_camera_offset.y}));
    yyjson_mut_obj_add_real(document, root, "viewport_camera_zoom",
        project->viewport_camera_zoom);
    yyjson_mut_obj_add_bool(document, root, "viewport_local_view",
        project->viewport_local_view);
    {
        const EditorNavigationState *navigation = &project->navigation;
        yyjson_mut_val *value = yyjson_mut_obj(document);
        yyjson_mut_obj_add_uint(document, value, "mode", navigation->mode);
        yyjson_mut_obj_add_uint(document, value, "selection", navigation->selection);
        yyjson_mut_obj_add_uint(document, value, "object", navigation->object);
        yyjson_mut_obj_add_uint(document, value, "line", navigation->selected_line);
        yyjson_mut_obj_add_uint(document, value, "vertex", navigation->selected_vertex);
        yyjson_mut_obj_add_uint(document, value, "rigid_body", navigation->rigid_body);
        yyjson_mut_obj_add_uint(document, value, "hitbox", navigation->hitbox);
        yyjson_mut_obj_add_uint(document, value, "joint", navigation->joint);
        yyjson_mut_obj_add_uint(document, value, "anchor", navigation->anchor);
        yyjson_mut_obj_add_uint(document, value, "soft_body", navigation->soft_body);
        yyjson_mut_obj_add_uint(document, value, "soft_node", navigation->soft_node);
        yyjson_mut_obj_add_uint(document, value, "soft_beam", navigation->soft_beam);
        yyjson_mut_obj_add_uint(document, value, "sprite", navigation->sprite);
        yyjson_mut_obj_add_uint(document, value, "animated_sprite",
            navigation->animated_sprite);
        yyjson_mut_obj_add_uint(document, value, "animation_frame",
            navigation->animation_frame);
        yyjson_mut_obj_add_uint(document, value, "origin_kind", navigation->origin_kind);
        yyjson_mut_obj_add_val(document, root, "navigation", value);
    }
    yyjson_mut_obj_add_uint(document, root, "selected", project->selected);
    yyjson_mut_obj_add_uint(document, root, "next_object_id", project->next_id);
    yyjson_mut_obj_add_uint(document, root, "next_vertex_id", project->next_vertex_id);
    yyjson_mut_obj_add_uint(document, root, "next_rigid_body_id", project->next_rigid_body_id);
    yyjson_mut_obj_add_uint(document, root, "next_hitbox_id", project->next_hitbox_id);
    yyjson_mut_obj_add_uint(document, root, "next_joint_id", project->next_joint_id);
    yyjson_mut_obj_add_uint(document, root, "next_anchor_id", project->next_anchor_id);
    yyjson_mut_obj_add_uint(document, root, "next_soft_body_id", project->next_soft_body_id);
    yyjson_mut_obj_add_uint(document, root, "next_soft_node_id", project->next_soft_node_id);
    yyjson_mut_obj_add_uint(document, root, "next_soft_beam_id", project->next_soft_beam_id);
    yyjson_mut_obj_add_uint(document, root, "next_soft_area_id", project->next_soft_area_id);
    yyjson_mut_obj_add_uint(document, root, "next_sprite_id", project->next_sprite_id);
    yyjson_mut_obj_add_uint(document, root, "next_animated_sprite_id",
        project->next_animated_sprite_id);
    for(size_t i = 0; i < project->collision_mask_count; i += 1) {
        yyjson_mut_arr_add_strcpy(document, collision_masks,
            project->collision_masks[i].name);
    }
    yyjson_mut_obj_add_val(document, root, "collision_masks", collision_masks);
    for(size_t i = 0; i < project->object_count; i += 1) {
        const EditorObject *object = &project->objects[i];
        yyjson_mut_val *value = yyjson_mut_obj(document);
        yyjson_mut_val *bodies = yyjson_mut_arr(document);
        yyjson_mut_val *anchors = yyjson_mut_arr(document);
        yyjson_mut_val *joint_values = yyjson_mut_arr(document);
        yyjson_mut_val *soft_body_values = yyjson_mut_arr(document);
        yyjson_mut_val *sprites = yyjson_mut_arr(document);
        yyjson_mut_val *animated_sprite_values = yyjson_mut_arr(document);
        yyjson_mut_val *hierarchy = yyjson_mut_arr(document);
        yyjson_mut_obj_add_uint(document, value, "id", object->id);
        yyjson_mut_obj_add_strcpy(document, value, "name", object->name);
        yyjson_mut_obj_add_val(document, value, "position",
            editor_json_position_write(document, object->position));
        yyjson_mut_obj_add_bool(document, value, "visible", object->visible);
        for(size_t j = 0; j < object->rigid_body_count; j += 1)
            yyjson_mut_arr_add_val(bodies, editor_json_body_write(document,
                &object->rigid_bodies[j]));
        for(size_t j = 0; j < object->anchor_count; j += 1)
            yyjson_mut_arr_add_val(anchors, editor_json_anchor_write(document,
                &object->anchors[j]));
        for(size_t j = 0; j < object->joint_count; j += 1)
            yyjson_mut_arr_add_val(joint_values, editor_json_joint_write(document,
                &object->joint_items[j]));
        for(size_t j = 0; j < object->soft_body_count; j += 1)
            yyjson_mut_arr_add_val(soft_body_values, editor_json_soft_body_write(document,
                &object->soft_body_items[j]));
        for(size_t j = 0; j < object->sprite_count; j += 1) {
            yyjson_mut_val *sprite = yyjson_mut_obj(document);
            yyjson_mut_obj_add_uint(document, sprite, "id", object->sprites[j].id);
            yyjson_mut_obj_add_strcpy(document, sprite, "name", object->sprites[j].name);
            yyjson_mut_obj_add_strcpy(document, sprite, "path", object->sprites[j].path);
            yyjson_mut_obj_add_val(document, sprite, "position",
                editor_json_position_write(document, object->sprites[j].position));
            yyjson_mut_obj_add_real(document, sprite, "rotation",
                object->sprites[j].rotation);
            yyjson_mut_obj_add_uint(document, sprite, "rigid_body",
                object->sprites[j].rigid_body);
            yyjson_mut_obj_add_real(document, sprite, "width", object->sprites[j].size.x);
            yyjson_mut_obj_add_real(document, sprite, "height", object->sprites[j].size.y);
            yyjson_mut_obj_add_bool(document, sprite, "follow_body_rotation",
                object->sprites[j].follow_body_rotation);
            yyjson_mut_obj_add_bool(document, sprite, "visible",
                object->sprites[j].visible);
            yyjson_mut_arr_add_val(sprites, sprite);
        }
        for(size_t j = 0; j < object->animated_sprite_count; j += 1)
            yyjson_mut_arr_add_val(animated_sprite_values,
                editor_json_animated_sprite_write(document,
                    &object->animated_sprite_items[j]));
        for(size_t j = 0; j < object->hierarchy_count; j += 1) {
            yyjson_mut_val *item = yyjson_mut_obj(document);
            yyjson_mut_obj_add_uint(document, item, "kind", object->hierarchy[j].kind);
            yyjson_mut_obj_add_uint(document, item, "id", object->hierarchy[j].id);
            yyjson_mut_arr_add_val(hierarchy, item);
        }
        yyjson_mut_obj_add_val(document, value, "rigid_bodies", bodies);
        yyjson_mut_obj_add_val(document, value, "anchors", anchors);
        yyjson_mut_obj_add_val(document, value, "joints", joint_values);
        yyjson_mut_obj_add_val(document, value, "soft_bodies", soft_body_values);
        yyjson_mut_obj_add_val(document, value, "sprites", sprites);
        yyjson_mut_obj_add_val(document, value, "animated_sprites",
            animated_sprite_values);
        yyjson_mut_obj_add_val(document, value, "hierarchy", hierarchy);
        yyjson_mut_arr_add_val(objects, value);
    }
    yyjson_mut_obj_add_val(document, root, "objects", objects);
    success = yyjson_mut_write_file(path, document, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(document);
    return success;
}

static bool editor_json_hitbox_read(yyjson_val *value, EditorHitbox *hitbox,
    EditorProject *project) {
    yyjson_val *vertices = yyjson_obj_get(value, "vertices");
    yyjson_val *lines = yyjson_obj_get(value, "lines");
    size_t count;
    if(!yyjson_is_obj(value) || !editor_json_uint(value, "id", &hitbox->id) ||
            hitbox->id == 0 || !editor_json_name(value, hitbox->name) ||
            !editor_json_bool(value, "visible", &hitbox->visible) ||
            !yyjson_is_arr(vertices) || (lines != NULL && !yyjson_is_arr(lines))) return false;
    editor_project_property_name_format(hitbox->name, sizeof(hitbox->name), hitbox->name);
    count = yyjson_arr_size(vertices);
    if(count < EDITOR_HITBOX_VERTEX_MIN ||
            (lines != NULL && yyjson_arr_size(lines) != count)) return false;
    if(!EDITOR_ARRAY_RESERVE(hitbox->vertices, hitbox->vertex_capacity, count))
        return false;
    hitbox->line_names = calloc(hitbox->vertex_capacity,
        sizeof(*hitbox->line_names));
    if(hitbox->line_names == NULL) return false;
    hitbox->vertex_count = (uint32_t)count;
    for(size_t i = 0; i < count; i += 1) {
        yyjson_val *item = yyjson_arr_get(vertices, i);
        yyjson_val *line = lines == NULL ? NULL : yyjson_arr_get(lines, i);
        yyjson_val *name = yyjson_obj_get(item, "name");
        if(!yyjson_is_obj(item) || !editor_json_uint(item, "id", &hitbox->vertices[i].id) ||
                hitbox->vertices[i].id == 0 || !editor_json_position_read(
                    yyjson_obj_get(item, "position"), &hitbox->vertices[i].position) ||
                !editor_json_bool(item, "position_locked",
                    &hitbox->vertices[i].position_locked) ||
                (name != NULL && (!yyjson_is_str(name) || yyjson_get_len(name) == 0 ||
                    yyjson_get_len(name) >= EDITOR_OBJECT_NAME_MAX)) ||
                (line != NULL && (!yyjson_is_str(line) || yyjson_get_len(line) == 0 ||
                    yyjson_get_len(line) >= EDITOR_OBJECT_NAME_MAX))) {
            return false;
        }
        if(name == NULL) snprintf(hitbox->vertices[i].name,
            sizeof(hitbox->vertices[i].name), "vertex_%zu", i + 1);
        else memcpy(hitbox->vertices[i].name, yyjson_get_str(name),
            yyjson_get_len(name) + 1);
        if(line == NULL) snprintf(hitbox->line_names[i],
            sizeof(hitbox->line_names[i]), "line_%zu", i + 1);
        else memcpy(hitbox->line_names[i], yyjson_get_str(line), yyjson_get_len(line) + 1);
        editor_project_property_name_format(hitbox->vertices[i].name,
            sizeof(hitbox->vertices[i].name), hitbox->vertices[i].name);
        editor_project_property_name_format(hitbox->line_names[i],
            sizeof(hitbox->line_names[i]), hitbox->line_names[i]);
        if(project->next_vertex_id <= hitbox->vertices[i].id)
            project->next_vertex_id = hitbox->vertices[i].id + 1;
    }
    if(project->next_hitbox_id <= hitbox->id) project->next_hitbox_id = hitbox->id + 1;
    return true;
}

static bool editor_json_body_read(yyjson_val *value, EditorRigidBody *body,
    EditorProject *project) {
    yyjson_val *hitboxes = yyjson_obj_get(value, "hitboxes");
    yyjson_val *collision_enabled = yyjson_obj_get(value, "collision_enabled");
    yyjson_val *collision_category = yyjson_obj_get(value, "collision_category");
    yyjson_val *collision_with = yyjson_obj_get(value, "collision_with");
    yyjson_val *particle = yyjson_obj_get(value, "particle");
    yyjson_val *particle_auto_fit = yyjson_obj_get(value, "particle_auto_fit");
    yyjson_val *particle_radius = yyjson_obj_get(value, "particle_radius");
    yyjson_val *particle_origin = yyjson_obj_get(value, "particle_origin");
    yyjson_val *particle_ring_color = yyjson_obj_get(value, "particle_ring_color");
    yyjson_val *particle_fill_color = yyjson_obj_get(value, "particle_fill_color");
    yyjson_val *border_color = yyjson_obj_get(value, "border_color");
    yyjson_val *surface_color = yyjson_obj_get(value, "surface_color");
    yyjson_val *active_hitbox_index = yyjson_obj_get(value, "active_hitbox_index");
    yyjson_val *bindings = yyjson_obj_get(value, "hitbox_animation_bindings");
    uint32_t count;
    *body = editor_project_rigid_body_default_get();
    if(!yyjson_is_obj(value) || !editor_json_uint(value, "id", &body->id) || body->id == 0 ||
            !editor_json_name(value, body->name) || !editor_json_position_read(
                yyjson_obj_get(value, "position"), &body->position) ||
            !editor_json_real(value, "rotation", &body->rotation) ||
            !editor_json_real(value, "mass", &body->mass_value) ||
            !editor_json_real(value, "friction", &body->friction) ||
            !editor_json_real(value, "restitution", &body->restitution) ||
            !editor_json_bool(value, "static", &body->static_body) ||
            !editor_json_bool(value, "rotation_locked", &body->rotation_locked) ||
            !editor_json_bool(value, "gravity_enabled", &body->gravity_enabled) ||
            !editor_json_bool(value, "visible", &body->visible) || !yyjson_is_arr(hitboxes)) {
        return false;
    }
    if(collision_enabled != NULL && (!editor_json_bool(value, "collision_enabled",
                &body->collision_enabled) ||
            !editor_json_uint64(value, "collision_category", &body->collision_category) ||
            !editor_json_uint64(value, "collision_with", &body->collision_with))) return false;
    if(particle != NULL && !editor_json_bool(value, "particle", &body->particle)) return false;
    if(particle_auto_fit != NULL && !editor_json_bool(
            value, "particle_auto_fit", &body->particle_auto_fit)) return false;
    if((particle_radius != NULL && !editor_json_real(
                value, "particle_radius", &body->particle_radius)) ||
            (particle_origin != NULL && !editor_json_position_read(
                particle_origin, &body->particle_origin)) ||
            (particle_ring_color != NULL && !editor_json_uint(
                value, "particle_ring_color", &body->particle_ring_color)) ||
            (particle_fill_color != NULL && !editor_json_uint(
                value, "particle_fill_color", &body->particle_fill_color))) return false;
    body->particle_radius = fmaxf(0.0f, body->particle_radius);
    if((border_color != NULL && !editor_json_uint(value, "border_color", &body->border_color)) ||
            (surface_color != NULL && !editor_json_uint(
                value, "surface_color", &body->surface_color))) return false;
    if(!body->collision_enabled) body->particle = false;
    if(collision_enabled == NULL &&
            (collision_category != NULL || collision_with != NULL)) return false;
    editor_project_property_name_format(body->name, sizeof(body->name), body->name);
    count = (uint32_t)yyjson_arr_size(hitboxes);
    if(!EDITOR_ARRAY_RESERVE(body->hitboxes, body->hitbox_capacity, count))
        return false;
    if(count > 0) memset(body->hitboxes, 0,
        count * sizeof(*body->hitboxes));
    body->hitbox_count = count;
    for(size_t i = 0; i < count; i += 1)
        if(!editor_json_hitbox_read(yyjson_arr_get(hitboxes, i), &body->hitboxes[i], project))
            return false;
    if(active_hitbox_index != NULL) {
        uint32_t active;
        if(!editor_json_uint(value, "active_hitbox_index", &active) ||
                (count == 0 ? active != 0 : active >= count)) return false;
        body->active_hitbox_index = active;
    }
    if(bindings != NULL) {
        if(!yyjson_is_arr(bindings) ||
                !EDITOR_ARRAY_RESERVE(body->hitbox_animation_bindings,
                    body->hitbox_animation_binding_capacity,
                    yyjson_arr_size(bindings))) return false;
        for(size_t i = 0; i < yyjson_arr_size(bindings); i += 1) {
            yyjson_val *item = yyjson_arr_get(bindings, i);
            EditorHitboxAnimationBinding binding = {0};
            if(!yyjson_is_obj(item) ||
                    !editor_json_uint(item, "animation", &binding.animation) ||
                    !editor_json_uint(item, "frame", &binding.frame) ||
                    !editor_json_uint(item, "hitbox", &binding.hitbox) ||
                    binding.animation == 0 || binding.frame == 0 ||
                    editor_project_hitbox_get(body, binding.hitbox) == NULL)
                return false;
            body->hitbox_animation_bindings[
                body->hitbox_animation_binding_count++] = binding;
        }
    }
    if(project->next_rigid_body_id <= body->id) project->next_rigid_body_id = body->id + 1;
    return true;
}

static bool editor_json_anchor_read(yyjson_val *value, EditorAnchor *anchor,
    EditorProject *project) {
    if(!yyjson_is_obj(value) || !editor_json_uint(value, "id", &anchor->id) ||
            anchor->id == 0 || !editor_json_name(value, anchor->name) ||
            !editor_json_position_read(yyjson_obj_get(value, "position"), &anchor->position) ||
            !editor_json_real(value, "rotation", &anchor->rotation) ||
            !editor_json_uint(value, "rigid_body", &anchor->rigid_body) ||
            !editor_json_bool(value, "position_follows_body", &anchor->position_follows_body) ||
            !editor_json_bool(value, "rotation_follows_body", &anchor->rotation_follows_body) ||
            !editor_json_bool(value, "visible", &anchor->visible)) return false;
    editor_project_property_name_format(anchor->name, sizeof(anchor->name), anchor->name);
    if(project->next_anchor_id <= anchor->id) project->next_anchor_id = anchor->id + 1;
    return true;
}

static bool editor_json_joint_read(yyjson_val *value, EditorJoint *joint,
    EditorProject *project) {
    uint32_t kind;
    if(!yyjson_is_obj(value) || !editor_json_uint(value, "id", &joint->id) ||
            joint->id == 0 || !editor_json_name(value, joint->name) ||
            !editor_json_uint(value, "kind", &kind) || kind > EDITOR_JOINT_SPRING ||
            !editor_json_uint(value, "anchor_a", &joint->anchor_a) ||
            !editor_json_uint(value, "anchor_b", &joint->anchor_b) ||
            !editor_json_real(value, "rest_length", &joint->rest_length) ||
            !editor_json_real(value, "stiffness", &joint->stiffness) ||
            !editor_json_real(value, "damping", &joint->damping) ||
            !editor_json_real(value, "rest_angle", &joint->rest_angle) ||
            !editor_json_real(value, "visual_size", &joint->visual_size) ||
            !editor_json_bool(value, "visible", &joint->visible)) return false;
    editor_project_property_name_format(joint->name, sizeof(joint->name), joint->name);
    joint->kind = (EditorJointKind)kind;
    if(project->next_joint_id <= joint->id) project->next_joint_id = joint->id + 1;
    return true;
}

static bool editor_json_soft_body_read(yyjson_val *value, EditorSoftBody *body,
    EditorProject *project) {
    yyjson_val *nodes = yyjson_obj_get(value, "nodes");
    yyjson_val *beams = yyjson_obj_get(value, "beams");
    yyjson_val *areas = yyjson_obj_get(value, "areas");
    yyjson_val *hierarchy = yyjson_obj_get(value, "hierarchy");
    yyjson_val *rotation = yyjson_obj_get(value, "rotation");
    *body = (EditorSoftBody){
        .node_color = UINT32_C(0xffaa46ff),
        .beam_color = UINT32_C(0xebf0f5ff),
        .area_color = UINT32_C(0x505a78ff)
    };
    if(!yyjson_is_obj(value) || !editor_json_uint(value, "id", &body->id) || body->id == 0 ||
            !editor_json_name(value, body->name) || !editor_json_position_read(
                yyjson_obj_get(value, "position"), &body->position) ||
            !editor_json_bool(value, "visible", &body->visible) || !yyjson_is_arr(nodes) ||
            !yyjson_is_arr(beams)) return false;
    if(areas != NULL && !yyjson_is_arr(areas)) return false;
    if(hierarchy != NULL && !yyjson_is_arr(hierarchy)) return false;
    if(yyjson_obj_get(value, "node_color") != NULL &&
            (!editor_json_uint(value, "node_color", &body->node_color) ||
            !editor_json_uint(value, "beam_color", &body->beam_color) ||
            !editor_json_uint(value, "area_color", &body->area_color))) return false;
    if(rotation != NULL && !editor_json_real(value, "rotation", &body->rotation)) {
        return false;
    }
    editor_project_property_name_format(body->name, sizeof(body->name), body->name);
    body->node_count = yyjson_arr_size(nodes);
    body->beam_count = yyjson_arr_size(beams);
    if(!EDITOR_ARRAY_RESERVE(body->nodes, body->node_capacity,
            body->node_count) || !EDITOR_ARRAY_RESERVE(body->beams,
                body->beam_capacity, body->beam_count)) return false;
    for(size_t i = 0; i < body->node_count; i += 1) {
        yyjson_val *item = yyjson_arr_get(nodes, i);
        EditorSoftNode *node = &body->nodes[i];
        yyjson_val *collision_enabled = yyjson_obj_get(item, "collision_enabled");
        yyjson_val *friction = yyjson_obj_get(item, "friction");
        yyjson_val *restitution = yyjson_obj_get(item, "restitution");
        yyjson_val *radius = yyjson_obj_get(item, "radius");
        *node = (EditorSoftNode){
            .radius = 4.0f,
            .friction = 0.0f,
            .restitution = 0.25f,
            .collision_enabled = true,
            .collision_category = UINT64_C(1),
            .collision_with = UINT64_C(1),
            .color = body->node_color
        };
        if(!yyjson_is_obj(item) || !editor_json_uint(item, "id", &node->id) || node->id == 0 ||
                !editor_json_name(item, node->name) || !editor_json_position_read(
                    yyjson_obj_get(item, "position"), &node->position) ||
                !editor_json_real(item, "mass", &node->node_mass) ||
                !editor_json_bool(item, "gravity_enabled", &node->gravity_enabled) ||
                !editor_json_bool(item, "visible", &node->visible)) return false;
        if((friction != NULL && !editor_json_real(item, "friction", &node->friction)) ||
                (restitution != NULL && !editor_json_real(
                    item, "restitution", &node->restitution))) return false;
        if(radius != NULL && !editor_json_real(item, "radius", &node->radius)) return false;
        if(yyjson_obj_get(item, "color") != NULL &&
                (!editor_json_uint(item, "color", &node->color) ||
                !editor_json_bool(item, "color_overridden", &node->color_overridden))) {
            return false;
        }
        if(node->radius <= 0.0f) return false;
        if(collision_enabled != NULL &&
                (!editor_json_bool(item, "collision_enabled", &node->collision_enabled) ||
                !editor_json_uint64(item, "collision_category", &node->collision_category) ||
                !editor_json_uint64(item, "collision_with", &node->collision_with))) return false;
        editor_project_property_name_format(node->name, sizeof(node->name), node->name);
        if(project->next_soft_node_id <= node->id) project->next_soft_node_id = node->id + 1;
    }
    for(size_t i = 0; i < body->beam_count; i += 1) {
        yyjson_val *item = yyjson_arr_get(beams, i);
        EditorSoftBeam *beam = &body->beams[i];
        yyjson_val *damping = yyjson_obj_get(item, "damping");
        *beam = (EditorSoftBeam){.damping = 0.0f, .color = body->beam_color};
        if(!yyjson_is_obj(item) || !editor_json_uint(item, "id", &beam->id) || beam->id == 0 ||
                !editor_json_name(item, beam->name) ||
                !editor_json_uint(item, "node_a", &beam->node_a) ||
                !editor_json_uint(item, "node_b", &beam->node_b) ||
                !editor_json_real(item, "stiffness", &beam->stiffness) ||
                !editor_json_bool(item, "visible", &beam->visible)) return false;
        if(damping != NULL && !editor_json_real(item, "damping", &beam->damping)) {
            return false;
        }
        if(yyjson_obj_get(item, "color") != NULL &&
                (!editor_json_uint(item, "color", &beam->color) ||
                !editor_json_bool(item, "color_overridden", &beam->color_overridden))) {
            return false;
        }
        editor_project_property_name_format(beam->name, sizeof(beam->name), beam->name);
        if(project->next_soft_beam_id <= beam->id) project->next_soft_beam_id = beam->id + 1;
    }
    if(areas != NULL) {
        body->area_count = yyjson_arr_size(areas);
        if(!EDITOR_ARRAY_RESERVE(body->areas, body->area_capacity,
                body->area_count)) return false;
        if(body->area_count > 0) memset(body->areas, 0,
            body->area_count * sizeof(*body->areas));
        for(size_t i = 0; i < body->area_count; i += 1) {
            yyjson_val *item = yyjson_arr_get(areas, i);
            yyjson_val *area_nodes = yyjson_obj_get(item, "nodes");
            EditorSoftArea *area = &body->areas[i];
            if(!yyjson_is_obj(item) || !editor_json_uint(item, "id", &area->id) ||
                    area->id == 0 || !editor_json_name(item, area->name) ||
                    !editor_json_uint(item, "color", &area->color) ||
                    !editor_json_bool(item, "color_overridden", &area->color_overridden) ||
                    !editor_json_bool(item, "visible", &area->visible)) return false;
            if(area_nodes != NULL) {
                if(!yyjson_is_arr(area_nodes) || yyjson_arr_size(area_nodes) < 3)
                    return false;
                area->node_count = yyjson_arr_size(area_nodes);
                if(!EDITOR_ARRAY_RESERVE(area->nodes, area->node_capacity,
                        area->node_count)) return false;
                for(size_t node_index = 0; node_index < area->node_count; node_index += 1) {
                    yyjson_val *node = yyjson_arr_get(area_nodes, node_index);
                    if(!yyjson_is_uint(node) || yyjson_get_uint(node) > UINT32_MAX) return false;
                    area->nodes[node_index] = (EditorSoftNodeId)yyjson_get_uint(node);
                }
            } else {
                area->node_count = 3;
                if(!EDITOR_ARRAY_RESERVE(area->nodes, area->node_capacity, 3))
                    return false;
                if(!editor_json_uint(item, "node_a", &area->nodes[0]) ||
                        !editor_json_uint(item, "node_b", &area->nodes[1]) ||
                        !editor_json_uint(item, "node_c", &area->nodes[2])) return false;
            }
            editor_project_property_name_format(area->name, sizeof(area->name), area->name);
            if(project->next_soft_area_id <= area->id) {
                project->next_soft_area_id = area->id + 1;
            }
        }
    }
    editor_project_soft_areas_sync(project, body);
    if(hierarchy != NULL) {
        body->hierarchy_count = yyjson_arr_size(hierarchy);
        if(!EDITOR_ARRAY_RESERVE(body->hierarchy, body->hierarchy_capacity,
                body->hierarchy_count)) return false;
        for(size_t i = 0; i < body->hierarchy_count; i += 1) {
            yyjson_val *item = yyjson_arr_get(hierarchy, i);
            uint32_t kind;
            if(!yyjson_is_obj(item) || !editor_json_uint(item, "kind", &kind) ||
                    kind > EDITOR_SOFT_HIERARCHY_AREA ||
                    !editor_json_uint(item, "id", &body->hierarchy[i].id) ||
                    body->hierarchy[i].id == 0) return false;
            body->hierarchy[i].kind = (EditorSoftHierarchyItemKind)kind;
        }
    }
    {
        size_t serialized_count = body->hierarchy_count;
        size_t expected_count = body->node_count + body->beam_count + body->area_count;
        editor_project_soft_body_hierarchy_sync(body);
        if(hierarchy != NULL && (body->hierarchy_count != serialized_count ||
                body->hierarchy_count != expected_count)) return false;
    }
    if(project->next_soft_body_id <= body->id) project->next_soft_body_id = body->id + 1;
    return true;
}

static bool editor_json_animated_sprite_read(yyjson_val *value,
        EditorAnimatedSprite *sprite, EditorProject *project) {
    yyjson_val *frames;
    yyjson_val *time;
    uint32_t direction;
    uint64_t ticks;
    if(!yyjson_is_obj(value) || sprite == NULL || project == NULL ||
            !editor_json_uint(value, "id", &sprite->id) || sprite->id == 0 ||
            !editor_json_name(value, sprite->name) ||
            !editor_json_uint(value, "rigid_body", &sprite->rigid_body) ||
            !editor_json_position_read(yyjson_obj_get(value, "editor_position"),
                &sprite->editor_position) ||
            !editor_json_optional_real(value, "editor_rotation",
                &sprite->editor_rotation, 0.0f) ||
            !editor_json_uint64(value, "ticks_per_frame", &ticks) ||
            !editor_json_uint(value, "starting_frame", &sprite->starting_frame) ||
            !editor_json_real(value, "scale_x", &sprite->scale.x) ||
            !editor_json_real(value, "scale_y", &sprite->scale.y) ||
            !editor_json_uint(value, "direction", &direction) ||
            direction > DIRECTION_RIGHT ||
            !editor_json_bool(value, "follow_body_rotation",
                &sprite->follow_body_rotation) ||
            !editor_json_bool(value, "visible", &sprite->visible) ||
            !editor_json_bool(value, "playing", &sprite->playing)) return false;
    time = yyjson_obj_get(value, "time_per_frame");
    frames = yyjson_obj_get(value, "frames");
    if(!yyjson_is_num(time) || yyjson_get_real(time) < 0.0 ||
            !yyjson_is_arr(frames) || yyjson_arr_size(frames) > MAX_ANIMATIONS_FRAMES ||
            sprite->scale.x <= 0.0f || sprite->scale.y <= 0.0f) return false;
    editor_project_property_name_format(sprite->name, sizeof(sprite->name),
        sprite->name);
    sprite->ticks_per_frame = (Tick)ticks;
    sprite->time_per_frame = (Time)yyjson_get_real(time);
    sprite->direction = (Direction)direction;
    sprite->frame_count = yyjson_arr_size(frames);
    if(!EDITOR_ARRAY_RESERVE(sprite->frames, sprite->frame_capacity,
            sprite->frame_count)) return false;
    for(size_t i = 0; i < sprite->frame_count; i += 1) {
        yyjson_val *frame = yyjson_arr_get(frames, i);
        uint32_t id;
        yyjson_val *path = yyjson_obj_get(frame, "path");
        if(!yyjson_is_obj(frame) || !editor_json_uint(frame, "id", &id) || id == 0 ||
                !editor_json_name(frame, sprite->frames[i].name) ||
                !yyjson_is_str(path) || yyjson_get_len(path) == 0 ||
                yyjson_get_len(path) >= sizeof(sprite->frames[i].path) ||
                !editor_json_real(frame, "width", &sprite->frames[i].size.x) ||
                !editor_json_real(frame, "height", &sprite->frames[i].size.y) ||
                sprite->frames[i].name[0] == '\0' ||
                sprite->frames[i].size.x <= 0.0f || sprite->frames[i].size.y <= 0.0f)
            return false;
        memcpy(sprite->frames[i].path, yyjson_get_str(path), yyjson_get_len(path) + 1);
        sprite->frames[i].id = (EditorSpriteId)id;
        editor_project_property_name_format(sprite->frames[i].name,
            sizeof(sprite->frames[i].name), sprite->frames[i].name);
        if(project->next_sprite_id <= sprite->frames[i].id)
            project->next_sprite_id = sprite->frames[i].id + 1;
    }
    if(project->next_animated_sprite_id <= sprite->id)
        project->next_animated_sprite_id = sprite->id + 1;
    return true;
}

static bool editor_json_references_valid(EditorProject *project) {
    uint64_t valid_masks = project->collision_mask_count == 64 ? UINT64_MAX :
        (UINT64_C(1) << project->collision_mask_count) - 1;
    for(size_t i = 0; i < project->object_count; i += 1) {
        EditorObject *object = &project->objects[i];
        for(size_t j = 0; j < object->rigid_body_count; j += 1) {
            EditorRigidBody *body = &object->rigid_bodies[j];
            if((body->collision_category & ~valid_masks) != 0 ||
                    (body->collision_with & ~valid_masks) != 0) return false;
        }
        for(size_t j = 0; j < object->anchor_count; j += 1)
            if(object->anchors[j].rigid_body != 0 && editor_project_rigid_body_get(
                    object, object->anchors[j].rigid_body) == NULL) return false;
        for(size_t j = 0; j < object->joint_count; j += 1) {
            EditorJoint *joint = &object->joint_items[j];
            if((joint->anchor_a != 0 && editor_project_anchor_get(object, joint->anchor_a) == NULL) ||
                    (joint->anchor_b != 0 && editor_project_anchor_get(
                        object, joint->anchor_b) == NULL)) return false;
        }
        for(size_t j = 0; j < object->soft_body_count; j += 1) {
            EditorSoftBody *body = &object->soft_body_items[j];
            for(size_t k = 0; k < body->node_count; k += 1) {
                EditorSoftNode *node = &body->nodes[k];
                if((node->collision_category & ~valid_masks) != 0 ||
                        (node->collision_with & ~valid_masks) != 0) return false;
            }
            for(size_t k = 0; k < body->beam_count; k += 1) {
                bool found_a = body->beams[k].node_a == 0;
                bool found_b = body->beams[k].node_b == 0;
                for(size_t n = 0; n < body->node_count; n += 1) {
                    found_a = found_a || body->nodes[n].id == body->beams[k].node_a;
                    found_b = found_b || body->nodes[n].id == body->beams[k].node_b;
                }
                if(!found_a || !found_b) return false;
            }
        }
        for(size_t j = 0; j < object->sprite_count; j += 1) {
            EditorSprite *sprite = &object->sprites[j];
            if(sprite->rigid_body != 0 &&
                    editor_project_rigid_body_get(object, sprite->rigid_body) == NULL)
                return false;
            for(size_t other = 0; other < j; other += 1)
                if(sprite->rigid_body != 0 &&
                        object->sprites[other].rigid_body == sprite->rigid_body)
                    return false;
        }
        for(size_t j = 0; j < object->animated_sprite_count; j += 1) {
            EditorAnimatedSprite *sprite = &object->animated_sprite_items[j];
            if(sprite->rigid_body != 0 &&
                    editor_project_rigid_body_get(object, sprite->rigid_body) == NULL)
                return false;
            if(sprite->frame_count > MAX_ANIMATIONS_FRAMES ||
                    (sprite->frame_count > 0 &&
                        sprite->starting_frame >= sprite->frame_count))
                return false;
            for(size_t other = 0; other < j; other += 1)
                if(sprite->rigid_body != 0 &&
                        object->animated_sprite_items[other].rigid_body ==
                            sprite->rigid_body) return false;
        }
        for(size_t j = 0; j < object->rigid_body_count; j += 1) {
            EditorRigidBody *body = &object->rigid_bodies[j];
            for(size_t k = 0; k < body->hitbox_animation_binding_count; k += 1) {
                EditorHitboxAnimationBinding *binding =
                    &body->hitbox_animation_bindings[k];
                EditorAnimatedSprite *animation =
                    editor_project_animated_sprite_get(object,
                        binding->animation);
                bool frame_found = false;
                if(animation == NULL || animation->rigid_body != body->id ||
                        editor_project_hitbox_get(body, binding->hitbox) == NULL)
                    return false;
                for(size_t frame = 0; frame < animation->frame_count; frame += 1)
                    frame_found = frame_found ||
                        animation->frames[frame].id == binding->frame;
                if(!frame_found) return false;
                for(size_t other = 0; other < k; other += 1)
                    if(body->hitbox_animation_bindings[other].animation ==
                            binding->animation &&
                            body->hitbox_animation_bindings[other].frame ==
                                binding->frame) return false;
            }
        }
    }
    return project->selected == 0 || editor_project_selected_get(project) != NULL;
}

EditorResult editor_project_load(EditorProject *project, const char *path) {
    static EditorProject loaded;
    yyjson_doc *document;
    yyjson_read_err read_error = {0};
    yyjson_val *root;
    yyjson_val *objects;
    yyjson_val *collision_masks;
    uint32_t version;
    EditorResult result = editor_result_error(EDITOR_ERROR_SCHEMA_INVALID,
        "Project editor state does not match the current schema: %s",
        path == NULL ? "(null)" : path);
    if(project == NULL || path == NULL || path[0] == '\0')
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Project editor-state load received an invalid argument");
    document = yyjson_read_file(path, 0, NULL, &read_error);
    if(document == NULL) {
        return editor_result_error(
            read_error.code == YYJSON_READ_ERROR_FILE_OPEN ||
                    read_error.code == YYJSON_READ_ERROR_FILE_READ ?
                EDITOR_ERROR_FILE_IO : EDITOR_ERROR_JSON_PARSE,
            "Could not parse project editor state '%s': %s at byte %zu",
            path, read_error.msg == NULL ? "unknown JSON error" : read_error.msg,
            read_error.pos);
    }
    root = yyjson_doc_get_root(document);
    objects = yyjson_obj_get(root, "objects");
    collision_masks = yyjson_obj_get(root, "collision_masks");
    editor_project_destroy(&loaded);
    editor_project_init(&loaded);
    if(!yyjson_is_obj(root)) goto done;
    if(!editor_json_uint(root, "format_version", &version)) {
        result = editor_result_error(EDITOR_ERROR_SCHEMA_VERSION,
            "Project editor state '%s' is missing integer format_version; expected %u",
            path, EDITOR_PROJECT_FORMAT_VERSION);
        goto done;
    }
    if(version != EDITOR_PROJECT_FORMAT_VERSION) {
        result = editor_result_error(EDITOR_ERROR_SCHEMA_VERSION,
            "Project editor state '%s' uses format_version %u; this editor requires %u",
            path, version, EDITOR_PROJECT_FORMAT_VERSION);
        goto done;
    }
    {
        yyjson_val *camera_offset = yyjson_obj_get(root, "viewport_camera_offset");
        yyjson_val *camera_zoom = yyjson_obj_get(root, "viewport_camera_zoom");
        yyjson_val *local_view = yyjson_obj_get(root, "viewport_local_view");
        Position offset;
        if((camera_offset != NULL &&
                !editor_json_position_read(camera_offset, &offset)) ||
                (camera_zoom != NULL && (!yyjson_is_num(camera_zoom) ||
                    yyjson_get_real(camera_zoom) < 0.1 ||
                    yyjson_get_real(camera_zoom) > 8.0)) ||
                (local_view != NULL && !yyjson_is_bool(local_view))) goto done;
        if(camera_offset != NULL)
            loaded.viewport_camera_offset = (Vec2D){offset.x, offset.y};
        if(camera_zoom != NULL)
            loaded.viewport_camera_zoom = (float)yyjson_get_real(camera_zoom);
        if(local_view != NULL)
            loaded.viewport_local_view = yyjson_get_bool(local_view);
    }
    {
        yyjson_val *navigation = yyjson_obj_get(root, "navigation");
        if(navigation != NULL && (!yyjson_is_obj(navigation) ||
                !editor_json_uint(navigation, "mode", &loaded.navigation.mode) ||
                !editor_json_uint(navigation, "selection", &loaded.navigation.selection) ||
                !editor_json_uint(navigation, "object", &loaded.navigation.object) ||
                !editor_json_uint(navigation, "line", &loaded.navigation.selected_line) ||
                !editor_json_uint(navigation, "vertex", &loaded.navigation.selected_vertex) ||
                !editor_json_uint(navigation, "rigid_body", &loaded.navigation.rigid_body) ||
                !editor_json_uint(navigation, "hitbox", &loaded.navigation.hitbox) ||
                !editor_json_uint(navigation, "joint", &loaded.navigation.joint) ||
                !editor_json_uint(navigation, "anchor", &loaded.navigation.anchor) ||
                !editor_json_uint(navigation, "soft_body", &loaded.navigation.soft_body) ||
                !editor_json_uint(navigation, "soft_node", &loaded.navigation.soft_node) ||
                !editor_json_uint(navigation, "soft_beam", &loaded.navigation.soft_beam) ||
                !editor_json_uint(navigation, "origin_kind", &loaded.navigation.origin_kind) ||
                loaded.navigation.mode > EDITOR_NAVIGATION_MODE_MAX ||
                loaded.navigation.selection > EDITOR_NAVIGATION_SELECTION_MAX ||
                loaded.navigation.origin_kind > 2)) goto done;
        if(navigation != NULL) {
            yyjson_val *sprite = yyjson_obj_get(navigation, "sprite");
            yyjson_val *animated = yyjson_obj_get(navigation, "animated_sprite");
            yyjson_val *frame = yyjson_obj_get(navigation, "animation_frame");
            if((sprite != NULL && (!yyjson_is_uint(sprite) ||
                        yyjson_get_uint(sprite) > UINT32_MAX)) ||
                    (animated != NULL && (!yyjson_is_uint(animated) ||
                        yyjson_get_uint(animated) > UINT32_MAX)) ||
                    (frame != NULL && (!yyjson_is_uint(frame) ||
                        yyjson_get_uint(frame) > UINT32_MAX))) goto done;
            if(sprite != NULL)
                loaded.navigation.sprite = (uint32_t)yyjson_get_uint(sprite);
            if(animated != NULL)
                loaded.navigation.animated_sprite =
                    (uint32_t)yyjson_get_uint(animated);
            if(frame != NULL)
                loaded.navigation.animation_frame =
                    (uint32_t)yyjson_get_uint(frame);
        }
    }
    if(!editor_json_uint(root, "selected", &loaded.selected) || !yyjson_is_arr(objects) ||
            !editor_json_uint(root, "next_object_id", &loaded.next_id) ||
            !editor_json_uint(root, "next_vertex_id", &loaded.next_vertex_id) ||
            !editor_json_uint(root, "next_rigid_body_id", &loaded.next_rigid_body_id) ||
            !editor_json_uint(root, "next_hitbox_id", &loaded.next_hitbox_id) ||
            !editor_json_uint(root, "next_joint_id", &loaded.next_joint_id) ||
            !editor_json_uint(root, "next_anchor_id", &loaded.next_anchor_id) ||
            !editor_json_uint(root, "next_soft_body_id", &loaded.next_soft_body_id) ||
            !editor_json_uint(root, "next_soft_node_id", &loaded.next_soft_node_id) ||
            !editor_json_uint(root, "next_soft_beam_id", &loaded.next_soft_beam_id) ||
            !editor_json_uint(root, "next_soft_area_id", &loaded.next_soft_area_id) ||
            loaded.next_id == 0 || loaded.next_vertex_id == 0 ||
            loaded.next_rigid_body_id == 0 || loaded.next_hitbox_id == 0 ||
            loaded.next_joint_id == 0 || loaded.next_anchor_id == 0 ||
            loaded.next_soft_body_id == 0 || loaded.next_soft_node_id == 0 ||
            loaded.next_soft_beam_id == 0 || loaded.next_soft_area_id == 0 ||
            !yyjson_is_arr(collision_masks) || yyjson_arr_size(collision_masks) == 0 ||
            yyjson_arr_size(collision_masks) > EDITOR_COLLISION_MASK_MAX)
        goto done;
    {
        yyjson_val *next_sprite = yyjson_obj_get(root, "next_sprite_id");
        yyjson_val *next_animated = yyjson_obj_get(root, "next_animated_sprite_id");
        if((next_sprite != NULL && !editor_json_uint(root, "next_sprite_id",
                    &loaded.next_sprite_id)) ||
                (next_animated != NULL && !editor_json_uint(root,
                    "next_animated_sprite_id", &loaded.next_animated_sprite_id)) ||
                loaded.next_sprite_id == 0 || loaded.next_animated_sprite_id == 0) goto done;
    }
    loaded.collision_mask_count = yyjson_arr_size(collision_masks);
    if(!EDITOR_ARRAY_RESERVE(loaded.collision_masks,
            loaded.collision_mask_capacity, loaded.collision_mask_count)) goto done;
    for(size_t i = 0; i < loaded.collision_mask_count; i += 1) {
        yyjson_val *name = yyjson_arr_get(collision_masks, i);
        if(!yyjson_is_str(name) || yyjson_get_len(name) == 0 ||
                yyjson_get_len(name) >= EDITOR_OBJECT_NAME_MAX) goto done;
        memcpy(loaded.collision_masks[i].name, yyjson_get_str(name),
            yyjson_get_len(name) + 1);
        editor_project_property_name_format(loaded.collision_masks[i].name,
            sizeof(loaded.collision_masks[i].name), loaded.collision_masks[i].name);
    }
    loaded.object_count = yyjson_arr_size(objects);
    if(!EDITOR_ARRAY_RESERVE(loaded.objects, loaded.object_capacity,
            loaded.object_count)) goto done;
    if(loaded.object_count > 0) memset(loaded.objects, 0,
        loaded.object_count * sizeof(*loaded.objects));
    for(size_t i = 0; i < loaded.object_count; i += 1) {
        yyjson_val *value = yyjson_arr_get(objects, i);
        EditorObject *object = &loaded.objects[i];
        yyjson_val *bodies = yyjson_obj_get(value, "rigid_bodies");
        yyjson_val *anchors = yyjson_obj_get(value, "anchors");
        yyjson_val *joint_values = yyjson_obj_get(value, "joints");
        yyjson_val *soft_body_values = yyjson_obj_get(value, "soft_bodies");
        yyjson_val *sprites = yyjson_obj_get(value, "sprites");
        yyjson_val *animated_sprite_values = yyjson_obj_get(value,
            "animated_sprites");
        yyjson_val *hierarchy = yyjson_obj_get(value, "hierarchy");
        if(!yyjson_is_obj(value) || !editor_json_uint(value, "id", &object->id) ||
                object->id == 0 || !editor_json_name(value, object->name) ||
                !editor_json_position_read(yyjson_obj_get(value, "position"), &object->position) ||
                !editor_json_bool(value, "visible", &object->visible) ||
                !yyjson_is_arr(bodies) || !yyjson_is_arr(anchors) ||
                !yyjson_is_arr(joint_values) || !yyjson_is_arr(soft_body_values) ||
                (sprites != NULL && !yyjson_is_arr(sprites)) ||
                (animated_sprite_values != NULL &&
                    !yyjson_is_arr(animated_sprite_values)) ||
                (hierarchy != NULL && !yyjson_is_arr(hierarchy))) goto done;
        editor_project_object_name_format(object->name, sizeof(object->name), object->name);
        object->rigid_body_count = yyjson_arr_size(bodies);
        object->anchor_count = yyjson_arr_size(anchors);
        object->joint_count = yyjson_arr_size(joint_values);
        object->soft_body_count = yyjson_arr_size(soft_body_values);
        object->sprite_count = sprites == NULL ? 0 : yyjson_arr_size(sprites);
        object->animated_sprite_count = animated_sprite_values == NULL ? 0 :
            yyjson_arr_size(animated_sprite_values);
        if(!EDITOR_ARRAY_RESERVE(object->rigid_bodies,
                object->rigid_body_capacity, object->rigid_body_count) ||
                !EDITOR_ARRAY_RESERVE(object->anchors, object->anchor_capacity,
                    object->anchor_count) ||
                !EDITOR_ARRAY_RESERVE(object->joint_items, object->joint_capacity,
                    object->joint_count) ||
                !EDITOR_ARRAY_RESERVE(object->soft_body_items,
                    object->soft_body_capacity, object->soft_body_count) ||
                !EDITOR_ARRAY_RESERVE(object->sprites, object->sprite_capacity,
                    object->sprite_count) ||
                !EDITOR_ARRAY_RESERVE(object->animated_sprite_items,
                    object->animated_sprite_capacity,
                    object->animated_sprite_count)) goto done;
        if(object->rigid_body_count > 0) memset(object->rigid_bodies, 0,
            object->rigid_body_count * sizeof(*object->rigid_bodies));
        if(object->anchor_count > 0) memset(object->anchors, 0,
            object->anchor_count * sizeof(*object->anchors));
        if(object->joint_count > 0) memset(object->joint_items, 0,
            object->joint_count * sizeof(*object->joint_items));
        if(object->soft_body_count > 0) memset(object->soft_body_items, 0,
            object->soft_body_count * sizeof(*object->soft_body_items));
        for(size_t j = 0; j < object->sprite_count; j += 1) {
            yyjson_val *sprite_value = yyjson_arr_get(sprites, j);
            yyjson_val *path_value = yyjson_obj_get(sprite_value, "path");
            EditorSprite *sprite = &object->sprites[j];
            if(!yyjson_is_obj(sprite_value) ||
                    !editor_json_uint(sprite_value, "id", &sprite->id) ||
                    sprite->id == 0 || !editor_json_name(sprite_value, sprite->name) ||
                    !yyjson_is_str(path_value) || yyjson_get_len(path_value) == 0 ||
                    yyjson_get_len(path_value) >= EDITOR_ASSET_PATH_MAX ||
                    !editor_json_position_read(yyjson_obj_get(sprite_value, "position"),
                        &sprite->position) ||
                    !editor_json_optional_real(sprite_value, "rotation",
                        &sprite->rotation, 0.0f) ||
                    !editor_json_uint(sprite_value, "rigid_body", &sprite->rigid_body) ||
                    !editor_json_real(sprite_value, "width", &sprite->size.x) ||
                    !editor_json_real(sprite_value, "height", &sprite->size.y) ||
                    !editor_json_bool(sprite_value, "follow_body_rotation",
                        &sprite->follow_body_rotation) ||
                    !editor_json_bool(sprite_value, "visible", &sprite->visible) ||
                    sprite->size.x <= 0.0f || sprite->size.y <= 0.0f) goto done;
            editor_project_property_name_format(sprite->name, sizeof(sprite->name),
                sprite->name);
            memcpy(sprite->path, yyjson_get_str(path_value),
                yyjson_get_len(path_value) + 1);
            if(loaded.next_sprite_id <= sprite->id)
                loaded.next_sprite_id = sprite->id + 1;
        }
        if(object->animated_sprite_count > 0) memset(object->animated_sprite_items, 0,
            object->animated_sprite_count * sizeof(*object->animated_sprite_items));
        for(size_t j = 0; j < object->rigid_body_count; j += 1)
            if(!editor_json_body_read(yyjson_arr_get(bodies, j),
                    &object->rigid_bodies[j], &loaded)) goto done;
        for(size_t j = 0; j < object->anchor_count; j += 1)
            if(!editor_json_anchor_read(yyjson_arr_get(anchors, j),
                    &object->anchors[j], &loaded)) goto done;
        for(size_t j = 0; j < object->joint_count; j += 1)
            if(!editor_json_joint_read(yyjson_arr_get(joint_values, j),
                    &object->joint_items[j], &loaded)) goto done;
        for(size_t j = 0; j < object->soft_body_count; j += 1)
            if(!editor_json_soft_body_read(yyjson_arr_get(soft_body_values, j),
                    &object->soft_body_items[j], &loaded)) goto done;
        for(size_t j = 0; j < object->animated_sprite_count; j += 1)
            if(!editor_json_animated_sprite_read(
                    yyjson_arr_get(animated_sprite_values, j),
                    &object->animated_sprite_items[j], &loaded)) goto done;
        if(hierarchy != NULL) {
            object->hierarchy_count = yyjson_arr_size(hierarchy);
            if(!EDITOR_ARRAY_RESERVE(object->hierarchy,
                    object->hierarchy_capacity, object->hierarchy_count)) goto done;
            for(size_t j = 0; j < object->hierarchy_count; j += 1) {
                yyjson_val *item = yyjson_arr_get(hierarchy, j);
                uint32_t kind;
                if(!yyjson_is_obj(item) || !editor_json_uint(item, "kind", &kind) ||
                        kind > EDITOR_HIERARCHY_ANIMATED_SPRITE ||
                        !editor_json_uint(item, "id", &object->hierarchy[j].id) ||
                        object->hierarchy[j].id == 0) goto done;
                object->hierarchy[j].kind = (EditorHierarchyItemKind)kind;
            }
        }
        {
            size_t serialized_count = object->hierarchy_count;
            size_t expected_count = object->rigid_body_count + object->joint_count +
                object->soft_body_count + object->sprite_count +
                object->animated_sprite_count;
            editor_project_object_hierarchy_sync(object);
            if(hierarchy != NULL && (object->hierarchy_count != serialized_count ||
                    object->hierarchy_count != expected_count)) goto done;
        }
        if(loaded.next_id <= object->id) loaded.next_id = object->id + 1;
    }
    if(!editor_json_references_valid(&loaded)) {
        result = editor_result_error(EDITOR_ERROR_REFERENCE_INVALID,
            "Project editor state '%s' contains an invalid entity, joint, beam, or collision-mask reference",
            path);
        goto done;
    }
    editor_project_destroy(project);
    *project = loaded;
    loaded = (EditorProject){0};
    result = editor_result_value(true);
done:
    yyjson_doc_free(document);
    return result;
}
