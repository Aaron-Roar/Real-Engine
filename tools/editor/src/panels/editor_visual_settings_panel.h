/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_EDITOR_VISUAL_SETTINGS_PANEL_H
#define ROHR_EDITOR_VISUAL_SETTINGS_PANEL_H

#include "rohr.h"
#include "editor_config.h"

typedef struct EditorVisualSettingsPanel {
    bool open;
    size_t aspect_index;
    size_t resolution_index;
    size_t window_mode_index;
    EditorGuiState state;
    char state_path[EDITOR_WORKSPACE_PATH_MAX * 2];
    bool state_ready;
    TextAsset menu_label;
    TextAsset title;
    TextAsset aspect_label;
    TextAsset resolution_label;
    TextAsset window_mode_label;
    TextAsset aspect_options[5];
    TextAsset resolution_options[3];
    TextAsset window_mode_options[3];
    TextAsset close_label;
} EditorVisualSettingsPanel;

bool editor_visual_settings_panel_create(EditorVisualSettingsPanel *panel,
    const FontAsset *font);
bool editor_visual_settings_panel_state_set(EditorVisualSettingsPanel *panel,
    const EditorGuiState *state, const char *path);
EditorResult editor_visual_settings_panel_grid_visible_set(
    EditorVisualSettingsPanel *panel, bool visible);
void editor_visual_settings_panel_open(EditorVisualSettingsPanel *panel);
void editor_visual_settings_panel_draw(EditorVisualSettingsPanel *panel,
    UIRect bounds);
void editor_visual_settings_panel_destroy(EditorVisualSettingsPanel *panel);

#endif
