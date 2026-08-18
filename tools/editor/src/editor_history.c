#include "editor_history.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
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

typedef enum EditorHistoryActionKind {
    EDITOR_HISTORY_ACTION_COMMAND,
    EDITOR_HISTORY_ACTION_OBJECT,
    EDITOR_HISTORY_ACTION_AGGREGATE
} EditorHistoryActionKind;

typedef enum EditorHistoryAggregateKind {
    EDITOR_HISTORY_AGGREGATE_OBJECT,
    EDITOR_HISTORY_AGGREGATE_RIGID_BODY,
    EDITOR_HISTORY_AGGREGATE_SOFT_BODY
} EditorHistoryAggregateKind;

struct EditorHistoryAggregateChange {
    EditorHistoryAggregateKind kind;
    EditorObjectId object;
    uint32_t item;
    void *value;
    size_t size;
};

struct EditorHistoryObjectChange {
    bool present;
    size_t index;
    EditorObject value;
    EditorObjectId selected;
};

typedef struct EditorHistoryAction {
    EditorHistoryActionKind kind;
    union {
        EditorCommand command;
        EditorHistoryObjectChange *object;
        EditorHistoryAggregateChange *aggregate;
    } data;
} EditorHistoryAction;

typedef struct EditorHistoryCommandPair {
    EditorHistoryAction forward;
    EditorHistoryAction inverse;
} EditorHistoryCommandPair;

struct EditorHistoryEntry {
    EditorHistoryEntryKind kind;
    EditorHistoryChange *changes;
    size_t change_count;
    size_t memory;
    EditorHistoryCommandPair *commands;
    size_t command_count;
};

static EditorSoftBody *editor_history_soft_body_get(EditorObject *object,
    EditorSoftBodyId id);
static void editor_history_entry_destroy(EditorHistoryEntry *entry);

#define EDITOR_HISTORY_BLOCK_SIZE 64

static size_t editor_history_block_size_get(size_t offset) {
    size_t remaining = sizeof(EditorProject) - offset;
    return remaining < EDITOR_HISTORY_BLOCK_SIZE ? remaining :
        EDITOR_HISTORY_BLOCK_SIZE;
}

static void editor_history_aggregate_destroy(EditorHistoryAggregateChange *change) {
    if(change == NULL) return;
    free(change->value);
    free(change);
}

static EditorHistoryAggregateChange *editor_history_aggregate_capture(
        EditorProject *project, EditorItemKind kind, EditorObjectId object_id,
        uint32_t parent) {
    EditorHistoryAggregateChange *change;
    EditorObject *object = editor_object_query_get(project, object_id);
    const void *value = NULL;
    size_t size = 0;
    EditorHistoryAggregateKind aggregate_kind = EDITOR_HISTORY_AGGREGATE_OBJECT;
    uint32_t item = 0;
    if(object == NULL) return NULL;
    if(kind == EDITOR_ITEM_HITBOX || kind == EDITOR_ITEM_VERTEX ||
            kind == EDITOR_ITEM_LINE) {
        EditorRigidBody *body = editor_project_rigid_body_get(object, parent);
        if(body == NULL) return NULL;
        aggregate_kind = EDITOR_HISTORY_AGGREGATE_RIGID_BODY;
        item = parent;
        value = body;
        size = sizeof(*body);
    } else if(kind == EDITOR_ITEM_SOFT_NODE || kind == EDITOR_ITEM_SOFT_BEAM ||
            kind == EDITOR_ITEM_SOFT_AREA) {
        EditorSoftBody *body = editor_history_soft_body_get(object, parent);
        if(body == NULL) return NULL;
        aggregate_kind = EDITOR_HISTORY_AGGREGATE_SOFT_BODY;
        item = parent;
        value = body;
        size = sizeof(*body);
    } else {
        value = object;
        size = sizeof(*object);
    }
    change = calloc(1, sizeof(*change));
    if(change == NULL) return NULL;
    change->value = malloc(size);
    if(change->value == NULL) {
        free(change);
        return NULL;
    }
    memcpy(change->value, value, size);
    change->kind = aggregate_kind;
    change->object = object_id;
    change->item = item;
    change->size = size;
    return change;
}

