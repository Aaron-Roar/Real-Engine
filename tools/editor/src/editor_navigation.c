#include "editor_navigation.h"

#include <stdlib.h>
#include <string.h>

typedef struct EditorReorderStorage {
    unsigned char *items;
    size_t count;
    size_t stride;
} EditorReorderStorage;

static bool editor_selection_sibling_check(EditorSelectionRef first,
        EditorSelectionRef second) {
    bool object_children = (first.kind == EDITOR_SELECTION_RIGID_BODY ||
            first.kind == EDITOR_SELECTION_JOINT ||
            first.kind == EDITOR_SELECTION_SOFT_BODY) &&
        (second.kind == EDITOR_SELECTION_RIGID_BODY ||
            second.kind == EDITOR_SELECTION_JOINT ||
            second.kind == EDITOR_SELECTION_SOFT_BODY);
    return (first.kind == second.kind || object_children) &&
        first.object == second.object &&
        first.parent == second.parent && first.container == second.container;
}

static bool editor_selection_equal(EditorSelectionRef first,
        EditorSelectionRef second) {
    return first.kind == second.kind &&
        editor_selection_sibling_check(first, second) &&
        first.item == second.item;
}

static EditorHierarchyItemKind editor_hierarchy_kind_get(
        EditorHierarchySelection kind) {
    if(kind == EDITOR_SELECTION_JOINT) return EDITOR_HIERARCHY_JOINT;
    if(kind == EDITOR_SELECTION_SOFT_BODY) return EDITOR_HIERARCHY_SOFT_BODY;
    return EDITOR_HIERARCHY_RIGID_BODY;
}

static EditorSelectionRef editor_hierarchy_selection_get(EditorObjectId object,
        EditorHierarchyItem item) {
    EditorHierarchySelection kind = EDITOR_SELECTION_RIGID_BODY;
    if(item.kind == EDITOR_HIERARCHY_JOINT) kind = EDITOR_SELECTION_JOINT;
    else if(item.kind == EDITOR_HIERARCHY_SOFT_BODY) kind = EDITOR_SELECTION_SOFT_BODY;
    return (EditorSelectionRef){kind, object, 0, 0, item.id};
}

static bool editor_object_hierarchy_reorder(EditorProject *project,
        EditorViewportState *state, EditorSelectionRef source,
        EditorSelectionRef target, bool after, EditorHistory *history) {
    EditorObject *object = editor_object_query_get(project, source.object);
    EditorHierarchyItem ordered[EDITOR_OBJECT_HIERARCHY_MAX];
    EditorHierarchyItem selected_items[EDITOR_OBJECT_HIERARCHY_MAX];
    EditorHierarchyItem remaining[EDITOR_OBJECT_HIERARCHY_MAX];
    size_t selected_count = 0;
    size_t remaining_count = 0;
    size_t target_index;
    size_t insertion;
    bool source_selected;
    if(object == NULL) return false;
    editor_project_object_hierarchy_sync(object);
    target_index = editor_project_object_hierarchy_index_get(object,
        editor_hierarchy_kind_get(target.kind), target.item);
    if(target_index == SIZE_MAX) return false;
    source_selected = editor_viewport_selection_contains(state, source);
    for(size_t i = 0; i < object->hierarchy_count; i += 1) {
        EditorSelectionRef ref = editor_hierarchy_selection_get(object->id,
            object->hierarchy[i]);
        if((source_selected && editor_viewport_selection_contains(state, ref)) ||
                (!source_selected && editor_selection_equal(ref, source)))
            selected_items[selected_count++] = object->hierarchy[i];
        else remaining[remaining_count++] = object->hierarchy[i];
    }
    if(selected_count == 0) return false;
    insertion = 0;
    for(size_t i = 0; i < target_index + (after ? 1u : 0u); i += 1) {
        EditorSelectionRef ref = editor_hierarchy_selection_get(object->id,
            object->hierarchy[i]);
        if(!((source_selected && editor_viewport_selection_contains(state, ref)) ||
                (!source_selected && editor_selection_equal(ref, source))))
            insertion += 1;
    }
    memcpy(ordered, remaining, insertion * sizeof(*ordered));
    memcpy(&ordered[insertion], selected_items, selected_count * sizeof(*ordered));
    memcpy(&ordered[insertion + selected_count], &remaining[insertion],
        (remaining_count - insertion) * sizeof(*ordered));
    if(memcmp(ordered, object->hierarchy,
            object->hierarchy_count * sizeof(*ordered)) == 0) return false;
    if(history != NULL && !editor_history_transaction_begin(history)) return false;
    memcpy(object->hierarchy, ordered,
        object->hierarchy_count * sizeof(*ordered));
    return history == NULL || editor_history_transaction_end(history);
}

