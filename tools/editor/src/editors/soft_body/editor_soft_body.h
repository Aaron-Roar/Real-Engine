/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef EDITOR_SOFT_BODY_H
#define EDITOR_SOFT_BODY_H

#include "editors/editor_mode_context.h"
#include "editors/geometry/editor_auto_shape.h"

typedef struct EditorSoftBodyEditor {
    FontAsset *font;
    TextAsset name_label, x_label, y_label, rotation_label;
    TextAsset node_color_label, beam_color_label, area_color_label;
    TextAsset origin_label, auto_shape_label, add_node_label, add_beam_label;
    TextAsset visible_label, hidden_label, delete_label;
    TextAsset x_field, y_field, rotation_field;
    TextAsset body_names[EDITOR_SOFT_BODY_MAX];
    TextAsset node_names[EDITOR_SOFT_NODE_MAX];
    TextAsset beam_names[EDITOR_SOFT_BEAM_MAX];
    TextAsset area_names[EDITOR_SOFT_AREA_MAX];
    char body_cache[EDITOR_SOFT_BODY_MAX][EDITOR_OBJECT_NAME_MAX];
    char node_cache[EDITOR_SOFT_NODE_MAX][EDITOR_OBJECT_NAME_MAX];
    char beam_cache[EDITOR_SOFT_BEAM_MAX][EDITOR_OBJECT_NAME_MAX];
    char area_cache[EDITOR_SOFT_AREA_MAX][EDITOR_OBJECT_NAME_MAX];
    bool auto_shape_picker_open;
} EditorSoftBodyEditor;

bool editor_soft_body_editor_create(EditorSoftBodyEditor *editor,
    FontAsset *font);
void editor_soft_body_editor_destroy(EditorSoftBodyEditor *editor);
bool editor_soft_body_editor_draw(EditorSoftBodyEditor *editor,
    EditorAutoShapeEditor *auto_shape, const EditorModeContext *context);

#endif
