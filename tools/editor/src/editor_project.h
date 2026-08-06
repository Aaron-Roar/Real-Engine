#ifndef ROHR_EDITOR_PROJECT_H
#define ROHR_EDITOR_PROJECT_H

#include "rohr.h"

#define EDITOR_OBJECT_MAX 64
#define EDITOR_HITBOX_VERTEX_MIN 3
#define EDITOR_HITBOX_VERTEX_MAX 8

typedef uint32_t EditorObjectId;
typedef uint32_t EditorVertexId;

#define EDITOR_OBJECT_INVALID 0

typedef struct EditorVertex {
    EditorVertexId id;
    Position position;
    bool position_locked;
} EditorVertex;

typedef struct EditorHitbox {
    EditorVertex vertices[EDITOR_HITBOX_VERTEX_MAX];
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
    EditorVertexId next_vertex_id;
    EditorObjectId selected;
} EditorProject;

void editor_project_init(EditorProject *project);
EditorObject *editor_project_object_add(EditorProject *project, Position position);
EditorObject *editor_project_selected_get(EditorProject *project);
bool editor_project_object_select(EditorProject *project, EditorObjectId id);
void editor_project_hitbox_add(EditorProject *project, EditorObject *object);
bool editor_project_hitbox_vertex_insert(EditorProject *project, EditorObject *object,
    uint32_t line_index);
float editor_project_hitbox_line_length_get(const EditorObject *object,
    uint32_t line_index);
bool editor_project_hitbox_line_length_set(EditorObject *object,
    uint32_t line_index, float length);

#endif
