/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_coordinate_toggle.h"

#include "editor_command.h"

#include <string.h>

static bool text_create(FontAsset *font, const char *value, TextAsset *output) {
    TextAssetResult result = rohr_graphics_text_create(font, value,
        (Color){235, 238, 245, 255});
    if(rohr_error_check(result)) return false;
    *output = result.result.value;
    return true;
}

bool editor_coordinate_toggle_create(EditorCoordinateToggle *toggle,
        FontAsset *font) {
    if(toggle == NULL || font == NULL) return false;
    *toggle = (EditorCoordinateToggle){0};
    if(!text_create(font, "World", &toggle->world_label) ||
            !text_create(font, "Local", &toggle->local_label)) {
        editor_coordinate_toggle_destroy(toggle);
        return false;
    }
    return true;
}

void editor_coordinate_toggle_destroy(EditorCoordinateToggle *toggle) {
    if(toggle == NULL) return;
    rohr_graphics_text_destroy(&toggle->world_label);
    rohr_graphics_text_destroy(&toggle->local_label);
    *toggle = (EditorCoordinateToggle){0};
}

void editor_coordinate_toggle_draw(EditorCoordinateToggle *toggle,
        EditorProject *project, const EditorViewportState *viewport,
        float menu_height) {
    if(toggle == NULL || project == NULL || viewport == NULL ||
            viewport->mode == EDITOR_VIEWPORT_HIERARCHY) return;
    if(rohr_ui_button("editor.viewport.coordinates",
            project->viewport_local_view ? &toggle->local_label :
                &toggle->world_label,
            (UIRect){10.0f, menu_height + 10.0f, 84.0f, 30.0f}, NULL).clicked) {
        EditorCommand command = {.type = EDITOR_COMMAND_VIEWPORT_COORDINATES,
            .data.viewport_coordinates.local = !project->viewport_local_view};
        (void)editor_command_execute(project, &command);
    }
}
