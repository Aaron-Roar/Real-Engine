#include "editor_file_browser.h"
#include "editor_navigation.h"
#include "editor_layout.h"
#include "panels/editor_bulk_panel.h"

#include <stdio.h>
#include <string.h>

float editor_viewport_width = WINDOW_WIDTH * 0.8f;
float editor_window_width = WINDOW_WIDTH;
float editor_window_height = WINDOW_HEIGHT;
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
    EditorRigidBody *body_b;
    EditorHitbox *hitbox;
    EditorAnchor *anchor;
    EditorJoint *joint;
    EditorSoftBody *soft_body;
    EditorSoftNode *node_a;
    EditorSoftNode *node_b;
    EditorSoftBeam *beam;
    EditorViewportState state = {0};
    EditorHistory history;
    EditorFileBrowser browser = {.mode = EDITOR_FILE_BROWSER_DIRECTORY};
    char path[EDITOR_FILE_BROWSER_PATH_MAX];

    editor_project_init(&project);
    if(!editor_history_init(&history, &project)) return 1;
    object = editor_project_object_add(&project, (Position){0});
    body = editor_project_rigid_body_add(&project, object);
    body_b = editor_project_rigid_body_add(&project, object);
    if(object == NULL || body == NULL || body_b == NULL) return 1;
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

    {
        EditorSelectionRef first = {EDITOR_SELECTION_RIGID_BODY,
            object->id, 0, 0, body->id};
        EditorSelectionRef second = {EDITOR_SELECTION_RIGID_BODY,
            object->id, 0, 0, body_b->id};
        EditorSelectionRef mixed = {EDITOR_SELECTION_SOFT_BODY,
            object->id, 0, 0, soft_body->id};
        if(!editor_viewport_selection_set(&project, &state, first, false) ||
                !editor_viewport_selection_set(&project, &state, second, true) ||
                state.selected_item_count != 2 ||
                !editor_viewport_selection_contains(&state, first) ||
                !editor_viewport_selection_contains(&state, second)) return 1;
        if(!editor_viewport_selection_set(&project, &state, second, true) ||
                state.selected_item_count != 1 ||
                state.selected_rigid_body != body->id) return 1;
        state.mode = EDITOR_VIEWPORT_RIGID_BODY;
        if(!editor_viewport_selection_set(&project, &state, mixed, true) ||
                state.selected_item_count != 2 ||
                editor_viewport_selection_homogeneous_check(&state) ||
                state.selection != EDITOR_SELECTION_SOFT_BODY) return 1;
        editor_viewport_multi_selection_dismiss(&project, &state);
        if(state.selected_item_count != 0 ||
                state.mode != EDITOR_VIEWPORT_RIGID_BODY ||
                state.selection != EDITOR_SELECTION_RIGID_BODY ||
                state.selected_rigid_body != body->id) return 1;
        state.selection = EDITOR_SELECTION_RIGID_BODY;
        state.selected_rigid_body = body->id;
        state.mode = EDITOR_VIEWPORT_RIGID_BODY;
        editor_viewport_selection_clear(&state);
        if(!editor_viewport_selection_set(&project, &state, mixed, true) ||
                state.selected_item_count != 2 ||
                state.selected_items[0].kind != EDITOR_SELECTION_RIGID_BODY ||
                state.selected_items[1].kind != EDITOR_SELECTION_SOFT_BODY)
            return 1;
        editor_viewport_multi_selection_dismiss(&project, &state);
        if(state.mode != EDITOR_VIEWPORT_RIGID_BODY ||
                state.selection != EDITOR_SELECTION_RIGID_BODY ||
                state.selected_rigid_body != body->id) return 1;
    }
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

    state.mode = EDITOR_VIEWPORT_AUTO_SHAPE;
    state.auto_shape_parent_mode = EDITOR_VIEWPORT_HITBOX;
    editor_viewport_back(&state);
    if(state.mode != EDITOR_VIEWPORT_HITBOX ||
            state.selection != EDITOR_SELECTION_HITBOX) return 1;
    state.mode = EDITOR_VIEWPORT_AUTO_SHAPE;
    state.auto_shape_parent_mode = EDITOR_VIEWPORT_SOFT_BODY;
    editor_viewport_back(&state);
    if(state.mode != EDITOR_VIEWPORT_SOFT_BODY ||
            state.selection != EDITOR_SELECTION_SOFT_BODY) return 1;

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
    {
        Position center = {EDITOR_VIEWPORT_WIDTH * 0.5f,
            EDITOR_MENU_HEIGHT +
                (EDITOR_VIEWPORT_BOTTOM - EDITOR_MENU_HEIGHT) * 0.5f};
        state.mode = EDITOR_VIEWPORT_RIGID_BODY;
        state.selection = EDITOR_SELECTION_RIGID_BODY;
        state.selected_rigid_body = body->id;
        if(!editor_project_object_select(&project, object->id)) return 1;
        editor_viewport_marquee_begin(&state,
            (Position){center.x - 100.0f, center.y - 100.0f});
        editor_viewport_marquee_update(&state,
            (Position){center.x + 100.0f, center.y + 100.0f});
        if(!state.marquee_active ||
                !editor_viewport_marquee_finish(&state, &project,
                    (Position){center.x + 100.0f, center.y + 100.0f}) ||
                state.marquee_active || state.selected_item_count != 2 ||
                !editor_viewport_selection_homogeneous_check(&state)) return 1;
        state.mode = EDITOR_VIEWPORT_OBJECT;
        state.selection = EDITOR_SELECTION_NONE;
        editor_viewport_marquee_begin(&state,
            (Position){center.x - 100.0f, center.y - 100.0f});
        if(!editor_viewport_marquee_finish(&state, &project,
                    (Position){center.x + 100.0f, center.y + 100.0f}) ||
                state.selected_item_count != 4 ||
                state.selected_items[0].kind != EDITOR_SELECTION_RIGID_BODY ||
                state.mode != EDITOR_VIEWPORT_ANCHOR ||
                editor_viewport_selection_homogeneous_check(&state)) return 1;
    }
    {
        EditorSelectionRef node_ref = {EDITOR_SELECTION_SOFT_NODE,
            object->id, soft_body->id, 0, node_a->id};
        EditorSelectionRef beam_ref = {EDITOR_SELECTION_SOFT_BEAM,
            object->id, soft_body->id, 0, beam->id};
        editor_history_reset(&history);
        editor_viewport_selection_clear(&state);
        if(!editor_navigation_selection_reorder(&project, &state,
                node_ref, beam_ref, true, &history) ||
                editor_project_soft_body_hierarchy_index_get(soft_body,
                    EDITOR_SOFT_HIERARCHY_NODE, node_a->id) <=
                editor_project_soft_body_hierarchy_index_get(soft_body,
                    EDITOR_SOFT_HIERARCHY_BEAM, beam->id) ||
                history.undo_count != 1 || !editor_history_undo(&history) ||
                editor_project_soft_body_hierarchy_index_get(soft_body,
                    EDITOR_SOFT_HIERARCHY_NODE, node_a->id) >=
                editor_project_soft_body_hierarchy_index_get(soft_body,
                    EDITOR_SOFT_HIERARCHY_BEAM, beam->id)) return 1;
    }
    {
        EditorRigidBody *body_c = editor_project_rigid_body_add(&project, object);
        EditorRigidBodyId body_id = body->id;
        EditorRigidBodyId body_b_id = body_b->id;
        EditorRigidBodyId body_c_id;
        EditorSelectionRef first = {EDITOR_SELECTION_RIGID_BODY,
            object->id, 0, 0, body_id};
        EditorSelectionRef second = {EDITOR_SELECTION_RIGID_BODY,
            object->id, 0, 0, body_b_id};
        EditorSelectionRef third;
        if(body_c == NULL) return 1;
        body_c_id = body_c->id;
        third = (EditorSelectionRef){EDITOR_SELECTION_RIGID_BODY,
            object->id, 0, 0, body_c_id};
        editor_history_reset(&history);
        editor_viewport_selection_clear(&state);
        if(!editor_navigation_selection_reorder(&project, &state,
                second, first, false, &history) ||
                object->hierarchy[0].id != body_b_id ||
                object->hierarchy[1].id != body_id ||
                history.undo_count != 1 || !editor_history_undo(&history) ||
                object->hierarchy[0].id != body_id ||
                object->hierarchy[1].id != body_b_id) return 1;
        editor_history_reset(&history);
        if(!editor_viewport_selection_set(&project, &state, first, false) ||
                !editor_viewport_selection_set(&project, &state, second, true) ||
                !editor_navigation_selection_reorder(&project, &state,
                    first, third, true, &history) ||
                editor_project_object_hierarchy_index_get(object,
                    EDITOR_HIERARCHY_RIGID_BODY, body_c_id) >=
                    editor_project_object_hierarchy_index_get(object,
                        EDITOR_HIERARCHY_RIGID_BODY, body_id) ||
                editor_project_object_hierarchy_index_get(object,
                    EDITOR_HIERARCHY_RIGID_BODY, body_id) + 1 !=
                    editor_project_object_hierarchy_index_get(object,
                        EDITOR_HIERARCHY_RIGID_BODY, body_b_id) ||
                history.undo_count != 1 || !editor_history_undo(&history) ||
                object->hierarchy[0].id != body_id ||
                object->hierarchy[1].id != body_b_id ||
                editor_project_object_hierarchy_index_get(object,
                    EDITOR_HIERARCHY_RIGID_BODY, body_c_id) != 4) return 1;
        editor_history_reset(&history);
        editor_viewport_selection_clear(&state);
        if(!editor_viewport_selection_set(&project, &state, second, false) ||
                !editor_viewport_selection_set(&project, &state, third, true) ||
                !editor_navigation_selection_reorder(&project, &state,
                    second, first, false, &history) ||
                object->hierarchy[0].id != body_b_id ||
                object->hierarchy[1].id != body_c_id ||
                object->hierarchy[2].id != body_id ||
                !editor_history_undo(&history)) return 1;
        editor_history_reset(&history);
        editor_viewport_selection_clear(&state);
        if(!editor_navigation_selection_reorder(&project, &state,
                first, third, true, &history) ||
                editor_project_object_hierarchy_index_get(object,
                    EDITOR_HIERARCHY_RIGID_BODY, body_id) + 1 !=
                    object->hierarchy_count ||
                !editor_history_undo(&history)) return 1;
        editor_viewport_selection_clear(&state);
        if(!editor_project_rigid_body_remove(object, body_c_id)) return 1;
        editor_history_reset(&history);
        {
            EditorSelectionRef joint_ref = {EDITOR_SELECTION_JOINT,
                object->id, 0, 0, joint->id};
            EditorSelectionRef soft_ref = {EDITOR_SELECTION_SOFT_BODY,
                object->id, 0, 0, soft_body->id};
            if(!editor_navigation_selection_reorder(&project, &state,
                    joint_ref, soft_ref, true, &history) ||
                    editor_project_object_hierarchy_index_get(object,
                        EDITOR_HIERARCHY_JOINT, joint->id) <=
                    editor_project_object_hierarchy_index_get(object,
                        EDITOR_HIERARCHY_SOFT_BODY, soft_body->id) ||
                    !editor_history_undo(&history)) return 1;
            editor_history_reset(&history);
        }
    }
    {
        EditorSelectionRef first = {EDITOR_SELECTION_RIGID_BODY,
            object->id, 0, 0, body->id};
        EditorSelectionRef second = {EDITOR_SELECTION_RIGID_BODY,
            object->id, 0, 0, body_b->id};
        EditorPropertySetCommand mass_property = {
            .property = EDITOR_PROPERTY_MASS,
            .value_kind = EDITOR_PROPERTY_VALUE_FLOAT,
            .value.number = 7.0f
        };
        if(!editor_viewport_selection_set(&project, &state, first, false) ||
                !editor_viewport_selection_set(&project, &state, second, true) ||
                !editor_bulk_property_set(&project, &state, &history,
                    &mass_property) ||
                body->mass_value != 7.0f || body_b->mass_value != 7.0f ||
                history.undo_count != 1 || !editor_history_undo(&history) ||
                body->mass_value == 7.0f || body_b->mass_value == 7.0f ||
                !editor_history_redo(&history) || body->mass_value != 7.0f ||
                body_b->mass_value != 7.0f)
            return 1;
        editor_history_reset(&history);
        editor_viewport_selection_clear(&state);
        if(!editor_viewport_selection_set(&project, &state, first, false) ||
                !editor_viewport_selection_set(&project, &state,
                    (EditorSelectionRef){EDITOR_SELECTION_SOFT_BODY,
                        object->id, 0, 0, soft_body->id}, true) ||
                editor_viewport_selection_homogeneous_check(&state) ||
                !editor_navigation_multi_selection_delete(
                    &project, &state, &history) ||
                object->rigid_body_count != 1 || object->soft_body_count != 0 ||
                history.undo_count != 1 || !editor_history_undo(&history) ||
                object->rigid_body_count != 2 || object->soft_body_count != 1 ||
                !editor_history_redo(&history) || object->rigid_body_count != 1 ||
                object->soft_body_count != 0)
            return 1;
    }
    {
        editor_history_reset(&history);
        editor_viewport_selection_clear(&state);
        EditorObject *frame_object = editor_project_object_add(&project, (Position){0});
        EditorAnimatedSprite *animation = editor_project_animated_sprite_add(
            &project, frame_object);
        EditorAnimatedSpriteId animation_id;
        EditorSpriteId first_id;
        EditorSpriteId second_id;
        EditorSelectionRef first;
        EditorSelectionRef second;
        if(frame_object == NULL || animation == NULL ||
                !editor_project_animation_frame_add(&project, animation,
                    "first", "assets/first.png", (Scale){16.0f, 16.0f}) ||
                !editor_project_animation_frame_add(&project, animation,
                    "second", "assets/second.png", (Scale){24.0f, 24.0f})) return 1;
        animation_id = animation->id;
        first_id = animation->frames[0].id;
        second_id = animation->frames[1].id;
        first = (EditorSelectionRef){EDITOR_SELECTION_ANIMATION_FRAME,
            frame_object->id, animation_id, 0, first_id};
        second = (EditorSelectionRef){EDITOR_SELECTION_ANIMATION_FRAME,
            frame_object->id, animation_id, 0, second_id};
        if(!editor_viewport_selection_set(&project, &state, first, false) ||
                !editor_viewport_selection_set(&project, &state, second, true) ||
                state.selected_item_count != 2) return 1;
        editor_viewport_selection_clear(&state);
        if(!editor_viewport_selection_set(&project, &state, second, false) ||
                !editor_navigation_selection_reorder(&project, &state,
                    second, first, false, &history) ||
                animation->frames[0].id != second_id || history.undo_count != 1 ||
                !editor_history_undo(&history)) return 1;
        frame_object = editor_project_selected_get(&project);
        animation = editor_project_animated_sprite_get(frame_object, animation_id);
        if(animation == NULL || animation->frames[0].id != first_id ||
                !editor_history_redo(&history)) return 1;
        frame_object = editor_project_selected_get(&project);
        animation = editor_project_animated_sprite_get(frame_object, animation_id);
        if(animation == NULL || animation->frames[0].id != second_id) return 1;
        editor_history_reset(&history);
        editor_viewport_selection_clear(&state);
        if(!editor_viewport_selection_set(&project, &state, first, false) ||
                !editor_viewport_selection_set(&project, &state, second, true) ||
                !editor_navigation_multi_selection_delete(&project, &state, &history))
            return 1;
        frame_object = editor_project_selected_get(&project);
        animation = editor_project_animated_sprite_get(frame_object, animation_id);
        if(animation == NULL || animation->frame_count != 0 ||
                history.undo_count != 1 || !editor_history_undo(&history)) return 1;
        frame_object = editor_project_selected_get(&project);
        animation = editor_project_animated_sprite_get(frame_object, animation_id);
        if(animation == NULL || animation->frame_count != 2) return 1;
    }
    {
        EditorObject *sprite_object = editor_project_selected_get(&project);
        size_t sprite_count = sprite_object == NULL ? 0 : sprite_object->sprite_count;
        size_t animation_count = sprite_object == NULL ? 0 :
            sprite_object->animated_sprite_count;
        EditorSprite *sprite = editor_project_sprite_add(&project, sprite_object,
            "standalone", "assets/standalone.png");
        EditorAnimatedSprite *animation = editor_project_animated_sprite_add(
            &project, sprite_object);
        EditorSelectionRef sprite_ref;
        EditorSelectionRef animation_ref;
        if(sprite == NULL || animation == NULL) return 1;
        sprite_ref = (EditorSelectionRef){EDITOR_SELECTION_SPRITE,
            sprite_object->id, 0, 0, sprite->id};
        animation_ref = (EditorSelectionRef){EDITOR_SELECTION_ANIMATED_SPRITE,
            sprite_object->id, 0, 0, animation->id};
        editor_history_reset(&history);
        editor_viewport_selection_clear(&state);
        if(!editor_viewport_selection_set(&project, &state, sprite_ref, false) ||
                !editor_viewport_selection_set(&project, &state,
                    animation_ref, true) || state.selected_item_count != 2 ||
                !editor_navigation_multi_selection_delete(
                    &project, &state, &history) ||
                sprite_object->sprite_count != sprite_count ||
                sprite_object->animated_sprite_count != animation_count ||
                history.undo_count != 1 || !editor_history_undo(&history)) return 1;
        sprite_object = editor_project_selected_get(&project);
        if(sprite_object == NULL ||
                sprite_object->sprite_count != sprite_count + 1 ||
                sprite_object->animated_sprite_count != animation_count + 1) return 1;
    }
    editor_history_destroy(&history);
    editor_viewport_state_destroy(&state);
    return 0;
}
