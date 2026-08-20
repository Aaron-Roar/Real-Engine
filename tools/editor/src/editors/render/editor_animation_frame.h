/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef EDITOR_ANIMATION_FRAME_H
#define EDITOR_ANIMATION_FRAME_H

#include "editors/editor_mode_context.h"

typedef struct EditorAnimationFrameEditor {
    FontAsset *font;
    TextAsset name_label;
    TextAsset path_label;
    TextAsset width_label;
    TextAsset height_label;
    TextAsset delete_label;
    TextAsset name_field;
    TextAsset path_field;
    TextAsset width_field;
    TextAsset height_field;
    char name_cache[EDITOR_OBJECT_NAME_MAX];
} EditorAnimationFrameEditor;

bool editor_animation_frame_editor_create(EditorAnimationFrameEditor *editor,
    FontAsset *font);
void editor_animation_frame_editor_destroy(EditorAnimationFrameEditor *editor);
bool editor_animation_frame_editor_draw(EditorAnimationFrameEditor *editor,
    const EditorModeContext *context);

#endif
