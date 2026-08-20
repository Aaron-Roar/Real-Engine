/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef EDITOR_OBJECT_H
#define EDITOR_OBJECT_H

#include "editors/editor_mode_context.h"

typedef void (*EditorSpriteBrowserOpenFunction)(void *context,
    EditorObjectId object);

typedef struct EditorObjectEditor {
    FontAsset *font;
    TextAsset object_name_label, add_rigid_body_label, add_joint_label;
    TextAsset add_soft_body_label, add_sprite_label, add_animation_label;
    TextAsset visible_label, hidden_label, delete_label;
    TextAsset object_names[EDITOR_OBJECT_MAX];
    TextAsset rigid_body_names[EDITOR_RIGID_BODY_MAX];
    TextAsset joint_names[EDITOR_JOINT_MAX];
    TextAsset soft_body_names[EDITOR_SOFT_BODY_MAX];
    TextAsset sprite_names[64], animation_names[32];
    char object_cache[EDITOR_OBJECT_MAX][EDITOR_OBJECT_NAME_MAX];
    char rigid_body_cache[EDITOR_RIGID_BODY_MAX][EDITOR_OBJECT_NAME_MAX];
    char joint_cache[EDITOR_JOINT_MAX][EDITOR_OBJECT_NAME_MAX];
    char soft_body_cache[EDITOR_SOFT_BODY_MAX][EDITOR_OBJECT_NAME_MAX];
    char sprite_cache[64][EDITOR_OBJECT_NAME_MAX];
    char animation_cache[32][EDITOR_OBJECT_NAME_MAX];
} EditorObjectEditor;

bool editor_object_editor_create(EditorObjectEditor *editor, FontAsset *font);
void editor_object_editor_destroy(EditorObjectEditor *editor);
bool editor_object_editor_draw(EditorObjectEditor *editor,
    const EditorModeContext *context, EditorSpriteBrowserOpenFunction browser_open,
    void *browser_context, bool additive_selection);

#endif
