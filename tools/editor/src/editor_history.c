#include "editor_history.h"

#include <stdlib.h>
#include <string.h>

typedef struct EditorHistoryChange {
    size_t offset;
    size_t size;
    unsigned char *before;
    unsigned char *after;
} EditorHistoryChange;

struct EditorHistoryEntry {
    EditorHistoryChange *changes;
    size_t change_count;
    size_t memory;
};

#define EDITOR_HISTORY_BLOCK_SIZE 64

static size_t editor_history_block_size_get(size_t offset) {
    size_t remaining = sizeof(EditorProject) - offset;
    return remaining < EDITOR_HISTORY_BLOCK_SIZE ? remaining :
        EDITOR_HISTORY_BLOCK_SIZE;
}

static bool editor_history_block_changed(const unsigned char *before,
        const unsigned char *after, size_t offset) {
    return memcmp(&before[offset], &after[offset],
        editor_history_block_size_get(offset)) != 0;
}

static void editor_history_entry_destroy(EditorHistoryEntry *entry) {
    if(entry == NULL) return;
    for(size_t i = 0; i < entry->change_count; i += 1) {
        free(entry->changes[i].before);
        free(entry->changes[i].after);
    }
    free(entry->changes);
    free(entry);
}

static void editor_history_stack_clear(EditorHistoryEntry **items, size_t *count) {
    if(items == NULL || count == NULL) return;
    while(*count > 0) {
        *count -= 1;
        editor_history_entry_destroy(items[*count]);
        items[*count] = NULL;
    }
}

static bool editor_history_stack_push(EditorHistoryEntry **items, size_t *count,
        EditorHistoryEntry *entry) {
    if(items == NULL || count == NULL || entry == NULL) return false;
    if(*count == EDITOR_HISTORY_CAPACITY) {
        editor_history_entry_destroy(items[0]);
        memmove(&items[0], &items[1],
            (EDITOR_HISTORY_CAPACITY - 1) * sizeof(items[0]));
        *count -= 1;
    }
    items[*count] = entry;
    *count += 1;
    return true;
}

static EditorHistoryEntry *editor_history_entry_create(
        const EditorProject *before, const EditorProject *after) {
    const unsigned char *before_bytes = (const unsigned char *)before;
    const unsigned char *after_bytes = (const unsigned char *)after;
    EditorHistoryEntry *entry;
    size_t change_count = 0;
    size_t index = 0;
    if(before == NULL || after == NULL) return NULL;
    while(index < sizeof(*before)) {
        if(!editor_history_block_changed(before_bytes, after_bytes, index)) {
            index += editor_history_block_size_get(index);
            continue;
        }
        change_count += 1;
        while(index < sizeof(*before) &&
                editor_history_block_changed(before_bytes, after_bytes, index))
            index += editor_history_block_size_get(index);
    }
    if(change_count == 0) return NULL;
    entry = calloc(1, sizeof(*entry));
    if(entry == NULL) return NULL;
    entry->changes = calloc(change_count, sizeof(*entry->changes));
    if(entry->changes == NULL) {
        free(entry);
        return NULL;
    }
    entry->change_count = change_count;
    entry->memory = sizeof(*entry) + change_count * sizeof(*entry->changes);
    index = 0;
    change_count = 0;
    while(index < sizeof(*before)) {
        size_t begin;
        EditorHistoryChange *change;
        if(!editor_history_block_changed(before_bytes, after_bytes, index)) {
            index += editor_history_block_size_get(index);
            continue;
        }
        begin = index;
        while(index < sizeof(*before) &&
                editor_history_block_changed(before_bytes, after_bytes, index))
            index += editor_history_block_size_get(index);
        change = &entry->changes[change_count];
        change->offset = begin;
        change->size = index - begin;
        change->before = malloc(change->size);
        change->after = malloc(change->size);
        if(change->before == NULL || change->after == NULL) {
            editor_history_entry_destroy(entry);
            return NULL;
        }
        memcpy(change->before, &before_bytes[begin], change->size);
        memcpy(change->after, &after_bytes[begin], change->size);
        entry->memory += change->size * 2;
        change_count += 1;
    }
    return entry;
}

static void editor_history_entry_apply(EditorProject *project,
        const EditorHistoryEntry *entry, bool forward) {
    unsigned char *bytes = (unsigned char *)project;
    if(project == NULL || entry == NULL) return;
    for(size_t i = 0; i < entry->change_count; i += 1) {
        const EditorHistoryChange *change = &entry->changes[i];
        memcpy(&bytes[change->offset], forward ? change->after : change->before,
            change->size);
    }
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
    history->project = project;
    return true;
}

void editor_history_destroy(EditorHistory *history) {
    if(history == NULL) return;
    editor_history_stack_clear(history->undo, &history->undo_count);
    editor_history_stack_clear(history->redo, &history->redo_count);
    free(history->pending);
    free(history->transaction_before);
    memset(history, 0, sizeof(*history));
}

void editor_history_reset(EditorHistory *history) {
    if(history == NULL || history->project == NULL) return;
    editor_history_stack_clear(history->undo, &history->undo_count);
    editor_history_stack_clear(history->redo, &history->redo_count);
    free(history->pending);
    history->pending = NULL;
    free(history->transaction_before);
    history->transaction_before = NULL;
    history->transaction_active = false;
    history->continuous = false;
    history->continuous_recorded = false;
    history->recorded_since_continuous_update = false;
}

