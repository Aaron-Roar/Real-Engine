#include "editor_project.h"

#include <math.h>

static uint32_t editor_vertex_count_clamp(uint32_t vertex_count) {
    if(vertex_count < EDITOR_HITBOX_VERTEX_MIN) {
        return EDITOR_HITBOX_VERTEX_MIN;
    }
    if(vertex_count > EDITOR_HITBOX_VERTEX_MAX) {
        return EDITOR_HITBOX_VERTEX_MAX;
    }
    return vertex_count;
}

static void editor_hitbox_regular_set(EditorHitbox *hitbox, uint32_t vertex_count) {
    const float radius = 70.0f;

    if(hitbox == NULL) return;
    vertex_count = editor_vertex_count_clamp(vertex_count);
    hitbox->vertex_count = vertex_count;
    for(uint32_t i = 0; i < vertex_count; i += 1) {
        float angle = -1.57079632679f + 6.28318530718f *
            (float)i / (float)vertex_count;
        hitbox->vertices[i] = (Position){
            cosf(angle) * radius,
            sinf(angle) * radius
        };
    }
}

void editor_project_init(EditorProject *project) {
    if(project == NULL) return;
    *project = (EditorProject){
        .next_id = 1
    };
}

EditorObject *editor_project_object_add(EditorProject *project, Position position) {
    EditorObject *object;

    if(project == NULL || project->object_count >= EDITOR_OBJECT_MAX) return NULL;
    object = &project->objects[project->object_count++];
    *object = (EditorObject){
        .id = project->next_id++,
        .position = position
    };
    project->selected = object->id;
    return object;
}

EditorObject *editor_project_selected_get(EditorProject *project) {
    if(project == NULL || project->selected == EDITOR_OBJECT_INVALID) return NULL;
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(project->objects[i].id == project->selected) return &project->objects[i];
    }
    return NULL;
}

void editor_project_hitbox_add(EditorObject *object, uint32_t vertex_count) {
    if(object == NULL) return;
    object->has_hitbox = true;
    editor_hitbox_regular_set(&object->hitbox, vertex_count);
}

void editor_project_hitbox_vertex_count_set(
    EditorObject *object,
    uint32_t vertex_count
) {
    if(object == NULL || !object->has_hitbox ||
            object->hitbox.vertex_count == editor_vertex_count_clamp(vertex_count)) return;
    editor_hitbox_regular_set(&object->hitbox, vertex_count);
}
