#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "controller.h"
#include "graphics.h"

/** Axis-aligned rectangle in logical screen coordinates. */
typedef struct UIRect {
    float x;
    float y;
    float width;
    float height;
} UIRect;

/** Pointer input consumed by the UI during one frame. */
typedef struct UIInput {
    Position pointer;
    MouseButtonState primary_button;
} UIInput;

/** Colors used to draw a button in each interaction state. */
typedef struct UIButtonStyle {
    Color idle;
    Color hovered;
    Color pressed;
    Color disabled;
} UIButtonStyle;

/** Interaction result returned for one button. */
typedef struct UIButtonResult {
    bool hovered;
    bool pressed;
    bool clicked;
} UIButtonResult;

/** Start a UI frame with pointer coordinates in logical screen space. */
void ui_begin_frame(UIInput input);

/**
 * Draw and update a button identified by a stable, non-empty string.
 * Passing NULL for style uses the default button colors.
 */
UIButtonResult ui_button(const char *id, UIRect bounds, const UIButtonStyle *style);

/** Draw a disabled button that cannot capture or consume input. */
void ui_button_disabled(UIRect bounds, const UIButtonStyle *style);

/** Return whether a UI control consumed pointer input this frame. */
bool ui_pointer_consumed(void);

/** Finish a UI frame and release stale pointer capture when appropriate. */
void ui_end_frame(void);

/** Return the engine default button colors. */
UIButtonStyle ui_default_button_style(void);

#endif
