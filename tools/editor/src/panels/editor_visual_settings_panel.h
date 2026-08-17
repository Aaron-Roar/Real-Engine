#ifndef ROHR_EDITOR_VISUAL_SETTINGS_PANEL_H
#define ROHR_EDITOR_VISUAL_SETTINGS_PANEL_H

#include "rohr.h"

typedef struct EditorVisualSettingsPanel {
    bool open;
    size_t aspect_index;
    size_t resolution_index;
    size_t window_mode_index;
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
void editor_visual_settings_panel_open(EditorVisualSettingsPanel *panel);
void editor_visual_settings_panel_draw(EditorVisualSettingsPanel *panel,
    UIRect bounds);
void editor_visual_settings_panel_destroy(EditorVisualSettingsPanel *panel);

#endif
