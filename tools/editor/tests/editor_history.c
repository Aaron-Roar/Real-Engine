/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_history.h"
#include "editor_layout.h"
#include "editor_navigation.h"
#include "editor_shortcuts.h"
#include "editor_viewport.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

float editor_viewport_width = 1024.0f;
float editor_window_width = 1280.0f;
float editor_window_height = 720.0f;
float editor_viewport_bottom = 720.0f;

static EditorHistory *callback_history;

static void history_begin(const EditorProject *project,
        const EditorCommand *command, void *context) {
    (void)context;
    editor_history_command_begin(callback_history, project, command);
}

static void history_finish(const EditorCommand *command,
        const EditorCommandResult *result, void *context) {
    (void)context;
    editor_history_command_finish(callback_history, command, result);
}

static Position test_world_to_screen(Position world) {
    return (Position){editor_viewport_width * 0.5f + world.x,
        EDITOR_MENU_HEIGHT +
            (editor_viewport_bottom - EDITOR_MENU_HEIGHT) * 0.5f - world.y};
}

static bool viewport_pointer_update(EditorHistory *history,
        EditorViewportState *viewport, EditorProject *project, Position pointer,
        MouseButtonState button) {
    bool active = editor_viewport_transform_active_check(viewport);
    bool consumed = editor_viewport_update(viewport, project, pointer, button,
        MOUSE_BUTTON_STATE_UP, false, 0.0f, false);
    assert(editor_navigation_viewport_transform_history_update(
        project, viewport, history, active));
    return consumed;
}

static bool auto_shape_pointer_update(EditorHistory *history,
        EditorViewportState *viewport, EditorProject *project,
        EditorAutoShapeConfig *config, Position pointer,
        MouseButtonState button) {
    bool active = editor_viewport_transform_active_check(viewport);
    bool consumed = editor_viewport_auto_shape_update(viewport, project, config,
        pointer, button, MOUSE_BUTTON_STATE_UP, false, 0.0f, false);
    assert(editor_navigation_viewport_transform_history_update(
        project, viewport, history, active));
    return consumed;
}

static void shortcut_apply(EditorHistory *history, SDL_Keycode key) {
    SDL_Event shortcut = {0};
    EditorHistoryShortcutResult result;
    shortcut.type = SDL_EVENT_KEY_DOWN;
    shortcut.key.key = key;
    shortcut.key.mod = SDL_KMOD_CTRL;
    result = editor_history_shortcut_handle(&shortcut, true, history);
    assert(result.consumed && result.restored);
}

