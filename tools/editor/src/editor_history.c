#include "editor_history.h"
#include "editor_array.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum EditorHistoryActionKind {
    EDITOR_HISTORY_ACTION_COMMAND,
    EDITOR_HISTORY_ACTION_OBJECT,
    EDITOR_HISTORY_ACTION_AGGREGATE,
    EDITOR_HISTORY_ACTION_COLLISION,
    EDITOR_HISTORY_ACTION_SPRITES,
    EDITOR_HISTORY_ACTION_OBJECT_ORDER
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
    bool tracked;
};

struct EditorHistoryCollisionChange {
    EditorCollisionMask *masks;
    size_t count;
};

struct EditorHistorySpriteChange {
    EditorObjectId object;
    EditorSprite *sprites;
    size_t count;
    EditorSpriteId next_id;
};

typedef struct EditorHistoryObjectOrderChange {
    EditorObjectId *ids;
    size_t count;
    bool tracked;
} EditorHistoryObjectOrderChange;

struct EditorHistoryObjectChange {
    bool present;
    size_t index;
    EditorObject *value;
    EditorObjectId selected;
};

static void editor_history_object_change_destroy(EditorHistoryObjectChange *change) {
    if(change == NULL) return;
    if(change->value != NULL) {
        editor_project_object_destroy(change->value);
        free(change->value);
    }
    free(change);
}

static EditorHistoryObjectChange *editor_history_object_change_clone(
        const EditorHistoryObjectChange *source) {
    EditorHistoryObjectChange *change;
    if(source == NULL) return NULL;
    change = calloc(1, sizeof(*change));
    if(change == NULL) return NULL;
    *change = *source;
    change->value = NULL;
    if(source->value != NULL) {
        change->value = calloc(1, sizeof(*change->value));
        if(change->value == NULL || !editor_project_object_clone(
                change->value, source->value)) {
            editor_history_object_change_destroy(change);
            return NULL;
        }
    }
    return change;
}

typedef struct EditorHistoryAction {
    EditorHistoryActionKind kind;
    union {
        EditorCommand command;
        EditorHistoryObjectChange *object;
        EditorHistoryAggregateChange *aggregate;
        EditorHistoryCollisionChange *collision;
        EditorHistorySpriteChange *sprites;
        EditorHistoryObjectOrderChange *order;
    } data;
} EditorHistoryAction;

typedef struct EditorHistoryCommandPair {
    EditorHistoryAction forward;
    EditorHistoryAction inverse;
} EditorHistoryCommandPair;

struct EditorHistoryEntry {
    size_t memory;
    EditorHistoryCommandPair *commands;
    size_t command_count;
};

static EditorSoftBody *editor_history_soft_body_get(EditorObject *object,
    EditorSoftBodyId id);
static void editor_history_entry_destroy(EditorHistoryEntry *entry);
static void editor_history_collision_destroy(EditorHistoryCollisionChange *change);
static void editor_history_sprites_destroy(EditorHistorySpriteChange *change);

static void editor_history_aggregate_destroy(EditorHistoryAggregateChange *change) {
    if(change == NULL) return;
    if(change->value != NULL) {
        if(change->kind == EDITOR_HISTORY_AGGREGATE_OBJECT)
            editor_project_object_destroy(change->value);
        else if(change->kind == EDITOR_HISTORY_AGGREGATE_RIGID_BODY)
            editor_project_rigid_body_destroy(change->value);
        else editor_project_soft_body_destroy(change->value);
    }
    free(change->value);
    free(change);
}

