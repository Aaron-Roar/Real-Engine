#ifndef ROHR_EDITOR_NAVIGATION_H
#define ROHR_EDITOR_NAVIGATION_H

#include "editor_viewport.h"

bool editor_navigation_selected_open(EditorProject *project,
    EditorViewportState *state);
bool editor_navigation_open_item_selection_set(EditorViewportState *state);
void editor_navigation_current_selection_clear(EditorProject *project,
    EditorViewportState *state);

#endif
