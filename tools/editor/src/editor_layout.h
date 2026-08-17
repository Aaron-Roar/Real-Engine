#ifndef ROHR_EDITOR_LAYOUT_H
#define ROHR_EDITOR_LAYOUT_H

#include "rohr.h"

#define EDITOR_VIEWPORT_WIDTH editor_viewport_width
#define EDITOR_TOOLS_WIDTH (editor_window_width - editor_viewport_width)
#define EDITOR_WINDOW_HEIGHT editor_window_height
#define EDITOR_MENU_HEIGHT 34.0f
#define EDITOR_ACTION_BAR_HEIGHT 28.0f
#define EDITOR_ACTION_BAR_TOP (EDITOR_WINDOW_HEIGHT - EDITOR_ACTION_BAR_HEIGHT)
#define EDITOR_VIEWPORT_BOTTOM editor_viewport_bottom

enum {
    EDITOR_GRAPHICS_LAYER_CONTENT = 0,
    EDITOR_GRAPHICS_LAYER_OVERLAY = 100,
    EDITOR_GRAPHICS_LAYER_TOP_MENU = 200,
    EDITOR_GRAPHICS_LAYER_MODAL = 300,
    EDITOR_GRAPHICS_LAYER_NOTIFICATION = 400
};

extern float editor_viewport_width;
extern float editor_window_width;
extern float editor_window_height;
extern float editor_viewport_bottom;

#endif
