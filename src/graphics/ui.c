#include <stdint.h>
#include <math.h>
#include "ui.h"
#include "physics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UI_DROPDOWN_OPTION_MAX 128
#define UI_DROPDOWN_VISIBLE_MAX 8
#define UI_SCROLL_REGION_MAX 8
#define UI_NAVIGATION_ITEM_MAX 512
#define UI_DROPDOWN_DIVIDER_INSET 0.05f

typedef struct UINavigationItem {
    uint64_t id;
    uint64_t dropdown_id;
    UIRect bounds;
} UINavigationItem;

typedef struct UIContext {
    UIInput input;
    uint64_t active_id;
    uint64_t last_clicked_id;
    Uint64 last_clicked_at;
    bool active_seen;
    bool pointer_claimed;
    bool pointer_consumed;
    bool frame_active;
    uint64_t field_id;
    bool field_seen;
    char field_edit[UI_FIELD_EDIT_MAX];
    SDL_Keycode field_keys[UI_FIELD_KEY_EVENT_MAX];
    SDL_Keymod field_modifiers[UI_FIELD_KEY_EVENT_MAX];
    size_t field_key_count;
    size_t field_cursor;
    float field_scroll_y;
    bool field_select_all;
    uint64_t dropdown_id;
    bool dropdown_seen;
    uint64_t dropdown_render_id;
    const TextAsset *dropdown_options[UI_DROPDOWN_OPTION_MAX];
    uint64_t dropdown_option_ids[UI_DROPDOWN_OPTION_MAX];
    size_t dropdown_option_count;
    UIRect dropdown_bounds;
    UIButtonStyle dropdown_style;
    size_t dropdown_first_option;
    bool dropdown_option_interaction;
    float wheel_y;
    float translation_y;
    float translation_stack[UI_SCROLL_REGION_MAX];
    UIRect scroll_clip_stack[UI_SCROLL_REGION_MAX];
    float scroll_offset_stack[UI_SCROLL_REGION_MAX];
    float scroll_content_stack[UI_SCROLL_REGION_MAX];
    size_t scroll_depth;
    bool modal_active;
    bool modal_controls;
    UIRect modal_bounds;
    UINavigationItem navigation_items[UI_NAVIGATION_ITEM_MAX];
    UINavigationItem navigation_previous[UI_NAVIGATION_ITEM_MAX];
    size_t navigation_item_count;
    size_t navigation_previous_count;
    uint64_t navigation_focus_id;
    uint64_t navigation_focus_changed_id;
    uint64_t navigation_activate_id;
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

static UIRect ui_bounds_resolve(UIRect bounds) {
    bounds.y += ui_context.translation_y;
    return bounds;
}

UIRect ui_component_bounds_get(UIRect bounds, const TextAsset *const *texts,
    size_t text_count, UIComponentConfig config) {
    float width = 0.0f;
    float height = 0.0f;

    if((config.components & UI_COMPONENT_SIZE_TO_TEXT) == 0 || texts == NULL) {
        return bounds;
    }
    for(size_t i = 0; i < text_count; i += 1) {
        if(texts[i] == NULL) continue;
        if(texts[i]->size.x > width) width = texts[i]->size.x;
        if(texts[i]->size.y > height) height = texts[i]->size.y;
    }
    bounds.width = width + fmaxf(0.0f, config.text_padding_x) * 2.0f;
    bounds.height = height + fmaxf(0.0f, config.text_padding_y) * 2.0f;
    return bounds;
}

static bool ui_point_in_scroll_clip(Position point) {
    if(ui_context.scroll_depth == 0) return true;
    return ui_point_in_rect(point,
        ui_context.scroll_clip_stack[ui_context.scroll_depth - 1]);
}

static void ui_border_raw(UIRect bounds, float thickness, Color color) {
    if(bounds.width <= 0.0f || bounds.height <= 0.0f || thickness <= 0.0f) return;
    (void)graphics_screen_rect_draw(bounds.x, bounds.y, bounds.width, thickness, color);
    (void)graphics_screen_rect_draw(bounds.x, bounds.y + bounds.height - thickness,
        bounds.width, thickness, color);
    (void)graphics_screen_rect_draw(bounds.x, bounds.y, thickness, bounds.height, color);
    (void)graphics_screen_rect_draw(bounds.x + bounds.width - thickness, bounds.y,
        thickness, bounds.height, color);
}

static void ui_label_raw(const TextAsset *text, UIRect bounds);
static void ui_navigation_item_register(uint64_t id, UIRect bounds) {
    if(id == 0 || ui_context.navigation_item_count >= UI_NAVIGATION_ITEM_MAX) return;
    ui_context.navigation_items[ui_context.navigation_item_count++] =
        (UINavigationItem){.id = id,
            .dropdown_id = ui_context.dropdown_option_interaction ?
                ui_context.dropdown_id : 0,
            .bounds = bounds};
}
static void ui_surface_raw(UIRect bounds, Color color) {
    (void)graphics_screen_rect_draw(bounds.x, bounds.y, bounds.width, bounds.height, color);
}

static void ui_dropdown_divider_raw(UIRect bounds) {
    float inset = bounds.width * UI_DROPDOWN_DIVIDER_INSET;
    (void)graphics_screen_rect_draw(bounds.x + inset, bounds.y,
        bounds.width - inset * 2.0f, 1.0f, (Color){170, 174, 182, 255});
}

static void ui_quad_raw(Position center, float width, float height, float angle,
    Color color) {
    (void)graphics_screen_quad_draw(center, width, height, angle, color);
}

static bool ui_clip_raw_begin(UIRect bounds) {
    return graphics_screen_clip_push(bounds.x, bounds.y, bounds.width, bounds.height);
}

void ui_surface(UIRect bounds, Color color) {
    bounds = ui_bounds_resolve(bounds);
    ui_surface_raw(bounds, color);
}

void ui_border(UIRect bounds, float thickness, Color color) {
    ui_border_raw(ui_bounds_resolve(bounds), thickness, color);
}

void ui_content(const TextAsset *text, UIRect bounds) {
    ui_label_raw(text, ui_bounds_resolve(bounds));
}

void ui_quad(Position center, float width, float height, float angle, Color color) {
    center.y += ui_context.translation_y;
    ui_quad_raw(center, width, height, angle, color);
}

bool ui_clip_begin(UIRect bounds) {
    bounds = ui_bounds_resolve(bounds);
    return ui_clip_raw_begin(bounds);
}

void ui_clip_end(void) {
    graphics_screen_clip_pop();
}

static UIButtonResult ui_interaction_id(uint64_t interaction_id, bool hovered) {
    UIButtonResult result = {0};

    if(!ui_context.frame_active || interaction_id == 0) return result;
    result.focused = ui_context.navigation_focus_id == interaction_id;
    result.focus_changed = ui_context.navigation_focus_changed_id == interaction_id;
    if(ui_context.navigation_activate_id == interaction_id) {
        result.clicked = true;
        result.double_clicked = true;
        result.keyboard_activated = true;
        ui_context.navigation_activate_id = 0;
    }
    if(hovered && !ui_context.pointer_claimed) {
        result.hovered = true;
        ui_context.pointer_claimed = true;
    }
    if(ui_context.active_id == interaction_id) {
        ui_context.active_seen = true;
        ui_context.pointer_consumed = true;
        result.pressed = ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED ||
            ui_context.input.primary_button == MOUSE_BUTTON_STATE_DOWN;
        if(ui_context.input.primary_button == MOUSE_BUTTON_STATE_RELEASED) {
            bool pointer_clicked = result.hovered;
            result.clicked = result.clicked || pointer_clicked;
            if(pointer_clicked) {
                Uint64 now = SDL_GetTicks();
                result.double_clicked = result.double_clicked ||
                    (ui_context.last_clicked_id == interaction_id &&
                        now - ui_context.last_clicked_at <= 400);
                ui_context.last_clicked_id = interaction_id;
                ui_context.last_clicked_at = now;
            }
            ui_context.active_id = 0;
        }
    } else if(result.hovered && ui_context.active_id == 0 &&
            ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED) {
        ui_context.navigation_focus_id = interaction_id;
        result.focused = true;
        ui_context.active_id = interaction_id;
        ui_context.active_seen = true;
        ui_context.pointer_consumed = true;
        result.pressed = true;
    }
    if(result.hovered) ui_context.pointer_consumed = true;
    return result;
}

static UIButtonResult ui_interaction_resolved(const char *id, UIRect bounds) {
    uint64_t interaction_id = ui_hash_id(id);
    if(ui_context.modal_active && !ui_context.modal_controls)
        return (UIButtonResult){0};
    ui_navigation_item_register(interaction_id, bounds);
    return ui_interaction_id(interaction_id,
        ui_point_in_rect(ui_context.input.pointer, bounds) &&
            (!ui_context.modal_active ||
                ui_point_in_rect(ui_context.input.pointer, ui_context.modal_bounds)) &&
            ui_point_in_scroll_clip(ui_context.input.pointer));
}

UIButtonResult ui_interaction(const char *id, UIRect bounds) {
    return ui_interaction_resolved(id, ui_bounds_resolve(bounds));
}

bool ui_navigation_move(UINavigationDirection direction) {
    const UINavigationItem *current = NULL;
    size_t best = SIZE_MAX;
    float best_score = INFINITY;
    uint64_t dropdown_id = ui_context.dropdown_id;

    if(ui_context.navigation_previous_count == 0) return dropdown_id != 0;
    for(size_t i = 0; i < ui_context.navigation_previous_count; i += 1) {
        if(ui_context.navigation_previous[i].id == ui_context.navigation_focus_id) {
            current = &ui_context.navigation_previous[i];
            break;
        }
    }
    if(current == NULL) {
        size_t first = 0;
        if(dropdown_id != 0) {
            while(first < ui_context.navigation_previous_count &&
                    ui_context.navigation_previous[first].dropdown_id != dropdown_id) {
                first += 1;
            }
            if(first == ui_context.navigation_previous_count) return true;
        }
        ui_context.navigation_focus_id = ui_context.navigation_previous[first].id;
        ui_context.navigation_focus_changed_id = ui_context.navigation_focus_id;
        return true;
    }
    {
        float current_x = current->bounds.x + current->bounds.width * 0.5f;
        float current_y = current->bounds.y + current->bounds.height * 0.5f;
        for(size_t i = 0; i < ui_context.navigation_previous_count; i += 1) {
            const UINavigationItem *candidate = &ui_context.navigation_previous[i];
            float x = candidate->bounds.x + candidate->bounds.width * 0.5f;
            float y = candidate->bounds.y + candidate->bounds.height * 0.5f;
            float dx = x - current_x;
            float dy = y - current_y;
            float primary;
            float cross;
            bool eligible = false;
            if(candidate->id == current->id) continue;
            if(dropdown_id != 0 && candidate->dropdown_id != dropdown_id) continue;
            if(direction == UI_NAVIGATION_UP && dy < -0.5f) {
                primary = -dy; cross = fabsf(dx); eligible = true;
            } else if(direction == UI_NAVIGATION_DOWN && dy > 0.5f) {
                primary = dy; cross = fabsf(dx); eligible = true;
            } else if(direction == UI_NAVIGATION_LEFT && dx < -0.5f) {
                primary = -dx; cross = fabsf(dy); eligible = true;
            } else if(direction == UI_NAVIGATION_RIGHT && dx > 0.5f) {
                primary = dx; cross = fabsf(dy); eligible = true;
            }
            if(eligible && primary + cross * 0.25f < best_score) {
                best_score = primary + cross * 0.25f;
                best = i;
            }
        }
    }
    if(best == SIZE_MAX) return dropdown_id != 0;
    ui_context.navigation_focus_id = ui_context.navigation_previous[best].id;
    ui_context.navigation_focus_changed_id = ui_context.navigation_focus_id;
    return true;
}

bool ui_navigation_activate(void) {
    if(ui_context.navigation_focus_id == 0) return false;
    ui_context.navigation_activate_id = ui_context.navigation_focus_id;
    return true;
}

bool ui_navigation_focus_bounds_get(UIRect *bounds) {
    if(bounds == NULL || ui_context.navigation_focus_id == 0) return false;
    for(size_t i = 0; i < ui_context.navigation_previous_count; i += 1) {
        if(ui_context.navigation_previous[i].id == ui_context.navigation_focus_id) {
            *bounds = ui_context.navigation_previous[i].bounds;
            return true;
        }
    }
    return false;
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
    ui_context.dropdown_seen = false;
    ui_context.pointer_claimed = false;
    ui_context.pointer_consumed = false;
    ui_context.translation_y = 0.0f;
    ui_context.scroll_depth = 0;
    ui_context.modal_active = false;
    ui_context.modal_controls = false;
    ui_context.navigation_item_count = 0;
    ui_context.frame_active = true;
}

void ui_modal_set(UIRect bounds) {
    if(!ui_context.frame_active) return;
    ui_context.modal_active = true;
    ui_context.modal_bounds = bounds;
}

void ui_modal_controls_begin(void) {
    if(ui_context.modal_active) ui_context.modal_controls = true;
}

void ui_modal_controls_end(void) {
    ui_context.modal_controls = false;
}

void ui_event_add(const SDL_Event *event) {
    if(event == NULL) return;
    if(event->type == SDL_EVENT_MOUSE_WHEEL) {
        ui_context.wheel_y += event->wheel.y;
        return;
    }
    if(event->type != SDL_EVENT_KEY_DOWN ||
            ui_context.field_key_count >= UI_FIELD_KEY_EVENT_MAX) return;
    ui_context.field_keys[ui_context.field_key_count] = event->key.key;
    ui_context.field_modifiers[ui_context.field_key_count] = event->key.mod;
    ui_context.field_key_count += 1;
}

void ui_field_event_add(const SDL_Event *event) {
    ui_event_add(event);
}

void ui_field_focus_clear(void) {
    ui_context.field_id = 0;
    ui_context.field_select_all = false;
    ui_context.field_edit[0] = '\0';
    ui_context.field_cursor = 0;
}

static void ui_field_binding_display_set(UIFieldBinding binding, TextAsset *display) {
    char value[UI_FIELD_EDIT_MAX] = {0};

    if(binding.kind == UI_FIELD_FLOAT && binding.number != NULL) {
        snprintf(value, sizeof(value), "%.1f", *binding.number);
    } else if(binding.kind == UI_FIELD_STRING && binding.string != NULL) {
        snprintf(value, sizeof(value), "%s", binding.string);
    }
    snprintf(ui_context.field_edit, sizeof(ui_context.field_edit), "%s", value);
    ui_context.field_cursor = strlen(ui_context.field_edit);
    if(display != NULL) (void)graphics_text_value_set(display, value);
}

static bool ui_field_value_valid(UIFieldBinding binding, const char *value) {
    bool decimal = false;
    size_t decimal_digits = 0;
    if(binding.kind != UI_FIELD_FLOAT) return true;
    for(size_t i = 0; value[i] != '\0'; i += 1) {
        char character = value[i];
        if(character == '-' && i == 0) continue;
        if(character == '.' && !decimal) {
            decimal = true;
            continue;
        }
        if(character < '0' || character > '9') return false;
        if(decimal && ++decimal_digits > 1) return false;
    }
    return true;
}

static bool ui_field_character_add(UIFieldBinding binding, char character,
        bool multiline) {
    char candidate[UI_FIELD_EDIT_MAX];
    size_t length = strlen(ui_context.field_edit);
    size_t cursor = ui_context.field_cursor;

    if((character < 32 || character > 126) && !(multiline && character == '\n'))
        return false;
    if(character == ',') return false;
    if(ui_context.field_select_all) length = cursor = 0;
    if(length + 1 >= sizeof(ui_context.field_edit)) return false;
    if(ui_context.field_select_all) candidate[0] = '\0';
    else memcpy(candidate, ui_context.field_edit, length + 1);
    memmove(candidate + cursor + 1, candidate + cursor, length - cursor + 1);
    candidate[cursor] = character;
    if(!ui_field_value_valid(binding, candidate)) return false;
    memcpy(ui_context.field_edit, candidate, length + 2);
    ui_context.field_cursor = cursor + 1;
    ui_context.field_select_all = false;
    return true;
}

static float ui_field_text_width_get(const TextAsset *display, size_t length) {
    TTF_Font *font;
    int width = 0;
    int height = 0;

    if(display == NULL || display->text == NULL || length == 0) return 0.0f;
    font = TTF_GetTextFont(display->text);
    if(font == NULL || !TTF_GetStringSize(font, ui_context.field_edit, length,
            &width, &height)) return 0.0f;
    return (float)width;
}

static void ui_field_cursor_from_pointer(const TextAsset *display, UIRect bounds,
        bool multiline) {
    size_t length = strlen(ui_context.field_edit);
    if(display == NULL || display->text == NULL || length == 0) {
        ui_context.field_cursor = length;
        return;
    }
    if(multiline) {
        TTF_SubString substring;
        int x = (int)(ui_context.input.pointer.x - bounds.x - 6.0f);
        int y = (int)(ui_context.input.pointer.y - bounds.y - 6.0f +
            ui_context.field_scroll_y);
        if(TTF_GetTextSubStringForPoint(display->text, x, y, &substring))
            ui_context.field_cursor = (size_t)substring.offset;
    } else {
        float text_width = ui_field_text_width_get(display, length);
        float left = bounds.x + (bounds.width - text_width) * 0.5f;
        float pointer_x = ui_context.input.pointer.x - left;
        float closest_distance = fabsf(pointer_x);
        ui_context.field_cursor = 0;
        for(size_t cursor = 1; cursor <= length; cursor += 1) {
            float cursor_x = ui_field_text_width_get(display, cursor);
            float distance = fabsf(pointer_x - cursor_x);
            if(distance < closest_distance) {
                closest_distance = distance;
                ui_context.field_cursor = cursor;
            }
        }
    }
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

static UIFieldResult ui_field_draw(const char *id, UIFieldBinding binding,
        TextAsset *display, UIRect bounds, const UIButtonStyle *style,
        bool multiline) {
    UIFieldResult result = {0};
    UIButtonStyle resolved = style == NULL ? ui_button_style_default_get() : *style;
    uint64_t field_id = ui_hash_id(id);
    UIButtonResult interaction;
    UIRect resolved_bounds;

    if(!ui_context.frame_active || field_id == 0 || bounds.width <= 0.0f ||
            bounds.height <= 0.0f) return result;
    interaction = ui_interaction(id, bounds);
    resolved_bounds = ui_bounds_resolve(bounds);
    result.hovered = interaction.hovered;
    if(result.hovered && ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED) {
        bool newly_active = ui_context.field_id != field_id;
        if(newly_active) {
            ui_field_binding_display_set(binding, display);
            ui_context.field_scroll_y = 0.0f;
        }
        ui_context.field_id = field_id;
        ui_context.field_select_all = newly_active && binding.kind == UI_FIELD_FLOAT;
        if(!ui_context.field_select_all) {
            ui_field_cursor_from_pointer(display, resolved_bounds, multiline);
        }
    } else if(interaction.keyboard_activated) {
        ui_context.field_id = field_id;
        ui_context.field_select_all = false;
        ui_field_binding_display_set(binding, display);
    } else if(ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED &&
            ui_context.field_id == field_id && !result.hovered) {
        ui_context.field_id = 0;
        ui_context.field_select_all = false;
    }
    result.active = ui_context.field_id == field_id;
    if(result.active) ui_context.field_seen = true;
    if(result.active) {
        for(size_t i = 0; i < ui_context.field_key_count; i += 1) {
            SDL_Keycode key = ui_context.field_keys[i];
            SDL_Keymod modifiers = ui_context.field_modifiers[i];
            bool edited = false;

            if((modifiers & SDL_KMOD_CTRL) && key == SDLK_A) {
                ui_context.field_select_all = true;
                ui_context.field_cursor = strlen(ui_context.field_edit);
            } else if((key == SDLK_RETURN || key == SDLK_KP_ENTER) && !multiline) {
                if(interaction.keyboard_activated) continue;
                result.submitted = true;
                ui_context.field_id = 0;
            } else if(key == SDLK_ESCAPE) {
                ui_field_binding_display_set(binding, display);
                ui_context.field_id = 0;
                ui_context.field_select_all = false;
            } else if((key == SDLK_RETURN || key == SDLK_KP_ENTER) && multiline) {
                edited = ui_field_character_add(binding, '\n', true);
            } else if(key == SDLK_LEFT || key == SDLK_RIGHT ||
                    key == SDLK_UP || key == SDLK_DOWN || key == SDLK_HOME ||
                    key == SDLK_END) {
                size_t length = strlen(ui_context.field_edit);
                if(ui_context.field_select_all) {
                    ui_context.field_cursor = key == SDLK_LEFT || key == SDLK_UP ||
                        key == SDLK_HOME ? 0 : length;
                    ui_context.field_select_all = false;
                } else if((key == SDLK_LEFT && ui_context.field_cursor > 0)) {
                    ui_context.field_cursor -= 1;
                } else if(key == SDLK_RIGHT && ui_context.field_cursor < length) {
                    ui_context.field_cursor += 1;
                } else if(key == SDLK_UP || key == SDLK_HOME) {
                    ui_context.field_cursor = 0;
                } else if(key == SDLK_DOWN || key == SDLK_END) {
                    ui_context.field_cursor = length;
                }
            } else if(key == SDLK_BACKSPACE) {
                size_t length = strlen(ui_context.field_edit);
                if(ui_context.field_select_all) {
                    ui_context.field_edit[0] = '\0';
                    ui_context.field_cursor = 0;
                    ui_context.field_select_all = false;
                    edited = true;
                } else if(ui_context.field_cursor > 0) {
                    memmove(ui_context.field_edit + ui_context.field_cursor - 1,
                        ui_context.field_edit + ui_context.field_cursor,
                        length - ui_context.field_cursor + 1);
                    ui_context.field_cursor -= 1;
                    edited = true;
                }
            } else if(key == SDLK_DELETE) {
                size_t length = strlen(ui_context.field_edit);
                if(ui_context.field_select_all) {
                    ui_context.field_edit[0] = '\0';
                    ui_context.field_cursor = 0;
                    ui_context.field_select_all = false;
                    edited = true;
                } else if(ui_context.field_cursor < length) {
                    memmove(ui_context.field_edit + ui_context.field_cursor,
                        ui_context.field_edit + ui_context.field_cursor + 1,
                        length - ui_context.field_cursor);
                    edited = true;
                }
            } else if(!(modifiers & SDL_KMOD_CTRL) && key >= 0 && key <= 127) {
                edited = ui_field_character_add(binding, (char)key, multiline);
            }
            if(edited && ui_field_binding_store(binding)) result.changed = true;
        }
        if(display != NULL) {
            (void)graphics_text_value_set(display, ui_context.field_edit);
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
    if(multiline && display != NULL && display->text != NULL) {
        int text_width;
        int text_height;
        (void)TTF_SetTextWrapWidth(display->text,
            (int)fmaxf(1.0f, resolved_bounds.width - 12.0f));
        if(TTF_GetTextSize(display->text, &text_width, &text_height))
            display->size = (Scale){(float)text_width, (float)text_height};
        if(result.active && result.hovered && ui_context.wheel_y != 0.0f) {
            float maximum = fmaxf(0.0f, display->size.y - resolved_bounds.height + 12.0f);
            ui_context.field_scroll_y = fmaxf(0.0f, fminf(maximum,
                ui_context.field_scroll_y - ui_context.wheel_y * 24.0f));
            ui_context.wheel_y = 0.0f;
        }
    }
    ui_surface_raw(resolved_bounds,
        result.active ? resolved.hovered :
            ((result.hovered || interaction.focused) ? resolved.hovered : resolved.idle));
    if(result.active) {
        ui_border_raw(resolved_bounds, 2.0f, (Color){165, 195, 245, 255});
        if(display != NULL && display->text != NULL) {
            bool clipped = ui_clip_raw_begin(resolved_bounds);
            float text_width = ui_field_text_width_get(display,
                strlen(ui_context.field_edit));
            float text_left = multiline ? resolved_bounds.x + 6.0f :
                resolved_bounds.x + (resolved_bounds.width - text_width) * 0.5f;
            float text_top = multiline ? resolved_bounds.y + 6.0f -
                ui_context.field_scroll_y : resolved_bounds.y +
                (resolved_bounds.height - display->size.y) * 0.5f;
            if(ui_context.field_select_all && text_width > 0.0f) {
                ui_surface_raw((UIRect){text_left, text_top, text_width,
                    display->size.y}, (Color){80, 120, 185, 210});
            } else {
                if(multiline) {
                    TTF_SubString substring;
                    if(TTF_GetTextSubString(display->text,
                            (int)ui_context.field_cursor, &substring))
                        ui_surface_raw((UIRect){text_left + substring.rect.x,
                            text_top + substring.rect.y, 1.0f,
                            (float)substring.rect.h}, (Color){245, 248, 252, 255});
                } else {
                    float cursor_x = text_left +
                        ui_field_text_width_get(display, ui_context.field_cursor);
                    ui_surface_raw((UIRect){cursor_x, text_top, 1.0f,
                        display->size.y}, (Color){245, 248, 252, 255});
                }
            }
            if(clipped) ui_clip_end();
        }
    }
    if(multiline) {
        bool clipped = ui_clip_raw_begin(resolved_bounds);
        if(display != NULL) (void)graphics_text_draw(display,
            (Position){resolved_bounds.x + 6.0f,
                resolved_bounds.y + 6.0f - ui_context.field_scroll_y});
        if(clipped) ui_clip_end();
    } else ui_label_raw(display, resolved_bounds);
    return result;
}

UIFieldResult ui_field(const char *id, UIFieldBinding binding,
        TextAsset *display, UIRect bounds, const UIButtonStyle *style) {
    return ui_field_draw(id, binding, display, bounds, style, false);
}

UIFieldResult ui_multiline_field(const char *id, UIFieldBinding binding,
        TextAsset *display, UIRect bounds, const UIButtonStyle *style) {
    if(binding.kind != UI_FIELD_STRING) return (UIFieldResult){0};
    return ui_field_draw(id, binding, display, bounds, style, true);
}

UIButtonResult ui_button(
    const char *id,
    const TextAsset *label,
    UIRect bounds,
    const UIButtonStyle *style
) {
    UIButtonResult result;
    UIButtonStyle resolved_style = style == NULL ? ui_button_style_default_get() : *style;
    result = ui_interaction(id, bounds);
    ui_surface(bounds,
        result.pressed ? resolved_style.pressed :
            ((result.hovered || result.focused) ? resolved_style.hovered : resolved_style.idle)
    );
    ui_content(label, bounds);
    return result;
}

static UIDropdownResult ui_dropdown_draw(const char *id, const TextAsset *label,
    const TextAsset *const *options, size_t option_count, size_t selected_index,
    bool always_changed, UIRect bounds, const UIButtonStyle *style) {
    UIDropdownResult result = {.selected_index = selected_index, .hovered_index = -1};
    uint64_t dropdown_id = ui_hash_id(id);
    UIButtonResult button;
    UIRect resolved_bounds = ui_bounds_resolve(bounds);

    if(!ui_context.frame_active || dropdown_id == 0 || options == NULL ||
            option_count == 0 || option_count > UI_DROPDOWN_OPTION_MAX ||
            selected_index >= option_count ||
            bounds.width <= 0.0f || bounds.height <= 0.0f) return result;
    button = ui_button(id, label, bounds, style);
    ui_border_raw(resolved_bounds, 2.0f, (Color){0, 0, 0, 255});
    result.button_hovered = button.hovered;
    result.hovered = button.hovered;
    if(button.clicked) {
        if(ui_context.dropdown_id == dropdown_id) {
            ui_context.dropdown_id = 0;
        } else {
            ui_context.dropdown_id = dropdown_id;
            ui_context.dropdown_first_option = selected_index >= UI_DROPDOWN_VISIBLE_MAX ?
                selected_index - UI_DROPDOWN_VISIBLE_MAX + 1 : 0;
        }
    }
    result.open = ui_context.dropdown_id == dropdown_id;
    if(!result.open) return result;
    ui_context.dropdown_seen = true;
    {
        size_t visible_count = option_count < UI_DROPDOWN_VISIBLE_MAX ?
            option_count : UI_DROPDOWN_VISIBLE_MAX;
        UIRect menu_bounds = {resolved_bounds.x, resolved_bounds.y + resolved_bounds.height,
            resolved_bounds.width, resolved_bounds.height * (float)visible_count};
        if(ui_point_in_rect(ui_context.input.pointer, menu_bounds) &&
                ui_context.wheel_y != 0.0f && option_count > visible_count) {
            int direction = ui_context.wheel_y < 0.0f ? 1 : -1;
            int first = (int)ui_context.dropdown_first_option + direction;
            int maximum = (int)(option_count - visible_count);
            if(first < 0) first = 0;
            if(first > maximum) first = maximum;
            ui_context.dropdown_first_option = (size_t)first;
            ui_context.wheel_y = 0.0f;
            ui_context.pointer_consumed = true;
        }
    }
    ui_context.dropdown_render_id = dropdown_id;
    ui_context.dropdown_option_count = option_count - ui_context.dropdown_first_option;
    if(ui_context.dropdown_option_count > UI_DROPDOWN_VISIBLE_MAX) {
        ui_context.dropdown_option_count = UI_DROPDOWN_VISIBLE_MAX;
    }
    ui_context.dropdown_bounds = resolved_bounds;
    ui_context.dropdown_style = style == NULL ? ui_button_style_default_get() : *style;
    for(size_t i = 0; i < ui_context.dropdown_option_count; i += 1) {
        ui_context.dropdown_options[i] = options[ui_context.dropdown_first_option + i];
    }
    for(size_t slot = 0; slot < ui_context.dropdown_option_count; slot += 1) {
        size_t i = ui_context.dropdown_first_option + slot;
        char option_id[160];
        UIRect option_bounds = {bounds.x, bounds.y + bounds.height * (float)(slot + 1),
            bounds.width, bounds.height};
        UIButtonResult option_result;
        snprintf(option_id, sizeof(option_id), "%s.option.%zu", id, i);
        ui_context.dropdown_option_ids[slot] = ui_hash_id(option_id);
        ui_context.dropdown_option_interaction = true;
        option_result = ui_button(option_id, options[i], option_bounds, style);
        ui_context.dropdown_option_interaction = false;
        ui_dropdown_divider_raw(ui_bounds_resolve(option_bounds));
        if(option_result.hovered) {
            result.hovered = true;
            result.hovered_index = (int)i;
        }
        if(option_result.clicked) {
            result.selected_index = i;
            result.changed = always_changed || i != selected_index;
            result.open = false;
            ui_context.navigation_focus_id = dropdown_id;
            ui_context.dropdown_id = 0;
        }
    }
    ui_border_raw((UIRect){resolved_bounds.x, resolved_bounds.y + resolved_bounds.height,
        resolved_bounds.width,
        resolved_bounds.height * (float)ui_context.dropdown_option_count},
        2.0f, (Color){0, 0, 0, 255});
    if(ui_context.input.primary_button == MOUSE_BUTTON_STATE_PRESSED &&
            !ui_point_in_rect(ui_context.input.pointer, (UIRect){resolved_bounds.x,
                resolved_bounds.y, resolved_bounds.width,
                resolved_bounds.height * (float)(ui_context.dropdown_option_count + 1)})) {
        ui_context.dropdown_id = 0;
        result.open = false;
    }
    return result;
}

UIDropdownResult ui_dropdown(const char *id, const TextAsset *const *options,
    size_t option_count, size_t selected_index, UIRect bounds,
    const UIButtonStyle *style) {
    if(options == NULL || selected_index >= option_count) return (UIDropdownResult){0};
    return ui_dropdown_draw(id, options[selected_index], options, option_count,
        selected_index, false, bounds, style);
}

UIDropdownResult ui_menu(const char *id, const TextAsset *label,
    const TextAsset *const *options, size_t option_count, UIRect bounds,
    const UIButtonStyle *style) {
    return ui_dropdown_draw(id, label, options, option_count, 0, true, bounds, style);
}

UIScrollRegionResult ui_scroll_region_begin(const char *id, UIRect bounds,
    float content_height, float offset, float wheel_step) {
    UIScrollRegionResult result = {.offset = offset};
    UIRect resolved_bounds;
    float maximum;

    if(!ui_context.frame_active || ui_hash_id(id) == 0 || bounds.width <= 0.0f ||
            bounds.height <= 0.0f || content_height < 0.0f || wheel_step <= 0.0f ||
            ui_context.scroll_depth >= UI_SCROLL_REGION_MAX) return result;
    resolved_bounds = ui_bounds_resolve(bounds);
    result.hovered = ui_point_in_rect(ui_context.input.pointer, resolved_bounds) &&
        ui_point_in_scroll_clip(ui_context.input.pointer);
    maximum = fmaxf(0.0f, content_height - bounds.height);
    if(result.hovered && ui_context.wheel_y != 0.0f) {
        result.offset -= ui_context.wheel_y * wheel_step;
        ui_context.wheel_y = 0.0f;
        result.changed = true;
        ui_context.pointer_consumed = true;
    }
    result.offset = fminf(maximum, fmaxf(0.0f, result.offset));
    ui_context.translation_stack[ui_context.scroll_depth] = ui_context.translation_y;
    ui_context.scroll_clip_stack[ui_context.scroll_depth] = resolved_bounds;
    ui_context.scroll_offset_stack[ui_context.scroll_depth] = result.offset;
    ui_context.scroll_content_stack[ui_context.scroll_depth] = content_height;
    ui_context.scroll_depth += 1;
    ui_context.translation_y -= result.offset;
    (void)ui_clip_raw_begin(resolved_bounds);
    return result;
}

void ui_scroll_region_end(void) {
    UIRect bounds;
    float offset;
    float content_height;

    if(!ui_context.frame_active || ui_context.scroll_depth == 0) return;
    ui_context.scroll_depth -= 1;
    bounds = ui_context.scroll_clip_stack[ui_context.scroll_depth];
    offset = ui_context.scroll_offset_stack[ui_context.scroll_depth];
    content_height = ui_context.scroll_content_stack[ui_context.scroll_depth];
    ui_context.translation_y = ui_context.translation_stack[ui_context.scroll_depth];
    ui_clip_end();
    if(content_height > bounds.height) {
        float handle_height = fmaxf(20.0f, bounds.height * bounds.height / content_height);
        float travel = bounds.height - handle_height;
        float maximum = content_height - bounds.height;
        float handle_y = bounds.y + (maximum <= 0.0f ? 0.0f : offset / maximum * travel);
        ui_surface_raw((UIRect){bounds.x + bounds.width - 5.0f, bounds.y,
            5.0f, bounds.height}, (Color){18, 20, 25, 210});
        ui_surface_raw((UIRect){bounds.x + bounds.width - 5.0f, handle_y,
            5.0f, handle_height}, (Color){105, 115, 135, 255});
    }
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
    UIButtonResult slider_interaction;
    int step_index;

    resolved.center.y += ui_context.translation_y;

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
            ) && ui_point_in_scroll_clip(ui_context.input.pointer);
            ui_navigation_item_register(step_id, (UIRect){step_centers[step_index].x - 0.5f,
                step_centers[step_index].y - 0.5f, 1.0f, 1.0f});
            UIButtonResult interaction = ui_interaction_id(step_id, inside);
            step_hovered[step_index] = interaction.hovered;
            step_pressed[step_index] = interaction.pressed;
            result.hovered = result.hovered || interaction.hovered;
            result.pressed = result.pressed || interaction.pressed;
            if(interaction.clicked) {
                float direction = step_index == 0 ? -1.0f : 1.0f;
                if(resolved.max_value < resolved.min_value) direction = -direction;
                result.value = ui_slider_snap(result.value + direction * resolved.step,
                    &resolved);
                result.changed = result.changed || result.value != value;
            }
        }
    }
    contains_pointer = ui_point_in_slider(ui_context.input.pointer, &resolved) &&
        ui_point_in_scroll_clip(ui_context.input.pointer);
    ui_navigation_item_register(slider_id, (UIRect){resolved.center.x - 0.5f,
        resolved.center.y - 0.5f, 1.0f, 1.0f});
    slider_interaction = ui_interaction_id(slider_id, contains_pointer);
    result.hovered = result.hovered || slider_interaction.hovered;
    result.pressed = result.pressed || slider_interaction.pressed;
    if(slider_interaction.pressed || slider_interaction.clicked) {
        float previous = result.value;
        amount = ui_slider_pointer_amount(ui_context.input.pointer, &resolved);
        result.value = ui_slider_snap(resolved.min_value
            + amount * (resolved.max_value - resolved.min_value), &resolved);
        result.changed = result.changed || result.value != previous;
    }

    amount = ui_clamp_unit(
        (result.value - resolved.min_value) /
        (resolved.max_value - resolved.min_value)
    );
    start = (Position){
        resolved.center.x - axis.x * resolved.length * 0.5f,
        resolved.center.y - axis.y * resolved.length * 0.5f,
    };
    ui_quad_raw(
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
            ui_quad_raw(
                step_centers[step_index],
                resolved.style.step_button_size,
                resolved.style.step_button_size,
                resolved.angle,
                step_pressed[step_index] ? resolved.style.handle_pressed
                    : (step_hovered[step_index] ? resolved.style.handle_hovered
                        : resolved.style.handle)
            );
            if(text != NULL) symbol = is_minus ? text->minus : text->plus;
            ui_label_raw(symbol, symbol_bounds);
        }
    }
    fill_length = resolved.length * amount;
    if(fill_length > 0.0f) {
        Position fill_center = {
            start.x + axis.x * fill_length * 0.5f,
            start.y + axis.y * fill_length * 0.5f,
        };
        ui_quad_raw(
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
    ui_quad_raw(
        handle_center,
        resolved.style.handle_width,
        resolved.style.handle_height,
        resolved.angle,
        result.pressed ? resolved.style.handle_pressed
            : ((result.hovered || slider_interaction.focused) ?
                resolved.style.handle_hovered : resolved.style.handle)
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
        ui_label_raw(text->label, label_bounds);
        ui_label_raw(text->value, value_bounds);
    }
    return result;
}

UISliderResult ui_slider(const char *id, float value, const UISliderConfig *config) {
    return ui_slider_with_text(id, value, config, NULL);
}

static void ui_label_raw(const TextAsset *text, UIRect bounds) {
    Position position;
    bool clipped;

    if(text == NULL || text->text == NULL) {
        return;
    }
    position = (Position){
        .x = bounds.x + (bounds.width - text->size.x) * 0.5f,
        .y = bounds.y + (bounds.height - text->size.y) * 0.5f,
    };
    clipped = ui_clip_raw_begin(bounds);
    (void)graphics_text_draw(text, position);
    if(clipped) ui_clip_end();
}

void ui_label(const TextAsset *text, UIRect bounds) {
    ui_label_raw(text, ui_bounds_resolve(bounds));
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
    ui_surface((UIRect){WINDOW_WIDTH - width - 5.0f, 5.0f, width, height},
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

    ui_surface(bounds, resolved_style.disabled);
}

bool ui_pointer_consumed_get(void) {
    return ui_context.pointer_consumed;
}

void ui_frame_end(void) {
    if(!ui_context.frame_active) {
        return;
    }
    if(ui_context.dropdown_id != 0 &&
            ui_context.dropdown_render_id == ui_context.dropdown_id) {
        for(size_t i = 0; i < ui_context.dropdown_option_count; i += 1) {
            UIRect bounds = {ui_context.dropdown_bounds.x,
                ui_context.dropdown_bounds.y + ui_context.dropdown_bounds.height *
                    (float)(i + 1), ui_context.dropdown_bounds.width,
                ui_context.dropdown_bounds.height};
            bool hovered = ui_point_in_rect(ui_context.input.pointer, bounds);
            uint64_t option_id = ui_context.dropdown_option_ids[i];
            ui_surface_raw(bounds,
                ui_context.active_id == option_id ? ui_context.dropdown_style.pressed :
                    ((hovered || ui_context.navigation_focus_id == option_id) ?
                        ui_context.dropdown_style.hovered :
                        ui_context.dropdown_style.idle));
            ui_label_raw(ui_context.dropdown_options[i], bounds);
            ui_dropdown_divider_raw(bounds);
        }
        ui_border_raw((UIRect){ui_context.dropdown_bounds.x,
            ui_context.dropdown_bounds.y + ui_context.dropdown_bounds.height,
            ui_context.dropdown_bounds.width,
            ui_context.dropdown_bounds.height *
                (float)ui_context.dropdown_option_count},
            2.0f, (Color){0, 0, 0, 255});
    }
    if(ui_context.input.primary_button == MOUSE_BUTTON_STATE_RELEASED ||
            (ui_context.active_id != 0 && !ui_context.active_seen)) {
        ui_context.active_id = 0;
    }
    if(ui_context.field_id != 0 && !ui_context.field_seen) {
        ui_context.field_id = 0;
    }
    if(ui_context.dropdown_id != 0 && !ui_context.dropdown_seen) {
        ui_context.dropdown_id = 0;
    }
    ui_context.navigation_previous_count = ui_context.navigation_item_count;
    memcpy(ui_context.navigation_previous, ui_context.navigation_items,
        ui_context.navigation_item_count * sizeof(ui_context.navigation_items[0]));
    if(ui_context.navigation_focus_id != 0) {
        bool found = false;
        for(size_t i = 0; i < ui_context.navigation_previous_count; i += 1) {
            if(ui_context.navigation_previous[i].id == ui_context.navigation_focus_id) {
                found = true;
                break;
            }
        }
        if(!found) ui_context.navigation_focus_id = 0;
    }
    ui_context.navigation_activate_id = 0;
    ui_context.navigation_focus_changed_id = 0;
    ui_context.field_key_count = 0;
    ui_context.wheel_y = 0.0f;
    ui_context.frame_active = false;
}