static EditorHistoryAggregateChange *editor_history_aggregate_recapture(
        EditorProject *project, const EditorHistoryAggregateChange *source) {
    EditorObject *object;
    const void *value = NULL;
    EditorHistoryAggregateChange *change;
    if(project == NULL || source == NULL) return NULL;
    object = editor_object_query_get(project, source->object);
    if(source->kind == EDITOR_HISTORY_AGGREGATE_OBJECT) value = object;
    else if(source->kind == EDITOR_HISTORY_AGGREGATE_RIGID_BODY)
        value = editor_project_rigid_body_get(object, source->item);
    else value = editor_history_soft_body_get(object, source->item);
    if(value == NULL) return NULL;
    change = calloc(1, sizeof(*change));
    if(change == NULL) return NULL;
    *change = *source;
    change->value = malloc(source->size);
    if(change->value == NULL) {
        free(change);
        return NULL;
    }
    memcpy(change->value, value, source->size);
    return change;
}

static EditorHistoryAggregateChange *editor_history_command_aggregate_capture(
        EditorProject *project, const EditorCommand *command) {
    EditorItemKind kind;
    EditorObjectId object;
    uint32_t parent;
    if(project == NULL || command == NULL) return NULL;
    switch(command->type) {
        case EDITOR_COMMAND_ITEM_RENAME:
            kind = command->data.item_rename.kind;
            object = command->data.item_rename.object;
            parent = command->data.item_rename.parent;
            break;
        case EDITOR_COMMAND_PROPERTY_SET:
            kind = command->data.property_set.kind;
            object = command->data.property_set.object;
            parent = command->data.property_set.parent;
            break;
        case EDITOR_COMMAND_COLLISION_FILTER_SET:
            kind = command->data.collision_filter_set.kind;
            object = command->data.collision_filter_set.object;
            parent = command->data.collision_filter_set.parent;
            break;
        case EDITOR_COMMAND_RELATIONSHIP_SET:
            object = command->data.relationship_set.object;
            if(command->data.relationship_set.kind ==
                    EDITOR_RELATIONSHIP_SOFT_BEAM_NODE) {
                kind = EDITOR_ITEM_SOFT_BEAM;
                parent = command->data.relationship_set.parent;
            } else {
                kind = EDITOR_ITEM_JOINT;
                parent = 0;
            }
            break;
        case EDITOR_COMMAND_AUTO_SHAPE:
            kind = command->data.auto_shape.kind;
            object = command->data.auto_shape.object;
            parent = command->data.auto_shape.parent;
            if(kind == EDITOR_ITEM_SOFT_BODY) {
                kind = EDITOR_ITEM_SOFT_NODE;
                parent = command->data.auto_shape.item;
            }
            break;
        case EDITOR_COMMAND_RIGID_BODY_ORIGIN:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_RIGID_BODY,
                command->data.origin.object, command->data.origin.body);
        case EDITOR_COMMAND_SOFT_BODY_ORIGIN:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_SOFT_NODE,
                command->data.origin.object, command->data.origin.body);
        default: return NULL;
    }
    if(kind == EDITOR_ITEM_OBJECT) return NULL;
    return editor_history_aggregate_capture(project, kind, object, parent);
}

static EditorHistoryEntry *editor_history_aggregate_entry_create(
        EditorHistoryAggregateChange *forward,
        EditorHistoryAggregateChange *inverse) {
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
    entry->commands[0] = (EditorHistoryCommandPair){
        .forward = {.kind = EDITOR_HISTORY_ACTION_AGGREGATE,
            .data.aggregate = forward},
        .inverse = {.kind = EDITOR_HISTORY_ACTION_AGGREGATE,
            .data.aggregate = inverse}};
    entry->command_count = 1;
    entry->memory = sizeof(*entry) + sizeof(*entry->commands) +
        sizeof(*forward) + sizeof(*inverse) + forward->size + inverse->size;
    return entry;
}