void editor_history_command_begin(EditorHistory *history,
        const EditorProject *project, const EditorCommand *command) {
    if(history == NULL) return;
    if(history->transaction_active) return;
    free(history->pending);
    history->pending = NULL;
    if(project == NULL || !editor_history_command_record_check(command)) return;
    history->pending = malloc(sizeof(*history->pending));
    if(history->pending != NULL)
        memcpy(history->pending, project, sizeof(*project));
}

void editor_history_command_finish(EditorHistory *history,
        const EditorCommand *command, const EditorCommandResult *result) {
    EditorHistoryEntry *entry = NULL;
    if(history == NULL || history->project == NULL) return;
    if(history->transaction_active) return;
    if(history->pending != NULL && command != NULL && result != NULL &&
            result->kind == ERROR_RESULT_VALUE) {
        if(history->continuous && history->continuous_recorded &&
                history->undo_count > 0) {
            EditorHistoryEntry *previous = history->undo[history->undo_count - 1];
            editor_history_entry_apply(history->pending, previous, false);
            entry = editor_history_entry_create(history->pending, history->project);
            if(entry != NULL) {
                history->undo[history->undo_count - 1] = entry;
                editor_history_entry_destroy(previous);
            }
        } else {
            entry = editor_history_entry_create(history->pending, history->project);
            if(entry != NULL && editor_history_stack_push(history->undo,
                    &history->undo_count, entry)) {
                editor_history_stack_clear(history->redo, &history->redo_count);
                history->recorded_since_continuous_update = true;
                if(history->continuous) history->continuous_recorded = true;
            } else if(entry != NULL) {
                editor_history_entry_destroy(entry);
            }
        }
    }
    free(history->pending);
    history->pending = NULL;
}

void editor_history_continuous_set(EditorHistory *history, bool continuous) {
    if(history == NULL) return;
    if(!history->continuous && continuous &&
            history->recorded_since_continuous_update)
        history->continuous_recorded = true;
    if(history->continuous && !continuous) history->continuous_recorded = false;
    history->continuous = continuous;
    history->recorded_since_continuous_update = false;
}

bool editor_history_transaction_begin(EditorHistory *history) {
    if(history == NULL || history->project == NULL || history->transaction_active)
        return false;
    history->transaction_before = malloc(sizeof(*history->transaction_before));
    if(history->transaction_before == NULL) return false;
    memcpy(history->transaction_before, history->project,
        sizeof(*history->transaction_before));
    history->transaction_active = true;
    return true;
}

bool editor_history_transaction_end(EditorHistory *history) {
    EditorHistoryEntry *entry;
    if(history == NULL || history->project == NULL ||
            !history->transaction_active || history->transaction_before == NULL)
        return false;
    entry = editor_history_entry_create(history->transaction_before, history->project);
    free(history->transaction_before);
    history->transaction_before = NULL;
    history->transaction_active = false;
    if(entry == NULL) return true;
    if(!editor_history_stack_push(history->undo, &history->undo_count, entry)) {
        editor_history_entry_destroy(entry);
        return false;
    }
    editor_history_stack_clear(history->redo, &history->redo_count);
    history->continuous = false;
    history->continuous_recorded = false;
    history->recorded_since_continuous_update = false;
    return true;
}

void editor_history_transaction_cancel(EditorHistory *history) {
    if(history == NULL) return;
    if(history->project != NULL && history->transaction_before != NULL)
        memcpy(history->project, history->transaction_before,
            sizeof(*history->project));
    free(history->transaction_before);
    history->transaction_before = NULL;
    history->transaction_active = false;
}

static bool editor_history_restore(EditorHistory *history,
        EditorHistoryEntry **from, size_t *from_count,
        EditorHistoryEntry **to, size_t *to_count, bool forward) {
    EditorHistoryEntry *entry;
    if(history == NULL || history->project == NULL ||
            from == NULL || from_count == NULL || *from_count == 0) return false;
    *from_count -= 1;
    entry = from[*from_count];
    from[*from_count] = NULL;
    editor_history_entry_apply(history->project, entry, forward);
    if(!editor_history_stack_push(to, to_count, entry)) {
        editor_history_entry_destroy(entry);
        return false;
    }
    history->continuous = false;
    history->continuous_recorded = false;
    history->recorded_since_continuous_update = false;
    return true;
}

bool editor_history_undo(EditorHistory *history) {
    return editor_history_restore(history, history != NULL ? history->undo : NULL,
        history != NULL ? &history->undo_count : NULL,
        history != NULL ? history->redo : NULL,
        history != NULL ? &history->redo_count : NULL, false);
}

bool editor_history_redo(EditorHistory *history) {
    return editor_history_restore(history, history != NULL ? history->redo : NULL,
        history != NULL ? &history->redo_count : NULL,
        history != NULL ? history->undo : NULL,
        history != NULL ? &history->undo_count : NULL, true);
}

bool editor_history_undo_check(const EditorHistory *history) {
    return history != NULL && history->undo_count > 0;
}

bool editor_history_redo_check(const EditorHistory *history) {
    return history != NULL && history->redo_count > 0;
}

size_t editor_history_memory_get(const EditorHistory *history) {
    size_t memory = 0;
    if(history == NULL) return 0;
    for(size_t i = 0; i < history->undo_count; i += 1)
        memory += history->undo[i]->memory;
    for(size_t i = 0; i < history->redo_count; i += 1)
        memory += history->redo[i]->memory;
    return memory;
}
