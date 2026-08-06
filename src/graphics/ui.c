#include <stdint.h>
#include <math.h>
#include "ui.h"
#include "physics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct UIContext {
    UIInput input;
    uint64_t active_id;
    bool active_seen;
    bool pointer_claimed;
    bool pointer_consumed;
    bool frame_active;
    uint64_t field_id;
    bool field_seen;
    char field_edit[UI_FIELD_EDIT_MAX];
    SDL_Keycode field_keys[UI_FIELD_KEY_EVENT_MAX];
    size_t field_key_count;
    bool field_replace_pending;
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

UIButtonStyle ui_button_style_default_get(void) {
    return (UIButtonStyle){
        .idle = {55, 65, 81, 255},
        .hovered = {75, 86, 105, 255},
        .pressed = {39, 48, 62, 255},
        .disabled = {45, 48, 54, 180},
    };
}

UISliderConfig ui_slider_config_default_get(void) {
    return (UISliderConfig){
        .center = {0.0f, 0.0f},
        .length = 160.0f,
        .angle = 0.0f,
        .min_value = 0.0f,
        .max_value = 1.0f,
        .style = {
            .track = {55, 65, 81, 255},
            .fill = {85, 135, 225, 255},
            .handle = {220, 226, 236, 255},
            .handle_hovered = {245, 248, 252, 255},
            .handle_pressed = {165, 190, 235, 255},
            .track_thickness = 8.0f,
            .handle_width = 16.0f,
            .handle_height = 28.0f,
            .step_button_size = 0.0f,
            .step_button_gap = 10.0f,
        },
    };
}

static float ui_slider_snap(float value, const UISliderConfig *config) {
    float low = fminf(config->min_value, config->max_value);
    float high = fmaxf(config->min_value, config->max_value);
    if(config->step > 0.0f) {
        value = config->min_value
            + roundf((value - config->min_value) / config->step) * config->step;
    }
    return fminf(high, fmaxf(low, value));
}

static float ui_clamp_unit(float value) {
    if(value < 0.0f) return 0.0f;
    if(value > 1.0f) return 1.0f;
    return value;
}

static Vec2D ui_slider_axis(float angle) {
    return (Vec2D){cosf(angle), -sinf(angle)};
}

static bool ui_point_in_slider(Position point, const UISliderConfig *config) {
    Vec2D axis = ui_slider_axis(config->angle);
    Vec2D perpendicular = {sinf(config->angle), cosf(config->angle)};
    Vec2D relative = {
        point.x - config->center.x,
        point.y - config->center.y,
    };
    float along = relative.x * axis.x + relative.y * axis.y;
    float across = relative.x * perpendicular.x + relative.y * perpendicular.y;
    float hit_height = fmaxf(config->style.track_thickness, config->style.handle_height);

    return fabsf(along) <= config->length * 0.5f + config->style.handle_width * 0.5f
        && fabsf(across) <= hit_height * 0.5f;
}

static float ui_slider_pointer_amount(Position pointer, const UISliderConfig *config) {
    Vec2D axis = ui_slider_axis(config->angle);
    Vec2D relative = {
        pointer.x - config->center.x,
        pointer.y - config->center.y,
    };
    float along = relative.x * axis.x + relative.y * axis.y;
    return ui_clamp_unit(along / config->length + 0.5f);
}

static bool ui_point_in_oriented_square(
        Position point,
        Position center,
        float size,
        float angle
) {
    Vec2D axis = ui_slider_axis(angle);
    Vec2D perpendicular = {sinf(angle), cosf(angle)};
    Vec2D relative = {point.x - center.x, point.y - center.y};
    return fabsf(relative.x * axis.x + relative.y * axis.y) <= size * 0.5f
        && fabsf(relative.x * perpendicular.x + relative.y * perpendicular.y)
            <= size * 0.5f;
}

void ui_frame_begin(UIInput input) {
    ui_context.input = input;
    ui_context.active_seen = false;
    ui_context.field_seen = false;
    ui_context.pointer_claimed = false;
    ui_context.pointer_consumed = false;
    ui_context.frame_active = true;
}

void ui_field_event_add(const SDL_Event *event) {
    if(event == NULL || event->type != SDL_EVENT_KEY_DOWN || event->key.repeat ||
            ui_context.field_key_count >= UI_FIELD_KEY_EVENT_MAX) return;
    ui_context.field_keys[ui_context.field_key_count++] = event->key.key;
}

