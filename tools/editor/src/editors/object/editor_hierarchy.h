/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef EDITOR_HIERARCHY_H
#define EDITOR_HIERARCHY_H

#include "editors/editor_mode_context.h"

typedef struct EditorHierarchyEditor {
    FontAsset *font;
    TextAsset add_object_label, visible_label, hidden_label;
    TextAsset object_names[EDITOR_OBJECT_MAX];
    char object_cache[EDITOR_OBJECT_MAX][EDITOR_OBJECT_NAME_MAX];
} EditorHierarchyEditor;

bool editor_hierarchy_editor_create(EditorHierarchyEditor *editor,
    FontAsset *font);
void editor_hierarchy_editor_destroy(EditorHierarchyEditor *editor);
void editor_hierarchy_editor_draw(EditorHierarchyEditor *editor,
    const EditorModeContext *context);

#endif
