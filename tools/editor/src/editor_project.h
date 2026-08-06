#ifndef ROHR_EDITOR_PROJECT_H
#define ROHR_EDITOR_PROJECT_H

#include "rohr.h"

#define EDITOR_OBJECT_MAX 64
#define EDITOR_HITBOX_VERTEX_MIN 3
#define EDITOR_HITBOX_VERTEX_MAX 8

typedef uint32_t EditorObjectId;

#define EDITOR_OBJECT_INVALID 0

typedef struct EditorHitbox {
    Position vertices[EDITOR_HITBOX_VERTEX_MAX];
    uint32_t vertex_count;
} EditorHitbox;

typedef struct EditorObject {
    EditorObjectId id;
    Position position;
    bool has_hitbox;
    EditorHitbox hitbox;
} EditorObject;

typedef struct EditorProject {
    EditorObject objects[EDITOR_OBJECT_MAX];
    size_t object_count;
    EditorObjectId next_id;
    EditorObjectId selected;
} EditorProject;

void editor_project_init(EditorProject *project);
EditorObject *editor_project_object_add(EditorProject *project, Position position);
EditorObject *editor_project_selected_get(EditorProject *project);
bool editor_project_object_select(EditorProject *project, EditorObjectId id);
void editor_project_hitbox_add(EditorObject *object, uint32_t vertex_count);
void editor_project_hitbox_vertex_count_set(
    EditorObject *object,
    uint32_t vertex_count
);

#endif
