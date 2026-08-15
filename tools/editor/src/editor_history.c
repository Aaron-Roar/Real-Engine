#include "editor_history.h"

#include <stdlib.h>
#include <string.h>

static void editor_history_stack_clear(EditorProject **items, size_t *count) {
    if(items == NULL || count == NULL) return;
    while(*count > 0) {
        *count -= 1;
        free(items[*count]);
        items[*count] = NULL;
    }
}

static bool editor_history_stack_push(EditorProject **items, size_t *count,
        const EditorProject *project) {
    EditorProject *snapshot;
    if(items == NULL || count == NULL || project == NULL) return false;
    snapshot = malloc(sizeof(*snapshot));
    if(snapshot == NULL) return false;
    memcpy(snapshot, project, sizeof(*snapshot));
    if(*count == EDITOR_HISTORY_CAPACITY) {
        free(items[0]);
        memmove(&items[0], &items[1],
            (EDITOR_HISTORY_CAPACITY - 1) * sizeof(items[0]));
        *count -= 1;
    }
    items[*count] = snapshot;
    *count += 1;
    return true;
}

static bool editor_history_command_record_check(const EditorCommand *command) {
    if(command == NULL) return false;
    return command->type != EDITOR_COMMAND_NAVIGATION_SET &&
        command->type != EDITOR_COMMAND_VIEWPORT_CAMERA &&
        command->type != EDITOR_COMMAND_VIEWPORT_COORDINATES;
}

bool editor_history_init(EditorHistory *history, EditorProject *project) {
    if(history == NULL || project == NULL) return false;
    memset(history, 0, sizeof(*history));
    history->shadow = malloc(sizeof(*history->shadow));
    if(history->shadow == NULL) return false;
    history->project = project;
    memcpy(history->shadow, project, sizeof(*project));
    return true;
}

void editor_history_destroy(EditorHistory *history) {
    if(history == NULL) return;
    editor_history_stack_clear(history->undo, &history->undo_count);
    editor_history_stack_clear(history->redo, &history->redo_count);
    free(history->shadow);
    memset(history, 0, sizeof(*history));
}

void editor_history_reset(EditorHistory *history) {
    if(history == NULL || history->project == NULL || history->shadow == NULL) return;
    editor_history_stack_clear(history->undo, &history->undo_count);
    editor_history_stack_clear(history->redo, &history->redo_count);
    memcpy(history->shadow, history->project, sizeof(*history->project));
    history->continuous = false;
    history->continuous_recorded = false;
}

void editor_history_command_record(EditorHistory *history,
        const EditorCommand *command) {
    bool record;
    if(history == NULL || history->project == NULL || history->shadow == NULL) return;
    record = editor_history_command_record_check(command);
    if(record && (!history->continuous || !history->continuous_recorded)) {
        if(editor_history_stack_push(history->undo, &history->undo_count,
                history->shadow)) {
            editor_history_stack_clear(history->redo, &history->redo_count);
            if(history->continuous) history->continuous_recorded = true;
        }
    }
    memcpy(history->shadow, history->project, sizeof(*history->project));
}

void editor_history_continuous_set(EditorHistory *history, bool continuous) {
    if(history == NULL) return;
    if(history->continuous && !continuous) history->continuous_recorded = false;
    history->continuous = continuous;
}

static bool editor_history_restore(EditorHistory *history,
        EditorProject **from, size_t *from_count,
        EditorProject **to, size_t *to_count) {
    EditorProject *snapshot;
    EditorNavigationState navigation;
    Vec2D camera_offset;
    float camera_zoom;
    bool local_view;
    if(history == NULL || history->project == NULL || history->shadow == NULL ||
            from == NULL || from_count == NULL || *from_count == 0) return false;
    if(!editor_history_stack_push(to, to_count, history->project)) return false;
    *from_count -= 1;
    snapshot = from[*from_count];
    from[*from_count] = NULL;
    navigation = history->project->navigation;
    camera_offset = history->project->viewport_camera_offset;
    camera_zoom = history->project->viewport_camera_zoom;
    local_view = history->project->viewport_local_view;
    memcpy(history->project, snapshot, sizeof(*history->project));
    history->project->navigation = navigation;
    history->project->selected = navigation.object;
    history->project->viewport_camera_offset = camera_offset;
    history->project->viewport_camera_zoom = camera_zoom;
    history->project->viewport_local_view = local_view;
    memcpy(history->shadow, history->project, sizeof(*history->project));
    free(snapshot);
    history->continuous = false;
    history->continuous_recorded = false;
    return true;
}

bool editor_history_undo(EditorHistory *history) {
    return editor_history_restore(history, history != NULL ? history->undo : NULL,
        history != NULL ? &history->undo_count : NULL,
        history != NULL ? history->redo : NULL,
        history != NULL ? &history->redo_count : NULL);
}

bool editor_history_redo(EditorHistory *history) {
    return editor_history_restore(history, history != NULL ? history->redo : NULL,
        history != NULL ? &history->redo_count : NULL,
        history != NULL ? history->undo : NULL,
        history != NULL ? &history->undo_count : NULL);
}

bool editor_history_undo_check(const EditorHistory *history) {
    return history != NULL && history->undo_count > 0;
}

bool editor_history_redo_check(const EditorHistory *history) {
    return history != NULL && history->redo_count > 0;
}