static bool editor_history_aggregate_entry_append(EditorHistoryEntry *entry,
        EditorHistoryAggregateChange *forward,
        EditorHistoryAggregateChange *inverse) {
    EditorHistoryCommandPair *commands;
    if(entry == NULL || forward == NULL || inverse == NULL) return false;
    commands = realloc(entry->commands,
        (entry->command_count + 1) * sizeof(*commands));
    if(commands == NULL) return false;
    entry->commands = commands;
    entry->commands[entry->command_count] = (EditorHistoryCommandPair){
        .forward = {.kind = EDITOR_HISTORY_ACTION_AGGREGATE,
            .data.aggregate = forward},
        .inverse = {.kind = EDITOR_HISTORY_ACTION_AGGREGATE,
            .data.aggregate = inverse}};
    entry->command_count += 1;
    entry->memory += sizeof(*commands) + sizeof(*forward) + sizeof(*inverse) +
        forward->size + inverse->size;
    return true;
}

static EditorHistoryAggregateChange *editor_history_object_aggregate_create(
        const EditorObject *object) {
    EditorHistoryAggregateChange *change;
    if(object == NULL) return NULL;
    change = calloc(1, sizeof(*change));
    if(change == NULL) return NULL;
    change->value = malloc(sizeof(*object));
    if(change->value == NULL) {
        free(change);
        return NULL;
    }
    memcpy(change->value, object, sizeof(*object));
    change->kind = EDITOR_HISTORY_AGGREGATE_OBJECT;
    change->object = object->id;
    change->size = sizeof(*object);
    return change;
}

static EditorHistoryEntry *editor_history_object_differences_create(
        const EditorProject *before, const EditorProject *after) {
    EditorHistoryEntry *entry;
    size_t objects_offset = offsetof(EditorProject, objects);
    size_t objects_end = objects_offset + sizeof(before->objects);
    if(before == NULL || after == NULL ||
            before->object_count != after->object_count ||
            memcmp(before, after, objects_offset) != 0 ||
            memcmp((const unsigned char *)before + objects_end,
                (const unsigned char *)after + objects_end,
                sizeof(*before) - objects_end) != 0) return NULL;
    entry = calloc(1, sizeof(*entry));
    if(entry == NULL) return NULL;
    entry->kind = EDITOR_HISTORY_ENTRY_COMMANDS;
    entry->memory = sizeof(*entry);
    for(size_t i = 0; i < before->object_count; i += 1) {
        EditorHistoryAggregateChange *forward;
        EditorHistoryAggregateChange *inverse;
        if(before->objects[i].id != after->objects[i].id) goto fail;
        if(memcmp(&before->objects[i], &after->objects[i],
                sizeof(before->objects[i])) == 0) continue;
        forward = editor_history_object_aggregate_create(&after->objects[i]);
        inverse = editor_history_object_aggregate_create(&before->objects[i]);
        if(forward == NULL || inverse == NULL ||
                !editor_history_aggregate_entry_append(entry, forward, inverse)) {
            editor_history_aggregate_destroy(forward);
            editor_history_aggregate_destroy(inverse);
            goto fail;
        }
    }
    if(entry->command_count == 0) goto fail;
    return entry;
fail:
    editor_history_entry_destroy(entry);
    return NULL;
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
    } else {
        for(size_t i = 0; i < entry->command_count; i += 1) {
            if(entry->commands[i].forward.kind == EDITOR_HISTORY_ACTION_OBJECT)
                free(entry->commands[i].forward.data.object);
            if(entry->commands[i].inverse.kind == EDITOR_HISTORY_ACTION_OBJECT)
                free(entry->commands[i].inverse.data.object);
            if(entry->commands[i].forward.kind == EDITOR_HISTORY_ACTION_AGGREGATE) {
                if(entry->commands[i].forward.data.aggregate != NULL)
                    free(entry->commands[i].forward.data.aggregate->value);
                free(entry->commands[i].forward.data.aggregate);
            }
            if(entry->commands[i].inverse.kind == EDITOR_HISTORY_ACTION_AGGREGATE) {
                if(entry->commands[i].inverse.data.aggregate != NULL)
                    free(entry->commands[i].inverse.data.aggregate->value);
                free(entry->commands[i].inverse.data.aggregate);
            }
        }
        free(entry->commands);
    }
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

