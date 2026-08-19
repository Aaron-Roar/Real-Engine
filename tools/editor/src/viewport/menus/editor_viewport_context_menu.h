#ifndef EDITOR_VIEWPORT_CONTEXT_MENU_H
#define EDITOR_VIEWPORT_CONTEXT_MENU_H

#include "rohr.h"

typedef struct EditorViewportContextMenu {
    TextAsset action_labels[3];
    bool open;
    Position position;
} EditorViewportContextMenu;

bool editor_viewport_context_menu_create(EditorViewportContextMenu *menu,
    FontAsset *font);
void editor_viewport_context_menu_destroy(EditorViewportContextMenu *menu);
void editor_viewport_context_menu_draw(EditorViewportContextMenu *menu,
    const MouseState *mouse, float viewport_width, float menu_height,
    float viewport_bottom, float window_height);
void editor_viewport_context_menu_close(EditorViewportContextMenu *menu);
bool editor_viewport_context_menu_open_check(
    const EditorViewportContextMenu *menu);

#endif