static void ui_field_binding_display_set(UIFieldBinding binding, TextAsset *display) {
    char value[UI_FIELD_EDIT_MAX] = {0};

    if(binding.kind == UI_FIELD_FLOAT && binding.number != NULL) {
        snprintf(value, sizeof(value), "%.1f", *binding.number);
    } else if(binding.kind == UI_FIELD_STRING && binding.string != NULL) {
        snprintf(value, sizeof(value), "%s", binding.string);
    }
    snprintf(ui_context.field_edit, sizeof(ui_context.field_edit), "%s", value);
    if(display != NULL) (void)graphics_text_value_set(display, value);
}

static bool ui_field_character_add(UIFieldBinding binding, char character) {
    size_t length = strlen(ui_context.field_edit);

    if(binding.kind == UI_FIELD_FLOAT) {
        char *decimal;

        if(character == ',') return false;
        if(character != '-' && character != '.' &&
                (character < '0' || character > '9')) return false;
        if(ui_context.field_replace_pending) {
            ui_context.field_edit[0] = '\0';
            length = 0;
        }
        decimal = strchr(ui_context.field_edit, '.');
        if(character == '-' && length != 0) return false;
        if(character == '.' && decimal != NULL) return false;
        if(decimal != NULL && character >= '0' && character <= '9' &&
                strlen(decimal + 1) >= 1) return false;
    } else if(character < 32 || character > 126) {
        return false;
    }
    ui_context.field_replace_pending = false;
    if(length + 1 >= sizeof(ui_context.field_edit)) return false;
    ui_context.field_edit[length] = character;
    ui_context.field_edit[length + 1] = '\0';
    return true;
}

static bool ui_field_binding_store(UIFieldBinding binding) {
    if(binding.kind == UI_FIELD_STRING) {
        if(binding.string == NULL || binding.string_capacity == 0) return false;
        snprintf(binding.string, binding.string_capacity, "%s", ui_context.field_edit);
        return true;
    }
    if(binding.kind == UI_FIELD_FLOAT && binding.number != NULL &&
            ui_context.field_edit[0] != '\0' &&
            strcmp(ui_context.field_edit, "-") != 0 &&
            strcmp(ui_context.field_edit, ".") != 0) {
        *binding.number = strtof(ui_context.field_edit, NULL);
        return true;
    }
    return false;
}

UIFieldResult ui_field(const char *id, UIFieldBinding binding,
    TextAsset *display, UIRect bounds, const UIButtonStyle *style) {
    UIFieldResult result = {0};
    UIButtonStyle resolved = style == NULL ? ui_button_style_default_get() : *style;
    uint64_t field_id = ui_hash_id(id);

    if(!ui_context.frame_active || field_id == 0 || bounds.width <= 0.0f ||
            bounds.height <= 0.0f) return result;
    result.hovered = ui_point_in_rect(ui_context.input.pointer, bounds) &&
        !ui_context.pointer_claimed;
    if(result.hovered) {
        ui_context.pointer_claimed = true;
        ui_context.pointer_consumed = true;
    }
    if(result.hovered && ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED) {
        ui_context.field_id = field_id;
        ui_context.field_replace_pending = binding.kind == UI_FIELD_FLOAT;
        ui_field_binding_display_set(binding, display);
    } else if(ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED &&
            ui_context.field_id == field_id && !result.hovered) {
        ui_context.field_id = 0;
    }
    result.active = ui_context.field_id == field_id;
    if(result.active) ui_context.field_seen = true;
    if(result.active) {
        for(size_t i = 0; i < ui_context.field_key_count; i += 1) {
            SDL_Keycode key = ui_context.field_keys[i];
            bool edited = false;

            if(key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                result.submitted = true;
                ui_context.field_id = 0;
            } else if(key == SDLK_ESCAPE) {
                ui_field_binding_display_set(binding, display);
                ui_context.field_id = 0;
            } else if(key == SDLK_BACKSPACE) {
                size_t length = strlen(ui_context.field_edit);
                if(ui_context.field_replace_pending) {
                    ui_context.field_edit[0] = '\0';
                    ui_context.field_replace_pending = false;
                    edited = true;
                } else if(length > 0) {
                    ui_context.field_edit[length - 1] = '\0';
                    edited = true;
                }
            } else if(key >= 0 && key <= 127) {
                edited = ui_field_character_add(binding, (char)key);
            }
            if(edited && ui_field_binding_store(binding)) result.changed = true;
            if(display != NULL) {
                (void)graphics_text_value_set(display, ui_context.field_edit);
            }
        }
    } else if(display != NULL) {
        char value[UI_FIELD_EDIT_MAX];
        if(binding.kind == UI_FIELD_FLOAT && binding.number != NULL) {
            snprintf(value, sizeof(value), "%.1f", *binding.number);
            (void)graphics_text_value_set(display, value);
        } else if(binding.kind == UI_FIELD_STRING && binding.string != NULL) {
            (void)graphics_text_value_set(display, binding.string);
        }
    }
    (void)graphics_screen_rect_draw(bounds.x, bounds.y, bounds.width, bounds.height,
        result.active ? resolved.pressed : (result.hovered ? resolved.hovered : resolved.idle));
    ui_label(display, bounds);
    return result;
}

