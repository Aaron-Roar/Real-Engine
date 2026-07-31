#include <stdint.h>
#include "ui.h"

typedef struct UIContext {
    UIInput input;
    uint64_t active_id;
    bool active_seen;
    bool pointer_claimed;
    bool pointer_consumed;
    bool frame_active;
} UIContext;

static UIContext ui_context = {0};

static uint64_t ui_hash_id(const char *id) {
    uint64_t hash = UINT64_C(14695981039346656037);

    if(id == NULL || id[0] == '\0') {
        return 0;
    }
    while(*id != '\0') {
        hash ^= (uint8_t)*id;
        hash *= UINT64_C(1099511628211);
        id += 1;
    }
    return hash;
}

static bool ui_point_in_rect(Position point, UIRect rect) {
    if(rect.width <= 0.0f || rect.height <= 0.0f) {
        return false;
    }
    return point.x >= rect.x && point.x < rect.x + rect.width &&
        point.y >= rect.y && point.y < rect.y + rect.height;
}

UIButtonStyle ui_default_button_style(void) {
    return (UIButtonStyle){
        .idle = {55, 65, 81, 255},
        .hovered = {75, 86, 105, 255},
        .pressed = {39, 48, 62, 255},
        .disabled = {45, 48, 54, 180},
    };
}

void ui_begin_frame(UIInput input) {
    ui_context.input = input;
    ui_context.active_seen = false;
    ui_context.pointer_claimed = false;
    ui_context.pointer_consumed = false;
    ui_context.frame_active = true;
}

UIButtonResult ui_button(
    const char *id,
    const TextAsset *label,
    UIRect bounds,
    const UIButtonStyle *style
) {
    UIButtonResult result = {0};
    UIButtonStyle resolved_style = style == NULL ? ui_default_button_style() : *style;
    uint64_t button_id = ui_hash_id(id);
    bool contains_pointer;

    if(!ui_context.frame_active || button_id == 0) {
        return result;
    }

    contains_pointer = ui_point_in_rect(ui_context.input.pointer, bounds);
    if(contains_pointer && !ui_context.pointer_claimed) {
        result.hovered = true;
        ui_context.pointer_claimed = true;
    }

    if(ui_context.active_id == button_id) {
        ui_context.active_seen = true;
        ui_context.pointer_consumed = true;
        result.pressed = ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED ||
            ui_context.input.primary_button == MOUSE_BUTTON_STATE_DOWN;

        if(ui_context.input.primary_button == MOUSE_BUTTON_STATE_RELEASED) {
            result.clicked = result.hovered;
            ui_context.active_id = 0;
        }
    }
    else if(result.hovered &&
            ui_context.active_id == 0 &&
            ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED) {
        ui_context.active_id = button_id;
        ui_context.active_seen = true;
        ui_context.pointer_consumed = true;
        result.pressed = true;
    }

    if(result.hovered) {
        ui_context.pointer_consumed = true;
    }

    graphics_draw_screen_rect(
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height,
        result.pressed ? resolved_style.pressed :
            (result.hovered ? resolved_style.hovered : resolved_style.idle)
    );
    ui_label(label, bounds);
    return result;
}

void ui_label(const TextAsset *text, UIRect bounds) {
    Position position;

    if(text == NULL || text->text == NULL) {
        return;
    }
    position = (Position){
        .x = bounds.x + (bounds.width - text->size.x) * 0.5f,
        .y = bounds.y + (bounds.height - text->size.y) * 0.5f,
    };
    (void)graphics_draw_text(text, position);
}

void ui_button_disabled(UIRect bounds, const UIButtonStyle *style) {
    UIButtonStyle resolved_style = style == NULL ? ui_default_button_style() : *style;

    graphics_draw_screen_rect(
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height,
        resolved_style.disabled
    );
}

bool ui_pointer_consumed(void) {
    return ui_context.pointer_consumed;
}

void ui_end_frame(void) {
    if(!ui_context.frame_active) {
        return;
    }
    if(ui_context.input.primary_button == MOUSE_BUTTON_STATE_RELEASED ||
            (ui_context.active_id != 0 && !ui_context.active_seen)) {
        ui_context.active_id = 0;
    }
    ui_context.frame_active = false;
}
