/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_EDITOR_MODE_CONTROLS_H
#define ROHR_EDITOR_MODE_CONTROLS_H

#include "editors/editor_mode_context.h"

bool editor_mode_text_create(FontAsset *font, const char *value,
    TextAsset *output);
void editor_mode_numeric_disabled_draw(TextAsset *display, float value,
    UIRect bounds);
bool editor_mode_checkbox_left(const char *id, const TextAsset *label,
    UIRect bounds, bool *checked);
bool editor_mode_color_swatch(const char *id, uint32_t *color, bool disabled,
    UIRect bounds,
    const EditorModeContext *context, EditorItemKind kind,
    EditorObjectId object, uint32_t parent, uint32_t item,
    EditorPropertyKind property);
bool editor_mode_named_text_sync(FontAsset *font, const char *name,
    TextAsset *label, char *cache, size_t cache_capacity);
UIFieldResult editor_mode_name_field(const char *id, char *name,
    size_t capacity, TextAsset *display, UIRect bounds);
UIButtonStyle editor_mode_delete_style_get(void);

#endif
