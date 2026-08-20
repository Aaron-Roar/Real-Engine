/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_project_launcher_state.h"

#include "editor_layout.h"

void editor_project_launcher_state_init(EditorProjectLauncherState *state,
        const TextAsset *new_project_label,
        const TextAsset *load_project_label) {
    if(state == NULL) return;
    state->new_project_label = new_project_label;
    state->load_project_label = load_project_label;
}

EditorProjectLauncherRequest editor_project_launcher_state_draw(
        const EditorProjectLauncherState *state,
        float window_width,
        float viewport_bottom) {
    UIRect dialog;

    if(state == NULL || state->new_project_label == NULL ||
            state->load_project_label == NULL)
        return EDITOR_PROJECT_LAUNCHER_REQUEST_NONE;
    dialog = (UIRect){
        window_width * 0.5f - 230.0f,
        EDITOR_MENU_HEIGHT +
            (viewport_bottom - EDITOR_MENU_HEIGHT) * 0.5f - 90.0f,
        460.0f,
        180.0f
    };
    rohr_ui_surface((UIRect){0.0f, EDITOR_MENU_HEIGHT, window_width,
        viewport_bottom - EDITOR_MENU_HEIGHT}, (Color){12, 14, 18, 238});
    rohr_ui_surface(dialog, (Color){42, 47, 58, 255});
    rohr_ui_border(dialog, 2.0f, (Color){8, 9, 12, 255});
    if(rohr_ui_button("editor.start.new", state->new_project_label,
            (UIRect){dialog.x + 30.0f, dialog.y + 58.0f, 190.0f, 58.0f},
            NULL).clicked)
        return EDITOR_PROJECT_LAUNCHER_REQUEST_NEW_PROJECT;
    if(rohr_ui_button("editor.start.load", state->load_project_label,
            (UIRect){dialog.x + 240.0f, dialog.y + 58.0f, 190.0f, 58.0f},
            NULL).clicked)
        return EDITOR_PROJECT_LAUNCHER_REQUEST_LOAD_PROJECT;
    return EDITOR_PROJECT_LAUNCHER_REQUEST_NONE;
}