static bool editor_history_aggregate_value_clone(
        EditorHistoryAggregateChange *change, const void *value) {
    if(change == NULL || value == NULL) return false;
    change->value = calloc(1, change->size);
    if(change->value == NULL) return false;
    if(change->kind == EDITOR_HISTORY_AGGREGATE_OBJECT)
        return editor_project_object_clone(change->value, value);
    if(change->kind == EDITOR_HISTORY_AGGREGATE_RIGID_BODY)
        return editor_project_rigid_body_clone(change->value, value);
    return editor_project_soft_body_clone(change->value, value);
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
    change->kind = aggregate_kind;
    change->object = object_id;
    change->item = item;
    change->size = size;
    if(!editor_history_aggregate_value_clone(change, value)) {
        editor_history_aggregate_destroy(change);
        return NULL;
    }
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
    change->value = NULL;
    if(!editor_history_aggregate_value_clone(change, value)) {
        editor_history_aggregate_destroy(change);
        return NULL;
    }
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
        case EDITOR_COMMAND_SPRITE_ADD:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.sprite_add.object, 0);
        case EDITOR_COMMAND_SPRITE_REMOVE:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.sprite_remove.object, 0);
        case EDITOR_COMMAND_SPRITE_RENAME:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.sprite_rename.object, 0);
        case EDITOR_COMMAND_SPRITE_PATH_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.sprite_path_set.object, 0);
        case EDITOR_COMMAND_SPRITE_SIZE_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.sprite_size_set.object, 0);
        case EDITOR_COMMAND_ANIMATED_SPRITE_ADD:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animated_sprite_add.object, 0);
        case EDITOR_COMMAND_ANIMATED_SPRITE_REMOVE:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animated_sprite_remove.object, 0);
        case EDITOR_COMMAND_ANIMATED_SPRITE_RENAME:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animated_sprite_rename.object, 0);
        case EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animated_sprite_body_set.object, 0);
        case EDITOR_COMMAND_ANIMATED_SPRITE_POSITION_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animated_sprite_position_set.object, 0);
        case EDITOR_COMMAND_ANIMATED_SPRITE_SCALE_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animated_sprite_scale_set.object, 0);
        case EDITOR_COMMAND_ANIMATED_SPRITE_TIMING_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animated_sprite_timing_set.object, 0);
        case EDITOR_COMMAND_ANIMATED_SPRITE_STARTING_FRAME_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animated_sprite_starting_frame_set.object, 0);
        case EDITOR_COMMAND_ANIMATED_SPRITE_DIRECTION_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animated_sprite_direction_set.object, 0);
        case EDITOR_COMMAND_ANIMATED_SPRITE_FOLLOW_ROTATION_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET:
        case EDITOR_COMMAND_ANIMATED_SPRITE_PLAYING_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animated_sprite_boolean_set.object, 0);
        case EDITOR_COMMAND_ANIMATION_FRAME_ADD:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animation_frame_add.object, 0);
        case EDITOR_COMMAND_ANIMATION_FRAME_REMOVE:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animation_frame_remove.object, 0);
        case EDITOR_COMMAND_ANIMATION_FRAME_RENAME:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animation_frame_rename.object, 0);
        case EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animation_frame_path_set.object, 0);
        case EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.animation_frame_size_set.object, 0);
        case EDITOR_COMMAND_SPRITE_POSITION_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.sprite_position_set.object, 0);
        case EDITOR_COMMAND_SPRITE_VISIBILITY_SET:
            return editor_history_aggregate_capture(project, EDITOR_ITEM_OBJECT,
                command->data.sprite_visibility_set.object, 0);
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
    change->kind = EDITOR_HISTORY_AGGREGATE_OBJECT;
    change->object = object->id;
    change->size = sizeof(*object);
    if(!editor_history_aggregate_value_clone(change, object)) {
        editor_history_aggregate_destroy(change);
        return NULL;
    }
    return change;
}