static void editor_history_action_apply(EditorProject *project,
        const EditorHistoryAction *action) {
    if(project == NULL || action == NULL) return;
    if(action->kind == EDITOR_HISTORY_ACTION_COMMAND) {
        (void)editor_command_execute(project, &action->data.command);
        return;
    }
    if(action->kind == EDITOR_HISTORY_ACTION_AGGREGATE) {
        EditorHistoryAggregateChange *change = action->data.aggregate;
        EditorObject *object;
        if(change == NULL || change->value == NULL) return;
        object = editor_object_query_get(project, change->object);
        if(change->kind == EDITOR_HISTORY_AGGREGATE_OBJECT) {
            if(object != NULL && change->size == sizeof(*object))
                memcpy(object, change->value, change->size);
        } else if(change->kind == EDITOR_HISTORY_AGGREGATE_RIGID_BODY) {
            EditorRigidBody *body = editor_project_rigid_body_get(object, change->item);
            if(body != NULL && change->size == sizeof(*body))
                memcpy(body, change->value, change->size);
        } else {
            EditorSoftBody *body = editor_history_soft_body_get(object, change->item);
            if(body != NULL && change->size == sizeof(*body))
                memcpy(body, change->value, change->size);
        }
        return;
    }
    if(action->data.object == NULL) return;
    if(action->data.object->present) {
        size_t index = action->data.object->index;
        if(index > project->object_count ||
                project->object_count >= EDITOR_OBJECT_MAX) return;
        for(size_t i = project->object_count; i > index; i -= 1)
            project->objects[i] = project->objects[i - 1];
        project->objects[index] = action->data.object->value;
        project->object_count += 1;
    } else {
        for(size_t index = 0; index < project->object_count; index += 1) {
            if(project->objects[index].id != action->data.object->value.id) continue;
            for(size_t i = index + 1; i < project->object_count; i += 1)
                project->objects[i - 1] = project->objects[i];
            project->object_count -= 1;
            project->objects[project->object_count] = (EditorObject){0};
            break;
        }
    }
    project->selected = action->data.object->selected;
}

