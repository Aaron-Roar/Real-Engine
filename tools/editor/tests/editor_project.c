#include "editor_project.h"

int main(void) {
    EditorProject project;
    EditorObject *first;
    EditorObject *second;

    editor_project_init(&project);
    first = editor_project_object_add(&project, (Position){10.0f, 20.0f});
    second = editor_project_object_add(&project, (Position){30.0f, 40.0f});
    if(first == NULL || second == NULL || first->id == second->id ||
            project.selected != second->id || second->has_hitbox) return 1;
    editor_project_hitbox_add(second, 4);
    if(!second->has_hitbox || second->hitbox.vertex_count != 4) return 1;
    editor_project_hitbox_vertex_count_set(second, 7);
    if(second->hitbox.vertex_count != 7) return 1;
    editor_project_hitbox_vertex_count_set(second, 100);
    if(second->hitbox.vertex_count != EDITOR_HITBOX_VERTEX_MAX) return 1;
    return 0;
}
