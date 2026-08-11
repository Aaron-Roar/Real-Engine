#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "controller.h"
#include "graphics.h"

#define UI_DEFINITION_NAME_MAX 64
#define UI_LABEL_MAX 128
#define UI_FONT_PATH_MAX 512
#define UI_FIELD_EDIT_MAX 256
#define UI_FIELD_KEY_EVENT_MAX 64

/** Axis-aligned rectangle in logical screen coordinates. */
typedef struct UIRect {
    float x;
    float y;
    float width;
    float height;
} UIRect;

typedef enum UIComponentTag {
    UI_COMPONENT_SIZE_TO_TEXT = 1 << 0
} UIComponentTag;

typedef uint32_t UIComponentMask;

typedef struct UIComponentConfig {
    UIComponentMask components;
    float text_padding_x;
    float text_padding_y;
} UIComponentConfig;

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
    bool focused;
    bool focus_changed;
    bool pressed;
    bool clicked;
    bool double_clicked;
    bool keyboard_activated;
} UIButtonResult;

typedef enum UINavigationDirection {
    UI_NAVIGATION_UP,
    UI_NAVIGATION_DOWN,
    UI_NAVIGATION_LEFT,
    UI_NAVIGATION_RIGHT
} UINavigationDirection;

typedef struct UIDropdownResult {
    bool hovered;
    bool button_hovered;
    bool open;
    bool changed;
    size_t selected_index;
    int hovered_index;
} UIDropdownResult;

typedef struct UIScrollRegionResult {
    bool hovered;
    bool changed;
    float offset;
} UIScrollRegionResult;

typedef enum UIFieldKind {
    UI_FIELD_STRING,
    UI_FIELD_FLOAT
} UIFieldKind;

typedef struct UIFieldBinding {
    UIFieldKind kind;
    char *string;
    size_t string_capacity;
    float *number;
} UIFieldBinding;

typedef struct UIFieldResult {
    bool hovered;
    bool active;
    bool changed;
    bool submitted;
} UIFieldResult;

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

typedef struct UIPhysicsDebugPanel {
    FontAsset font;
    TextAsset text;
} UIPhysicsDebugPanel;

EngineResult ui_physics_debug_panel_init(UIPhysicsDebugPanel *panel, FontDescriptor font);
void ui_physics_debug_panel_draw(UIPhysicsDebugPanel *panel);
void ui_physics_debug_panel_destroy(UIPhysicsDebugPanel *panel);

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
void ui_frame_begin(UIInput input);
UIRect ui_component_bounds_get(UIRect bounds, const TextAsset *const *texts,
    size_t text_count, UIComponentConfig config);
void ui_event_add(const SDL_Event *event);
void ui_field_event_add(const SDL_Event *event);
UIFieldResult ui_field(const char *id, UIFieldBinding binding,
    TextAsset *display, UIRect bounds, const UIButtonStyle *style);

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

/** Draw a caller-owned dropdown. Options and text assets remain caller-owned. */
UIDropdownResult ui_dropdown(const char *id, const TextAsset *const *options,
    size_t option_count, size_t selected_index, UIRect bounds,
    const UIButtonStyle *style);
UIDropdownResult ui_menu(const char *id, const TextAsset *label,
    const TextAsset *const *options, size_t option_count, UIRect bounds,
    const UIButtonStyle *style);

/** Begin a clipped vertical scroll region and translate child UI controls. */
UIScrollRegionResult ui_scroll_region_begin(const char *id, UIRect bounds,
    float content_height, float offset, float wheel_step);
/** End the current scroll region. */
void ui_scroll_region_end(void);

/** Update pointer interaction without drawing a visual element. */
UIButtonResult ui_interaction(const char *id, UIRect bounds);
/** Draw a filled rectangular UI surface. */
void ui_surface(UIRect bounds, Color color);
/** Draw a rectangular border. */
void ui_border(UIRect bounds, float thickness, Color color);
/** Draw a centered, clipped text asset. */
void ui_content(const TextAsset *text, UIRect bounds);
/** Draw an oriented rectangular primitive. */
void ui_quad(Position center, float width, float height, float angle, Color color);
/** Begin and end a clipped UI component region. */
bool ui_clip_begin(UIRect bounds);
void ui_clip_end(void);
/** Move keyboard focus spatially among controls registered last frame. */
bool ui_navigation_move(UINavigationDirection direction);
/** Activate the currently keyboard-focused control on its next draw. */
bool ui_navigation_activate(void);
/** Return the focused control's last registered screen bounds. */
bool ui_navigation_focus_bounds_get(UIRect *bounds);

/** Draw reusable text centered inside logical screen-space bounds. */
void ui_label(const TextAsset *text, UIRect bounds);

/** Draw a disabled button that cannot capture or consume input. */
void ui_button_disabled(UIRect bounds, const UIButtonStyle *style);

/** Return whether a UI control consumed pointer input this frame. */
bool ui_pointer_consumed_get(void);

/** Finish a UI frame and release stale pointer capture when appropriate. */
void ui_frame_end(void);

/** Return the engine default button colors. */
UIButtonStyle ui_button_style_default_get(void);

/** Return default slider geometry, 0..1 range, and colors. */
UISliderConfig ui_slider_config_default_get(void);

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
