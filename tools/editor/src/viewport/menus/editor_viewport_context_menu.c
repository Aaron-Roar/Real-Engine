#include "editor_viewport_context_menu.h"

#include <math.h>
#include <stdio.h>

static bool text_create(FontAsset *font, const char *value, TextAsset *output) {
    TextAssetResult result = rohr_graphics_text_create(font, value,
        (Color){235, 238, 245, 255});
    if(rohr_error_check(result)) return false;
    *output = result.result.value;
    return true;
}

bool editor_viewport_context_menu_create(EditorViewportContextMenu *menu,
        FontAsset *font) {
    if(menu == NULL || font == NULL) return false;
    *menu = (EditorViewportContextMenu){0};
    if(!text_create(font, "Action 1", &menu->action_labels[0]) ||
            !text_create(font, "Action 2", &menu->action_labels[1]) ||
            !text_create(font, "Action 3", &menu->action_labels[2])) {
        editor_viewport_context_menu_destroy(menu);
        return false;
    }
    return true;
}

void editor_viewport_context_menu_destroy(EditorViewportContextMenu *menu) {
    if(menu == NULL) return;
    for(size_t i = 0; i < 3; i += 1)
        rohr_graphics_text_destroy(&menu->action_labels[i]);
    *menu = (EditorViewportContextMenu){0};
}

void editor_viewport_context_menu_draw(EditorViewportContextMenu *menu,
        const MouseState *mouse, float viewport_width, float menu_height,
        float viewport_bottom, float window_height) {
    Position pointer;
    UIRect bounds;
    if(menu == NULL || mouse == NULL) return;
    pointer = rohr_graphics_mouse_screen_position_get();
    if(mouse->button_states[MOUSE_BUTTON_RIGHT] == MOUSE_BUTTON_STATE_PRESSED &&
            pointer.x >= 0.0f && pointer.x < viewport_width &&
            pointer.y >= menu_height && pointer.y < viewport_bottom) {
        menu->open = true;
        menu->position = (Position){fminf(pointer.x, viewport_width - 150.0f),
            fminf(pointer.y, window_height - 104.0f)};
    }
    if(!menu->open) return;
    bounds = (UIRect){menu->position.x, menu->position.y, 144.0f, 100.0f};
    rohr_ui_surface(bounds, (Color){24, 27, 34, 255});
    rohr_ui_border(bounds, 2.0f, (Color){0, 0, 0, 255});
    bool close = false;
    for(size_t i = 0; i < 3; i += 1) {
        char id[48];
        snprintf(id, sizeof(id), "editor.viewport.context.action_%zu", i + 1);
        close = rohr_ui_button(id, &menu->action_labels[i],
            (UIRect){bounds.x + 4.0f, bounds.y + 4.0f + (float)i * 32.0f,
                bounds.width - 8.0f, 28.0f}, NULL).clicked || close;
    }
    if(mouse->button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_PRESSED &&
            (pointer.x < bounds.x || pointer.x > bounds.x + bounds.width ||
             pointer.y < bounds.y || pointer.y > bounds.y + bounds.height))
        close = true;
    if(close) menu->open = false;
}

void editor_viewport_context_menu_close(EditorViewportContextMenu *menu) {
    if(menu != NULL) menu->open = false;
}

bool editor_viewport_context_menu_open_check(
        const EditorViewportContextMenu *menu) {
    return menu != NULL && menu->open;
}
