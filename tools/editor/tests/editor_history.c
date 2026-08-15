#include "editor_history.h"
#include "editor_layout.h"
#include "editor_shortcuts.h"
#include "editor_viewport.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

float editor_viewport_width = 1024.0f;
float editor_window_width = 1280.0f;
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
    EditorViewportState viewport;

    editor_project_init(&project);
    assert(editor_history_init(&history, &project));
    snprintf(command.data.item_add.name, sizeof(command.data.item_add.name), "Car");
    editor_history_command_begin(&history, &project, &command);
    result = editor_command_execute(&project, &command);
    editor_history_command_finish(&history, &command, &result);
    assert(result.kind == ERROR_RESULT_VALUE);
    assert(project.object_count == 1);
    assert(editor_history_memory_get(&history) < sizeof(project));
    assert(editor_history_undo(&history));
    assert(project.object_count == 0);
    assert(editor_history_redo(&history));
    assert(project.object_count == 1);
    assert(strcmp(project.objects[0].name, "Car") == 0);

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
        editor_history_continuous_set(&history, true);
        assert(editor_viewport_update(&viewport, &project, grab,
            MOUSE_BUTTON_STATE_PRESSED, MOUSE_BUTTON_STATE_UP, false, 0.0f, false));
        assert(history.undo_count == 0);
        assert(editor_viewport_update(&viewport, &project, grab,
            MOUSE_BUTTON_STATE_DOWN, MOUSE_BUTTON_STATE_UP, false, 0.0f, false));
        assert(history.undo_count == 0);
        grab.x += 20.0f;
        assert(editor_viewport_update(&viewport, &project, grab,
            MOUSE_BUTTON_STATE_DOWN, MOUSE_BUTTON_STATE_UP, false, 0.0f, false));
        editor_history_continuous_set(&history, false);
        assert(history.undo_count == 1);
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
        editor_history_continuous_set(&history, true);
        assert(editor_viewport_update(&viewport, &project, grab,
            MOUSE_BUTTON_STATE_PRESSED, MOUSE_BUTTON_STATE_UP, false, 0.0f, false));
        assert(editor_viewport_update(&viewport, &project, grab,
            MOUSE_BUTTON_STATE_DOWN, MOUSE_BUTTON_STATE_UP, false, 0.0f, false));
        assert(history.undo_count == 0);
        grab.x += 20.0f;
        assert(editor_viewport_update(&viewport, &project, grab,
            MOUSE_BUTTON_STATE_DOWN, MOUSE_BUTTON_STATE_UP, false, 0.0f, false));
        editor_history_continuous_set(&history, false);
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
        editor_history_continuous_set(&history, true);
        assert(editor_viewport_update(&viewport, &project, grab,
            MOUSE_BUTTON_STATE_PRESSED, MOUSE_BUTTON_STATE_UP, false, 0.0f, false));
        assert(editor_viewport_update(&viewport, &project, grab,
            MOUSE_BUTTON_STATE_DOWN, MOUSE_BUTTON_STATE_UP, false, 0.0f, false));
        assert(history.undo_count == 0);
        grab.x += 20.0f;
        assert(editor_viewport_update(&viewport, &project, grab,
            MOUSE_BUTTON_STATE_DOWN, MOUSE_BUTTON_STATE_UP, false, 0.0f, false));
        editor_history_continuous_set(&history, false);
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
    editor_command_executing_callback_set(NULL, NULL);
    editor_command_finished_callback_set(NULL, NULL);
    callback_history = NULL;

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

    editor_history_destroy(&history);
    return 0;
}