static bool editor_reorder_storage_get(EditorProject *project,
        EditorSelectionRef selection, EditorReorderStorage *storage) {
    EditorObject *object;
    EditorRigidBody *body;
    EditorSoftBody *soft_body;
    if(project == NULL || storage == NULL) return false;
    if(selection.kind == EDITOR_SELECTION_OBJECT) {
        *storage = (EditorReorderStorage){(unsigned char *)project->objects,
            project->object_count, sizeof(project->objects[0])};
        return true;
    }
    object = editor_object_query_get(project, selection.object);
    if(object == NULL) return false;
    switch(selection.kind) {
        case EDITOR_SELECTION_RIGID_BODY:
        case EDITOR_SELECTION_PARTICLE:
            *storage = (EditorReorderStorage){(unsigned char *)object->rigid_bodies,
                object->rigid_body_count, sizeof(object->rigid_bodies[0])};
            return true;
        case EDITOR_SELECTION_JOINT:
            *storage = (EditorReorderStorage){(unsigned char *)object->joint_items,
                object->joint_count, sizeof(object->joint_items[0])};
            return true;
        case EDITOR_SELECTION_ANCHOR:
            *storage = (EditorReorderStorage){(unsigned char *)object->anchors,
                object->anchor_count, sizeof(object->anchors[0])};
            return true;
        case EDITOR_SELECTION_SOFT_BODY:
            *storage = (EditorReorderStorage){(unsigned char *)object->soft_body_items,
                object->soft_body_count, sizeof(object->soft_body_items[0])};
            return true;
        default: break;
    }
    if(selection.kind == EDITOR_SELECTION_HITBOX) {
        body = editor_project_rigid_body_get(object, selection.parent);
        if(body == NULL) return false;
        *storage = (EditorReorderStorage){(unsigned char *)body->hitboxes,
            body->hitbox_count, sizeof(body->hitboxes[0])};
        return true;
    }
    soft_body = NULL;
    for(size_t i = 0; i < object->soft_body_count; i += 1)
        if(object->soft_body_items[i].id == selection.parent)
            soft_body = &object->soft_body_items[i];
    if(soft_body == NULL) return false;
    if(selection.kind == EDITOR_SELECTION_SOFT_NODE)
        *storage = (EditorReorderStorage){(unsigned char *)soft_body->nodes,
            soft_body->node_count, sizeof(soft_body->nodes[0])};
    else if(selection.kind == EDITOR_SELECTION_SOFT_BEAM)
        *storage = (EditorReorderStorage){(unsigned char *)soft_body->beams,
            soft_body->beam_count, sizeof(soft_body->beams[0])};
    else if(selection.kind == EDITOR_SELECTION_SOFT_AREA)
        *storage = (EditorReorderStorage){(unsigned char *)soft_body->areas,
            soft_body->area_count, sizeof(soft_body->areas[0])};
    else return false;
    return true;
}

static uint32_t editor_reorder_item_id_get(const EditorReorderStorage *storage,
        size_t index) {
    uint32_t id = 0;
    memcpy(&id, &storage->items[index * storage->stride], sizeof(id));
    return id;
}

