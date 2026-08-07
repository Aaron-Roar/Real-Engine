#include "editor_project.h"

#include <math.h>

static bool position_equal(Position a, Position b) {
    return fabsf(a.x - b.x) < 0.001f && fabsf(a.y - b.y) < 0.001f;
}

int main(void) {
    EditorProject project;
    EditorObject *first;
    EditorObject *second;
    EditorVertex original[EDITOR_HITBOX_VERTEX_MIN];

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
    editor_project_selection_clear(&project);
    if(editor_project_selected_get(&project) != NULL ||
            !editor_project_object_select(&project, second->id)) return 1;
    editor_project_hitbox_add(&project, second);
    if(!second->has_hitbox || second->hitbox.vertex_count != 3) return 1;
    for(uint32_t i = 0; i < EDITOR_HITBOX_VERTEX_MIN; i += 1) {
        original[i] = second->hitbox.vertices[i];
    }
    if(!editor_project_hitbox_vertex_insert(&project, second, 0) ||
            second->hitbox.vertex_count != 4) return 1;
    if(second->hitbox.vertices[0].id != original[0].id ||
            second->hitbox.vertices[2].id != original[1].id ||
            second->hitbox.vertices[3].id != original[2].id ||
            !position_equal(second->hitbox.vertices[0].position, original[0].position) ||
            !position_equal(second->hitbox.vertices[1].position, (Position){
                (original[0].position.x + original[1].position.x) * 0.5f,
                (original[0].position.y + original[1].position.y) * 0.5f}) ||
            !position_equal(second->hitbox.vertices[2].position, original[1].position) ||
            !position_equal(second->hitbox.vertices[3].position, original[2].position)) return 1;
    if(!editor_project_hitbox_line_remove(second, 0) ||
            second->hitbox.vertex_count != 3 ||
            editor_project_hitbox_vertex_remove(second, 0)) return 1;
    if(!editor_project_hitbox_vertex_insert(&project, second, 2) ||
            second->hitbox.vertex_count != 4 ||
            !position_equal(second->hitbox.vertices[0].position, original[0].position) ||
            !position_equal(second->hitbox.vertices[1].position, original[1].position) ||
            !position_equal(second->hitbox.vertices[2].position, original[2].position) ||
            !position_equal(second->hitbox.vertices[3].position, (Position){
                (original[2].position.x + original[0].position.x) * 0.5f,
                (original[2].position.y + original[0].position.y) * 0.5f}) ||
            !editor_project_hitbox_vertex_remove(second, 3)) return 1;
    if(!editor_project_hitbox_vertex_insert(&project, second, 0) ||
            !editor_project_hitbox_vertex_remove(second, 1) ||
            second->hitbox.vertex_count != 3) return 1;
    second->hitbox.vertices[0].position_locked = true;
    if(!editor_project_hitbox_line_length_set(second, 0, 100.0f)) return 1;
    second->hitbox.vertices[1].position_locked = true;
    if(editor_project_hitbox_line_length_set(second, 0, 50.0f)) return 1;
    if(!editor_project_hitbox_remove(second) || second->has_hitbox ||
            second->hitbox.vertex_count != 0 ||
            editor_project_hitbox_remove(second)) return 1;
    if(!editor_project_object_remove(&project, second->id) ||
            project.object_count != 1 || project.selected != EDITOR_OBJECT_INVALID ||
            editor_project_object_remove(&project, second->id)) return 1;
    return 0;
}
