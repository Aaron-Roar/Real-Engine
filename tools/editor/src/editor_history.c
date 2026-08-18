#include "editor_history.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct EditorHistoryChange {
    size_t offset;
    size_t size;
    unsigned char *before;
    unsigned char *after;
} EditorHistoryChange;

typedef enum EditorHistoryEntryKind {
    EDITOR_HISTORY_ENTRY_CHANGES,
    EDITOR_HISTORY_ENTRY_COMMANDS
} EditorHistoryEntryKind;

typedef struct EditorHistoryCommandPair {
    EditorCommand forward;
    EditorCommand inverse;
} EditorHistoryCommandPair;

struct EditorHistoryEntry {
    EditorHistoryEntryKind kind;
    EditorHistoryChange *changes;
    size_t change_count;
    size_t memory;
    EditorHistoryCommandPair *commands;
    size_t command_count;
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
    if(entry->kind == EDITOR_HISTORY_ENTRY_CHANGES) {
        for(size_t i = 0; i < entry->change_count; i += 1) {
            free(entry->changes[i].before);
            free(entry->changes[i].after);
        }
        free(entry->changes);
    } else free(entry->commands);
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
    entry->kind = EDITOR_HISTORY_ENTRY_CHANGES;
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
        const EditorHistoryEntry *entry, bool forward, EditorHistory *history) {
    unsigned char *bytes = (unsigned char *)project;
    if(project == NULL || entry == NULL) return;
    if(entry->kind == EDITOR_HISTORY_ENTRY_COMMANDS) {
        if(history != NULL) history->restoring = true;
        if(forward) {
            for(size_t i = 0; i < entry->command_count; i += 1)
                (void)editor_command_execute(project, &entry->commands[i].forward);
        } else {
            for(size_t i = entry->command_count; i > 0; i -= 1)
                (void)editor_command_execute(project, &entry->commands[i - 1].inverse);
        }
        if(history != NULL) history->restoring = false;
        return;
    }
    for(size_t i = 0; i < entry->change_count; i += 1) {
        const EditorHistoryChange *change = &entry->changes[i];
        memcpy(&bytes[change->offset], forward ? change->after : change->before,
            change->size);
    }
}

static EditorHistoryEntry *editor_history_command_entry_create(
        const EditorCommand *forward, const EditorCommand *inverse) {
    EditorHistoryEntry *entry;
    if(forward == NULL || inverse == NULL) return NULL;
    entry = calloc(1, sizeof(*entry));
    if(entry == NULL) return NULL;
    entry->commands = malloc(sizeof(*entry->commands));
    if(entry->commands == NULL) {
        free(entry);
        return NULL;
    }
    entry->kind = EDITOR_HISTORY_ENTRY_COMMANDS;
    entry->commands[0] = (EditorHistoryCommandPair){*forward, *inverse};
    entry->command_count = 1;
    entry->memory = sizeof(*entry) + sizeof(*entry->commands);
    return entry;
}