bool editor_navigation_selection_reorder(EditorProject *project,
        EditorViewportState *state, EditorSelectionRef source,
        EditorSelectionRef target, bool after, EditorHistory *history) {
    EditorReorderStorage storage;
    bool *selected;
    unsigned char *ordered;
    size_t source_index = SIZE_MAX;
    size_t target_index = SIZE_MAX;
    size_t selected_before_slot = 0;
    size_t insertion;
    size_t output = 0;
    bool source_selected;

    if(project == NULL || state == NULL ||
            !editor_selection_sibling_check(source, target)) return false;
    if((source.kind == EDITOR_SELECTION_RIGID_BODY ||
                source.kind == EDITOR_SELECTION_JOINT ||
                source.kind == EDITOR_SELECTION_SOFT_BODY) &&
                (target.kind == EDITOR_SELECTION_RIGID_BODY ||
                    target.kind == EDITOR_SELECTION_JOINT ||
                    target.kind == EDITOR_SELECTION_SOFT_BODY) &&
                source.object == target.object && source.parent == 0)
        return editor_object_hierarchy_reorder(project, state, source,
            target, after, history);
    if(!editor_reorder_storage_get(project, source, &storage) ||
            storage.count < 2) return false;
    selected = calloc(storage.count, sizeof(*selected));
    ordered = malloc(storage.count * storage.stride);
    if(selected == NULL || ordered == NULL) {
        free(selected);
        free(ordered);
        return false;
    }
    for(size_t i = 0; i < storage.count; i += 1) {
        uint32_t id = editor_reorder_item_id_get(&storage, i);
        if(id == source.item) source_index = i;
        if(id == target.item) target_index = i;
    }
    if(source.kind == EDITOR_SELECTION_OBJECT) {
        for(size_t i = 0; i < storage.count; i += 1) {
            uint32_t id = editor_reorder_item_id_get(&storage, i);
            if(id == source.object) source_index = i;
            if(id == target.object) target_index = i;
        }
    }
    if(source_index == SIZE_MAX || target_index == SIZE_MAX) goto fail;
    source_selected = editor_viewport_selection_contains(state, source);
    for(size_t i = 0; i < storage.count; i += 1) {
        uint32_t id = editor_reorder_item_id_get(&storage, i);
        EditorSelectionRef candidate = source;
        candidate.item = id;
        if(candidate.kind == EDITOR_SELECTION_OBJECT) {
            candidate.object = id;
            candidate.item = id;
        }
        selected[i] = source_selected &&
            editor_viewport_selection_contains(state, candidate);
    }
    if(!source_selected) selected[source_index] = true;
    insertion = target_index + (after ? 1u : 0u);
    for(size_t i = 0; i < insertion; i += 1)
        if(selected[i]) selected_before_slot += 1;
    insertion -= selected_before_slot;
    for(size_t i = 0, unselected_index = 0; i <= storage.count; i += 1) {
        if(i == insertion) {
            for(size_t j = 0; j < storage.count; j += 1)
                if(selected[j]) {
                    memcpy(&ordered[output * storage.stride],
                        &storage.items[j * storage.stride], storage.stride);
                    output += 1;
                }
        }
        if(i == storage.count) break;
        while(unselected_index < storage.count && selected[unselected_index])
            unselected_index += 1;
        if(unselected_index < storage.count) {
            memcpy(&ordered[output * storage.stride],
                &storage.items[unselected_index * storage.stride], storage.stride);
            output += 1;
            unselected_index += 1;
        }
    }
    if(output != storage.count ||
            memcmp(storage.items, ordered, storage.count * storage.stride) == 0)
        goto fail;
    if(history != NULL && !editor_history_transaction_begin(history)) goto fail;
    memcpy(storage.items, ordered, storage.count * storage.stride);
    free(selected);
    free(ordered);
    return history == NULL || editor_history_transaction_end(history);

fail:
    free(selected);
    free(ordered);
    return false;
}

static bool editor_selection_remove_command_get(EditorSelectionRef selection,
        EditorCommand *command) {
    EditorItemKind kind;
    if(command == NULL) return false;
    switch(selection.kind) {
        case EDITOR_SELECTION_OBJECT: kind = EDITOR_ITEM_OBJECT; break;
        case EDITOR_SELECTION_RIGID_BODY:
        case EDITOR_SELECTION_PARTICLE: kind = EDITOR_ITEM_RIGID_BODY; break;
        case EDITOR_SELECTION_HITBOX: kind = EDITOR_ITEM_HITBOX; break;
        case EDITOR_SELECTION_JOINT: kind = EDITOR_ITEM_JOINT; break;
        case EDITOR_SELECTION_ANCHOR: kind = EDITOR_ITEM_ANCHOR; break;
        case EDITOR_SELECTION_SOFT_BODY: kind = EDITOR_ITEM_SOFT_BODY; break;
        case EDITOR_SELECTION_SOFT_NODE: kind = EDITOR_ITEM_SOFT_NODE; break;
        case EDITOR_SELECTION_SOFT_BEAM: kind = EDITOR_ITEM_SOFT_BEAM; break;
        case EDITOR_SELECTION_SOFT_AREA: kind = EDITOR_ITEM_SOFT_AREA; break;
        case EDITOR_SELECTION_VERTEX: kind = EDITOR_ITEM_VERTEX; break;
        case EDITOR_SELECTION_LINE: kind = EDITOR_ITEM_LINE; break;
        default: return false;
    }
    *command = (EditorCommand){.type = EDITOR_COMMAND_ITEM_REMOVE,
        .data.item_remove = {.kind = kind, .object = selection.object,
            .parent = selection.parent,
            .item = selection.kind == EDITOR_SELECTION_VERTEX ||
                    selection.kind == EDITOR_SELECTION_LINE ?
                selection.container : selection.item,
            .index = selection.kind == EDITOR_SELECTION_VERTEX ||
                    selection.kind == EDITOR_SELECTION_LINE ?
                selection.item : 0}};
    return true;
}

