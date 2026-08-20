/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef EDITOR_ANIMATED_SPRITE_H
#define EDITOR_ANIMATED_SPRITE_H

#include "editors/editor_mode_context.h"
#include "editors/render/editor_sprite.h"

typedef void (*EditorAnimationFrameBrowserOpenFunction)(void *context,
    EditorObjectId object, EditorAnimatedSpriteId sprite);

typedef struct EditorAnimatedSpriteEditor {
    FontAsset *font;
    TextAsset name_label, body_label, x_label, y_label, rotation_label;
    TextAsset scale_x_label, scale_y_label, ticks_label, time_label;
    TextAsset starting_label, direction_label, left_label, right_label;
    TextAsset follow_label, playing_label, add_frame_label;
    TextAsset visible_label, hidden_label, none_label, delete_label;
    TextAsset name_values[32], body_names[EDITOR_RIGID_BODY_MAX];
    TextAsset frame_names[64];
    TextAsset x_field, y_field, rotation_field, scale_x_field, scale_y_field;
    TextAsset ticks_field, time_field, starting_field;
    char name_cache[32][EDITOR_OBJECT_NAME_MAX];
    char body_cache[EDITOR_RIGID_BODY_MAX][EDITOR_OBJECT_NAME_MAX];
    char frame_cache[64][EDITOR_OBJECT_NAME_MAX];
} EditorAnimatedSpriteEditor;

bool editor_animated_sprite_editor_create(EditorAnimatedSpriteEditor *editor,
    FontAsset *font);
void editor_animated_sprite_editor_destroy(EditorAnimatedSpriteEditor *editor);
bool editor_animated_sprite_editor_draw(EditorAnimatedSpriteEditor *editor,
    const EditorModeContext *context, EditorBodyPreviewFunction preview,
    void *preview_context, EditorAnimationFrameBrowserOpenFunction browser_open,
    void *browser_context, bool additive_selection,
    bool *frame_multi_edit_open);

#endif
