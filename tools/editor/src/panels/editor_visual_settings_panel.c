/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_visual_settings_panel.h"
#include "editor_layout.h"

#include <stdio.h>
#include <string.h>

static bool editor_visual_text_create(const FontAsset *font, const char *value,
        TextAsset *text) {
    TextAssetResult result = rohr_graphics_text_create(font, value,
        (Color){235, 238, 244, 255});
    if(rohr_error_check(result)) return false;
    *text = result.result.value;
    return true;
}

static void editor_visual_resolution_labels_update(
        EditorVisualSettingsPanel *panel) {
    static const int heights[] = {720, 1080, 1440};
    static const int ratios[][2] = {{0, 0}, {16, 9}, {16, 10}, {4, 3}, {21, 9}};
    char value[32];
    for(size_t i = 0; i < 3; i += 1) {
        if(panel->aspect_index == 0) {
            snprintf(value, sizeof(value), "Auto x %d", heights[i]);
        } else {
            int width = (int)((float)heights[i] *
                (float)ratios[panel->aspect_index][0] /
                (float)ratios[panel->aspect_index][1] + 0.5f);
            snprintf(value, sizeof(value), "%dx%d", width, heights[i]);
        }
        (void)rohr_graphics_text_value_set(&panel->resolution_options[i], value);
    }
}

static void editor_visual_settings_apply(EditorVisualSettingsPanel *panel) {
    static const int heights[] = {720, 1080, 1440};
    static const int ratios[][2] = {{0, 0}, {16, 9}, {16, 10}, {4, 3}, {21, 9}};
    static const char *ratio_names[] = {"auto", "16:9", "16:10", "4:3", "21:9"};
    static const char *mode_names[] = {
        "windowed", "borderless_fullscreen", "fullscreen"};
    int height = heights[panel->resolution_index];
    int ratio = panel->aspect_index == 0 ? 1 : (int)panel->aspect_index;
    int width = panel->aspect_index == 0 ? height :
        (int)((float)height * (float)ratios[ratio][0] /
            (float)ratios[ratio][1] + 0.5f);
    GraphicsWindowPresentationConfig config =
        rohr_graphics_window_presentation_default_get();
    config.mode = (GraphicsWindowMode)panel->window_mode_index;
    config.window_width = width;
    config.window_height = height;
    config.logical_width = width;
    config.logical_height = height;
    config.aspect_ratio_auto = panel->aspect_index == 0;
    if(rohr_error_check(rohr_graphics_window_presentation_set(config))) return;
    editor_window_height = (float)height;
    if(panel->state_ready) {
        EditorResult result;
        panel->state.logical_width = width;
        panel->state.logical_height = height;
        snprintf(panel->state.aspect_ratio, sizeof(panel->state.aspect_ratio), "%s",
            ratio_names[panel->aspect_index]);
        snprintf(panel->state.window_mode, sizeof(panel->state.window_mode), "%s",
            mode_names[panel->window_mode_index]);
        result = editor_gui_state_save(&panel->state, panel->state_path);
        if(editor_result_check(result))
            fprintf(stderr, "%s\n", result.result.error.message);
    }
}

bool editor_visual_settings_panel_create(EditorVisualSettingsPanel *panel,
        const FontAsset *font) {
    static const char *aspects[] = {"Auto", "16:9", "16:10", "4:3", "21:9"};
    static const char *window_modes[] = {
        "Windowed", "Borderless Fullscreen", "Fullscreen"};
    if(panel == NULL || font == NULL) return false;
    *panel = (EditorVisualSettingsPanel){0};
    if(!editor_visual_text_create(font, "Visuals", &panel->menu_label) ||
            !editor_visual_text_create(font, "Visual Settings", &panel->title) ||
            !editor_visual_text_create(font, "Aspect Ratio", &panel->aspect_label) ||
            !editor_visual_text_create(font, "Logical Resolution",
                &panel->resolution_label) ||
            !editor_visual_text_create(font, "Window Mode",
                &panel->window_mode_label) ||
            !editor_visual_text_create(font, "Close", &panel->close_label)) goto fail;
    for(size_t i = 0; i < 5; i += 1)
        if(!editor_visual_text_create(font, aspects[i], &panel->aspect_options[i]))
            goto fail;
    for(size_t i = 0; i < 3; i += 1)
        if(!editor_visual_text_create(font, "", &panel->resolution_options[i]))
            goto fail;
    for(size_t i = 0; i < 3; i += 1)
        if(!editor_visual_text_create(font, window_modes[i],
                &panel->window_mode_options[i])) goto fail;
    editor_visual_resolution_labels_update(panel);
    return true;
fail:
    editor_visual_settings_panel_destroy(panel);
    return false;
}