static int editor_selection_remove_order_compare(const void *first_value,
        const void *second_value) {
    const EditorSelectionRef *first = first_value;
    const EditorSelectionRef *second = second_value;
    if(first->object != second->object)
        return first->object < second->object ? -1 : 1;
    if(first->parent != second->parent)
        return first->parent < second->parent ? -1 : 1;
    if(first->container != second->container)
        return first->container < second->container ? -1 : 1;
    if(first->kind == EDITOR_SELECTION_LINE && first->item != second->item)
        return first->item > second->item ? -1 : 1;
    if(first->item == second->item) return 0;
    return first->item < second->item ? -1 : 1;
}

static bool editor_selection_removed_with_parent_check(
        const EditorSelectionRef *items, size_t count,
        EditorSelectionRef selection) {
    for(size_t i = 0; i < count; i += 1) {
        EditorSelectionRef parent = items[i];
        if(parent.object != selection.object) continue;
        if(parent.kind == EDITOR_SELECTION_OBJECT &&
                selection.kind != EDITOR_SELECTION_OBJECT) return true;
        if((parent.kind == EDITOR_SELECTION_RIGID_BODY ||
                parent.kind == EDITOR_SELECTION_PARTICLE) &&
                (selection.kind == EDITOR_SELECTION_RIGID_BODY ||
                    selection.kind == EDITOR_SELECTION_PARTICLE) &&
                parent.item == selection.item && parent.kind != selection.kind)
            return true;
        if(parent.kind == EDITOR_SELECTION_RIGID_BODY &&
                (selection.kind == EDITOR_SELECTION_HITBOX ||
                    selection.kind == EDITOR_SELECTION_VERTEX ||
                    selection.kind == EDITOR_SELECTION_LINE) &&
                parent.item == selection.parent) return true;
        if(parent.kind == EDITOR_SELECTION_HITBOX &&
                (selection.kind == EDITOR_SELECTION_VERTEX ||
                    selection.kind == EDITOR_SELECTION_LINE) &&
                parent.item == selection.container) return true;
        if(parent.kind == EDITOR_SELECTION_SOFT_BODY &&
                (selection.kind == EDITOR_SELECTION_SOFT_NODE ||
                    selection.kind == EDITOR_SELECTION_SOFT_BEAM ||
                    selection.kind == EDITOR_SELECTION_SOFT_AREA) &&
                parent.item == selection.parent) return true;
    }
    return false;
}

bool editor_navigation_multi_selection_delete(EditorProject *project,
        EditorViewportState *state, EditorHistory *history) {
    EditorSelectionRef *ordered;
    bool success = true;
    if(project == NULL || state == NULL || history == NULL ||
            state->selected_item_count < 2) return false;
    ordered = malloc(state->selected_item_count * sizeof(*ordered));
    if(ordered == NULL) return false;
    memcpy(ordered, state->selected_items,
        state->selected_item_count * sizeof(*ordered));
    qsort(ordered, state->selected_item_count, sizeof(*ordered),
        editor_selection_remove_order_compare);
    for(size_t i = 0; i < state->selected_item_count; i += 1) {
        EditorCommand command;
        if(editor_selection_removed_with_parent_check(ordered,
                state->selected_item_count, ordered[i])) continue;
        if(!editor_selection_remove_command_get(ordered[i], &command)) {
            free(ordered);
            return false;
        }
    }
    if(!editor_history_transaction_begin(history)) {
        free(ordered);
        return false;
    }
    for(size_t i = 0; i < state->selected_item_count; i += 1) {
        EditorCommand command;
        EditorCommandResult result;
        if(editor_selection_removed_with_parent_check(ordered,
                state->selected_item_count, ordered[i])) continue;
        (void)editor_selection_remove_command_get(ordered[i], &command);
        result = editor_command_execute(project, &command);
        if(result.kind == ERROR_RESULT_ERROR) {
            success = false;
            break;
        }
    }
    free(ordered);
    if(!success) {
        editor_history_transaction_cancel(history);
        return false;
    }
    if(!editor_history_transaction_end(history)) return false;
    editor_viewport_selection_clear(state);
    state->selection = EDITOR_SELECTION_NONE;
    if(state->mode != EDITOR_VIEWPORT_HIERARCHY)
        state->mode = EDITOR_VIEWPORT_OBJECT;
    return true;
}

static EditorRigidBody *editor_navigation_rigid_body_get(EditorObject *object,
        const EditorViewportState *state) {
    return object == NULL || state == NULL ? NULL :
        editor_project_rigid_body_get(object, state->selected_rigid_body);
}