static void editor_history_entry_apply(EditorProject *project,
        const EditorHistoryEntry *entry, bool forward, EditorHistory *history) {
    unsigned char *bytes = (unsigned char *)project;
    if(project == NULL || entry == NULL) return;
    if(entry->kind == EDITOR_HISTORY_ENTRY_COMMANDS) {
        if(history != NULL) history->restoring = true;
        if(forward) {
            for(size_t i = 0; i < entry->command_count; i += 1)
                editor_history_action_apply(project, &entry->commands[i].forward);
        } else {
            for(size_t i = entry->command_count; i > 0; i -= 1)
                editor_history_action_apply(project, &entry->commands[i - 1].inverse);
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
    entry->commands[0] = (EditorHistoryCommandPair){
        .forward = {.kind = EDITOR_HISTORY_ACTION_COMMAND,
            .data.command = *forward},
        .inverse = {.kind = EDITOR_HISTORY_ACTION_COMMAND,
            .data.command = *inverse}};
    entry->command_count = 1;
    entry->memory = sizeof(*entry) + sizeof(*entry->commands);
    return entry;
}

static bool editor_history_command_entry_append(EditorHistoryEntry *entry,
        const EditorCommand *forward, const EditorCommand *inverse) {
    EditorHistoryCommandPair *commands;
    if(entry == NULL || entry->kind != EDITOR_HISTORY_ENTRY_COMMANDS ||
            forward == NULL || inverse == NULL) return false;
    commands = realloc(entry->commands,
        (entry->command_count + 1) * sizeof(*commands));
    if(commands == NULL) return false;
    entry->commands = commands;
    entry->commands[entry->command_count] = (EditorHistoryCommandPair){
        .forward = {.kind = EDITOR_HISTORY_ACTION_COMMAND,
            .data.command = *forward},
        .inverse = {.kind = EDITOR_HISTORY_ACTION_COMMAND,
            .data.command = *inverse}};
    entry->command_count += 1;
    entry->memory = sizeof(*entry) +
        entry->command_count * sizeof(*entry->commands);
    return true;
}

static EditorHistoryEntry *editor_history_object_entry_create(
        EditorHistoryObjectChange forward, EditorHistoryObjectChange inverse) {
    EditorHistoryEntry *entry = calloc(1, sizeof(*entry));
    if(entry == NULL) return NULL;
    entry->commands = malloc(sizeof(*entry->commands));
    if(entry->commands == NULL) {
        free(entry);
        return NULL;
    }
    entry->kind = EDITOR_HISTORY_ENTRY_COMMANDS;
    entry->commands[0].forward.data.object = malloc(sizeof(forward));
    entry->commands[0].inverse.data.object = malloc(sizeof(inverse));
    if(entry->commands[0].forward.data.object == NULL ||
            entry->commands[0].inverse.data.object == NULL) {
        editor_history_entry_destroy(entry);
        return NULL;
    }
    entry->commands[0] = (EditorHistoryCommandPair){
        .forward = {.kind = EDITOR_HISTORY_ACTION_OBJECT,
            .data.object = entry->commands[0].forward.data.object},
        .inverse = {.kind = EDITOR_HISTORY_ACTION_OBJECT,
            .data.object = entry->commands[0].inverse.data.object}};
    *entry->commands[0].forward.data.object = forward;
    *entry->commands[0].inverse.data.object = inverse;
    entry->command_count = 1;
    entry->memory = sizeof(*entry) + sizeof(*entry->commands) +
        sizeof(forward) + sizeof(inverse);
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
        case EDITOR_COMMAND_OBJECT_RENAME:
            object = editor_object_query_get(project,
                command->data.object_rename.object);
            if(object == NULL) return false;
            snprintf(inverse->data.object_rename.name,
                sizeof(inverse->data.object_rename.name), "%s", object->name);
            return true;
        case EDITOR_COMMAND_ITEM_RENAME:
            if(command->data.item_rename.kind != EDITOR_ITEM_OBJECT) return false;
            object = editor_object_query_get(project,
                command->data.item_rename.object);
            if(object == NULL) return false;
            snprintf(inverse->data.item_rename.name,
                sizeof(inverse->data.item_rename.name), "%s", object->name);
            return true;
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
    free(history->pending_object);
    editor_history_aggregate_destroy(history->pending_aggregate);
    free(history->transaction_before);
    editor_history_entry_destroy(history->transaction_commands);
    memset(history, 0, sizeof(*history));
}

void editor_history_reset(EditorHistory *history) {
    if(history == NULL || history->project == NULL) return;
    editor_history_stack_clear(history->undo, &history->undo_count);
    editor_history_stack_clear(history->redo, &history->redo_count);
    free(history->pending);
    history->pending = NULL;
    free(history->pending_object);
    history->pending_object = NULL;
    editor_history_aggregate_destroy(history->pending_aggregate);
    history->pending_aggregate = NULL;
    history->pending_command_valid = false;
    free(history->transaction_before);
    history->transaction_before = NULL;
    editor_history_entry_destroy(history->transaction_commands);
    history->transaction_commands = NULL;
    history->transaction_active = false;
    history->continuous = false;
    history->continuous_recorded = false;
    history->recorded_since_continuous_update = false;
    history->restoring = false;
}

void editor_history_command_begin(EditorHistory *history,
        const EditorProject *project, const EditorCommand *command) {
    if(history == NULL || history->restoring) return;
    free(history->pending);
    history->pending = NULL;
    free(history->pending_object);
    history->pending_object = NULL;
    editor_history_aggregate_destroy(history->pending_aggregate);
    history->pending_aggregate = NULL;
    history->pending_command_valid = false;
    if(project == NULL || !editor_history_command_record_check(command)) return;
    if(command->type == EDITOR_COMMAND_OBJECT_ADD ||
            (command->type == EDITOR_COMMAND_ITEM_ADD &&
                command->data.item_add.kind == EDITOR_ITEM_OBJECT)) {
        history->pending_object = calloc(1, sizeof(*history->pending_object));
        if(history->pending_object != NULL)
            history->pending_object->selected = project->selected;
        return;
    }
    if(command->type == EDITOR_COMMAND_OBJECT_REMOVE ||
            (command->type == EDITOR_COMMAND_ITEM_REMOVE &&
                command->data.item_remove.kind == EDITOR_ITEM_OBJECT)) {
        EditorObjectId id = command->type == EDITOR_COMMAND_OBJECT_REMOVE ?
            command->data.object_remove.object : command->data.item_remove.object;
        for(size_t i = 0; i < project->object_count; i += 1) {
            if(project->objects[i].id != id) continue;
            history->pending_object = malloc(sizeof(*history->pending_object));
            if(history->pending_object != NULL)
                *history->pending_object = (EditorHistoryObjectChange){
                    .present = true, .index = i, .value = project->objects[i],
                    .selected = project->selected};
            return;
        }
    }
    if(command->type == EDITOR_COMMAND_ITEM_ADD &&
            command->data.item_add.kind != EDITOR_ITEM_OBJECT) {
        history->pending_aggregate = editor_history_aggregate_capture(
            history->project, command->data.item_add.kind,
            command->data.item_add.object, command->data.item_add.parent);
        return;
    }
    if(command->type == EDITOR_COMMAND_ITEM_REMOVE &&
            command->data.item_remove.kind != EDITOR_ITEM_OBJECT) {
        history->pending_aggregate = editor_history_aggregate_capture(
            history->project, command->data.item_remove.kind,
            command->data.item_remove.object, command->data.item_remove.parent);
        return;
    }
    history->pending_aggregate = editor_history_command_aggregate_capture(
        history->project, command);
    if(history->pending_aggregate != NULL) return;
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
    if(history->transaction_active) {
        if(history->pending_command_valid && command != NULL && result != NULL &&
                result->kind == ERROR_RESULT_VALUE &&
                !editor_history_command_value_equal(command,
                    &history->pending_inverse))
            (void)editor_history_command_entry_append(history->transaction_commands,
                command, &history->pending_inverse);
        free(history->pending);
        history->pending = NULL;
        free(history->pending_object);
        history->pending_object = NULL;
        editor_history_aggregate_destroy(history->pending_aggregate);
        history->pending_aggregate = NULL;
        history->pending_command_valid = false;
        return;
    }
    if(history->pending_object != NULL && command != NULL && result != NULL &&
            result->kind == ERROR_RESULT_VALUE) {
        EditorHistoryObjectChange forward = *history->pending_object;
        EditorHistoryObjectChange inverse = *history->pending_object;
        if(command->type == EDITOR_COMMAND_OBJECT_ADD ||
                (command->type == EDITOR_COMMAND_ITEM_ADD &&
                    command->data.item_add.kind == EDITOR_ITEM_OBJECT)) {
            EditorObjectId id = result->result.object;
            for(size_t i = 0; i < history->project->object_count; i += 1)
                if(history->project->objects[i].id == id) {
                    forward = (EditorHistoryObjectChange){.present = true,
                        .index = i, .value = history->project->objects[i],
                        .selected = history->project->selected};
                    inverse = forward;
                    inverse.present = false;
                    inverse.selected = history->pending_object->selected;
                }
        } else {
            forward.present = false;
            forward.selected = history->project->selected;
            inverse.present = true;
        }
        entry = editor_history_object_entry_create(forward, inverse);
        if(entry != NULL && editor_history_stack_push(history->undo,
                &history->undo_count, entry))
            editor_history_stack_clear(history->redo, &history->redo_count);
        else editor_history_entry_destroy(entry);
        goto finish;
    }
    if(history->pending_aggregate != NULL && command != NULL && result != NULL &&
            result->kind == ERROR_RESULT_VALUE) {
        EditorHistoryAggregateChange *after = editor_history_aggregate_recapture(
            history->project, history->pending_aggregate);
        if(after != NULL && after->size == history->pending_aggregate->size &&
                memcmp(after->value, history->pending_aggregate->value,
                    after->size) == 0) {
            editor_history_aggregate_destroy(after);
            goto finish;
        }
        entry = editor_history_aggregate_entry_create(after,
            history->pending_aggregate);
        if(entry != NULL) history->pending_aggregate = NULL;
        else editor_history_aggregate_destroy(after);
        if(entry != NULL && editor_history_stack_push(history->undo,
                &history->undo_count, entry))
            editor_history_stack_clear(history->redo, &history->redo_count);
        else editor_history_entry_destroy(entry);
        goto finish;
    }
    if(history->pending_command_valid && command != NULL && result != NULL &&
            result->kind == ERROR_RESULT_VALUE) {
        if(editor_history_command_value_equal(command, &history->pending_inverse))
            goto finish;
        if(history->continuous && history->continuous_recorded &&
                history->undo_count > 0 &&
                history->undo[history->undo_count - 1]->kind ==
                    EDITOR_HISTORY_ENTRY_COMMANDS) {
            history->undo[history->undo_count - 1]->commands[
                history->undo[history->undo_count - 1]->command_count - 1]
                .forward.data.command = *command;
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
    free(history->pending_object);
    history->pending_object = NULL;
    editor_history_aggregate_destroy(history->pending_aggregate);
    history->pending_aggregate = NULL;
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
    history->transaction_commands = calloc(1,
        sizeof(*history->transaction_commands));
    if(history->transaction_commands == NULL) {
        free(history->transaction_before);
        history->transaction_before = NULL;
        return false;
    }
    history->transaction_commands->kind = EDITOR_HISTORY_ENTRY_COMMANDS;
    history->transaction_commands->memory =
        sizeof(*history->transaction_commands);
    history->transaction_active = true;
    return true;
}

bool editor_history_transaction_end(EditorHistory *history) {
    EditorHistoryEntry *entry;
    EditorProject *replayed = NULL;
    if(history == NULL || history->project == NULL ||
            !history->transaction_active || history->transaction_before == NULL)
        return false;
    entry = NULL;
    if(history->transaction_commands != NULL &&
            history->transaction_commands->command_count > 0) {
        replayed = malloc(sizeof(*replayed));
        if(replayed != NULL) {
            memcpy(replayed, history->transaction_before, sizeof(*replayed));
            editor_history_entry_apply(replayed, history->transaction_commands,
                true, history);
            if(memcmp(replayed, history->project, sizeof(*replayed)) == 0) {
                entry = history->transaction_commands;
                history->transaction_commands = NULL;
            }
        }
    }
    free(replayed);
    if(entry == NULL)
        entry = editor_history_object_differences_create(
            history->transaction_before, history->project);
    if(entry == NULL)
        entry = editor_history_entry_create(history->transaction_before,
            history->project);
    free(history->transaction_before);
    history->transaction_before = NULL;
    history->transaction_active = false;
    editor_history_entry_destroy(history->transaction_commands);
    history->transaction_commands = NULL;
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
    editor_history_entry_destroy(history->transaction_commands);
    history->transaction_commands = NULL;
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
