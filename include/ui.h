#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "controller.h"
#include "graphics.h"

#define UI_DEFINITION_NAME_MAX 64
#define UI_ID_MAX 128
#define UI_LABEL_MAX 128
#define UI_FONT_PATH_MAX 512

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

/** Authored button data that can be loaded independently of runtime state. */
typedef struct UIButtonDefinition {
    char name[UI_DEFINITION_NAME_MAX];
    char id[UI_ID_MAX];
    char label[UI_LABEL_MAX];
    char font[UI_DEFINITION_NAME_MAX];
    Color text_color;
    UIRect bounds;
    UIButtonStyle style;
} UIButtonDefinition;

/** Result type for functions that return a button definition. */
ERROR_DECLARE_RESULT_TYPE(UIButtonDefinitionResult, UIButtonDefinition);

/** Authored font settings. The game owns the loaded FontAsset. */
typedef struct UIFontDefinition {
    char name[UI_DEFINITION_NAME_MAX];
    char file[UI_FONT_PATH_MAX];
    float point_size;
} UIFontDefinition;

ERROR_DECLARE_RESULT_TYPE(UIFontDefinitionResult, UIFontDefinition);

/** Authored standalone label data in logical screen coordinates. */
typedef struct UILabelDefinition {
    char name[UI_DEFINITION_NAME_MAX];
    char text[UI_LABEL_MAX];
    char font[UI_DEFINITION_NAME_MAX];
    Color color;
    UIRect bounds;
} UILabelDefinition;

ERROR_DECLARE_RESULT_TYPE(UILabelDefinitionResult, UILabelDefinition);

/** Start a UI frame with pointer coordinates in logical screen space. */
void ui_begin_frame(UIInput input);

/**
 * Draw and update a button identified by a stable, non-empty string.
 * Passing NULL for label, or an empty TextAsset created from "", draws no
 * automatic label. Passing NULL for style uses the default button colors.
 */
UIButtonResult ui_button(
    const char *id,
    const TextAsset *label,
    UIRect bounds,
    const UIButtonStyle *style
);

/** Draw reusable text centered inside logical screen-space bounds. */
void ui_label(const TextAsset *text, UIRect bounds);

/** Draw a disabled button that cannot capture or consume input. */
void ui_button_disabled(UIRect bounds, const UIButtonStyle *style);

/** Return whether a UI control consumed pointer input this frame. */
bool ui_pointer_consumed(void);

/** Finish a UI frame and release stale pointer capture when appropriate. */
void ui_end_frame(void);

/** Return the engine default button colors. */
UIButtonStyle ui_default_button_style(void);

#endif
