/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_EDITOR_VERTEX_H
#define ROHR_EDITOR_VERTEX_H

#include "editors/editor_mode_context.h"

typedef struct EditorVertexEditor {
    FontAsset *font;
    TextAsset name_label;
    TextAsset lock_label;
    TextAsset unlock_label;
    TextAsset x_label;
    TextAsset y_label;
    TextAsset x_field;
    TextAsset y_field;
    TextAsset delete_label;
    TextAsset vertex_labels[EDITOR_HITBOX_VERTEX_MAX];
    char vertex_name_cache[EDITOR_HITBOX_VERTEX_MAX][EDITOR_OBJECT_NAME_MAX];
} EditorVertexEditor;

bool editor_vertex_editor_create(EditorVertexEditor *editor, FontAsset *font);
void editor_vertex_editor_destroy(EditorVertexEditor *editor);
bool editor_vertex_editor_draw(EditorVertexEditor *editor,
    const EditorModeContext *context);

#endif