static void editor_history_entry_destroy(EditorHistoryEntry *entry) {
    if(entry == NULL) return;
    for(size_t i = 0; i < entry->command_count; i += 1) {
            if(entry->commands[i].forward.kind == EDITOR_HISTORY_ACTION_OBJECT)
                editor_history_object_change_destroy(
                    entry->commands[i].forward.data.object);
            if(entry->commands[i].inverse.kind == EDITOR_HISTORY_ACTION_OBJECT)
                editor_history_object_change_destroy(
                    entry->commands[i].inverse.data.object);
            if(entry->commands[i].forward.kind == EDITOR_HISTORY_ACTION_AGGREGATE) {
                editor_history_aggregate_destroy(
                    entry->commands[i].forward.data.aggregate);
            }
            if(entry->commands[i].inverse.kind == EDITOR_HISTORY_ACTION_AGGREGATE) {
                editor_history_aggregate_destroy(
                    entry->commands[i].inverse.data.aggregate);
            }
            if(entry->commands[i].forward.kind == EDITOR_HISTORY_ACTION_COLLISION) {
                free(entry->commands[i].forward.data.collision->masks);
                free(entry->commands[i].forward.data.collision);
            }
            if(entry->commands[i].inverse.kind == EDITOR_HISTORY_ACTION_COLLISION) {
                free(entry->commands[i].inverse.data.collision->masks);
                free(entry->commands[i].inverse.data.collision);
            }
            if(entry->commands[i].forward.kind == EDITOR_HISTORY_ACTION_SPRITES)
                editor_history_sprites_destroy(
                    entry->commands[i].forward.data.sprites);
            if(entry->commands[i].inverse.kind == EDITOR_HISTORY_ACTION_SPRITES)
                editor_history_sprites_destroy(
                    entry->commands[i].inverse.data.sprites);
            if(entry->commands[i].forward.kind == EDITOR_HISTORY_ACTION_OBJECT_ORDER) {
                free(entry->commands[i].forward.data.order->ids);
                free(entry->commands[i].forward.data.order);
            }
            if(entry->commands[i].inverse.kind == EDITOR_HISTORY_ACTION_OBJECT_ORDER) {
                free(entry->commands[i].inverse.data.order->ids);
                free(entry->commands[i].inverse.data.order);
            }
    }
    free(entry->commands);
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
                (void)editor_project_object_copy_set(object, change->value);
        } else if(change->kind == EDITOR_HISTORY_AGGREGATE_RIGID_BODY) {
            EditorRigidBody *body = editor_project_rigid_body_get(object, change->item);
            if(body != NULL && change->size == sizeof(*body))
                (void)editor_project_rigid_body_copy_set(body, change->value);
        } else {
            EditorSoftBody *body = editor_history_soft_body_get(object, change->item);
            if(body != NULL && change->size == sizeof(*body))
                (void)editor_project_soft_body_copy_set(body, change->value);
        }
        return;
    }
    if(action->kind == EDITOR_HISTORY_ACTION_COLLISION) {
        EditorHistoryCollisionChange *change = action->data.collision;
        if(change == NULL) return;
        if(!EDITOR_ARRAY_RESERVE(project->collision_masks,
                project->collision_mask_capacity, change->count)) return;
        memcpy(project->collision_masks, change->masks,
            change->count * sizeof(*change->masks));
        project->collision_mask_count = change->count;
        return;
    }
    if(action->kind == EDITOR_HISTORY_ACTION_SPRITES) {
        EditorHistorySpriteChange *change = action->data.sprites;
        EditorObject *object = change == NULL ? NULL :
            editor_object_query_get(project, change->object);
        if(object == NULL || !EDITOR_ARRAY_RESERVE(object->sprites,
                object->sprite_capacity, change->count)) return;
        if(change->count > 0) memcpy(object->sprites, change->sprites,
            change->count * sizeof(*change->sprites));
        object->sprite_count = change->count;
        project->next_sprite_id = change->next_id;
        return;
    }
    if(action->kind == EDITOR_HISTORY_ACTION_OBJECT_ORDER) {
        EditorHistoryObjectOrderChange *change = action->data.order;
        EditorObject *ordered;
        if(change == NULL || change->count != project->object_count) return;
        ordered = malloc(change->count * sizeof(*ordered));
        if(ordered == NULL) return;
        for(size_t i = 0; i < change->count; i += 1) {
            EditorObject *object = editor_object_query_get(project, change->ids[i]);
            if(object == NULL) {
                free(ordered);
                return;
            }
            ordered[i] = *object;
        }
        memcpy(project->objects, ordered,
            change->count * sizeof(project->objects[0]));
        free(ordered);
        return;
    }
    if(action->data.object == NULL) return;
    if(action->data.object->present) {
        size_t index = action->data.object->index;
        if(index > project->object_count || action->data.object->value == NULL ||
                !EDITOR_ARRAY_RESERVE(project->objects,
                    project->object_capacity, project->object_count + 1)) return;
        for(size_t i = project->object_count; i > index; i -= 1)
            project->objects[i] = project->objects[i - 1];
        project->objects[index] = (EditorObject){0};
        if(!editor_project_object_clone(&project->objects[index],
                action->data.object->value)) {
            for(size_t i = index; i < project->object_count; i += 1)
                project->objects[i] = project->objects[i + 1];
            return;
        }
        project->object_count += 1;
    } else {
        for(size_t index = 0; index < project->object_count; index += 1) {
            if(action->data.object->value == NULL ||
                    project->objects[index].id !=
                        action->data.object->value->id) continue;
            editor_project_object_destroy(&project->objects[index]);
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
    if(project == NULL || entry == NULL) return;
    if(history != NULL) history->restoring = true;
    if(forward) {
        for(size_t i = 0; i < entry->command_count; i += 1)
            editor_history_action_apply(project, &entry->commands[i].forward);
    } else {
        for(size_t i = entry->command_count; i > 0; i -= 1)
            editor_history_action_apply(project, &entry->commands[i - 1].inverse);
    }
    if(history != NULL) history->restoring = false;
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
    if(entry == NULL || forward == NULL || inverse == NULL) return false;
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
    entry->commands[0].forward.data.object =
        editor_history_object_change_clone(&forward);
    entry->commands[0].inverse.data.object =
        editor_history_object_change_clone(&inverse);
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
    entry->command_count = 1;
    entry->memory = sizeof(*entry) + sizeof(*entry->commands) +
        sizeof(forward) + sizeof(inverse);
    return entry;
}

static EditorHistoryCollisionChange *editor_history_collision_capture(
        const EditorProject *project) {
    EditorHistoryCollisionChange *change;
    if(project == NULL) return NULL;
    change = calloc(1, sizeof(*change));
    if(change == NULL) return NULL;
    change->count = project->collision_mask_count;
    if(change->count > 0) {
        change->masks = malloc(change->count * sizeof(*change->masks));
        if(change->masks == NULL) {
            free(change);
            return NULL;
        }
        memcpy(change->masks, project->collision_masks,
            change->count * sizeof(*change->masks));
    }
    return change;
}

static void editor_history_collision_destroy(EditorHistoryCollisionChange *change) {
    if(change == NULL) return;
    free(change->masks);
    free(change);
}

static EditorHistoryEntry *editor_history_collision_entry_create(
        EditorHistoryCollisionChange *forward,
        EditorHistoryCollisionChange *inverse) {
    EditorHistoryEntry *entry;
    if(forward == NULL || inverse == NULL) return NULL;
    entry = calloc(1, sizeof(*entry));
    if(entry == NULL) return NULL;
    entry->commands = malloc(sizeof(*entry->commands));
    if(entry->commands == NULL) {
        free(entry);
        return NULL;
    }
    entry->commands[0] = (EditorHistoryCommandPair){
        .forward = {.kind = EDITOR_HISTORY_ACTION_COLLISION,
            .data.collision = forward},
        .inverse = {.kind = EDITOR_HISTORY_ACTION_COLLISION,
            .data.collision = inverse}};
    entry->command_count = 1;
    entry->memory = sizeof(*entry) + sizeof(*entry->commands) +
        sizeof(*forward) + sizeof(*inverse);
    return entry;
}

static EditorHistorySpriteChange *editor_history_sprites_capture(
        const EditorProject *project, EditorObjectId object_id) {
    EditorHistorySpriteChange *change;
    EditorObject *object = editor_object_query_get((EditorProject *)project, object_id);
    if(project == NULL || object == NULL) return NULL;
    change = calloc(1, sizeof(*change));
    if(change == NULL) return NULL;
    change->object = object_id;
    change->count = object->sprite_count;
    change->next_id = project->next_sprite_id;
    if(change->count > 0) {
        change->sprites = malloc(change->count * sizeof(*change->sprites));
        if(change->sprites == NULL) { free(change); return NULL; }
        memcpy(change->sprites, object->sprites,
            change->count * sizeof(*change->sprites));
    }
    return change;
}

static void editor_history_sprites_destroy(EditorHistorySpriteChange *change) {
    if(change == NULL) return;
    free(change->sprites);
    free(change);
}

static bool editor_history_sprites_entry_append(EditorHistoryEntry *entry,
        EditorHistorySpriteChange *forward, EditorHistorySpriteChange *inverse) {
    EditorHistoryCommandPair *commands;
    if(entry == NULL || forward == NULL || inverse == NULL) return false;
    commands = realloc(entry->commands,
        (entry->command_count + 1) * sizeof(*commands));
    if(commands == NULL) return false;
    entry->commands = commands;
    entry->commands[entry->command_count++] = (EditorHistoryCommandPair){
        .forward = {.kind = EDITOR_HISTORY_ACTION_SPRITES,
            .data.sprites = forward},
        .inverse = {.kind = EDITOR_HISTORY_ACTION_SPRITES,
            .data.sprites = inverse}};
    entry->memory += sizeof(*commands) + sizeof(*forward) + sizeof(*inverse) +
        (forward->count + inverse->count) * sizeof(EditorSprite);
    return true;
}

static EditorHistoryEntry *editor_history_sprites_entry_create(
        EditorHistorySpriteChange *forward, EditorHistorySpriteChange *inverse) {
    EditorHistoryEntry *entry = calloc(1, sizeof(*entry));
    if(entry == NULL) return NULL;
    if(!editor_history_sprites_entry_append(entry, forward, inverse)) {
        free(entry);
        return NULL;
    }
    entry->memory += sizeof(*entry);
    return entry;
}

static bool editor_history_object_entry_append(EditorHistoryEntry *entry,
        EditorHistoryObjectChange forward, EditorHistoryObjectChange inverse) {
    EditorHistoryObjectChange *forward_copy =
        editor_history_object_change_clone(&forward);
    EditorHistoryObjectChange *inverse_copy =
        editor_history_object_change_clone(&inverse);
    EditorHistoryCommandPair *commands;
    if(entry == NULL || forward_copy == NULL || inverse_copy == NULL) {
        editor_history_object_change_destroy(forward_copy);
        editor_history_object_change_destroy(inverse_copy);
        return false;
    }
    commands = realloc(entry->commands,
        (entry->command_count + 1) * sizeof(*commands));
    if(commands == NULL) {
        editor_history_object_change_destroy(forward_copy);
        editor_history_object_change_destroy(inverse_copy);
        return false;
    }
    entry->commands = commands;
    entry->commands[entry->command_count] = (EditorHistoryCommandPair){
        .forward = {.kind = EDITOR_HISTORY_ACTION_OBJECT,
            .data.object = forward_copy},
        .inverse = {.kind = EDITOR_HISTORY_ACTION_OBJECT,
            .data.object = inverse_copy}};
    entry->command_count += 1;
    entry->memory += sizeof(*commands) + sizeof(*forward_copy) + sizeof(*inverse_copy);
    return true;
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
    editor_history_object_change_destroy(history->pending_object);
    editor_history_aggregate_destroy(history->pending_aggregate);
    editor_history_collision_destroy(history->pending_collision);
    editor_history_sprites_destroy(history->pending_sprites);
    editor_history_entry_destroy(history->transaction_commands);
    memset(history, 0, sizeof(*history));
}

void editor_history_reset(EditorHistory *history) {
    if(history == NULL || history->project == NULL) return;
    editor_history_stack_clear(history->undo, &history->undo_count);
    editor_history_stack_clear(history->redo, &history->redo_count);
    editor_history_object_change_destroy(history->pending_object);
    history->pending_object = NULL;
    editor_history_aggregate_destroy(history->pending_aggregate);
    history->pending_aggregate = NULL;
    editor_history_collision_destroy(history->pending_collision);
    history->pending_collision = NULL;
    editor_history_sprites_destroy(history->pending_sprites);
    history->pending_sprites = NULL;
    history->pending_command_valid = false;
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
    editor_history_object_change_destroy(history->pending_object);
    history->pending_object = NULL;
    editor_history_aggregate_destroy(history->pending_aggregate);
    history->pending_aggregate = NULL;
    editor_history_collision_destroy(history->pending_collision);
    history->pending_collision = NULL;
    editor_history_sprites_destroy(history->pending_sprites);
    history->pending_sprites = NULL;
    history->pending_command_valid = false;
    if(project == NULL || !editor_history_command_record_check(command)) return;
    if(command->type == EDITOR_COMMAND_COLLISION_MASK_ADD) {
        history->pending_collision = editor_history_collision_capture(project);
        return;
    }
    if(command->type >= EDITOR_COMMAND_SPRITE_ADD &&
            command->type <= EDITOR_COMMAND_SPRITE_SIZE_SET) {
        EditorObjectId object = command->type == EDITOR_COMMAND_SPRITE_ADD ?
            command->data.sprite_add.object : command->type == EDITOR_COMMAND_SPRITE_REMOVE ?
            command->data.sprite_remove.object : command->type == EDITOR_COMMAND_SPRITE_RENAME ?
            command->data.sprite_rename.object : command->type == EDITOR_COMMAND_SPRITE_PATH_SET ?
            command->data.sprite_path_set.object : command->data.sprite_size_set.object;
        history->pending_sprites = editor_history_sprites_capture(project, object);
        return;
    }
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
            EditorHistoryObjectChange source = {.present = true, .index = i,
                .value = &project->objects[i], .selected = project->selected};
            history->pending_object = editor_history_object_change_clone(&source);
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
}

void editor_history_command_finish(EditorHistory *history,
        const EditorCommand *command, const EditorCommandResult *result) {
    EditorHistoryEntry *entry = NULL;
    if(history == NULL || history->project == NULL || history->restoring) return;
    if(history->transaction_active) {
        if(history->pending_object != NULL && command != NULL && result != NULL &&
                result->kind == ERROR_RESULT_VALUE) {
            EditorHistoryObjectChange forward = *history->pending_object;
            EditorHistoryObjectChange inverse = *history->pending_object;
            if(command->type == EDITOR_COMMAND_OBJECT_ADD ||
                    (command->type == EDITOR_COMMAND_ITEM_ADD &&
                        command->data.item_add.kind == EDITOR_ITEM_OBJECT)) {
                for(size_t i = 0; i < history->project->object_count; i += 1)
                    if(history->project->objects[i].id == result->result.object) {
                        forward = (EditorHistoryObjectChange){.present = true,
                            .index = i, .value = &history->project->objects[i],
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
            (void)editor_history_object_entry_append(
                history->transaction_commands, forward, inverse);
        } else if(history->pending_sprites != NULL && command != NULL && result != NULL &&
                result->kind == ERROR_RESULT_VALUE) {
            EditorHistorySpriteChange *after =
                editor_history_sprites_capture(history->project,
                    history->pending_sprites->object);
            if(after == NULL || !editor_history_sprites_entry_append(
                    history->transaction_commands, after,
                    history->pending_sprites))
                editor_history_sprites_destroy(after);
            else history->pending_sprites = NULL;
        } else if(history->pending_aggregate != NULL && command != NULL && result != NULL &&
                result->kind == ERROR_RESULT_VALUE) {
            EditorHistoryAggregateChange *after = editor_history_aggregate_recapture(
                history->project, history->pending_aggregate);
            if(after == NULL || !editor_history_aggregate_entry_append(
                    history->transaction_commands, after,
                    history->pending_aggregate)) {
                editor_history_aggregate_destroy(after);
            } else history->pending_aggregate = NULL;
        } else if(history->pending_command_valid && command != NULL && result != NULL &&
                result->kind == ERROR_RESULT_VALUE &&
                !editor_history_command_value_equal(command,
                    &history->pending_inverse))
            (void)editor_history_command_entry_append(history->transaction_commands,
                command, &history->pending_inverse);
        editor_history_object_change_destroy(history->pending_object);
        history->pending_object = NULL;
        editor_history_aggregate_destroy(history->pending_aggregate);
        history->pending_aggregate = NULL;
        editor_history_collision_destroy(history->pending_collision);
        history->pending_collision = NULL;
        editor_history_sprites_destroy(history->pending_sprites);
        history->pending_sprites = NULL;
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
                        .index = i, .value = &history->project->objects[i],
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
    if(history->pending_sprites != NULL && command != NULL && result != NULL &&
            result->kind == ERROR_RESULT_VALUE) {
        EditorHistorySpriteChange *after =
            editor_history_sprites_capture(history->project,
                history->pending_sprites->object);
        entry = editor_history_sprites_entry_create(after,
            history->pending_sprites);
        if(entry != NULL) history->pending_sprites = NULL;
        else editor_history_sprites_destroy(after);
        if(entry != NULL && editor_history_stack_push(history->undo,
                &history->undo_count, entry))
            editor_history_stack_clear(history->redo, &history->redo_count);
        else editor_history_entry_destroy(entry);
        goto finish;
    }
    if(history->pending_collision != NULL && command != NULL && result != NULL &&
            result->kind == ERROR_RESULT_VALUE) {
        EditorHistoryCollisionChange *after =
            editor_history_collision_capture(history->project);
        entry = editor_history_collision_entry_create(after,
            history->pending_collision);
        if(entry != NULL) history->pending_collision = NULL;
        else editor_history_collision_destroy(after);
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
                history->undo_count > 0) {
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
    }
finish:
    editor_history_object_change_destroy(history->pending_object);
    history->pending_object = NULL;
    editor_history_aggregate_destroy(history->pending_aggregate);
    history->pending_aggregate = NULL;
    editor_history_collision_destroy(history->pending_collision);
    history->pending_collision = NULL;
    editor_history_sprites_destroy(history->pending_sprites);
    history->pending_sprites = NULL;
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
    history->transaction_commands = calloc(1,
        sizeof(*history->transaction_commands));
    if(history->transaction_commands == NULL) return false;
    history->transaction_commands->memory =
        sizeof(*history->transaction_commands);
    history->transaction_active = true;
    return true;
}

bool editor_history_transaction_object_track(EditorHistory *history,
        EditorObjectId object_id) {
    EditorObject *object;
    EditorHistoryAggregateChange *forward;
    EditorHistoryAggregateChange *inverse;
    if(history == NULL || !history->transaction_active ||
            history->transaction_commands == NULL) return false;
    for(size_t i = 0; i < history->transaction_commands->command_count; i += 1) {
        EditorHistoryAction *action =
            &history->transaction_commands->commands[i].inverse;
        if(action->kind == EDITOR_HISTORY_ACTION_AGGREGATE &&
                action->data.aggregate != NULL &&
                action->data.aggregate->kind == EDITOR_HISTORY_AGGREGATE_OBJECT &&
                action->data.aggregate->object == object_id) return true;
    }
    object = editor_object_query_get(history->project, object_id);
    if(object == NULL) return false;
    forward = editor_history_object_aggregate_create(object);
    inverse = editor_history_object_aggregate_create(object);
    if(forward == NULL || inverse == NULL) {
        editor_history_aggregate_destroy(forward);
        editor_history_aggregate_destroy(inverse);
        return false;
    }
    forward->tracked = true;
    if(!editor_history_aggregate_entry_append(history->transaction_commands,
            forward, inverse)) {
        editor_history_aggregate_destroy(forward);
        editor_history_aggregate_destroy(inverse);
        return false;
    }
    return true;
}

static EditorHistoryObjectOrderChange *editor_history_object_order_capture(
        const EditorProject *project) {
    EditorHistoryObjectOrderChange *change;
    if(project == NULL) return NULL;
    change = calloc(1, sizeof(*change));
    if(change == NULL) return NULL;
    change->count = project->object_count;
    if(change->count > 0) {
        change->ids = malloc(change->count * sizeof(*change->ids));
        if(change->ids == NULL) {
            free(change);
            return NULL;
        }
    }
    for(size_t i = 0; i < project->object_count; i += 1)
        change->ids[i] = project->objects[i].id;
    return change;
}

bool editor_history_transaction_object_order_track(EditorHistory *history) {
    EditorHistoryObjectOrderChange *forward;
    EditorHistoryObjectOrderChange *inverse;
    EditorHistoryCommandPair *commands;
    EditorHistoryEntry *entry;
    if(history == NULL || !history->transaction_active ||
            history->transaction_commands == NULL) return false;
    entry = history->transaction_commands;
    forward = editor_history_object_order_capture(history->project);
    inverse = editor_history_object_order_capture(history->project);
    if(forward == NULL || inverse == NULL) goto fail;
    commands = realloc(entry->commands,
        (entry->command_count + 1) * sizeof(*commands));
    if(commands == NULL) goto fail;
    forward->tracked = true;
    entry->commands = commands;
    entry->commands[entry->command_count] = (EditorHistoryCommandPair){
        .forward = {.kind = EDITOR_HISTORY_ACTION_OBJECT_ORDER,
            .data.order = forward},
        .inverse = {.kind = EDITOR_HISTORY_ACTION_OBJECT_ORDER,
            .data.order = inverse}};
    entry->command_count += 1;
    entry->memory += sizeof(*commands) + sizeof(*forward) + sizeof(*inverse);
    return true;
fail:
    if(forward != NULL) free(forward->ids);
    if(inverse != NULL) free(inverse->ids);
    free(forward);
    free(inverse);
    return false;
}

static bool editor_history_transaction_tracks_finalize(EditorHistory *history) {
    EditorHistoryEntry *entry;
    size_t output = 0;
    if(history == NULL || history->transaction_commands == NULL) return false;
    entry = history->transaction_commands;
    for(size_t i = 0; i < entry->command_count; i += 1) {
        EditorHistoryCommandPair pair = entry->commands[i];
        if(pair.forward.kind == EDITOR_HISTORY_ACTION_AGGREGATE &&
                pair.forward.data.aggregate != NULL &&
                pair.forward.data.aggregate->tracked) {
            EditorHistoryAggregateChange *updated =
                editor_history_aggregate_recapture(history->project,
                    pair.inverse.data.aggregate);
            if(updated == NULL) return false;
            editor_history_aggregate_destroy(pair.forward.data.aggregate);
            pair.forward.data.aggregate = updated;
        } else if(pair.forward.kind == EDITOR_HISTORY_ACTION_OBJECT_ORDER &&
                pair.forward.data.order != NULL &&
                pair.forward.data.order->tracked) {
            EditorHistoryObjectOrderChange *updated =
                editor_history_object_order_capture(history->project);
            if(updated == NULL) return false;
            free(pair.forward.data.order->ids);
            free(pair.forward.data.order);
            pair.forward.data.order = updated;
            if(updated->count == pair.inverse.data.order->count &&
                    memcmp(updated->ids, pair.inverse.data.order->ids,
                    updated->count * sizeof(updated->ids[0])) == 0) {
                free(pair.forward.data.order->ids);
                free(pair.inverse.data.order->ids);
                free(pair.forward.data.order);
                free(pair.inverse.data.order);
                continue;
            }
        }
        entry->commands[output++] = pair;
    }
    entry->command_count = output;
    return true;
}

bool editor_history_transaction_end(EditorHistory *history) {
    EditorHistoryEntry *entry;
    if(history == NULL || history->project == NULL ||
            !history->transaction_active || history->transaction_commands == NULL)
        return false;
    if(!editor_history_transaction_tracks_finalize(history)) return false;
    entry = history->transaction_commands;
    history->transaction_commands = NULL;
    history->transaction_active = false;
    if(entry->command_count == 0) {
        editor_history_entry_destroy(entry);
        return true;
    }
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
    if(history->project != NULL && history->transaction_commands != NULL)
        editor_history_entry_apply(history->project,
            history->transaction_commands, false, history);
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