bool editor_visual_settings_panel_state_set(EditorVisualSettingsPanel *panel,
        const EditorGuiState *state, const char *path) {
    static const char *ratios[] = {"auto", "16:9", "16:10", "4:3", "21:9"};
    static const char *modes[] = {
        "windowed", "borderless_fullscreen", "fullscreen"};
    static const int heights[] = {720, 1080, 1440};
    if(panel == NULL || state == NULL || path == NULL ||
            strlen(path) >= sizeof(panel->state_path)) return false;
    for(size_t i = 0; i < 5; i += 1) {
        if(strcmp(state->aspect_ratio, ratios[i]) == 0) panel->aspect_index = i;
    }
    for(size_t i = 0; i < 3; i += 1) {
        if(strcmp(state->window_mode, modes[i]) == 0) panel->window_mode_index = i;
        if(state->logical_height == heights[i]) panel->resolution_index = i;
    }
    panel->state = *state;
    snprintf(panel->state_path, sizeof(panel->state_path), "%s", path);
    panel->state_ready = true;
    editor_visual_resolution_labels_update(panel);
    return true;
}

EditorResult editor_visual_settings_panel_grid_visible_set(
        EditorVisualSettingsPanel *panel, bool visible) {
    if(panel == NULL || !panel->state_ready) return editor_result_error(
        EDITOR_ERROR_INVALID_ARGUMENT,
        "Grid visibility requires initialized editor GUI state");
    panel->state.grid_visible = visible;
    return editor_gui_state_save(&panel->state, panel->state_path);
}

void editor_visual_settings_panel_open(EditorVisualSettingsPanel *panel) {
    if(panel != NULL) panel->open = true;
}

void editor_visual_settings_panel_draw(EditorVisualSettingsPanel *panel,
        UIRect bounds) {
    const TextAsset *aspect_options[5];
    const TextAsset *resolution_options[3];
    const TextAsset *window_mode_options[3];
    UIDropdownResult result;
    if(panel == NULL || !panel->open) return;
    for(size_t i = 0; i < 5; i += 1) aspect_options[i] = &panel->aspect_options[i];
    for(size_t i = 0; i < 3; i += 1)
        resolution_options[i] = &panel->resolution_options[i];
    for(size_t i = 0; i < 3; i += 1)
        window_mode_options[i] = &panel->window_mode_options[i];
    rohr_ui_modal_controls_begin();
    rohr_ui_surface(bounds, (Color){37, 42, 52, 255});
    rohr_ui_border(bounds, 2.0f, (Color){5, 6, 8, 255});
    rohr_ui_label(&panel->title,
        (UIRect){bounds.x + 20.0f, bounds.y + 14.0f, bounds.width - 40.0f, 32.0f});
    rohr_ui_label(&panel->window_mode_label,
        (UIRect){bounds.x + 28.0f, bounds.y + 76.0f, 180.0f, 34.0f});
    result = rohr_ui_dropdown("editor.settings.visual.window_mode",
        window_mode_options, 3, panel->window_mode_index,
        (UIRect){bounds.x + 220.0f, bounds.y + 76.0f, 220.0f, 34.0f}, NULL);
    if(result.changed) {
        panel->window_mode_index = result.selected_index;
        editor_visual_settings_apply(panel);
    }
    rohr_ui_label(&panel->aspect_label,
        (UIRect){bounds.x + 28.0f, bounds.y + 128.0f, 180.0f, 34.0f});
    result = rohr_ui_dropdown("editor.settings.visual.aspect",
        aspect_options, 5, panel->aspect_index,
        (UIRect){bounds.x + 220.0f, bounds.y + 128.0f, 220.0f, 34.0f}, NULL);
    if(result.changed) {
        panel->aspect_index = result.selected_index;
        editor_visual_resolution_labels_update(panel);
        editor_visual_settings_apply(panel);
    }
    rohr_ui_label(&panel->resolution_label,
        (UIRect){bounds.x + 28.0f, bounds.y + 180.0f, 180.0f, 34.0f});
    result = rohr_ui_dropdown("editor.settings.visual.resolution",
        resolution_options, 3, panel->resolution_index,
        (UIRect){bounds.x + 220.0f, bounds.y + 180.0f, 220.0f, 34.0f}, NULL);
    if(result.changed) {
        panel->resolution_index = result.selected_index;
        editor_visual_settings_apply(panel);
    }
    if(rohr_ui_button("editor.settings.visual.close", &panel->close_label,
            (UIRect){bounds.x + bounds.width - 124.0f,
                bounds.y + bounds.height - 48.0f, 100.0f, 32.0f}, NULL).clicked)
        panel->open = false;
    rohr_ui_modal_controls_end();
}

void editor_visual_settings_panel_destroy(EditorVisualSettingsPanel *panel) {
    if(panel == NULL) return;
    rohr_graphics_text_destroy(&panel->menu_label);
    rohr_graphics_text_destroy(&panel->title);
    rohr_graphics_text_destroy(&panel->aspect_label);
    rohr_graphics_text_destroy(&panel->resolution_label);
    rohr_graphics_text_destroy(&panel->window_mode_label);
    for(size_t i = 0; i < 5; i += 1)
        rohr_graphics_text_destroy(&panel->aspect_options[i]);
    for(size_t i = 0; i < 3; i += 1)
        rohr_graphics_text_destroy(&panel->resolution_options[i]);
    for(size_t i = 0; i < 3; i += 1)
        rohr_graphics_text_destroy(&panel->window_mode_options[i]);
    rohr_graphics_text_destroy(&panel->close_label);
    *panel = (EditorVisualSettingsPanel){0};
}
