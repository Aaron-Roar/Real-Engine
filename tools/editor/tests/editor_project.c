#include "editor_project.h"

int main(void) {
    EditorProject project;
    EditorObject *first;
    EditorObject *second;

    editor_project_init(&project);
    first = editor_project_object_add(&project, (Position){10.0f, 20.0f});
    second = editor_project_object_add(&project, (Position){30.0f, 40.0f});
    if(first == NULL || second == NULL || first->id == second->id ||
            first->name[0] == '\0' || second->name[0] == '\0' ||
            project.selected != second->id || second->has_hitbox) return 1;
    if(!editor_project_object_select(&project, first->id) ||
            editor_project_selected_get(&project) != first ||
            editor_project_object_select(&project, EDITOR_OBJECT_INVALID)) return 1;
    if(!editor_project_object_select(&project, second->id)) return 1;
    editor_project_hitbox_add(&project, second);
    if(!second->has_hitbox || second->hitbox.vertex_count != 3) return 1;
    if(!editor_project_hitbox_vertex_insert(&project, second, 0) ||
            second->hitbox.vertex_count != 4) return 1;
    second->hitbox.vertices[0].position_locked = true;
    if(!editor_project_hitbox_line_length_set(second, 0, 100.0f)) return 1;
    second->hitbox.vertices[1].position_locked = true;
    if(editor_project_hitbox_line_length_set(second, 0, 50.0f)) return 1;
    return 0;
}