static bool editor_history_command_value_equal(const EditorCommand *first,
        const EditorCommand *second) {
    if(first == NULL || second == NULL || first->type != second->type) return false;
    switch(first->type) {
        case EDITOR_COMMAND_OBJECT_POSITION:
            return fabsf(first->data.object_position.position.x -
                    second->data.object_position.position.x) <= 0.0001f &&
                fabsf(first->data.object_position.position.y -
                    second->data.object_position.position.y) <= 0.0001f;
        case EDITOR_COMMAND_RIGID_BODY_TRANSFORM:
            return fabsf(first->data.rigid_body_transform.position.x -
                    second->data.rigid_body_transform.position.x) <= 0.0001f &&
                fabsf(first->data.rigid_body_transform.position.y -
                    second->data.rigid_body_transform.position.y) <= 0.0001f &&
                fabsf(first->data.rigid_body_transform.rotation -
                    second->data.rigid_body_transform.rotation) <= 0.0001f;
        case EDITOR_COMMAND_VERTEX_POSITION:
            return fabsf(first->data.vertex_position.position.x -
                    second->data.vertex_position.position.x) <= 0.0001f &&
                fabsf(first->data.vertex_position.position.y -
                    second->data.vertex_position.position.y) <= 0.0001f;
        case EDITOR_COMMAND_ANCHOR_TRANSFORM:
            return fabsf(first->data.anchor_transform.position.x -
                    second->data.anchor_transform.position.x) <= 0.0001f &&
                fabsf(first->data.anchor_transform.position.y -
                    second->data.anchor_transform.position.y) <= 0.0001f &&
                fabsf(first->data.anchor_transform.rotation -
                    second->data.anchor_transform.rotation) <= 0.0001f;
        case EDITOR_COMMAND_SOFT_BODY_TRANSFORM:
            return fabsf(first->data.soft_body_transform.position.x -
                    second->data.soft_body_transform.position.x) <= 0.0001f &&
                fabsf(first->data.soft_body_transform.position.y -
                    second->data.soft_body_transform.position.y) <= 0.0001f &&
                fabsf(first->data.soft_body_transform.rotation -
                    second->data.soft_body_transform.rotation) <= 0.0001f;
        case EDITOR_COMMAND_SOFT_NODE_POSITION:
            return fabsf(first->data.soft_node_position.position.x -
                    second->data.soft_node_position.position.x) <= 0.0001f &&
                fabsf(first->data.soft_node_position.position.y -
                    second->data.soft_node_position.position.y) <= 0.0001f;
        default: return memcmp(first, second, sizeof(*first)) == 0;
    }
}

static EditorSoftBody *editor_history_soft_body_get(EditorObject *object,
        EditorSoftBodyId id) {
    if(object == NULL) return NULL;
    for(size_t i = 0; i < object->soft_body_count; i += 1)
        if(object->soft_body_items[i].id == id) return &object->soft_body_items[i];
    return NULL;
}

