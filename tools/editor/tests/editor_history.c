#include "editor_history.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    static EditorProject project;
    EditorHistory history;
    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
        .data.item_add = {.kind = EDITOR_ITEM_OBJECT}};
    EditorCommandResult result;

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

    editor_history_reset(&history);
    editor_history_continuous_set(&history, true);
    command = (EditorCommand){.type = EDITOR_COMMAND_OBJECT_POSITION,
        .data.object_position = {.object = project.objects[0].id,
            .position = {10.0f, 0.0f}}};
    editor_history_command_begin(&history, &project, &command);
    result = editor_command_execute(&project, &command);
    editor_history_command_finish(&history, &command, &result);
    assert(result.kind == ERROR_RESULT_VALUE);
    command.data.object_position.position.x = 20.0f;
    editor_history_command_begin(&history, &project, &command);
    result = editor_command_execute(&project, &command);
    editor_history_command_finish(&history, &command, &result);
    assert(result.kind == ERROR_RESULT_VALUE);
    editor_history_continuous_set(&history, false);
    assert(editor_history_undo(&history));
    assert(project.objects[0].position.x == 0.0f);
    assert(!editor_history_undo_check(&history));

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
