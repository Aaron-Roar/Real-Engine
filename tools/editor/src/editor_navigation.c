#include "editor_navigation.h"

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
    if(state->mode == EDITOR_VIEWPORT_HIERARCHY) {
        editor_project_selection_clear(project);
        state->selection = EDITOR_SELECTION_NONE;
        return;
    }
    (void)editor_navigation_open_item_selection_set(state);
}
