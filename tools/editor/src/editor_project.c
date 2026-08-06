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

static void editor_hitbox_regular_set(EditorProject *project, EditorHitbox *hitbox,
    uint32_t vertex_count) {
    const float radius = 70.0f;

    if(project == NULL || hitbox == NULL) return;
    vertex_count = editor_vertex_count_clamp(vertex_count);
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
    }
}

void editor_project_init(EditorProject *project) {
    if(project == NULL) return;
    *project = (EditorProject){
        .next_id = 1,
        .next_vertex_id = 1
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

bool editor_project_object_select(EditorProject *project, EditorObjectId id) {
    if(project == NULL) return false;
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(project->objects[i].id != id) continue;
        project->selected = id;
        return true;
    }
    return false;
}

void editor_project_hitbox_add(EditorProject *project, EditorObject *object) {
    if(project == NULL || object == NULL) return;
    object->has_hitbox = true;
    editor_hitbox_regular_set(project, &object->hitbox, EDITOR_HITBOX_VERTEX_MIN);
}

bool editor_project_hitbox_vertex_insert(EditorProject *project, EditorObject *object,
    uint32_t line_index) {
    EditorHitbox *hitbox;
    uint32_t second;
    EditorVertex inserted;

    if(project == NULL || object == NULL || !object->has_hitbox) return false;
    hitbox = &object->hitbox;
    if(hitbox->vertex_count >= EDITOR_HITBOX_VERTEX_MAX ||
            line_index >= hitbox->vertex_count) return false;
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
    if(second == 0) {
        hitbox->vertices[hitbox->vertex_count] = inserted;
    } else {
        for(uint32_t i = hitbox->vertex_count; i > second; i -= 1) {
            hitbox->vertices[i] = hitbox->vertices[i - 1];
        }
        hitbox->vertices[second] = inserted;
    }
    hitbox->vertex_count += 1;
    return true;
}

float editor_project_hitbox_line_length_get(const EditorObject *object,
    uint32_t line_index) {
    uint32_t second;
    Vec2D delta;

    if(object == NULL || !object->has_hitbox ||
            line_index >= object->hitbox.vertex_count) return 0.0f;
    second = (line_index + 1) % object->hitbox.vertex_count;
    delta = (Vec2D){
        object->hitbox.vertices[second].position.x -
            object->hitbox.vertices[line_index].position.x,
        object->hitbox.vertices[second].position.y -
            object->hitbox.vertices[line_index].position.y
    };
    return sqrtf(delta.x * delta.x + delta.y * delta.y);
}

bool editor_project_hitbox_line_length_set(EditorObject *object,
    uint32_t line_index, float length) {
    EditorVertex *first;
    EditorVertex *second;
    Vec2D direction;
    float current;

    if(object == NULL || !object->has_hitbox || length <= 0.001f ||
            line_index >= object->hitbox.vertex_count) return false;
    first = &object->hitbox.vertices[line_index];
    second = &object->hitbox.vertices[(line_index + 1) %
        object->hitbox.vertex_count];
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
