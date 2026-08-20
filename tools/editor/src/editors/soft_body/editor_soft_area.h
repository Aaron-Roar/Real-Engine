/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef EDITOR_SOFT_AREA_H
#define EDITOR_SOFT_AREA_H

#include "editors/editor_mode_context.h"

typedef struct EditorSoftAreaEditor {
    FontAsset *font;
    TextAsset name_label, area_color_label, beam_color_label, inherit_label;
    TextAsset area_names[EDITOR_SOFT_AREA_MAX];
    TextAsset beam_names[EDITOR_SOFT_BEAM_MAX];
    char area_cache[EDITOR_SOFT_AREA_MAX][EDITOR_OBJECT_NAME_MAX];
    char beam_cache[EDITOR_SOFT_BEAM_MAX][EDITOR_OBJECT_NAME_MAX];
    uint32_t boundary_beam_color;
    EditorSoftAreaId boundary_color_area;
} EditorSoftAreaEditor;

bool editor_soft_area_editor_create(EditorSoftAreaEditor *editor,
    FontAsset *font);
void editor_soft_area_editor_destroy(EditorSoftAreaEditor *editor);
bool editor_soft_area_editor_draw(EditorSoftAreaEditor *editor,
    const EditorModeContext *context);

#endif
