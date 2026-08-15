#include "editor_navigation.h"

#include <stdlib.h>
#include <string.h>

static bool editor_selection_remove_command_get(EditorSelectionRef selection,
        EditorCommand *command) {
    EditorItemKind kind;
    if(command == NULL) return false;
    switch(selection.kind) {
        case EDITOR_SELECTION_OBJECT: kind = EDITOR_ITEM_OBJECT; break;
        case EDITOR_SELECTION_RIGID_BODY: kind = EDITOR_ITEM_RIGID_BODY; break;
        case EDITOR_SELECTION_HITBOX: kind = EDITOR_ITEM_HITBOX; break;
        case EDITOR_SELECTION_JOINT: kind = EDITOR_ITEM_JOINT; break;
        case EDITOR_SELECTION_ANCHOR: kind = EDITOR_ITEM_ANCHOR; break;
        case EDITOR_SELECTION_SOFT_BODY: kind = EDITOR_ITEM_SOFT_BODY; break;
        case EDITOR_SELECTION_SOFT_NODE: kind = EDITOR_ITEM_SOFT_NODE; break;
        case EDITOR_SELECTION_SOFT_BEAM: kind = EDITOR_ITEM_SOFT_BEAM; break;
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
