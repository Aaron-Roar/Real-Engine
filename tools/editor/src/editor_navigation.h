/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_EDITOR_NAVIGATION_H
#define ROHR_EDITOR_NAVIGATION_H

#include "editor_viewport.h"
#include "editor_history.h"

bool editor_navigation_selected_open(EditorProject *project,
    EditorViewportState *state);
bool editor_navigation_open_item_selection_set(EditorViewportState *state);
void editor_navigation_current_selection_clear(EditorProject *project,
    EditorViewportState *state);
bool editor_navigation_multi_selection_delete(EditorProject *project,
    EditorViewportState *state, EditorHistory *history);
bool editor_navigation_selection_reorder(EditorProject *project,
    EditorViewportState *state, EditorSelectionRef source,
    EditorSelectionRef target, bool after, EditorHistory *history);
bool editor_navigation_viewport_transform_history_update(EditorProject *project,
    EditorViewportState *state, EditorHistory *history, bool was_active);

#endif
