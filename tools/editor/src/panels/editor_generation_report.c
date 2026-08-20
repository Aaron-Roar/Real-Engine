/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "panels/editor_generation_report.h"

#include <stdio.h>
#include <string.h>

static bool editor_generation_line_write(EditorTerminalPanel *terminal,
        const char *prefix, const char *branch, const char *type,
        const char *name) {
    char line[EDITOR_OBJECT_NAME_MAX + 128];
    int count = snprintf(line, sizeof(line), "%s%s%s %s\r\n",
        prefix, branch, type, name);
    return count >= 0 && (size_t)count < sizeof(line) &&
        editor_terminal_panel_output_write(terminal, line);
}

static bool editor_generation_child_prefix_get(char *output, size_t capacity,
        const char *prefix, bool parent_last) {
    const char *suffix = parent_last ? "    " : "│   ";
    size_t prefix_length = strlen(prefix);
    size_t suffix_length = strlen(suffix);
    if(prefix_length + suffix_length >= capacity) return false;
    memcpy(output, prefix, prefix_length);
    memcpy(output + prefix_length, suffix, suffix_length + 1);
    return true;
}

static bool editor_generation_hitbox_write(EditorTerminalPanel *terminal,
        const EditorHitbox *hitbox, const char *prefix, bool last) {
    char child_prefix[96];
    size_t child_count = hitbox->vertex_count * 2;
    if(!editor_generation_line_write(terminal, prefix,
            last ? "└── " : "├── ", "Hitbox", hitbox->name)) return false;
    if(!editor_generation_child_prefix_get(child_prefix, sizeof(child_prefix),
            prefix, last)) return false;
    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
        if(!editor_generation_line_write(terminal, child_prefix,
                i + 1 == child_count ? "└── " : "├── ", "Vertex",
                hitbox->vertices[i].name)) return false;
    }
    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
        if(!editor_generation_line_write(terminal, child_prefix,
                hitbox->vertex_count + i + 1 == child_count ? "└── " : "├── ",
                "Line", hitbox->line_names[i])) return false;
    }
    return true;
}

static bool editor_generation_rigid_body_write(EditorTerminalPanel *terminal,
        const EditorRigidBody *body, const char *prefix, bool last) {
    char child_prefix[96];
    size_t child_count = body->hitbox_count + (body->particle ? 1 : 0);
    size_t child = 0;
    if(!editor_generation_line_write(terminal, prefix,
            last ? "└── " : "├── ", "RigidBody", body->name)) return false;
    if(!editor_generation_child_prefix_get(child_prefix, sizeof(child_prefix),
            prefix, last)) return false;
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        child += 1;
        if(!editor_generation_hitbox_write(terminal, &body->hitboxes[i],
                child_prefix, child == child_count)) return false;
    }
    if(body->particle && !editor_generation_line_write(terminal, child_prefix,
            "└── ", "Particle", body->name)) return false;
    return true;
}

static bool editor_generation_soft_body_write(EditorTerminalPanel *terminal,
        const EditorSoftBody *body, const char *prefix, bool last) {
    char child_prefix[96];
    size_t child_count = body->node_count + body->beam_count + body->area_count;
    size_t child = 0;
    if(!editor_generation_line_write(terminal, prefix,
            last ? "└── " : "├── ", "SoftBody", body->name)) return false;
    if(!editor_generation_child_prefix_get(child_prefix, sizeof(child_prefix),
            prefix, last)) return false;
#define EDITOR_GENERATION_CHILD(items, count, type) do { \
    for(size_t i = 0; i < (count); i += 1) { \
        child += 1; \
        if(!editor_generation_line_write(terminal, child_prefix, \
                child == child_count ? "└── " : "├── ", (type), \
                (items)[i].name)) return false; \
    } \
} while(0)
    EDITOR_GENERATION_CHILD(body->nodes, body->node_count, "Node");
    EDITOR_GENERATION_CHILD(body->beams, body->beam_count, "Beam");
    EDITOR_GENERATION_CHILD(body->areas, body->area_count, "Area");
#undef EDITOR_GENERATION_CHILD
    return true;
}

static bool editor_generation_object_write(EditorTerminalPanel *terminal,
        const EditorObject *object, bool last) {
    char prefix[8];
    size_t child_count = object->rigid_body_count + object->anchor_count +
        object->joint_count + object->soft_body_count;
    size_t child = 0;
    if(!editor_generation_line_write(terminal, "", last ? "└── " : "├── ",
            "Object", object->name)) return false;
    snprintf(prefix, sizeof(prefix), "%s", last ? "    " : "│   ");
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        child += 1;
        if(!editor_generation_rigid_body_write(terminal,
                &object->rigid_bodies[i], prefix, child == child_count)) return false;
    }
    for(size_t i = 0; i < object->anchor_count; i += 1) {
        child += 1;
        if(!editor_generation_line_write(terminal, prefix,
                child == child_count ? "└── " : "├── ", "Anchor",
                object->anchors[i].name)) return false;
    }
    for(size_t i = 0; i < object->joint_count; i += 1) {
        child += 1;
        if(!editor_generation_line_write(terminal, prefix,
                child == child_count ? "└── " : "├── ", "Joint",
                object->joint_items[i].name)) return false;
    }
    for(size_t i = 0; i < object->soft_body_count; i += 1) {
        child += 1;
        if(!editor_generation_soft_body_write(terminal,
                &object->soft_body_items[i], prefix, child == child_count)) return false;
    }
    return true;
}

static bool editor_generation_file_write(EditorTerminalPanel *terminal,
        const EditorProject *project, const char *path) {
    char heading[256];
    int count = snprintf(heading, sizeof(heading), "\r\nGenerated %s\r\n", path);
    if(count < 0 || (size_t)count >= sizeof(heading) ||
            !editor_terminal_panel_output_write(terminal, heading)) return false;
    for(size_t i = 0; i < project->object_count; i += 1)
        if(!editor_generation_object_write(terminal, &project->objects[i],
                i + 1 == project->object_count)) return false;
    return true;
}

bool editor_generation_report_write(EditorTerminalPanel *terminal,
        const EditorProject *project) {
    return terminal != NULL && project != NULL &&
        editor_generation_file_write(terminal, project,
            "src/generated/project_objects.h") &&
        editor_generation_file_write(terminal, project,
            "src/generated/project_objects.c");
}
