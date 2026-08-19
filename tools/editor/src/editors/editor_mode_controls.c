#include "editor_mode_controls.h"

#include <stdio.h>
#include <string.h>

bool editor_mode_text_create(FontAsset *font, const char *value,
        TextAsset *output) {
    TextAssetResult result;
    if(font == NULL || value == NULL || output == NULL) return false;
    result = rohr_graphics_text_create(font, value, (Color){230, 234, 242, 255});
    if(rohr_error_check(result)) return false;
    *output = result.result.value;
    return true;
}

void editor_mode_numeric_disabled_draw(TextAsset *display,
        float value,
        UIRect bounds) {
    char text[32];
    if(display == NULL) return;
    snprintf(text, sizeof(text), "%.1f", value);
    (void)rohr_graphics_text_value_set(display, text);
    rohr_ui_button_disabled(bounds, NULL);
    rohr_ui_label(display, bounds);
}

bool editor_mode_checkbox_left(const char *id,
        const TextAsset *label,
        UIRect bounds,
        bool *checked) {
    UIButtonResult interaction;
    UIRect box;
    Color background;
    if(id == NULL || label == NULL || checked == NULL) return false;
    interaction = rohr_ui_interaction(id, bounds);
    if(interaction.clicked) *checked = !*checked;
    background = interaction.pressed ? (Color){58, 65, 78, 255} :
        interaction.hovered || interaction.focused ? (Color){67, 75, 90, 255} :
        (Color){48, 54, 66, 255};
    rohr_ui_surface(bounds, background);
    box = (UIRect){bounds.x + bounds.width - bounds.height + 4.0f,
        bounds.y + 4.0f, bounds.height - 8.0f, bounds.height - 8.0f};
    rohr_ui_surface(box, (Color){22, 25, 31, 255});
    rohr_ui_border(box, 2.0f, (Color){8, 9, 12, 255});
    if(*checked)
        rohr_ui_surface((UIRect){box.x + 5.0f, box.y + 5.0f,
            box.width - 10.0f, box.height - 10.0f},
            (Color){225, 230, 240, 255});
    rohr_ui_label(label, (UIRect){bounds.x + 4.0f, bounds.y,
        bounds.width - bounds.height - 4.0f, bounds.height});
    return interaction.clicked;
}

bool editor_mode_color_swatch(const char *id,
        uint32_t *color,
        bool disabled,
        UIRect bounds,
        const EditorModeContext *context,
        EditorItemKind kind,
        EditorObjectId object,
        uint32_t parent,
        uint32_t item,
        EditorPropertyKind property) {
    UIButtonStyle style;
    Color displayed;
    UIButtonResult result;
    if(id == NULL || color == NULL || context == NULL) return false;
    style = rohr_ui_button_style_default_get();
    displayed = disabled ? (Color){70, 72, 78, 255} :
        rohr_graphics_color_hex_create(*color);
    style.idle = displayed;
    style.hovered = disabled ? displayed :
        (Color){displayed.red, displayed.green, displayed.blue, 220};
    style.pressed = displayed;
    style.disabled = displayed;
    if(disabled) {
        rohr_ui_button_disabled(bounds, &style);
        rohr_ui_border(bounds, 2.0f, (Color){18, 20, 24, 255});
        return false;
    }
    result = rohr_ui_button(id, NULL, bounds, &style);
    rohr_ui_border(bounds, 2.0f, (Color){8, 9, 12, 255});
    if(result.clicked && context->color_open != NULL)
        context->color_open(context->color_context, color, kind, object,
            parent, item, property);
    return result.clicked;
}

bool editor_mode_named_text_sync(FontAsset *font,
        const char *name,
        TextAsset *label,
        char *cache,
        size_t cache_capacity) {
    if(font == NULL || name == NULL || label == NULL || cache == NULL ||
            cache_capacity == 0) return false;
    if(strncmp(cache, name, cache_capacity) == 0) return true;
    if(label->text == NULL) {
        if(!editor_mode_text_create(font, name, label)) return false;
    } else if(!rohr_graphics_text_value_set(label, name)) return false;
    snprintf(cache, cache_capacity, "%s", name);
    return true;
}

UIFieldResult editor_mode_name_field(const char *id,
        char *name,
        size_t capacity,
        TextAsset *display,
        UIRect bounds) {
    UIFieldResult result = rohr_ui_field(id,
        (UIFieldBinding){.kind = UI_FIELD_STRING, .string = name,
            .string_capacity = capacity}, display, bounds, NULL);
    if(result.changed) editor_project_property_name_format(name, capacity, name);
    return result;
}

UIButtonStyle editor_mode_delete_style_get(void) {
    UIButtonStyle style = rohr_ui_button_style_default_get();
    style.idle = (Color){120, 38, 42, 255};
    style.hovered = (Color){155, 46, 52, 255};
    style.pressed = (Color){92, 29, 33, 255};
    style.disabled = (Color){70, 45, 47, 255};
    return style;
}