static EditorHitbox *editor_navigation_hitbox_get(EditorObject *object,
        const EditorViewportState *state) {
    EditorRigidBody *body = editor_navigation_rigid_body_get(object, state);
    return body == NULL ? NULL : editor_project_hitbox_get(body, state->selected_hitbox);
}

bool editor_navigation_selected_open(EditorProject *project,
        EditorViewportState *state) {
    EditorObject *selected;
    EditorHitbox *hitbox;

    if(project == NULL || state == NULL) return false;
    selected = editor_project_selected_get(project);
    if(selected == NULL) return false;
    hitbox = editor_navigation_hitbox_get(selected, state);
    switch(state->selection) {
        case EDITOR_SELECTION_OBJECT:
            editor_viewport_object_editor_enter(state);
            return true;
        case EDITOR_SELECTION_RIGID_BODY:
            if(editor_navigation_rigid_body_get(selected, state) == NULL) return false;
            state->mode = EDITOR_VIEWPORT_RIGID_BODY;
            return true;
        case EDITOR_SELECTION_HITBOX:
            if(hitbox == NULL) return false;
            editor_viewport_hitbox_editor_enter(state);
            return true;
        case EDITOR_SELECTION_JOINT:
            state->mode = EDITOR_VIEWPORT_JOINT;
            return true;
        case EDITOR_SELECTION_ANCHOR:
            if(editor_project_anchor_get(selected, state->selected_anchor) == NULL)
                return false;
            state->mode = EDITOR_VIEWPORT_ANCHOR;
            return true;
        case EDITOR_SELECTION_SOFT_BODY:
            state->mode = EDITOR_VIEWPORT_SOFT_BODY;
            return true;
        case EDITOR_SELECTION_SOFT_NODE:
            state->mode = EDITOR_VIEWPORT_SOFT_NODE;
            return true;
        case EDITOR_SELECTION_SOFT_BEAM:
            state->mode = EDITOR_VIEWPORT_SOFT_BEAM;
            return true;
        case EDITOR_SELECTION_VERTEX:
            if(hitbox == NULL || state->selected_vertex >= hitbox->vertex_count)
                return false;
            editor_viewport_vertex_editor_enter(state, state->selected_vertex);
            return true;
        case EDITOR_SELECTION_LINE:
            if(hitbox == NULL || state->selected_line >= hitbox->vertex_count)
                return false;
            editor_viewport_line_editor_enter(state, state->selected_line);
            return true;
        default:
            return false;
    }
}

bool editor_navigation_open_item_selection_set(EditorViewportState *state) {
    if(state == NULL) return false;
    switch(state->mode) {
        case EDITOR_VIEWPORT_OBJECT:
            state->selection = EDITOR_SELECTION_OBJECT;
            return true;
        case EDITOR_VIEWPORT_RIGID_BODY:
            state->selection = EDITOR_SELECTION_RIGID_BODY;
            return true;
        case EDITOR_VIEWPORT_HITBOX:
            state->selection = EDITOR_SELECTION_HITBOX;
            return true;
        case EDITOR_VIEWPORT_VERTEX:
            state->selection = EDITOR_SELECTION_VERTEX;
            return true;
        case EDITOR_VIEWPORT_LINE:
            state->selection = EDITOR_SELECTION_LINE;
            return true;
        case EDITOR_VIEWPORT_JOINT:
            state->selection = EDITOR_SELECTION_JOINT;
            return true;
        case EDITOR_VIEWPORT_ANCHOR:
            state->selection = EDITOR_SELECTION_ANCHOR;
            return true;
        case EDITOR_VIEWPORT_SOFT_BODY:
            state->selection = EDITOR_SELECTION_SOFT_BODY;
            return true;
        case EDITOR_VIEWPORT_SOFT_NODE:
            state->selection = EDITOR_SELECTION_SOFT_NODE;
            return true;
        case EDITOR_VIEWPORT_SOFT_BEAM:
            state->selection = EDITOR_SELECTION_SOFT_BEAM;
            return true;
        default:
            return false;
    }
}

void editor_navigation_current_selection_clear(EditorProject *project,
        EditorViewportState *state) {
    if(project == NULL || state == NULL) return;
    editor_viewport_selection_clear(state);
    if(state->mode == EDITOR_VIEWPORT_HIERARCHY) {
        editor_project_selection_clear(project);
        state->selection = EDITOR_SELECTION_NONE;
        return;
    }
    (void)editor_navigation_open_item_selection_set(state);
}
