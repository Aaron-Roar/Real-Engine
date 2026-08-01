#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "controller.h"
#include "graphics.h"

#define UI_DEFINITION_NAME_MAX 64
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

/** Customizable visual state for an immediate-mode slider. */
typedef struct UISliderStyle {
    Color track;
    Color fill;
    Color handle;
    Color handle_hovered;
    Color handle_pressed;
    float track_thickness;
    float handle_width;
    float handle_height;
    float step_button_size;
    float step_button_gap;
} UISliderStyle;

/** Runtime slider geometry and value range. */
typedef struct UISliderConfig {
    Position center;
    float length;
    /** Counterclockwise angle in logical screen-space radians. */
    float angle;
    float min_value;
    float max_value;
    /** Zero gives continuous movement; a positive value enables snapping. */
    float step;
    UISliderStyle style;
} UISliderConfig;

/** Optional, caller-owned text drawn with a slider. */
typedef struct UISliderText {
    const TextAsset *label;
    const TextAsset *value;
    const TextAsset *minus;
    const TextAsset *plus;
} UISliderText;

/** Interaction and updated value returned by a slider. */
typedef struct UISliderResult {
    bool hovered;
    bool pressed;
    bool changed;
    float value;
} UISliderResult;

/** Authored slider definition loaded independently of runtime value state. */
typedef struct UISliderDefinition {
    char name[UI_DEFINITION_NAME_MAX];
    char label[UI_LABEL_MAX];
    char font[UI_DEFINITION_NAME_MAX];
    char value_format[UI_LABEL_MAX];
    Color text_color;
    UISliderConfig config;
    float initial_value;
} UISliderDefinition;

ERROR_DECLARE_RESULT_TYPE(UISliderDefinitionResult, UISliderDefinition);

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
bool ui_pointer_consumed_is(void);

/** Finish a UI frame and release stale pointer capture when appropriate. */
void ui_end_frame(void);

/** Return the engine default button colors. */
UIButtonStyle ui_default_button_style(void);

/** Return default slider geometry, 0..1 range, and colors. */
UISliderConfig ui_default_slider_config(void);

/** Draw and update a slider while leaving its value owned by the caller. */
UISliderResult ui_slider(
    const char *id,
    float value,
    const UISliderConfig *config
);

/** Draw and update a slider with optional label, value, and -/+ text assets. */
UISliderResult ui_slider_with_text(
    const char *id,
    float value,
    const UISliderConfig *config,
    const UISliderText *text
);

#endif
