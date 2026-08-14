#include "editor_file_browser.h"
#include "editor_navigation.h"
#include "editor_layout.h"

#include <stdio.h>
#include <string.h>

float editor_viewport_width = WINDOW_WIDTH * 0.8f;
float editor_window_width = WINDOW_WIDTH;
float editor_viewport_bottom = WINDOW_HEIGHT;

static bool navigation_mode_open_check(EditorProject *project,
        EditorViewportState *state, EditorHierarchySelection selection,
        EditorViewportMode expected) {
    state->selection = selection;
    state->mode = EDITOR_VIEWPORT_HIERARCHY;
    return editor_navigation_selected_open(project, state) && state->mode == expected;
}

int main(void) {
    static EditorProject project;
    EditorObject *object;
    EditorRigidBody *body;
    EditorHitbox *hitbox;
    EditorAnchor *anchor;
    EditorJoint *joint;
    EditorSoftBody *soft_body;
    EditorSoftNode *node_a;
    EditorSoftNode *node_b;
    EditorSoftBeam *beam;
    EditorViewportState state;
    EditorFileBrowser browser = {.mode = EDITOR_FILE_BROWSER_DIRECTORY};
    char path[EDITOR_FILE_BROWSER_PATH_MAX];

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){0});
    body = editor_project_rigid_body_add(&project, object);
    if(object == NULL || body == NULL) return 1;
    anchor = editor_project_anchor_add(&project, object, (Position){0}, body->id);
    joint = editor_project_joint_add(&project, object, EDITOR_JOINT_SPRING);
    soft_body = editor_project_soft_body_add(&project, object);
    if(anchor == NULL || joint == NULL || soft_body == NULL) return 1;
    node_a = editor_project_soft_node_add(&project, soft_body, (Position){0});
    node_b = editor_project_soft_node_add(&project, soft_body, (Position){10.0f, 0.0f});
    if(node_a == NULL || node_b == NULL) return 1;
    beam = editor_project_soft_beam_add(
        &project, soft_body, node_a->id, node_b->id);
    if(beam == NULL) return 1;
    hitbox = &body->hitboxes[0];
    editor_viewport_state_init(&state);

    if(!navigation_mode_open_check(&project, &state, EDITOR_SELECTION_OBJECT,
                EDITOR_VIEWPORT_OBJECT)) return 1;
    state.selected_rigid_body = body->id;
    if(!navigation_mode_open_check(&project, &state, EDITOR_SELECTION_RIGID_BODY,
                EDITOR_VIEWPORT_RIGID_BODY)) return 1;
    state.selected_hitbox = hitbox->id;
    if(!navigation_mode_open_check(&project, &state, EDITOR_SELECTION_HITBOX,
                EDITOR_VIEWPORT_HITBOX)) return 1;
    state.selected_vertex = 0;
    if(!navigation_mode_open_check(&project, &state, EDITOR_SELECTION_VERTEX,
                EDITOR_VIEWPORT_VERTEX)) return 1;
    state.selected_line = 0;
    if(!navigation_mode_open_check(&project, &state, EDITOR_SELECTION_LINE,
                EDITOR_VIEWPORT_LINE)) return 1;
    state.selected_anchor = anchor->id;
    if(!navigation_mode_open_check(&project, &state, EDITOR_SELECTION_ANCHOR,
                EDITOR_VIEWPORT_ANCHOR)) return 1;
    state.selected_joint = joint->id;
    if(!navigation_mode_open_check(&project, &state, EDITOR_SELECTION_JOINT,
                EDITOR_VIEWPORT_JOINT)) return 1;
    state.selected_soft_body = soft_body->id;
    if(!navigation_mode_open_check(&project, &state, EDITOR_SELECTION_SOFT_BODY,
                EDITOR_VIEWPORT_SOFT_BODY)) return 1;
    state.selected_soft_node = node_a->id;
    if(!navigation_mode_open_check(&project, &state, EDITOR_SELECTION_SOFT_NODE,
                EDITOR_VIEWPORT_SOFT_NODE)) return 1;
    state.selected_soft_beam = beam->id;
    if(!navigation_mode_open_check(&project, &state, EDITOR_SELECTION_SOFT_BEAM,
                EDITOR_VIEWPORT_SOFT_BEAM)) return 1;

    state.selected_vertex = hitbox->vertex_count;
    state.selection = EDITOR_SELECTION_VERTEX;
    if(editor_navigation_selected_open(&project, &state)) return 1;
    state.mode = EDITOR_VIEWPORT_RIGID_BODY;
    state.selection = EDITOR_SELECTION_NONE;
    editor_navigation_current_selection_clear(&project, &state);
    if(state.selection != EDITOR_SELECTION_RIGID_BODY) return 1;
    state.mode = EDITOR_VIEWPORT_HIERARCHY;
    editor_navigation_current_selection_clear(&project, &state);
    if(state.selection != EDITOR_SELECTION_NONE || project.selected != 0) return 1;

    state.mode = EDITOR_VIEWPORT_SOFT_BEAM;
    state.selection = EDITOR_SELECTION_NONE;
    if(!editor_navigation_open_item_selection_set(&state) ||
            state.selection != EDITOR_SELECTION_SOFT_BEAM) return 1;
    state.mode = EDITOR_VIEWPORT_HIERARCHY;
    if(editor_navigation_open_item_selection_set(&state)) return 1;

    state.mode = EDITOR_VIEWPORT_ANCHOR;
    state.selection = EDITOR_SELECTION_ANCHOR;
    state.selected_anchor = anchor->id;
    editor_viewport_back(&state);
    if(state.mode != EDITOR_VIEWPORT_OBJECT ||
            state.selection != EDITOR_SELECTION_NONE ||
            state.selected_anchor != 0) return 1;

    snprintf(browser.directory, sizeof(browser.directory), "/projects");
    if(!editor_file_browser_directory_path_get(&browser, path, sizeof(path)) ||
            strcmp(path, "/projects") != 0) return 1;
    snprintf(browser.selected_directory, sizeof(browser.selected_directory),
        "/projects/game");
    snprintf(browser.preview_selected_path, sizeof(browser.preview_selected_path),
        "/projects/game/project.rohr.json");
    browser.preview_selected_directory = false;
    if(!editor_file_browser_directory_path_get(&browser, path, sizeof(path)) ||
            strcmp(path, "/projects/game") != 0) return 1;
    snprintf(browser.preview_selected_path, sizeof(browser.preview_selected_path),
        "/projects/game/assets");
    browser.preview_selected_directory = true;
    if(!editor_file_browser_directory_path_get(&browser, path, sizeof(path)) ||
            strcmp(path, "/projects/game/assets") != 0) return 1;
    return 0;
}