UIButtonResult ui_button(
    const char *id,
    const TextAsset *label,
    UIRect bounds,
    const UIButtonStyle *style
) {
    UIButtonResult result = {0};
    UIButtonStyle resolved_style = style == NULL ? ui_button_style_default_get() : *style;
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

    graphics_screen_rect_draw(
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

UISliderResult ui_slider_with_text(
        const char *id,
        float value,
        const UISliderConfig *config,
        const UISliderText *text
) {
    UISliderResult result = {.value = value};
    UISliderConfig resolved = config == NULL ? ui_slider_config_default_get() : *config;
    uint64_t slider_id = ui_hash_id(id);
    Vec2D axis;
    float amount;
    float fill_length;
    Position start;
    Position handle_center;
    bool contains_pointer;
    Position step_centers[2];
    bool step_hovered[2] = {false, false};
    bool step_pressed[2] = {false, false};
    int step_index;

    if(!ui_context.frame_active || slider_id == 0
            || !isfinite(value) || !isfinite(resolved.length)
            || !isfinite(resolved.angle) || !isfinite(resolved.min_value)
            || !isfinite(resolved.max_value) || !isfinite(resolved.step)
            || resolved.step < 0.0f || resolved.length <= 0.0f
            || resolved.min_value == resolved.max_value
            || resolved.style.track_thickness <= 0.0f
            || resolved.style.handle_width <= 0.0f
            || resolved.style.handle_height <= 0.0f
            || resolved.style.step_button_size < 0.0f
            || resolved.style.step_button_gap < 0.0f) {
        return result;
    }

    axis = ui_slider_axis(resolved.angle);
    for(step_index = 0; step_index < 2; step_index += 1) {
        float side = step_index == 0 ? -1.0f : 1.0f;
        step_centers[step_index] = (Position){
            resolved.center.x + axis.x * side * (resolved.length * 0.5f
                + resolved.style.step_button_gap
                + resolved.style.step_button_size * 0.5f),
            resolved.center.y + axis.y * side * (resolved.length * 0.5f
                + resolved.style.step_button_gap
                + resolved.style.step_button_size * 0.5f),
        };
    }

    amount = ui_clamp_unit(
        (value - resolved.min_value) / (resolved.max_value - resolved.min_value)
    );
    result.value = ui_slider_snap(resolved.min_value
        + amount * (resolved.max_value - resolved.min_value), &resolved);
    result.changed = result.value != value;
    if(resolved.step > 0.0f && resolved.style.step_button_size > 0.0f) {
        for(step_index = 0; step_index < 2; step_index += 1) {
            uint64_t step_id = slider_id ^ (step_index == 0
                ? UINT64_C(0x9e3779b97f4a7c15) : UINT64_C(0xc2b2ae3d27d4eb4f));
            bool inside = ui_point_in_oriented_square(
                ui_context.input.pointer,
                step_centers[step_index],
                resolved.style.step_button_size,
                resolved.angle
            );
            if(inside && !ui_context.pointer_claimed) {
                step_hovered[step_index] = true;
                result.hovered = true;
                ui_context.pointer_claimed = true;
                ui_context.pointer_consumed = true;
            }
            if(ui_context.active_id == step_id) {
                ui_context.active_seen = true;
                ui_context.pointer_consumed = true;
                step_pressed[step_index] =
                    ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED
                    || ui_context.input.primary_button == MOUSE_BUTTON_STATE_DOWN;
                result.pressed = result.pressed || step_pressed[step_index];
                if(ui_context.input.primary_button == MOUSE_BUTTON_STATE_RELEASED) {
                    if(step_hovered[step_index]) {
                        float direction = step_index == 0 ? -1.0f : 1.0f;
                        if(resolved.max_value < resolved.min_value) direction = -direction;
                        result.value = ui_slider_snap(
                            result.value + direction * resolved.step,
                            &resolved
                        );
                        result.changed = result.changed || result.value != value;
                    }
                    ui_context.active_id = 0;
                }
            } else if(step_hovered[step_index] && ui_context.active_id == 0
                    && ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED) {
                ui_context.active_id = step_id;
                ui_context.active_seen = true;
                ui_context.pointer_consumed = true;
                step_pressed[step_index] = true;
                result.pressed = true;
            }
        }
    }
    contains_pointer = ui_point_in_slider(ui_context.input.pointer, &resolved);
    if(contains_pointer && !ui_context.pointer_claimed) {
        result.hovered = true;
        ui_context.pointer_claimed = true;
    }

    if(ui_context.active_id == slider_id) {
        ui_context.active_seen = true;
        ui_context.pointer_consumed = true;
        result.pressed = ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED
            || ui_context.input.primary_button == MOUSE_BUTTON_STATE_DOWN;
        if(result.pressed || ui_context.input.primary_button == MOUSE_BUTTON_STATE_RELEASED) {
            float previous = result.value;
            amount = ui_slider_pointer_amount(ui_context.input.pointer, &resolved);
            result.value = ui_slider_snap(resolved.min_value
                + amount * (resolved.max_value - resolved.min_value), &resolved);
            result.changed = result.changed || result.value != previous;
        }
        if(ui_context.input.primary_button == MOUSE_BUTTON_STATE_RELEASED) {
            ui_context.active_id = 0;
        }
    } else if(result.hovered && ui_context.active_id == 0
            && ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED) {
        float previous = result.value;
        ui_context.active_id = slider_id;
        ui_context.active_seen = true;
        ui_context.pointer_consumed = true;
        result.pressed = true;
        amount = ui_slider_pointer_amount(ui_context.input.pointer, &resolved);
        result.value = ui_slider_snap(resolved.min_value
            + amount * (resolved.max_value - resolved.min_value), &resolved);
        result.changed = result.changed || result.value != previous;
    }
    if(result.hovered) ui_context.pointer_consumed = true;

    amount = ui_clamp_unit(
        (result.value - resolved.min_value) /
        (resolved.max_value - resolved.min_value)
    );
    start = (Position){
        resolved.center.x - axis.x * resolved.length * 0.5f,
        resolved.center.y - axis.y * resolved.length * 0.5f,
    };
    (void)graphics_screen_quad_draw(
        resolved.center,
        resolved.length,
        resolved.style.track_thickness,
        resolved.angle,
        resolved.style.track
    );
    if(resolved.step > 0.0f && resolved.style.step_button_size > 0.0f) {
        for(step_index = 0; step_index < 2; step_index += 1) {
            const TextAsset *symbol = NULL;
            bool start_is_minus = resolved.max_value > resolved.min_value;
            bool is_minus = step_index == 0 ? start_is_minus : !start_is_minus;
            UIRect symbol_bounds = {
                step_centers[step_index].x - resolved.style.step_button_size * 0.5f,
                step_centers[step_index].y - resolved.style.step_button_size * 0.5f,
                resolved.style.step_button_size,
                resolved.style.step_button_size,
            };
            (void)graphics_screen_quad_draw(
                step_centers[step_index],
                resolved.style.step_button_size,
                resolved.style.step_button_size,
                resolved.angle,
                step_pressed[step_index] ? resolved.style.handle_pressed
                    : (step_hovered[step_index] ? resolved.style.handle_hovered
                        : resolved.style.handle)
            );
            if(text != NULL) symbol = is_minus ? text->minus : text->plus;
            ui_label(symbol, symbol_bounds);
        }
    }
    fill_length = resolved.length * amount;
    if(fill_length > 0.0f) {
        Position fill_center = {
            start.x + axis.x * fill_length * 0.5f,
            start.y + axis.y * fill_length * 0.5f,
        };
        (void)graphics_screen_quad_draw(
            fill_center,
            fill_length,
            resolved.style.track_thickness,
            resolved.angle,
            resolved.style.fill
        );
    }
    handle_center = (Position){
        start.x + axis.x * resolved.length * amount,
        start.y + axis.y * resolved.length * amount,
    };
    (void)graphics_screen_quad_draw(
        handle_center,
        resolved.style.handle_width,
        resolved.style.handle_height,
        resolved.angle,
        result.pressed ? resolved.style.handle_pressed
            : (result.hovered ? resolved.style.handle_hovered : resolved.style.handle)
    );
    if(text != NULL) {
        float height = fmaxf(resolved.style.handle_height, resolved.style.track_thickness);
        UIRect label_bounds = {
            resolved.center.x - resolved.length * 0.5f,
            resolved.center.y - height * 0.5f - 42.0f,
            resolved.length,
            28.0f,
        };
        UIRect value_bounds = {
            resolved.center.x - 60.0f,
            resolved.center.y + height * 0.5f + 8.0f,
            120.0f,
            28.0f,
        };
        ui_label(text->label, label_bounds);
        ui_label(text->value, value_bounds);
    }
    return result;
}

UISliderResult ui_slider(const char *id, float value, const UISliderConfig *config) {
    return ui_slider_with_text(id, value, config, NULL);
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
    (void)graphics_text_draw(text, position);
}

EngineResult ui_physics_debug_panel_init(UIPhysicsDebugPanel *panel, FontDescriptor descriptor) {
    FontAssetResult font;
    TextAssetResult text;

    if(panel == NULL) return (EngineResult){.kind = ERROR_RESULT_ERROR,
        .result.error = ERROR_ENGINE_TEXT_CREATE_FAILED};
    *panel = (UIPhysicsDebugPanel){0};
    font = graphics_font_load(descriptor);
    if(font.kind == ERROR_RESULT_ERROR) return (EngineResult){.kind = ERROR_RESULT_ERROR,
        .result.error = font.result.error};
    panel->font = font.result.value;
    text = graphics_text_create(&panel->font, "Physics", (Color){255, 255, 255, 255});
    if(text.kind == ERROR_RESULT_ERROR) {
        graphics_font_destroy(&panel->font);
        return (EngineResult){.kind = ERROR_RESULT_ERROR, .result.error = text.result.error};
    }
    panel->text = text.result.value;
    physics_debug_stats_enabled_set(true);
    return (EngineResult){.kind = ERROR_RESULT_VALUE};
}

void ui_physics_debug_panel_draw(UIPhysicsDebugPanel *panel) {
    PhysicsDebugStats stats;
    char value[512];
    float width;
    float height;

    if(panel == NULL || panel->text.text == NULL) return;
    stats = physics_debug_stats_get();
    (void)snprintf(value, sizeof(value),
        "Physics %.3f ms\nBuild %.3f  Query %.3f ms\n"
        "Narrow %.3f  Response %.3f ms\nColliders %zu  Nodes %zu  Height %d\n"
        "Candidates %zu  Tests %zu\nOverlaps %zu  Contacts %zu",
        stats.total_ms, stats.broadphase_build_ms, stats.broadphase_query_ms,
        stats.narrowphase_ms, stats.response_ms, stats.collider_count,
        stats.tree_node_count, stats.tree_height, stats.candidate_pair_count,
        stats.narrowphase_test_count, stats.overlap_count, stats.contact_count);
    if(!graphics_text_value_set(&panel->text, value)) return;
    width = panel->text.size.x + 12.0f;
    height = panel->text.size.y + 12.0f;
    (void)graphics_screen_rect_draw(WINDOW_WIDTH - width - 5.0f, 5.0f, width, height,
        (Color){0, 0, 0, 190});
    ui_label(&panel->text,
        (UIRect){WINDOW_WIDTH - width - 5.0f, 5.0f, width, height});
}

void ui_physics_debug_panel_destroy(UIPhysicsDebugPanel *panel) {
    if(panel == NULL) return;
    graphics_text_destroy(&panel->text);
    graphics_font_destroy(&panel->font);
    physics_debug_stats_enabled_set(false);
    *panel = (UIPhysicsDebugPanel){0};
}

void ui_button_disabled(UIRect bounds, const UIButtonStyle *style) {
    UIButtonStyle resolved_style = style == NULL ? ui_button_style_default_get() : *style;

    graphics_screen_rect_draw(
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height,
        resolved_style.disabled
    );
}

bool ui_pointer_consumed_get(void) {
    return ui_context.pointer_consumed;
}

void ui_frame_end(void) {
    if(!ui_context.frame_active) {
        return;
    }
    if(ui_context.input.primary_button == MOUSE_BUTTON_STATE_RELEASED ||
            (ui_context.active_id != 0 && !ui_context.active_seen)) {
        ui_context.active_id = 0;
    }
    if(ui_context.field_id != 0 && !ui_context.field_seen) {
        ui_context.field_id = 0;
    }
    ui_context.field_key_count = 0;
    ui_context.frame_active = false;
}