int main(void) {
    static EditorProject project;
    EditorHistory history;
    EditorRigidBody *body;
    EditorHitbox *hitbox;
    EditorAnchor *anchor;
    EditorSoftBody *soft_body;
    EditorSoftNode *soft_node;
    Position original_vertex;
    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
        .data.item_add = {.kind = EDITOR_ITEM_OBJECT}};
    EditorCommandResult result;

    {
        EditorViewportState transform = {0};
#define TRANSFORM_FLAG_CHECK(member) \
        do { \
            editor_viewport_state_init(&transform); \
            transform.member = true; \
            assert(editor_viewport_transform_active_check(&transform)); \
            editor_viewport_transform_cancel(&transform); \
            assert(!editor_viewport_transform_active_check(&transform)); \
        } while(0)
        TRANSFORM_FLAG_CHECK(dragged_body);
        TRANSFORM_FLAG_CHECK(rotated_body);
        TRANSFORM_FLAG_CHECK(dragged_anchor);
        TRANSFORM_FLAG_CHECK(dragged_soft_node);
        TRANSFORM_FLAG_CHECK(dragged_soft_body);
        TRANSFORM_FLAG_CHECK(dragged_sprite);
        TRANSFORM_FLAG_CHECK(dragged_animated_sprite);
        TRANSFORM_FLAG_CHECK(rotated_sprite);
        TRANSFORM_FLAG_CHECK(rotated_animated_sprite);
        TRANSFORM_FLAG_CHECK(rotated_soft_body);
        TRANSFORM_FLAG_CHECK(dragged_origin);
        TRANSFORM_FLAG_CHECK(group_dragging);
        TRANSFORM_FLAG_CHECK(group_rotating);
        editor_viewport_state_init(&transform);
        transform.dragged_vertex = 0;
        assert(editor_viewport_transform_active_check(&transform));
        editor_viewport_transform_cancel(&transform);
        assert(!editor_viewport_transform_active_check(&transform));
#undef TRANSFORM_FLAG_CHECK
    }
    EditorViewportState viewport = {0};

    editor_project_init(&project);
    assert(editor_history_init(&history, &project));
    snprintf(command.data.item_add.name, sizeof(command.data.item_add.name), "Car");
    editor_history_command_begin(&history, &project, &command);
    result = editor_command_execute(&project, &command);
    editor_history_command_finish(&history, &command, &result);
    assert(result.kind == ERROR_RESULT_VALUE);
    assert(project.object_count == 1);
    assert(editor_history_memory_get(&history) < 8192);
    assert(editor_history_undo(&history));
    assert(project.object_count == 0);
    assert(editor_history_redo(&history));
    assert(project.object_count == 1);
    assert(strcmp(project.objects[0].name, "Car") == 0);

    editor_history_reset(&history);
    command = (EditorCommand){.type = EDITOR_COMMAND_COLLISION_MASK_ADD};
    snprintf(command.data.collision_mask_add.name,
        sizeof(command.data.collision_mask_add.name), "vehicle");
    editor_history_command_begin(&history, &project, &command);
    result = editor_command_execute(&project, &command);
    editor_history_command_finish(&history, &command, &result);
    assert(result.kind == ERROR_RESULT_VALUE && project.collision_mask_count == 2);
    assert(editor_history_undo(&history) && project.collision_mask_count == 1);
    assert(editor_history_redo(&history) && project.collision_mask_count == 2);

    editor_history_reset(&history);
    command = (EditorCommand){.type = EDITOR_COMMAND_ITEM_ADD,
        .data.item_add = {.kind = EDITOR_ITEM_RIGID_BODY,
            .object = project.objects[0].id}};
    editor_history_command_begin(&history, &project, &command);
    result = editor_command_execute(&project, &command);
    editor_history_command_finish(&history, &command, &result);
    assert(result.kind == ERROR_RESULT_VALUE);
    {
        EditorRigidBodyId added_body = result.result.object;
        assert(project.objects[0].rigid_body_count == 1);
        assert(editor_history_undo(&history));
        assert(project.objects[0].rigid_body_count == 0);
        assert(editor_history_redo(&history));
        assert(project.objects[0].rigid_body_count == 1);
        assert(project.objects[0].rigid_bodies[0].id == added_body);
        command = (EditorCommand){.type = EDITOR_COMMAND_ITEM_REMOVE,
            .data.item_remove = {.kind = EDITOR_ITEM_RIGID_BODY,
                .object = project.objects[0].id, .item = added_body}};
        editor_history_command_begin(&history, &project, &command);
        result = editor_command_execute(&project, &command);
        editor_history_command_finish(&history, &command, &result);
        assert(result.kind == ERROR_RESULT_VALUE);
        assert(project.objects[0].rigid_body_count == 0);
        assert(editor_history_undo(&history));
        assert(project.objects[0].rigid_body_count == 1);
        assert(editor_history_redo(&history));
        assert(project.objects[0].rigid_body_count == 0);
    }

    body = editor_project_rigid_body_add(&project, &project.objects[0]);
    assert(body != NULL);
    hitbox = editor_project_hitbox_add(&project, body);
    assert(hitbox != NULL && hitbox->vertex_count > 0);
    original_vertex = hitbox->vertices[0].position;
    editor_history_reset(&history);
    command = (EditorCommand){.type = EDITOR_COMMAND_VERTEX_POSITION,
        .data.vertex_position = {.object = project.objects[0].id,
            .body = body->id, .hitbox = hitbox->id,
            .vertex = hitbox->vertices[0].id,
            .position = {10.0f, 0.0f}}};
    editor_history_command_begin(&history, &project, &command);
    result = editor_command_execute(&project, &command);
    editor_history_command_finish(&history, &command, &result);
    assert(result.kind == ERROR_RESULT_VALUE);
    editor_history_continuous_set(&history, true);
    command.data.vertex_position.position.x = 20.0f;
    editor_history_command_begin(&history, &project, &command);
    result = editor_command_execute(&project, &command);
    editor_history_command_finish(&history, &command, &result);
    assert(result.kind == ERROR_RESULT_VALUE);
    editor_history_continuous_set(&history, false);
    assert(editor_history_undo(&history));
    body = editor_project_rigid_body_get(&project.objects[0], body->id);
    hitbox = editor_project_hitbox_get(body, hitbox->id);
    assert(hitbox != NULL);
    assert(hitbox->vertices[0].position.x == original_vertex.x);
    assert(hitbox->vertices[0].position.y == original_vertex.y);
    assert(!editor_history_undo_check(&history));

    editor_history_reset(&history);
    editor_viewport_state_init(&viewport);
    project.objects[0].visible = true;
    body->visible = true;
    hitbox->visible = true;
    project.selected = project.objects[0].id;
    viewport.mode = EDITOR_VIEWPORT_HITBOX;
    viewport.selection = EDITOR_SELECTION_HITBOX;
    viewport.selected_rigid_body = body->id;
    viewport.selected_hitbox = hitbox->id;
    callback_history = &history;
    editor_command_executing_callback_set(history_begin, NULL);
    editor_command_finished_callback_set(history_finish, NULL);
    {
        Position world = {
            project.objects[0].position.x + body->position.x +
                hitbox->vertices[0].position.x,
            project.objects[0].position.y + body->position.y +
                hitbox->vertices[0].position.y
        };
        Position grab = test_world_to_screen(world);
        grab.x += 5.0f;
        grab.y -= 3.0f;
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_PRESSED));
        assert(history.undo_count == 0);
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_DOWN));
        assert(history.undo_count == 0);
        grab.x += 20.0f;
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_DOWN));
        assert(!viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_RELEASED));
        assert(history.undo_count == 1);
        assert(editor_history_memory_get(&history) < 4096);
        shortcut_apply(&history, SDLK_Z);
        assert(hitbox->vertices[0].position.x == original_vertex.x);
        shortcut_apply(&history, SDLK_Y);
        assert(hitbox->vertices[0].position.x == original_vertex.x + 20.0f);
    }


    anchor = editor_project_anchor_add(&project, &project.objects[0],
        (Position){200.0f, 100.0f}, 0);
    assert(anchor != NULL);
    editor_history_reset(&history);
    editor_viewport_state_init(&viewport);
    viewport.mode = EDITOR_VIEWPORT_OBJECT;
    {
        Position original = anchor->position;
        Position grab = test_world_to_screen((Position){
            project.objects[0].position.x + original.x,
            project.objects[0].position.y + original.y});
        grab.x += 4.0f;
        grab.y -= 2.0f;
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_PRESSED));
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_DOWN));
        assert(history.undo_count == 0);
        grab.x += 20.0f;
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_DOWN));
        assert(!viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_RELEASED));
        assert(history.undo_count == 1);
        shortcut_apply(&history, SDLK_Z);
        assert(anchor->position.x == original.x);
        shortcut_apply(&history, SDLK_Y);
        assert(anchor->position.x == original.x + 20.0f);
    }

    soft_body = editor_project_soft_body_add(&project, &project.objects[0]);
    assert(soft_body != NULL);
    soft_node = editor_project_soft_node_add(&project, soft_body,
        (Position){-200.0f, -100.0f});
    assert(soft_node != NULL);
    editor_history_reset(&history);
    editor_viewport_state_init(&viewport);
    viewport.mode = EDITOR_VIEWPORT_SOFT_NODE;
    viewport.selection = EDITOR_SELECTION_SOFT_NODE;
    viewport.selected_soft_body = soft_body->id;
    viewport.selected_soft_node = soft_node->id;
    {
        Position original = soft_node->position;
        Position grab = test_world_to_screen((Position){
            project.objects[0].position.x + soft_body->position.x + original.x,
            project.objects[0].position.y + soft_body->position.y + original.y});
        grab.x += 3.0f;
        grab.y -= 4.0f;
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_PRESSED));
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_DOWN));
        assert(history.undo_count == 0);
        grab.x += 20.0f;
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_DOWN));
        assert(!viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_RELEASED));
        assert(history.undo_count == 1);
        shortcut_apply(&history, SDLK_Z);
        assert(soft_node->position.x == original.x);
        shortcut_apply(&history, SDLK_Y);
        assert(soft_node->position.x == original.x + 20.0f);
    }
    editor_history_reset(&history);
    {
        Position before[EDITOR_HITBOX_VERTEX_MAX];
        for(size_t i = 0; i < hitbox->vertex_count; i += 1)
            before[i] = hitbox->vertices[i].position;
        command = (EditorCommand){.type = EDITOR_COMMAND_AUTO_SHAPE,
            .data.auto_shape = {.kind = EDITOR_ITEM_HITBOX,
                .object = project.objects[0].id, .parent = body->id,
                .item = hitbox->id,
                .config = {.kind = EDITOR_AUTO_SHAPE_CIRCLE, .radius = 30.0f}}};
        result = editor_command_execute(&project, &command);
        assert(result.kind == ERROR_RESULT_VALUE);
        assert(history.undo_count == 1);
        shortcut_apply(&history, SDLK_Z);
        for(size_t i = 0; i < hitbox->vertex_count; i += 1) {
            assert(hitbox->vertices[i].position.x == before[i].x);
            assert(hitbox->vertices[i].position.y == before[i].y);
        }
        shortcut_apply(&history, SDLK_Y);
        assert(fabsf(hitbox->vertices[0].position.y + 30.0f) < 0.001f);
    }
    editor_history_reset(&history);
    editor_viewport_state_init(&viewport);
    viewport.mode = EDITOR_VIEWPORT_AUTO_SHAPE;
    viewport.auto_shape_parent_mode = EDITOR_VIEWPORT_HITBOX;
    viewport.selection = EDITOR_SELECTION_HITBOX;
    viewport.selected_rigid_body = body->id;
    viewport.selected_hitbox = hitbox->id;
    {
        EditorAutoShapeConfig config = {
            .kind = EDITOR_AUTO_SHAPE_CIRCLE, .radius = 30.0f
        };
        Position grab = test_world_to_screen((Position){
            project.objects[0].position.x + body->position.x,
            project.objects[0].position.y + body->position.y - config.radius});
        assert(auto_shape_pointer_update(&history, &viewport, &project, &config,
            grab, MOUSE_BUTTON_STATE_PRESSED));
        grab.y += 20.0f;
        assert(auto_shape_pointer_update(&history, &viewport, &project, &config,
            grab, MOUSE_BUTTON_STATE_DOWN));
        assert(!auto_shape_pointer_update(&history, &viewport, &project, &config,
            grab, MOUSE_BUTTON_STATE_RELEASED));
        assert(fabsf(config.radius - 50.0f) < 0.001f);
        assert(fabsf(hitbox->vertices[0].position.y + 50.0f) < 0.001f);
        for(size_t i = 0; i < hitbox->vertex_count; i += 1) {
            float radius = hypotf(hitbox->vertices[i].position.x,
                hitbox->vertices[i].position.y);
            assert(fabsf(radius - 50.0f) < 0.001f);
        }
        assert(history.undo_count == 1);
        shortcut_apply(&history, SDLK_Z);
        assert(fabsf(hitbox->vertices[0].position.y + 30.0f) < 0.001f);
        shortcut_apply(&history, SDLK_Y);
        assert(fabsf(hitbox->vertices[0].position.y + 50.0f) < 0.001f);
    }

    assert(editor_project_soft_node_add(&project, soft_body,
        (Position){0.0f, 0.0f}) != NULL);
    assert(editor_project_soft_node_add(&project, soft_body,
        (Position){0.0f, 0.0f}) != NULL);
    {
        EditorAutoShapeConfig config = {
            .kind = EDITOR_AUTO_SHAPE_CIRCLE, .radius = 40.0f
        };
        command = (EditorCommand){.type = EDITOR_COMMAND_AUTO_SHAPE,
            .data.auto_shape = {.kind = EDITOR_ITEM_SOFT_BODY,
                .object = project.objects[0].id, .item = soft_body->id,
                .config = config}};
        result = editor_command_execute(&project, &command);
        assert(result.kind == ERROR_RESULT_VALUE);
        editor_history_reset(&history);
        editor_viewport_state_init(&viewport);
        viewport.mode = EDITOR_VIEWPORT_AUTO_SHAPE;
        viewport.auto_shape_parent_mode = EDITOR_VIEWPORT_SOFT_BODY;
        viewport.selection = EDITOR_SELECTION_SOFT_BODY;
        viewport.selected_soft_body = soft_body->id;
        {
            Position grab = test_world_to_screen((Position){
                project.objects[0].position.x + soft_body->position.x,
                project.objects[0].position.y + soft_body->position.y - config.radius});
            assert(auto_shape_pointer_update(&history, &viewport, &project,
                &config, grab, MOUSE_BUTTON_STATE_PRESSED));
            grab.y += 10.0f;
            assert(auto_shape_pointer_update(&history, &viewport, &project,
                &config, grab, MOUSE_BUTTON_STATE_DOWN));
            assert(!auto_shape_pointer_update(&history, &viewport, &project,
                &config, grab, MOUSE_BUTTON_STATE_RELEASED));
            assert(fabsf(config.radius - 50.0f) < 0.001f);
            for(size_t i = 0; i < soft_body->node_count; i += 1) {
                float radius = hypotf(soft_body->nodes[i].position.x,
                    soft_body->nodes[i].position.y);
                assert(fabsf(radius - 50.0f) < 0.001f);
            }
            assert(history.undo_count == 1);
            shortcut_apply(&history, SDLK_Z);
            assert(fabsf(soft_body->nodes[0].position.y + 40.0f) < 0.001f);
            shortcut_apply(&history, SDLK_Y);
            assert(fabsf(soft_body->nodes[0].position.y + 50.0f) < 0.001f);
        }
    }
    editor_command_executing_callback_set(NULL, NULL);
    editor_command_finished_callback_set(NULL, NULL);
    callback_history = NULL;

    {
        EditorSelectionRef rigid = {EDITOR_SELECTION_RIGID_BODY,
            project.objects[0].id, 0, 0, body->id};
        EditorSelectionRef soft = {EDITOR_SELECTION_SOFT_BODY,
            project.objects[0].id, 0, 0, soft_body->id};
        Position handle;
        Position target;
        project.objects[0].position = (Position){0};
        body->position = (Position){-10.0f, 0.0f};
        body->rotation = 0.0f;
        soft_body->position = (Position){10.0f, 0.0f};
        soft_body->rotation = 0.0f;
        editor_history_reset(&history);
        editor_viewport_state_init(&viewport);
        viewport.mode = EDITOR_VIEWPORT_OBJECT;
        assert(editor_viewport_selection_set(&project, &viewport, rigid, false));
        assert(editor_viewport_selection_set(&project, &viewport, soft, true));
        handle = test_world_to_screen((Position){-10.0f, 0.0f});
        target = test_world_to_screen((Position){-8.0f, 0.0f});
        assert(viewport_pointer_update(&history, &viewport, &project, handle,
            MOUSE_BUTTON_STATE_PRESSED));
        assert(viewport.group_dragging && viewport.selected_item_count == 2);
        assert(viewport_pointer_update(&history, &viewport, &project, target,
            MOUSE_BUTTON_STATE_DOWN));
        assert(!viewport_pointer_update(&history, &viewport, &project, target,
            MOUSE_BUTTON_STATE_RELEASED));
        assert(fabsf(body->position.x + 8.0f) < 0.001f);
        assert(fabsf(soft_body->position.x - 12.0f) < 0.001f);
        assert(history.undo_count == 1);
        assert(editor_history_undo(&history));
        body = editor_project_rigid_body_get(&project.objects[0], rigid.item);
        soft_body = NULL;
        for(size_t i = 0; i < project.objects[0].soft_body_count; i += 1)
            if(project.objects[0].soft_body_items[i].id == soft.item)
                soft_body = &project.objects[0].soft_body_items[i];
        assert(body != NULL && soft_body != NULL);
        editor_history_reset(&history);
        assert(editor_history_transaction_begin(&history));
        assert(editor_history_transaction_object_track(&history, rigid.object));
        assert(editor_viewport_selection_nudge(&viewport, &project,
            (Vec2D){2.0f, 0.0f}));
        assert(editor_history_transaction_end(&history));
        assert(fabsf(body->position.x + 8.0f) < 0.001f);
        assert(fabsf(soft_body->position.x - 12.0f) < 0.001f);
        assert(history.undo_count == 1);
        assert(editor_history_undo(&history));
        body = editor_project_rigid_body_get(&project.objects[0], rigid.item);
        soft_body = NULL;
        for(size_t i = 0; i < project.objects[0].soft_body_count; i += 1)
            if(project.objects[0].soft_body_items[i].id == soft.item)
                soft_body = &project.objects[0].soft_body_items[i];
        assert(body != NULL && soft_body != NULL);
        editor_history_reset(&history);
        handle = test_world_to_screen((Position){0.0f,
            EDITOR_VIEWPORT_ROTATION_ARM_LENGTH});
        target = test_world_to_screen((Position){
            EDITOR_VIEWPORT_ROTATION_ARM_LENGTH, 0.0f});
        assert(viewport_pointer_update(&history, &viewport, &project, handle,
            MOUSE_BUTTON_STATE_PRESSED));
        assert(viewport.group_rotating);
        assert(viewport_pointer_update(&history, &viewport, &project, target,
            MOUSE_BUTTON_STATE_DOWN));
        assert(!viewport_pointer_update(&history, &viewport, &project, target,
            MOUSE_BUTTON_STATE_RELEASED));
        assert(fabsf(body->position.x) < 0.001f &&
            fabsf(body->position.y - 10.0f) < 0.001f);
        assert(fabsf(soft_body->position.x) < 0.001f &&
            fabsf(soft_body->position.y + 10.0f) < 0.001f);
        assert(fabsf(body->rotation + 1.57079632679f) < 0.001f);
        assert(fabsf(soft_body->rotation + 1.57079632679f) < 0.001f);
        assert(history.undo_count == 1);
        assert(editor_history_undo(&history));
        body = editor_project_rigid_body_get(&project.objects[0], rigid.item);
        soft_body = NULL;
        for(size_t i = 0; i < project.objects[0].soft_body_count; i += 1)
            if(project.objects[0].soft_body_items[i].id == soft.item)
                soft_body = &project.objects[0].soft_body_items[i];
        assert(body != NULL && soft_body != NULL);
        assert(fabsf(body->position.x + 10.0f) < 0.001f);
        assert(fabsf(soft_body->position.x - 10.0f) < 0.001f);
    }

    {
        EditorObject *object = &project.objects[0];
        EditorRigidBody *connected_body = editor_project_rigid_body_add(
            &project, object);
        EditorAnchor *first_anchor;
        EditorAnchor *second_anchor;
        EditorJoint *joint;
        EditorSelectionRef first;
        EditorSelectionRef second;

        assert(connected_body != NULL);
        body = editor_project_rigid_body_get(object, body->id);
        assert(body != NULL);
        body->position = (Position){-10.0f, 0.0f};
        connected_body->position = (Position){10.0f, 0.0f};
        first_anchor = editor_project_anchor_add(&project, object,
            (Position){10.0f, 0.0f}, body->id);
        second_anchor = editor_project_anchor_add(&project, object,
            (Position){-10.0f, 0.0f}, connected_body->id);
        joint = editor_project_joint_add(&project, object, EDITOR_JOINT_WELD);
        assert(first_anchor != NULL && second_anchor != NULL && joint != NULL);
        assert(editor_project_joint_anchor_set(object, joint, 0, first_anchor->id));
        assert(editor_project_joint_anchor_set(object, joint, 1, second_anchor->id));
        first = (EditorSelectionRef){EDITOR_SELECTION_RIGID_BODY,
            object->id, 0, 0, body->id};
        second = (EditorSelectionRef){EDITOR_SELECTION_RIGID_BODY,
            object->id, 0, 0, connected_body->id};
        editor_viewport_state_init(&viewport);
        viewport.mode = EDITOR_VIEWPORT_OBJECT;
        assert(editor_viewport_selection_set(&project, &viewport, first, false));
        assert(editor_viewport_selection_set(&project, &viewport, second, true));
        assert(editor_viewport_selection_nudge(&viewport, &project,
            (Vec2D){2.0f, 0.0f}));
        assert(fabsf(body->position.x + 8.0f) < 0.001f);
        assert(fabsf(connected_body->position.x - 12.0f) < 0.001f);

        {
            Position shared_handle = test_world_to_screen((Position){2.0f,
                EDITOR_VIEWPORT_ROTATION_ARM_LENGTH});
            Position shared_target = test_world_to_screen((Position){
                2.0f + EDITOR_VIEWPORT_ROTATION_ARM_LENGTH, 0.0f});
            assert(editor_viewport_update(&viewport, &project, shared_handle,
                MOUSE_BUTTON_STATE_PRESSED, MOUSE_BUTTON_STATE_UP,
                false, 0.0f, false));
            assert(viewport.group_rotating);
            assert(editor_viewport_update(&viewport, &project, shared_target,
                MOUSE_BUTTON_STATE_DOWN, MOUSE_BUTTON_STATE_UP,
                false, 0.0f, false));
            assert(fabsf(body->rotation + 1.57079632679f) < 0.001f);
            assert(fabsf(connected_body->rotation + 1.57079632679f) < 0.001f);
            assert(!editor_viewport_update(&viewport, &project, shared_target,
                MOUSE_BUTTON_STATE_RELEASED, MOUSE_BUTTON_STATE_UP,
                false, 0.0f, false));
        }

        body->position = (Position){-10.0f, 0.0f};
        body->rotation = 0.0f;
        connected_body->position = (Position){10.0f, 0.0f};
        connected_body->rotation = 0.0f;
        editor_viewport_state_init(&viewport);
        viewport.mode = EDITOR_VIEWPORT_RIGID_BODY;
        assert(editor_viewport_selection_set(&project, &viewport, first, false));
        assert(editor_viewport_selection_set(&project, &viewport, second, true));
        {
            EditorRigidBody *rotation_body = editor_project_rigid_body_get(
                object, viewport.selected_rigid_body);
            Position rotation_center;
            assert(rotation_body != NULL);
            rotation_center = rotation_body->position;
            Position own_handle = test_world_to_screen(
                (Position){rotation_center.x, rotation_center.y -
                    EDITOR_VIEWPORT_ROTATION_ARM_LENGTH});
            Position own_target = test_world_to_screen(
                (Position){rotation_center.x +
                    EDITOR_VIEWPORT_ROTATION_ARM_LENGTH, rotation_center.y});
            assert(editor_viewport_update(&viewport, &project, own_handle,
                MOUSE_BUTTON_STATE_PRESSED, MOUSE_BUTTON_STATE_UP,
                false, 0.0f, false));
            assert(viewport.rotated_body && !viewport.group_rotating);
            assert(editor_viewport_update(&viewport, &project, own_target,
                MOUSE_BUTTON_STATE_DOWN, MOUSE_BUTTON_STATE_UP,
                false, 0.0f, false));
            assert(fabsf(body->rotation - 1.57079632679f) < 0.001f);
            assert(fabsf(connected_body->rotation - 1.57079632679f) < 0.001f);
        }
    }

    {
        EditorObject *object = &project.objects[0];
        EditorSprite *sprite = editor_project_sprite_add(
            &project, object, "drag_sprite", "sprite.png");
        EditorSpriteId sprite_id;
        Position grab;
        Position moved;
        assert(sprite != NULL);
        sprite_id = sprite->id;
        sprite->position = (Position){300.0f, 150.0f};
        editor_history_reset(&history);
        editor_viewport_state_init(&viewport);
        viewport.mode = EDITOR_VIEWPORT_OBJECT;
        grab = test_world_to_screen((Position){300.0f, 150.0f});
        moved = grab;
        moved.x += 25.0f;
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_PRESSED));
        assert(viewport.dragged_sprite);
        assert(viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_DOWN));
        moved.x += 15.0f;
        assert(viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_DOWN));
        assert(!viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_RELEASED));
        assert(history.undo_count == 1);
        assert(editor_history_undo(&history));
        object = &project.objects[0];
        sprite = editor_project_sprite_get(object, sprite_id);
        assert(sprite != NULL && sprite->position.x == 300.0f);
        assert(editor_history_redo(&history));
        object = &project.objects[0];
        sprite = editor_project_sprite_get(object, sprite_id);
        assert(sprite != NULL && sprite->position.x == 340.0f);

        editor_history_reset(&history);
        editor_viewport_state_init(&viewport);
        viewport.mode = EDITOR_VIEWPORT_SPRITE;
        viewport.selection = EDITOR_SELECTION_SPRITE;
        viewport.selected_sprite = sprite_id;
        grab = test_world_to_screen((Position){sprite->position.x,
            sprite->position.y - EDITOR_VIEWPORT_ROTATION_ARM_LENGTH});
        moved = test_world_to_screen((Position){
            sprite->position.x + EDITOR_VIEWPORT_ROTATION_ARM_LENGTH,
            sprite->position.y});
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_PRESSED));
        assert(viewport.rotated_sprite);
        assert(viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_DOWN));
        assert(!viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_RELEASED));
        assert(history.undo_count == 1);
        assert(editor_history_undo(&history));
        object = &project.objects[0];
        sprite = editor_project_sprite_get(object, sprite_id);
        assert(sprite != NULL && fabsf(sprite->rotation) < 0.001f);
        assert(editor_history_redo(&history));
        object = &project.objects[0];
        sprite = editor_project_sprite_get(object, sprite_id);
        assert(sprite != NULL &&
            fabsf(sprite->rotation - 1.57079632679f) < 0.001f);
        assert(editor_project_sprite_remove(object, sprite_id));
    }

    {
        EditorObject *object = &project.objects[0];
        EditorRigidBodyId body_id = body->id;
        Position grab;
        Position moved;
        body = editor_project_rigid_body_get(object, body_id);
        assert(body != NULL);
        body->position = (Position){100.0f, -150.0f};
        editor_history_reset(&history);
        editor_viewport_state_init(&viewport);
        viewport.mode = EDITOR_VIEWPORT_ORIGIN;
        viewport.selection = EDITOR_SELECTION_ORIGIN;
        viewport.selected_origin_kind = EDITOR_ORIGIN_RIGID_BODY;
        viewport.selected_rigid_body = body_id;
        grab = test_world_to_screen(body->position);
        moved = grab;
        moved.x += 30.0f;
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_PRESSED));
        assert(viewport.dragged_origin);
        assert(viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_DOWN));
        assert(!viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_RELEASED));
        assert(history.undo_count == 1);
        assert(editor_history_undo(&history));
        object = &project.objects[0];
        body = editor_project_rigid_body_get(object, body_id);
        assert(body != NULL && body->position.x == 100.0f);
        assert(editor_history_redo(&history));
        object = &project.objects[0];
        body = editor_project_rigid_body_get(object, body_id);
        assert(body != NULL && body->position.x == 130.0f);
    }

    {
        EditorObject *object = &project.objects[0];
        EditorSoftBodyId body_id = soft_body->id;
        Position grab;
        Position moved;
        soft_body = NULL;
        for(size_t i = 0; i < object->soft_body_count; i += 1)
            if(object->soft_body_items[i].id == body_id)
                soft_body = &object->soft_body_items[i];
        assert(soft_body != NULL);
        soft_body->position = (Position){-100.0f, -150.0f};
        editor_history_reset(&history);
        editor_viewport_state_init(&viewport);
        viewport.mode = EDITOR_VIEWPORT_ORIGIN;
        viewport.selection = EDITOR_SELECTION_ORIGIN;
        viewport.selected_origin_kind = EDITOR_ORIGIN_SOFT_BODY;
        viewport.selected_soft_body = body_id;
        grab = test_world_to_screen(soft_body->position);
        moved = grab;
        moved.x += 30.0f;
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_PRESSED));
        assert(viewport.dragged_origin);
        assert(viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_DOWN));
        assert(!viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_RELEASED));
        assert(history.undo_count == 1);
        assert(editor_history_undo(&history));
        object = &project.objects[0];
        soft_body = NULL;
        for(size_t i = 0; i < object->soft_body_count; i += 1)
            if(object->soft_body_items[i].id == body_id)
                soft_body = &object->soft_body_items[i];
        assert(soft_body != NULL && soft_body->position.x == -100.0f);
        assert(editor_history_redo(&history));
        object = &project.objects[0];
        soft_body = NULL;
        for(size_t i = 0; i < object->soft_body_count; i += 1)
            if(object->soft_body_items[i].id == body_id)
                soft_body = &object->soft_body_items[i];
        assert(soft_body != NULL && soft_body->position.x == -70.0f);
    }

    {
        EditorObject *object = &project.objects[0];
        EditorAnimatedSprite *animation =
            editor_project_animated_sprite_add(&project, object);
        EditorAnimatedSpriteId animation_id;
        Position grab;
        Position moved;
        assert(animation != NULL);
        animation_id = animation->id;
        animation->editor_position = (Position){-300.0f, 150.0f};
        assert(editor_project_animation_frame_add(&project, animation,
            "frame", "frame.png", (Scale){64.0f, 64.0f}));
        editor_history_reset(&history);
        editor_viewport_state_init(&viewport);
        viewport.mode = EDITOR_VIEWPORT_OBJECT;
        grab = test_world_to_screen((Position){-300.0f, 150.0f});
        moved = grab;
        moved.x += 40.0f;
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_PRESSED));
        assert(viewport.dragged_animated_sprite);
        assert(viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_DOWN));
        assert(!viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_RELEASED));
        assert(history.undo_count == 1);
        assert(editor_history_undo(&history));
        object = &project.objects[0];
        animation = editor_project_animated_sprite_get(object, animation_id);
        assert(animation != NULL && animation->editor_position.x == -300.0f);
        assert(editor_history_redo(&history));
        object = &project.objects[0];
        animation = editor_project_animated_sprite_get(object, animation_id);
        assert(animation != NULL && animation->editor_position.x == -260.0f);

        editor_history_reset(&history);
        editor_viewport_state_init(&viewport);
        viewport.mode = EDITOR_VIEWPORT_ANIMATED_SPRITE;
        viewport.selection = EDITOR_SELECTION_ANIMATED_SPRITE;
        viewport.selected_animated_sprite = animation_id;
        grab = test_world_to_screen((Position){animation->editor_position.x,
            animation->editor_position.y - EDITOR_VIEWPORT_ROTATION_ARM_LENGTH});
        moved = test_world_to_screen((Position){
            animation->editor_position.x + EDITOR_VIEWPORT_ROTATION_ARM_LENGTH,
            animation->editor_position.y});
        assert(viewport_pointer_update(&history, &viewport, &project, grab,
            MOUSE_BUTTON_STATE_PRESSED));
        assert(viewport.rotated_animated_sprite);
        assert(viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_DOWN));
        assert(!viewport_pointer_update(&history, &viewport, &project, moved,
            MOUSE_BUTTON_STATE_RELEASED));
        assert(history.undo_count == 1);
        assert(editor_history_undo(&history));
        object = &project.objects[0];
        animation = editor_project_animated_sprite_get(object, animation_id);
        assert(animation != NULL && fabsf(animation->editor_rotation) < 0.001f);
        assert(editor_history_redo(&history));
        object = &project.objects[0];
        animation = editor_project_animated_sprite_get(object, animation_id);
        assert(animation != NULL &&
            fabsf(animation->editor_rotation - 1.57079632679f) < 0.001f);
        assert(editor_project_animated_sprite_remove(object, animation_id));
    }

    command = (EditorCommand){.type = EDITOR_COMMAND_ITEM_REMOVE,
        .data.item_remove = {.kind = EDITOR_ITEM_OBJECT,
            .object = project.objects[0].id}};
    editor_history_command_begin(&history, &project, &command);
    result = editor_command_execute(&project, &command);
    editor_history_command_finish(&history, &command, &result);
    assert(result.kind == ERROR_RESULT_VALUE);
    assert(project.object_count == 0);
    assert(editor_history_undo(&history));
    assert(project.object_count == 1);
    assert(strcmp(project.objects[0].name, "Car") == 0);
    assert(editor_history_redo(&history));
    assert(project.object_count == 0);

    assert(editor_history_undo(&history));
    command = (EditorCommand){.type = EDITOR_COMMAND_ITEM_RENAME,
        .data.item_rename = {.kind = EDITOR_ITEM_OBJECT,
            .object = project.objects[0].id}};
    snprintf(command.data.item_rename.name, sizeof(command.data.item_rename.name),
        "Truck");
    editor_history_command_begin(&history, &project, &command);
    result = editor_command_execute(&project, &command);
    editor_history_command_finish(&history, &command, &result);
    assert(result.kind == ERROR_RESULT_VALUE);
    assert(strcmp(project.objects[0].name, "Truck") == 0);
    assert(!editor_history_redo_check(&history));

    {
        size_t undo_count = history.undo_count;
        command = (EditorCommand){.type = EDITOR_COMMAND_OBJECT_POSITION,
            .data.object_position = {.object = UINT32_MAX,
                .position = {99.0f, 99.0f}}};
        editor_history_command_begin(&history, &project, &command);
        result = editor_command_execute(&project, &command);
        editor_history_command_finish(&history, &command, &result);
        assert(result.kind == ERROR_RESULT_ERROR);
        assert(history.undo_count == undo_count);
    }

    {
        EditorObject *second = editor_project_object_add(&project, (Position){0});
        EditorObjectId first_id = project.objects[0].id;
        EditorObjectId second_id;
        assert(second != NULL);
        second_id = second->id;
        editor_history_reset(&history);
        callback_history = &history;
        editor_command_executing_callback_set(history_begin, NULL);
        editor_command_finished_callback_set(history_finish, NULL);
        assert(editor_history_transaction_begin(&history));
        command = (EditorCommand){.type = EDITOR_COMMAND_OBJECT_POSITION,
            .data.object_position = {.object = first_id,
                .position = {10.0f, 20.0f}}};
        result = editor_command_execute(&project, &command);
        assert(result.kind == ERROR_RESULT_VALUE);
        command.data.object_position.object = second_id;
        command.data.object_position.position = (Position){30.0f, 40.0f};
        result = editor_command_execute(&project, &command);
        assert(result.kind == ERROR_RESULT_VALUE);
        assert(editor_history_transaction_end(&history));
        assert(history.undo_count == 1);
        assert(editor_history_memory_get(&history) < 8192);
        assert(editor_history_undo(&history));
        assert(project.objects[0].position.x == 0.0f);
        assert(project.objects[1].position.x == 0.0f);
        assert(editor_history_redo(&history));
        assert(project.objects[0].position.x == 10.0f);
        assert(project.objects[1].position.x == 30.0f);
        editor_command_executing_callback_set(NULL, NULL);
        editor_command_finished_callback_set(NULL, NULL);
        callback_history = NULL;
    }

    {
        EditorObjectId object_id = project.objects[0].id;
        editor_history_reset(&history);
        project.objects[0].position = (Position){0};
        callback_history = &history;
        editor_command_executing_callback_set(history_begin, NULL);
        editor_command_finished_callback_set(history_finish, NULL);
        assert(editor_history_transaction_begin(&history));
        assert(editor_history_transaction_object_track(&history, object_id));
        editor_history_transaction_commands_suppress_set(&history, true);
        for(int step = 1; step <= 3; step += 1) {
            command = (EditorCommand){.type = EDITOR_COMMAND_OBJECT_POSITION,
                .data.object_position = {.object = object_id,
                    .position = {(float)step * 10.0f, (float)step * 5.0f}}};
            result = editor_command_execute(&project, &command);
            assert(result.kind == ERROR_RESULT_VALUE);
        }
        assert(editor_history_transaction_end(&history));
        assert(history.undo_count == 1);
        assert(project.objects[0].position.x == 30.0f);
        assert(editor_history_undo(&history));
        assert(project.objects[0].position.x == 0.0f);
        assert(editor_history_redo(&history));
        assert(project.objects[0].position.x == 30.0f);
        editor_command_executing_callback_set(NULL, NULL);
        editor_command_finished_callback_set(NULL, NULL);
        callback_history = NULL;
    }

    {
        EditorSpriteId sprite_id;
        editor_history_reset(&history);
        callback_history = &history;
        editor_command_executing_callback_set(history_begin, NULL);
        editor_command_finished_callback_set(history_finish, NULL);
        command = (EditorCommand){.type = EDITOR_COMMAND_SPRITE_ADD,
            .data.sprite_add = {.object = project.objects[0].id,
                .name = "wheel", .path = "assets/wheel.png",
                .size = {32.0f, 32.0f}}};
        result = editor_command_execute(&project, &command);
        assert(result.kind == ERROR_RESULT_VALUE &&
            project.objects[0].sprite_count == 1);
        sprite_id = project.objects[0].sprites[0].id;
        assert(editor_history_undo(&history));
        assert(project.objects[0].sprite_count == 0);
        assert(editor_history_redo(&history));
        assert(project.objects[0].sprite_count == 1 &&
            project.objects[0].sprites[0].id == sprite_id);

        command = (EditorCommand){.type = EDITOR_COMMAND_SPRITE_POSITION_SET,
            .data.sprite_position_set = {.object = project.objects[0].id,
                .sprite = sprite_id, .position = {14.0f, 18.0f}}};
        result = editor_command_execute(&project, &command);
        assert(result.kind == ERROR_RESULT_VALUE &&
            project.objects[0].sprites[0].position.x == 14.0f);
        assert(editor_history_undo(&history));
        assert(project.objects[0].sprites[0].position.x == 0.0f);
        assert(editor_history_redo(&history));
        assert(project.objects[0].sprites[0].position.x == 14.0f);

        command = (EditorCommand){.type = EDITOR_COMMAND_SPRITE_ROTATION_SET,
            .data.sprite_rotation_set = {.object = project.objects[0].id,
                .sprite = sprite_id, .rotation = 0.75f}};
        result = editor_command_execute(&project, &command);
        assert(result.kind == ERROR_RESULT_VALUE &&
            project.objects[0].sprites[0].rotation == 0.75f);
        assert(editor_history_undo(&history));
        assert(project.objects[0].sprites[0].rotation == 0.0f);
        assert(editor_history_redo(&history));
        assert(project.objects[0].sprites[0].rotation == 0.75f);

        command = (EditorCommand){.type = EDITOR_COMMAND_ANIMATED_SPRITE_ADD,
            .data.animated_sprite_add = {.object = project.objects[0].id,
                .name = "rolling"}};
        result = editor_command_execute(&project, &command);
        assert(result.kind == ERROR_RESULT_VALUE &&
            project.objects[0].animated_sprite_count == 1);
        assert(editor_history_undo(&history));
        assert(project.objects[0].animated_sprite_count == 0);
        assert(editor_history_redo(&history));
        assert(project.objects[0].animated_sprite_count == 1);
        command = (EditorCommand){
            .type = EDITOR_COMMAND_ANIMATED_SPRITE_ROTATION_SET,
            .data.animated_sprite_rotation_set = {
                .object = project.objects[0].id,
                .sprite = project.objects[0].animated_sprite_items[0].id,
                .rotation = -0.5f}};
        result = editor_command_execute(&project, &command);
        assert(result.kind == ERROR_RESULT_VALUE &&
            project.objects[0].animated_sprite_items[0].editor_rotation == -0.5f);
        assert(editor_history_undo(&history));
        assert(project.objects[0].animated_sprite_items[0].editor_rotation == 0.0f);
        assert(editor_history_redo(&history));
        assert(project.objects[0].animated_sprite_items[0].editor_rotation == -0.5f);
        editor_command_executing_callback_set(NULL, NULL);
        editor_command_finished_callback_set(NULL, NULL);
        callback_history = NULL;
    }

    editor_viewport_state_destroy(&viewport);
    editor_history_destroy(&history);
    return 0;
}