static bool editor_history_inverse_get(EditorProject *project,
        const EditorCommand *command, EditorCommand *inverse) {
    EditorObject *object;
    if(project == NULL || command == NULL || inverse == NULL) return false;
    *inverse = *command;
    switch(command->type) {
        case EDITOR_COMMAND_OBJECT_POSITION:
            object = editor_object_query_get(project, command->data.object_position.object);
            if(object == NULL) return false;
            inverse->data.object_position.position = object->position;
            return true;
        case EDITOR_COMMAND_RIGID_BODY_TRANSFORM: {
            EditorRigidBody *body;
            object = editor_object_query_get(project,
                command->data.rigid_body_transform.object);
            body = editor_project_rigid_body_get(object,
                command->data.rigid_body_transform.body);
            if(body == NULL) return false;
            inverse->data.rigid_body_transform.position = body->position;
            inverse->data.rigid_body_transform.rotation = body->rotation;
            return true;
        }
        case EDITOR_COMMAND_VERTEX_POSITION: {
            EditorRigidBody *body;
            EditorHitbox *hitbox;
            object = editor_object_query_get(project,
                command->data.vertex_position.object);
            body = editor_project_rigid_body_get(object,
                command->data.vertex_position.body);
            hitbox = editor_project_hitbox_get(body,
                command->data.vertex_position.hitbox);
            if(hitbox == NULL) return false;
            for(size_t i = 0; i < hitbox->vertex_count; i += 1)
                if(hitbox->vertices[i].id == command->data.vertex_position.vertex) {
                    inverse->data.vertex_position.position = hitbox->vertices[i].position;
                    return true;
                }
            return false;
        }
        case EDITOR_COMMAND_ANCHOR_TRANSFORM: {
            EditorAnchor *anchor;
            object = editor_object_query_get(project,
                command->data.anchor_transform.object);
            anchor = editor_project_anchor_get(object,
                command->data.anchor_transform.anchor);
            if(anchor == NULL) return false;
            inverse->data.anchor_transform.position = anchor->position;
            inverse->data.anchor_transform.rotation = anchor->rotation;
            return true;
        }
        case EDITOR_COMMAND_SOFT_BODY_TRANSFORM: {
            EditorSoftBody *body;
            object = editor_object_query_get(project,
                command->data.soft_body_transform.object);
            body = editor_history_soft_body_get(object,
                command->data.soft_body_transform.body);
            if(body == NULL) return false;
            inverse->data.soft_body_transform.position = body->position;
            inverse->data.soft_body_transform.rotation = body->rotation;
            return true;
        }
        case EDITOR_COMMAND_SOFT_NODE_POSITION: {
            EditorSoftBody *body;
            object = editor_object_query_get(project,
                command->data.soft_node_position.object);
            body = editor_history_soft_body_get(object,
                command->data.soft_node_position.body);
            if(body == NULL) return false;
            for(size_t i = 0; i < body->node_count; i += 1)
                if(body->nodes[i].id == command->data.soft_node_position.node) {
                    inverse->data.soft_node_position.position = body->nodes[i].position;
                    return true;
                }
            return false;
        }
        case EDITOR_COMMAND_VISIBILITY: {
            inverse->data.visibility.visible = !command->data.visibility.visible;
            return true;
        }
        case EDITOR_COMMAND_VIEWPORT_CAMERA:
            inverse->data.viewport_camera.offset = project->viewport_camera_offset;
            inverse->data.viewport_camera.zoom = project->viewport_camera_zoom;
            return true;
        case EDITOR_COMMAND_VIEWPORT_COORDINATES:
            inverse->data.viewport_coordinates.local = project->viewport_local_view;
            return true;
        default: return false;
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
    history->pending_command_valid = false;
    free(history->transaction_before);
    history->transaction_before = NULL;
    history->transaction_active = false;
    history->continuous = false;
    history->continuous_recorded = false;
    history->recorded_since_continuous_update = false;
    history->restoring = false;
}

void editor_history_command_begin(EditorHistory *history,
        const EditorProject *project, const EditorCommand *command) {
    if(history == NULL || history->restoring) return;
    if(history->transaction_active) return;
    free(history->pending);
    history->pending = NULL;
    history->pending_command_valid = false;
    if(project == NULL || !editor_history_command_record_check(command)) return;
    if(editor_history_inverse_get(history->project, command,
            &history->pending_inverse)) {
        history->pending_forward = *command;
        history->pending_command_valid = true;
        return;
    }
    history->pending = malloc(sizeof(*history->pending));
    if(history->pending != NULL)
        memcpy(history->pending, project, sizeof(*project));
}

void editor_history_command_finish(EditorHistory *history,
        const EditorCommand *command, const EditorCommandResult *result) {
    EditorHistoryEntry *entry = NULL;
    if(history == NULL || history->project == NULL || history->restoring) return;
    if(history->transaction_active) return;
    if(history->pending_command_valid && command != NULL && result != NULL &&
            result->kind == ERROR_RESULT_VALUE) {
        if(editor_history_command_value_equal(command, &history->pending_inverse))
            goto finish;
        if(history->continuous && history->continuous_recorded &&
                history->undo_count > 0 &&
                history->undo[history->undo_count - 1]->kind ==
                    EDITOR_HISTORY_ENTRY_COMMANDS) {
            history->undo[history->undo_count - 1]->commands[
                history->undo[history->undo_count - 1]->command_count - 1].forward = *command;
        } else {
            entry = editor_history_command_entry_create(command,
                &history->pending_inverse);
            if(entry != NULL && editor_history_stack_push(history->undo,
                    &history->undo_count, entry)) {
                editor_history_stack_clear(history->redo, &history->redo_count);
                history->recorded_since_continuous_update = true;
                if(history->continuous) history->continuous_recorded = true;
            } else if(entry != NULL) editor_history_entry_destroy(entry);
        }
    } else if(history->pending != NULL && command != NULL && result != NULL &&
            result->kind == ERROR_RESULT_VALUE) {
        if(history->continuous && history->continuous_recorded &&
                history->undo_count > 0) {
            EditorHistoryEntry *previous = history->undo[history->undo_count - 1];
            editor_history_entry_apply(history->pending, previous, false, history);
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
finish:
    free(history->pending);
    history->pending = NULL;
    history->pending_command_valid = false;
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
    editor_history_entry_apply(history->project, entry, forward, history);
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
