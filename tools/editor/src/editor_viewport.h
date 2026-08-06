#ifndef ROHR_EDITOR_VIEWPORT_H
#define ROHR_EDITOR_VIEWPORT_H

#include "editor_project.h"

typedef struct EditorViewportState {
    int dragged_vertex;
} EditorViewportState;

void editor_viewport_state_init(EditorViewportState *state);
void editor_viewport_update(
    EditorViewportState *state,
    EditorProject *project,
    Position pointer,
    MouseButtonState primary_button,
    bool pointer_consumed
);
void editor_viewport_draw(const EditorProject *project);

#endif
