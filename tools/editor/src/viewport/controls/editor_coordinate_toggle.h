/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef EDITOR_COORDINATE_TOGGLE_H
#define EDITOR_COORDINATE_TOGGLE_H

#include "editor_project.h"
#include "editor_viewport.h"

typedef struct EditorCoordinateToggle {
    TextAsset world_label;
    TextAsset local_label;
} EditorCoordinateToggle;

bool editor_coordinate_toggle_create(EditorCoordinateToggle *toggle,
    FontAsset *font);
void editor_coordinate_toggle_destroy(EditorCoordinateToggle *toggle);
void editor_coordinate_toggle_draw(EditorCoordinateToggle *toggle,
    EditorProject *project, const EditorViewportState *viewport,
    float menu_height);

#endif
