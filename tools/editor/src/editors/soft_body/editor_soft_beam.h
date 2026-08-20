/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef EDITOR_SOFT_BEAM_H
#define EDITOR_SOFT_BEAM_H

#include "editors/editor_mode_context.h"

typedef struct EditorSoftBeamEditor {
    FontAsset *font;
    TextAsset name_label, node_a_label, node_b_label;
    TextAsset stiffness_label, damping_label, color_label, inherit_label;
    TextAsset none_label, visible_label, hidden_label, delete_label;
    TextAsset stiffness_field, damping_field;
    TextAsset beam_names[EDITOR_SOFT_BEAM_MAX];
    TextAsset node_names[EDITOR_SOFT_NODE_MAX];
    char beam_cache[EDITOR_SOFT_BEAM_MAX][EDITOR_OBJECT_NAME_MAX];
    char node_cache[EDITOR_SOFT_NODE_MAX][EDITOR_OBJECT_NAME_MAX];
} EditorSoftBeamEditor;

bool editor_soft_beam_editor_create(EditorSoftBeamEditor *editor,
    FontAsset *font);
void editor_soft_beam_editor_destroy(EditorSoftBeamEditor *editor);
bool editor_soft_beam_editor_draw(EditorSoftBeamEditor *editor,
    const EditorModeContext *context);

#endif
