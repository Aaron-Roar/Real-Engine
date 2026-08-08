#include "game_state.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "entity_components.h"
#include "graphics.h"
#include "physics.h"
#include "yyjson.h"

#define STATE_MAX_ANIMATIONS MAX_TEXTURES
#define STATE_MAX_UI_BUTTONS 256
#define STATE_MAX_UI_FONTS 64
#define STATE_MAX_UI_LABELS 256
#define STATE_MAX_UI_SLIDERS 256
#define STATE_ASSET_NAME_MAX 64
#define STATE_ASSET_PATH_MAX 512

typedef struct StateDocument {
    yyjson_doc *document;
    yyjson_val *entities;
    yyjson_val *groups;
    yyjson_val *animations;
    yyjson_val *ui_buttons;
    yyjson_val *ui_fonts;
    yyjson_val *ui_labels;
    yyjson_val *ui_sliders;
    yyjson_val *camera;
} StateDocument;

typedef struct StateLoadedEntity {
    Entity entity;
    yyjson_val *description;
    uint64_t instance;
    uint64_t instance_count;
} StateLoadedEntity;

typedef struct StateAnimation {
    char name[STATE_ASSET_NAME_MAX];
    char frame_paths[MAX_ANIMATIONS_FRAMES][STATE_ASSET_PATH_MAX];
    AnimationDescriptor descriptor;
    AnimationAsset asset;
} StateAnimation;

typedef struct StateSpriteReference {
    bool used;
    char animation[STATE_ASSET_NAME_MAX];
    Scale scale;
    Time time_per_frame;
    Tick ticks_per_frame;
    int start_frame;
} StateSpriteReference;

typedef struct StateUIButton {
    UIButtonDefinition definition;
} StateUIButton;

typedef struct StateUIFont {
    UIFontDefinition definition;
} StateUIFont;

typedef struct StateUILabel {
    UILabelDefinition definition;
} StateUILabel;

typedef struct StateUISlider {
    UISliderDefinition definition;
} StateUISlider;

static StateAnimation state_animations[STATE_MAX_ANIMATIONS] = {0};
static size_t state_animation_count = 0;
static StateSpriteReference state_sprite_references[MAX_ENTITIES] = {0};
static StateUIButton state_ui_buttons[STATE_MAX_UI_BUTTONS] = {0};
static size_t state_ui_button_count = 0;
static StateUIFont state_ui_fonts[STATE_MAX_UI_FONTS] = {0};
static size_t state_ui_font_count = 0;
static StateUILabel state_ui_labels[STATE_MAX_UI_LABELS] = {0};
static size_t state_ui_label_count = 0;
static StateUISlider state_ui_sliders[STATE_MAX_UI_SLIDERS] = {0};
static size_t state_ui_slider_count = 0;
static yyjson_doc *state_template_documents[GAME_STATE_MAX_TEMPLATE_DOCUMENTS] = {0};
static size_t state_template_document_count = 0;
static bool state_template_camera_retained = false;

static bool state_number(yyjson_val *object, const char *key, double *value);
static bool state_vec2(yyjson_val *object, Vec2D *value);

void game_state_runtime_reset(void) {
    size_t document_index;

    for(document_index = 0;
            document_index < state_template_document_count;
            document_index += 1) {
        yyjson_doc_free(state_template_documents[document_index]);
    }
    memset(state_animations, 0, sizeof(state_animations));
    memset(state_sprite_references, 0, sizeof(state_sprite_references));
    memset(state_ui_buttons, 0, sizeof(state_ui_buttons));
    memset(state_ui_fonts, 0, sizeof(state_ui_fonts));
    memset(state_ui_labels, 0, sizeof(state_ui_labels));
    memset(state_ui_sliders, 0, sizeof(state_ui_sliders));
    memset(state_template_documents, 0, sizeof(state_template_documents));
    state_animation_count = 0;
    state_ui_button_count = 0;
    state_ui_font_count = 0;
    state_ui_label_count = 0;
    state_ui_slider_count = 0;
    state_template_document_count = 0;
    state_template_camera_retained = false;
}

void game_state_entity_clear(EntityIndex index) {
    if(index < MAX_ENTITIES) {
        state_sprite_references[index] = (StateSpriteReference){0};
    }
}

static StateAnimation *state_find_animation(const char *name) {
    size_t i;

    if(name == NULL) return NULL;
    for(i = 0; i < state_animation_count; i += 1) {
        if(strcmp(state_animations[i].name, name) == 0) {
            return &state_animations[i];
        }
    }
    return NULL;
}

static EngineResult state_animation_definition_load(yyjson_val *definition) {
    StateAnimation *animation;
    yyjson_val *name;
    yyjson_val *frames;
    yyjson_val *frame;
    yyjson_val *ticks;
    double time_per_frame;
    size_t frame_index;
    size_t frame_count;
    AnimationAssetResult asset_result;

    name = yyjson_obj_get(definition, "name");
    frames = yyjson_obj_get(definition, "frames");
    ticks = yyjson_obj_get(definition, "ticks_per_frame");
    if(!yyjson_is_obj(definition)
            || !yyjson_is_str(name)
            || yyjson_get_len(name) == 0
            || yyjson_get_len(name) >= STATE_ASSET_NAME_MAX
            || !yyjson_is_arr(frames)
            || !state_number(definition, "time_per_frame", &time_per_frame)
            || !yyjson_is_uint(ticks)
            || state_animation_count >= STATE_MAX_ANIMATIONS) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    if(state_find_animation(yyjson_get_str(name)) != NULL) {
        return error_result_error(
            ERROR_ENGINE_STATE_DUPLICATE_ASSET_DEFINITION
        );
    }
    frame_count = yyjson_arr_size(frames);
    if(frame_count == 0 || frame_count > MAX_ANIMATIONS_FRAMES) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }

    animation = &state_animations[state_animation_count];
    *animation = (StateAnimation){0};
    memcpy(animation->name, yyjson_get_str(name), yyjson_get_len(name) + 1);
    animation->descriptor.amount_of_descriptors = (uint8_t)frame_count;
    animation->descriptor.ticks_per_frame = yyjson_get_uint(ticks);
    animation->descriptor.time_per_frame = time_per_frame;
    yyjson_arr_foreach(frames, frame_index, frame_count, frame) {
        yyjson_val *file = yyjson_obj_get(frame, "file");
        Vec2D size;
        size_t file_length;
        if(!yyjson_is_obj(frame)
                || !yyjson_is_str(file)
                || !state_vec2(yyjson_obj_get(frame, "size"), &size)) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        file_length = yyjson_get_len(file);
        if(file_length == 0 || file_length >= STATE_ASSET_PATH_MAX) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        memcpy(animation->frame_paths[frame_index], yyjson_get_str(file), file_length + 1);
        animation->descriptor.texture_descriptors[frame_index] = (TextureDescriptor){
            .file = animation->frame_paths[frame_index],
            .size = {size.x, size.y}
        };
    }
    asset_result = graphics_animation_load(animation->descriptor);
    if(asset_result.kind == ERROR_RESULT_ERROR) {
        *animation = (StateAnimation){0};
        return error_result_error(asset_result.result.error);
    }
    animation->asset = asset_result.result.value;
    state_animation_count += 1;
    return error_result_value(true);
}

static bool state_number(yyjson_val *object, const char *key, double *value) {
    yyjson_val *item = yyjson_obj_get(object, key);

    if(!yyjson_is_num(item) || value == NULL) {
        return false;
    }
    *value = yyjson_get_num(item);
    return true;
}

static bool state_color(yyjson_val *object, Color *color) {
    yyjson_val *red;
    yyjson_val *green;
    yyjson_val *blue;
    yyjson_val *alpha;

    if(!yyjson_is_obj(object) || color == NULL) return false;
    red = yyjson_obj_get(object, "red");
    green = yyjson_obj_get(object, "green");
    blue = yyjson_obj_get(object, "blue");
    alpha = yyjson_obj_get(object, "alpha");
    if(!yyjson_is_uint(red) || yyjson_get_uint(red) > UINT8_MAX
            || !yyjson_is_uint(green) || yyjson_get_uint(green) > UINT8_MAX
            || !yyjson_is_uint(blue) || yyjson_get_uint(blue) > UINT8_MAX
            || !yyjson_is_uint(alpha) || yyjson_get_uint(alpha) > UINT8_MAX) {
        return false;
    }
    *color = (Color){
        .red = (uint8_t)yyjson_get_uint(red),
        .green = (uint8_t)yyjson_get_uint(green),
        .blue = (uint8_t)yyjson_get_uint(blue),
        .alpha = (uint8_t)yyjson_get_uint(alpha),
    };
    return true;
}

static StateUIButton *state_find_ui_button_internal(const char *name) {
    size_t i;

    if(name == NULL) return NULL;
    for(i = 0; i < state_ui_button_count; i += 1) {
        if(strcmp(state_ui_buttons[i].definition.name, name) == 0) {
            return &state_ui_buttons[i];
        }
    }
    return NULL;
}

static StateUIFont *state_find_ui_font_internal(const char *name) {
    size_t i;

    if(name == NULL) return NULL;
    for(i = 0; i < state_ui_font_count; i += 1) {
        if(strcmp(state_ui_fonts[i].definition.name, name) == 0) {
            return &state_ui_fonts[i];
        }
    }
    return NULL;
}

static StateUILabel *state_find_ui_label_internal(const char *name) {
    size_t i;

    if(name == NULL) return NULL;
    for(i = 0; i < state_ui_label_count; i += 1) {
        if(strcmp(state_ui_labels[i].definition.name, name) == 0) {
            return &state_ui_labels[i];
        }
    }
    return NULL;
}

static StateUISlider *state_find_ui_slider_internal(const char *name) {
    size_t i;

    if(name == NULL) return NULL;
    for(i = 0; i < state_ui_slider_count; i += 1) {
        if(strcmp(state_ui_sliders[i].definition.name, name) == 0) {
            return &state_ui_sliders[i];
        }
    }
    return NULL;
}

UIButtonDefinitionResult ui_button_by_name_get(const char *name) {
    StateUIButton *button = state_find_ui_button_internal(name);

    if(button == NULL) {
        return ERROR_RESULT_MAKE_ERROR(
            UIButtonDefinitionResult,
            ERROR_ENGINE_UI_DEFINITION_NOT_FOUND
        );
    }
    return ERROR_RESULT_MAKE_VALUE(
        UIButtonDefinitionResult,
        button->definition
    );
}

UIFontDefinitionResult ui_font_by_name_get(const char *name) {
    StateUIFont *font = state_find_ui_font_internal(name);

    if(font == NULL) {
        return ERROR_RESULT_MAKE_ERROR(
            UIFontDefinitionResult,
            ERROR_ENGINE_UI_DEFINITION_NOT_FOUND
        );
    }
    return ERROR_RESULT_MAKE_VALUE(UIFontDefinitionResult, font->definition);
}

UILabelDefinitionResult ui_label_by_name_get(const char *name) {
    StateUILabel *label = state_find_ui_label_internal(name);

    if(label == NULL) {
        return ERROR_RESULT_MAKE_ERROR(
            UILabelDefinitionResult,
            ERROR_ENGINE_UI_DEFINITION_NOT_FOUND
        );
    }
    return ERROR_RESULT_MAKE_VALUE(UILabelDefinitionResult, label->definition);
}

UISliderDefinitionResult ui_slider_by_name_get(const char *name) {
    StateUISlider *slider = state_find_ui_slider_internal(name);

    if(slider == NULL) {
        return ERROR_RESULT_MAKE_ERROR(
            UISliderDefinitionResult,
            ERROR_ENGINE_UI_DEFINITION_NOT_FOUND
        );
    }
    return ERROR_RESULT_MAKE_VALUE(UISliderDefinitionResult, slider->definition);
}

static bool state_ui_bounds(yyjson_val *value, UIRect *bounds) {
    double x;
    double y;
    double width;
    double height;

    if(!yyjson_is_obj(value) || bounds == NULL
            || !state_number(value, "x", &x)
            || !state_number(value, "y", &y)
            || !state_number(value, "width", &width)
            || !state_number(value, "height", &height)
            || !isfinite(x) || !isfinite(y)
            || !isfinite(width) || width <= 0.0
            || !isfinite(height) || height <= 0.0) {
        return false;
    }
    *bounds = (UIRect){(float)x, (float)y, (float)width, (float)height};
    return true;
}

static EngineResult state_ui_font_definition_load(yyjson_val *value) {
    yyjson_val *name;
    yyjson_val *file;
    double point_size;
    UIFontDefinition definition = {0};

    if(!yyjson_is_obj(value) || state_ui_font_count >= STATE_MAX_UI_FONTS) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    name = yyjson_obj_get(value, "name");
    file = yyjson_obj_get(value, "file");
    if(!yyjson_is_str(name) || yyjson_get_len(name) == 0
            || yyjson_get_len(name) >= UI_DEFINITION_NAME_MAX
            || !yyjson_is_str(file) || yyjson_get_len(file) == 0
            || yyjson_get_len(file) >= UI_FONT_PATH_MAX
            || !state_number(value, "point_size", &point_size)
            || !isfinite(point_size) || point_size <= 0.0
            || state_find_ui_font_internal(yyjson_get_str(name)) != NULL) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    memcpy(definition.name, yyjson_get_str(name), yyjson_get_len(name) + 1);
    memcpy(definition.file, yyjson_get_str(file), yyjson_get_len(file) + 1);
    definition.point_size = (float)point_size;
    state_ui_fonts[state_ui_font_count++].definition = definition;
    return error_result_value(true);
}

static EngineResult state_ui_label_definition_load(yyjson_val *value) {
    yyjson_val *name;
    yyjson_val *text;
    yyjson_val *font;
    yyjson_val *color;
    UILabelDefinition definition = {0};

    if(!yyjson_is_obj(value) || state_ui_label_count >= STATE_MAX_UI_LABELS) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    name = yyjson_obj_get(value, "name");
    text = yyjson_obj_get(value, "text");
    font = yyjson_obj_get(value, "font");
    color = yyjson_obj_get(value, "color");
    if(!yyjson_is_str(name) || yyjson_get_len(name) == 0
            || yyjson_get_len(name) >= UI_DEFINITION_NAME_MAX
            || !yyjson_is_str(text) || yyjson_get_len(text) >= UI_LABEL_MAX
            || !yyjson_is_str(font) || yyjson_get_len(font) == 0
            || yyjson_get_len(font) >= UI_DEFINITION_NAME_MAX
            || state_find_ui_font_internal(yyjson_get_str(font)) == NULL
            || !state_color(color, &definition.color)
            || !state_ui_bounds(yyjson_obj_get(value, "bounds"), &definition.bounds)
            || state_find_ui_label_internal(yyjson_get_str(name)) != NULL) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    memcpy(definition.name, yyjson_get_str(name), yyjson_get_len(name) + 1);
    memcpy(definition.text, yyjson_get_str(text), yyjson_get_len(text) + 1);
    memcpy(definition.font, yyjson_get_str(font), yyjson_get_len(font) + 1);
    state_ui_labels[state_ui_label_count++].definition = definition;
    return error_result_value(true);
}

static EngineResult state_ui_slider_definition_load(yyjson_val *value) {
    yyjson_val *name;
    yyjson_val *range;
    yyjson_val *style;
    yyjson_val *field;
    yyjson_val *label;
    yyjson_val *font;
    yyjson_val *value_format;
    yyjson_val *text_color;
    UISliderDefinition definition = {0};
    double number;

    if(!yyjson_is_obj(value) || state_ui_slider_count >= STATE_MAX_UI_SLIDERS) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    name = yyjson_obj_get(value, "name");
    range = yyjson_obj_get(value, "range");
    style = yyjson_obj_get(value, "style");
    label = yyjson_obj_get(value, "label");
    font = yyjson_obj_get(value, "font");
    value_format = yyjson_obj_get(value, "value_format");
    text_color = yyjson_obj_get(value, "text_color");
    definition.config = ui_slider_config_default_get();
    if(!yyjson_is_str(name) || yyjson_get_len(name) == 0
            || yyjson_get_len(name) >= UI_DEFINITION_NAME_MAX
            || !state_vec2(yyjson_obj_get(value, "center"), &definition.config.center)
            || !state_number(value, "length", &number)
            || !isfinite(number) || number <= 0.0
            || (range != NULL && !yyjson_is_obj(range))
            || (style != NULL && !yyjson_is_obj(style))
            || (label != NULL && (!yyjson_is_str(label)
                || yyjson_get_len(label) >= UI_LABEL_MAX))
            || (value_format != NULL && (!yyjson_is_str(value_format)
                || yyjson_get_len(value_format) >= UI_LABEL_MAX))
            || state_find_ui_slider_internal(yyjson_get_str(name)) != NULL) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    definition.config.length = (float)number;
    definition.text_color = (Color){255, 255, 255, 255};
    if(label != NULL) memcpy(definition.label, yyjson_get_str(label), yyjson_get_len(label) + 1);
    if(value_format != NULL) memcpy(definition.value_format,
        yyjson_get_str(value_format), yyjson_get_len(value_format) + 1);
    if(definition.label[0] != '\0' || definition.value_format[0] != '\0') {
        if(!yyjson_is_str(font) || yyjson_get_len(font) == 0
                || yyjson_get_len(font) >= UI_DEFINITION_NAME_MAX
                || state_find_ui_font_internal(yyjson_get_str(font)) == NULL) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        memcpy(definition.font, yyjson_get_str(font), yyjson_get_len(font) + 1);
    } else if(font != NULL) {
        if(!yyjson_is_str(font) || yyjson_get_len(font) >= UI_DEFINITION_NAME_MAX) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        memcpy(definition.font, yyjson_get_str(font), yyjson_get_len(font) + 1);
    }
    if(text_color != NULL && !state_color(text_color, &definition.text_color)) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    field = yyjson_obj_get(value, "angle");
    if(field != NULL) {
        if(!yyjson_is_num(field) || !isfinite(yyjson_get_num(field))) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        definition.config.angle = (float)yyjson_get_num(field);
    }
    if(range != NULL) {
        double min_value;
        double max_value;
        if(!state_number(range, "min", &min_value)
                || !state_number(range, "max", &max_value)
                || !isfinite(min_value) || !isfinite(max_value)
                || min_value == max_value) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        definition.config.min_value = (float)min_value;
        definition.config.max_value = (float)max_value;
    }
    field = yyjson_obj_get(value, "step");
    if(field != NULL) {
        if(!yyjson_is_num(field) || !isfinite(yyjson_get_num(field))
                || yyjson_get_num(field) < 0.0) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        definition.config.step = (float)yyjson_get_num(field);
    }
    definition.initial_value = definition.config.min_value;
    field = yyjson_obj_get(value, "initial_value");
    if(field != NULL) {
        float low = fminf(definition.config.min_value, definition.config.max_value);
        float high = fmaxf(definition.config.min_value, definition.config.max_value);
        if(!yyjson_is_num(field) || !isfinite(yyjson_get_num(field))
                || yyjson_get_num(field) < low || yyjson_get_num(field) > high) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        definition.initial_value = (float)yyjson_get_num(field);
    }
    if(style != NULL) {
#define LOAD_SLIDER_COLOR(Key, Field) do { \
    yyjson_val *color_value = yyjson_obj_get(style, Key); \
    if(color_value != NULL && !state_color(color_value, &definition.config.style.Field)) { \
        return error_result_error(ERROR_ENGINE_STATE_INVALID); \
    } \
} while(0)
        LOAD_SLIDER_COLOR("track", track);
        LOAD_SLIDER_COLOR("fill", fill);
        LOAD_SLIDER_COLOR("handle", handle);
        LOAD_SLIDER_COLOR("handle_hovered", handle_hovered);
        LOAD_SLIDER_COLOR("handle_pressed", handle_pressed);
#undef LOAD_SLIDER_COLOR
#define LOAD_SLIDER_SIZE(Key, Field) do { \
    yyjson_val *size_value = yyjson_obj_get(style, Key); \
    if(size_value != NULL) { \
        if(!yyjson_is_num(size_value) || !isfinite(yyjson_get_num(size_value)) \
                || yyjson_get_num(size_value) <= 0.0) { \
            return error_result_error(ERROR_ENGINE_STATE_INVALID); \
        } \
        definition.config.style.Field = (float)yyjson_get_num(size_value); \
    } \
} while(0)
        LOAD_SLIDER_SIZE("track_thickness", track_thickness);
        LOAD_SLIDER_SIZE("handle_width", handle_width);
        LOAD_SLIDER_SIZE("handle_height", handle_height);
        field = yyjson_obj_get(style, "step_button_size");
        if(field != NULL) {
            if(!yyjson_is_num(field) || !isfinite(yyjson_get_num(field))
                    || yyjson_get_num(field) < 0.0) {
                return error_result_error(ERROR_ENGINE_STATE_INVALID);
            }
            definition.config.style.step_button_size = (float)yyjson_get_num(field);
        }
#undef LOAD_SLIDER_SIZE
        field = yyjson_obj_get(style, "step_button_gap");
        if(field != NULL) {
            if(!yyjson_is_num(field) || !isfinite(yyjson_get_num(field))
                    || yyjson_get_num(field) < 0.0) {
                return error_result_error(ERROR_ENGINE_STATE_INVALID);
            }
            definition.config.style.step_button_gap = (float)yyjson_get_num(field);
        }
    }
    if(state_find_ui_button_internal(yyjson_get_str(name)) != NULL) {
        return error_result_error(ERROR_ENGINE_STATE_DUPLICATE_ASSET_DEFINITION);
    }
    memcpy(definition.name, yyjson_get_str(name), yyjson_get_len(name) + 1);
    state_ui_sliders[state_ui_slider_count++].definition = definition;
    return error_result_value(true);
}

static EngineResult state_ui_button_definition_load(yyjson_val *value) {
    yyjson_val *name;
    yyjson_val *label;
    yyjson_val *font;
    yyjson_val *text_color;
    yyjson_val *bounds;
    yyjson_val *style;
    UIButtonDefinition definition = {0};

    if(!yyjson_is_obj(value) || state_ui_button_count >= STATE_MAX_UI_BUTTONS) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    name = yyjson_obj_get(value, "name");
    label = yyjson_obj_get(value, "label");
    font = yyjson_obj_get(value, "font");
    text_color = yyjson_obj_get(value, "text_color");
    bounds = yyjson_obj_get(value, "bounds");
    style = yyjson_obj_get(value, "style");
    if(!yyjson_is_str(name) || yyjson_get_len(name) == 0
            || yyjson_get_len(name) >= UI_DEFINITION_NAME_MAX
            || (label != NULL && (!yyjson_is_str(label)
                || yyjson_get_len(label) >= UI_LABEL_MAX))
            || (font != NULL && (!yyjson_is_str(font)
                || yyjson_get_len(font) == 0
                || yyjson_get_len(font) >= UI_DEFINITION_NAME_MAX
                || state_find_ui_font_internal(yyjson_get_str(font)) == NULL))
            || (label != NULL && yyjson_get_len(label) > 0 && font == NULL)
            || (text_color != NULL
                && !state_color(text_color, &definition.text_color))
            || !state_ui_bounds(bounds, &definition.bounds)
            || (style != NULL && !yyjson_is_obj(style))) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    if(state_find_ui_button_internal(yyjson_get_str(name)) != NULL) {
        return error_result_error(ERROR_ENGINE_STATE_DUPLICATE_ASSET_DEFINITION);
    }
    if(state_find_ui_slider_internal(yyjson_get_str(name)) != NULL) {
        return error_result_error(ERROR_ENGINE_STATE_DUPLICATE_ASSET_DEFINITION);
    }

    memcpy(definition.name, yyjson_get_str(name), yyjson_get_len(name) + 1);
    if(label != NULL) {
        memcpy(definition.label, yyjson_get_str(label), yyjson_get_len(label) + 1);
    }
    if(font != NULL) {
        memcpy(definition.font, yyjson_get_str(font), yyjson_get_len(font) + 1);
    }
    if(text_color == NULL) {
        definition.text_color = (Color){255, 255, 255, 255};
    }
    definition.style = ui_button_style_default_get();
    if(style != NULL) {
#define LOAD_UI_COLOR(Key, Field) do { \
    yyjson_val *color_value = yyjson_obj_get(style, Key); \
    if(color_value != NULL && !state_color(color_value, &definition.style.Field)) { \
        return error_result_error(ERROR_ENGINE_STATE_INVALID); \
    } \
} while(0)
        LOAD_UI_COLOR("idle", idle);
        LOAD_UI_COLOR("hovered", hovered);
        LOAD_UI_COLOR("pressed", pressed);
        LOAD_UI_COLOR("disabled", disabled);
#undef LOAD_UI_COLOR
    }
    state_ui_buttons[state_ui_button_count].definition = definition;
    state_ui_button_count += 1;
    return error_result_value(true);
}

static bool state_boolean(yyjson_val *object, const char *key, bool *value) {
    yyjson_val *item = yyjson_obj_get(object, key);

    if(!yyjson_is_bool(item) || value == NULL) {
        return false;
    }
    *value = yyjson_get_bool(item);
    return true;
}

static bool state_vec2(yyjson_val *object, Vec2D *value) {
    double x;
    double y;

    if(!yyjson_is_obj(object) || value == NULL
            || !state_number(object, "x", &x)
            || !state_number(object, "y", &y)) {
        return false;
    }
    *value = (Vec2D){(float)x, (float)y};
    return true;
}

static bool state_optional_boolean(
        yyjson_val *object,
        const char *key,
        bool default_value,
        bool *value
) {
    yyjson_val *item;

    if(!yyjson_is_obj(object) || value == NULL) return false;
    item = yyjson_obj_get(object, key);
    if(item == NULL) {
        *value = default_value;
        return true;
    }
    if(!yyjson_is_bool(item)) return false;
    *value = yyjson_get_bool(item);
    return true;
}

static EngineResult state_placement_apply(
        const StateLoadedEntity *loaded
) {
    yyjson_val *placement;
    yyjson_val *type;
    EntityIndex index;
    Position origin;
    Position generated;

    if(loaded == NULL) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    placement = yyjson_obj_get(loaded->description, "placement");
    if(placement == NULL) return error_result_value(true);
    type = yyjson_obj_get(placement, "type");
    if(!yyjson_is_obj(placement) || !yyjson_is_str(type)) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    if(strcmp(yyjson_get_str(type), "point") == 0) {
        return error_result_value(true);
    }
    if(!entity_index_get(loaded->entity, &index)
            || index >= positions_pool.capacity
            || positions_pool.used[index] == 0) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    origin = positions[index];
    generated = origin;

    if(strcmp(yyjson_get_str(type), "line") == 0) {
        Vec2D step;
        bool centered;
        double offset;
        if(!state_vec2(yyjson_obj_get(placement, "step"), &step)
                || !state_optional_boolean(
                    placement,
                    "centered",
                    false,
                    &centered
                )) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        offset = centered
            ? (double)loaded->instance
                - ((double)loaded->instance_count - 1.0) * 0.5
            : (double)loaded->instance;
        generated.x += step.x * (float)offset;
        generated.y += step.y * (float)offset;
    }
    else if(strcmp(yyjson_get_str(type), "grid") == 0) {
        yyjson_val *columns_value = yyjson_obj_get(placement, "columns");
        Vec2D spacing;
        uint64_t columns;
        uint64_t rows;
        uint64_t column;
        uint64_t row;
        bool centered;
        double column_offset;
        double row_offset;
        if(!yyjson_is_uint(columns_value)
                || yyjson_get_uint(columns_value) == 0
                || yyjson_get_uint(columns_value) > MAX_ENTITIES
                || !state_vec2(yyjson_obj_get(placement, "spacing"), &spacing)
                || !state_optional_boolean(
                    placement,
                    "centered",
                    false,
                    &centered
                )) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        columns = yyjson_get_uint(columns_value);
        rows = (loaded->instance_count + columns - 1) / columns;
        column = loaded->instance % columns;
        row = loaded->instance / columns;
        column_offset = centered
            ? (double)column
                - ((double)(columns < loaded->instance_count
                    ? columns
                    : loaded->instance_count) - 1.0) * 0.5
            : (double)column;
        row_offset = centered
            ? (double)row - ((double)rows - 1.0) * 0.5
            : (double)row;
        generated.x += spacing.x * (float)column_offset;
        generated.y += spacing.y * (float)row_offset;
    }
    else if(strcmp(yyjson_get_str(type), "circle") == 0) {
        double radius;
        double start_angle = 0.0;
        double angle;
        yyjson_val *start_angle_value = yyjson_obj_get(
            placement,
            "start_angle"
        );
        if(!state_number(placement, "radius", &radius) || radius < 0.0) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        if(start_angle_value != NULL
                && !state_number(placement, "start_angle", &start_angle)) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        angle = start_angle
            + 2.0 * (double)PI_F
                * (double)loaded->instance
                / (double)loaded->instance_count;
        generated.x += (float)(cos(angle) * radius);
        generated.y += (float)(sin(angle) * radius);
    }
    else {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    return physics_position_set(loaded->entity, generated);
}

static EngineResult state_resolve_name(yyjson_val *value, Entity *entity) {
    EntityResult result;

    if(!yyjson_is_str(value) || entity == NULL) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    result = entity_by_name_get(yyjson_get_str(value));
    if(result.kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_ENGINE_STATE_REFERENCE_NOT_FOUND);
    }
    *entity = result.result.value;
    return error_result_value(true);
}

static EngineResult state_camera_load(yyjson_val *camera) {
    yyjson_val *attachment;
    yyjson_val *transform;
    yyjson_val *entity_name;
    Vec2D position_offset;
    Position position;
    double orientation;
    double orientation_offset;
    bool follow_position;
    bool follow_orientation;
    Entity entity;
    EngineResult result;

    if(!yyjson_is_obj(camera)) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    attachment = yyjson_obj_get(camera, "attachment");
    transform = yyjson_obj_get(camera, "transform");
    if((attachment != NULL && !yyjson_is_obj(attachment))
            || (transform != NULL && !yyjson_is_obj(transform))
            || (attachment == NULL) == (transform == NULL)) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    if(transform != NULL) {
        if(!state_vec2(yyjson_obj_get(transform, "position"), &position)
                || !state_number(transform, "orientation", &orientation)) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        return graphics_camera_set(
            graphics_camera_active_get(),
            (Camera){
                .position = position,
                .orientation = (Orientation)orientation
            }
        );
    }

    entity_name = yyjson_obj_get(attachment, "entity");
    if(!state_vec2(
            yyjson_obj_get(attachment, "position_offset"),
            &position_offset
        )
            || !state_number(
                attachment,
                "orientation_offset",
                &orientation_offset
            )
            || !state_optional_boolean(
                attachment,
                "follow_position",
                true,
                &follow_position
            )
            || !state_optional_boolean(
                attachment,
                "follow_orientation",
                true,
                &follow_orientation
            )) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    result = state_resolve_name(entity_name, &entity);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    return graphics_camera_with_options_attach(
        entity,
        position_offset,
        (Orientation)orientation_offset,
        follow_position,
        follow_orientation
    );
}

static bool state_entity_count(yyjson_val *description, uint64_t *count) {
    yyjson_val *value;

    if(description == NULL || count == NULL) return false;
    value = yyjson_obj_get(description, "count");
    if(value == NULL) {
        *count = 1;
        return true;
    }
    if(!yyjson_is_uint(value)
            || yyjson_get_uint(value) == 0
            || yyjson_get_uint(value) > MAX_ENTITIES) {
        return false;
    }
    *count = yyjson_get_uint(value);
    return true;
}

static EngineResult state_make_entity_name(
        const char *base,
        uint64_t instance,
        char name[ENTITY_NAME_MAX]
) {
    int written;
    uint64_t suffix = instance;

    if(base == NULL || base[0] == '\0') {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY_NAME);
    }
    while(true) {
        if(suffix == 0) {
            written = snprintf(name, ENTITY_NAME_MAX, "%s", base);
        } else {
            written = snprintf(
                name,
                ENTITY_NAME_MAX,
                "%s_%llu",
                base,
                (unsigned long long)suffix
            );
        }
        if(written < 0 || written >= ENTITY_NAME_MAX) {
            return error_result_error(ERROR_ENGINE_ENTITY_NAME_TOO_LONG);
        }
        if(entity_by_name_get(name).kind == ERROR_RESULT_ERROR) {
            return error_result_value(true);
        }
        suffix += 1;
    }
}

static uint64_t state_variation_hash(uint64_t value) {
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}

static double state_variation_random_unit(
        uint64_t seed,
        uint64_t instance,
        uint64_t salt
) {
    uint64_t bits = state_variation_hash(
        seed ^ state_variation_hash(instance) ^ salt
    );
    return (double)(bits >> 11) * (1.0 / 9007199254740992.0);
}

static bool state_variation_scalar(
        yyjson_val *specification,
        uint64_t instance,
        uint64_t instance_count,
        uint64_t salt,
        double *value
) {
    yyjson_val *generator;

    if(!yyjson_is_obj(specification) || value == NULL) return false;
    generator = yyjson_obj_get(specification, "random");
    if(generator != NULL) {
        yyjson_val *seed_value = yyjson_obj_get(generator, "seed");
        double min;
        double max;
        double unit;
        if(!yyjson_is_obj(generator)
                || !yyjson_is_uint(seed_value)
                || !state_number(generator, "min", &min)
                || !state_number(generator, "max", &max)
                || max < min) {
            return false;
        }
        unit = state_variation_random_unit(
            yyjson_get_uint(seed_value),
            instance,
            salt
        );
        *value = min + (max - min) * unit;
        return true;
    }

    generator = yyjson_obj_get(specification, "cycle");
    if(generator != NULL) {
        yyjson_val *item;
        size_t count;
        if(!yyjson_is_arr(generator)) return false;
        count = yyjson_arr_size(generator);
        if(count == 0) return false;
        item = yyjson_arr_get(generator, (size_t)(instance % count));
        if(!yyjson_is_num(item)) return false;
        *value = yyjson_get_num(item);
        return true;
    }

    generator = yyjson_obj_get(specification, "sequence");
    if(generator != NULL) {
        yyjson_val *wrap_value;
        double start;
        double step;
        if(!yyjson_is_obj(generator)
                || !state_number(generator, "start", &start)
                || !state_number(generator, "step", &step)) {
            return false;
        }
        *value = start + step * (double)instance;
        wrap_value = yyjson_obj_get(generator, "wrap");
        if(wrap_value != NULL) {
            double wrap;
            if(!yyjson_is_num(wrap_value)
                    || (wrap = yyjson_get_num(wrap_value)) <= 0.0) {
                return false;
            }
            *value = fmod(*value, wrap);
            if(*value < 0.0) *value += wrap;
        }
        return true;
    }

    generator = yyjson_obj_get(specification, "linear");
    if(generator != NULL) {
        double from;
        double to;
        double amount = instance_count > 1
            ? (double)instance / ((double)instance_count - 1.0)
            : 0.0;
        if(!yyjson_is_obj(generator)
                || !state_number(generator, "from", &from)
                || !state_number(generator, "to", &to)) {
            return false;
        }
        *value = from + (to - from) * amount;
        return true;
    }
    return false;
}

static bool state_variation_vec2(
        yyjson_val *specification,
        uint64_t instance,
        uint64_t instance_count,
        uint64_t salt,
        Vec2D *value
) {
    yyjson_val *generator;

    if(!yyjson_is_obj(specification) || value == NULL) return false;
    generator = yyjson_obj_get(specification, "random");
    if(generator != NULL) {
        yyjson_val *seed_value = yyjson_obj_get(generator, "seed");
        Vec2D min;
        Vec2D max;
        if(!yyjson_is_obj(generator)
                || !yyjson_is_uint(seed_value)
                || !state_vec2(yyjson_obj_get(generator, "min"), &min)
                || !state_vec2(yyjson_obj_get(generator, "max"), &max)
                || max.x < min.x
                || max.y < min.y) {
            return false;
        }
        value->x = min.x + (max.x - min.x) * (float)state_variation_random_unit(
            yyjson_get_uint(seed_value),
            instance,
            salt
        );
        value->y = min.y + (max.y - min.y) * (float)state_variation_random_unit(
            yyjson_get_uint(seed_value),
            instance,
            salt ^ UINT64_C(0xa5a5a5a5a5a5a5a5)
        );
        return true;
    }

    generator = yyjson_obj_get(specification, "cycle");
    if(generator != NULL) {
        yyjson_val *item;
        size_t count;
        if(!yyjson_is_arr(generator)) return false;
        count = yyjson_arr_size(generator);
        if(count == 0) return false;
        item = yyjson_arr_get(generator, (size_t)(instance % count));
        return state_vec2(item, value);
    }

    generator = yyjson_obj_get(specification, "sequence");
    if(generator != NULL) {
        Vec2D start;
        Vec2D step;
        if(!yyjson_is_obj(generator)
                || !state_vec2(yyjson_obj_get(generator, "start"), &start)
                || !state_vec2(yyjson_obj_get(generator, "step"), &step)) {
            return false;
        }
        value->x = start.x + step.x * (float)instance;
        value->y = start.y + step.y * (float)instance;
        return true;
    }

    generator = yyjson_obj_get(specification, "linear");
    if(generator != NULL) {
        Vec2D from;
        Vec2D to;
        float amount = instance_count > 1
            ? (float)instance / (float)(instance_count - 1)
            : 0.0f;
        if(!yyjson_is_obj(generator)
                || !state_vec2(yyjson_obj_get(generator, "from"), &from)
                || !state_vec2(yyjson_obj_get(generator, "to"), &to)) {
            return false;
        }
        value->x = from.x + (to.x - from.x) * amount;
        value->y = from.y + (to.y - from.y) * amount;
        return true;
    }
    return false;
}

static RohrComponentMask state_flag_mask(const char *flag) {
    if(strcmp(flag, "static") == 0) return ROHR_STATIC;
    if(strcmp(flag, "dynamic") == 0) return ROHR_DYNAMIC;
    if(strcmp(flag, "collision") == 0) return ROHR_COLLISION;
    if(strcmp(flag, "targetable") == 0) return ROHR_TARGETABLE;
    if(strcmp(flag, "particle") == 0) return ROHR_PARTICLE;
    if(strcmp(flag, "hold") == 0) return ROHR_HOLD;
    return ROHR_NONE;
}

static EngineResult state_shape_load(EntityIndex index, yyjson_val *value) {
    yyjson_val *vertex;
    size_t vertex_index;
    size_t vertex_count;
    Shape shape = {0};

    if(!yyjson_is_arr(value)) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    vertex_count = yyjson_arr_size(value);
    if(vertex_count < MIN_VERTICIES || vertex_count > MAX_VERTICIES) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    yyjson_arr_foreach(value, vertex_index, vertex_count, vertex) {
        if(!state_vec2(vertex, &shape.vertices[vertex_index])) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
    }
    shape.amount_of_vertices = (uint16_t)vertex_count;
    if(ShapePool_store_at(&hit_boxes_pool, index, shape).kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[index] |= ROHR_HIT_BOX;
    return error_result_value(true);
}

static EngineResult state_components_load(
        Entity entity,
        yyjson_val *components,
        uint64_t instance,
        uint64_t instance_count
) {
    EntityIndex index;
    yyjson_val *value;
    yyjson_val *flag;
    size_t flag_index;
    size_t flag_count;
    double number;
    Vec2D vector;
    EngineResult result;

    if(!entity_index_get(entity, &index) || !yyjson_is_obj(components)) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }

    value = yyjson_obj_get(components, "mask");
    if(value != NULL) {
        if(!yyjson_is_uint(value) || yyjson_get_uint(value) > UINT32_MAX)
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        entity_mask[index] |= (RohrComponentMask)yyjson_get_uint(value);
    }

    value = yyjson_obj_get(components, "flags");
    if(value != NULL) {
        if(!yyjson_is_arr(value)) return error_result_error(ERROR_ENGINE_STATE_INVALID);
        yyjson_arr_foreach(value, flag_index, flag_count, flag) {
            RohrComponentMask mask;
            if(!yyjson_is_str(flag)) return error_result_error(ERROR_ENGINE_STATE_INVALID);
            mask = state_flag_mask(yyjson_get_str(flag));
            if(mask == ROHR_NONE) return error_result_error(ERROR_ENGINE_STATE_INVALID);
            entity_mask[index] |= mask;
        }
    }

#define LOAD_VEC2(Key, PoolType, PoolVariable, Bit) \
    value = yyjson_obj_get(components, Key); \
    if(value != NULL) { \
        if(!state_vec2(value, &vector) \
                || PoolType##_store_at(&PoolVariable, index, vector).kind == ERROR_RESULT_ERROR) \
            return error_result_error(ERROR_ENGINE_STATE_INVALID); \
        entity_mask[index] |= Bit; \
    }

    LOAD_VEC2("position", PositionPool, positions_pool, ROHR_NONE)
    LOAD_VEC2("velocity", VelocityPool, velocities_pool, ROHR_DYNAMIC)
    LOAD_VEC2("acceleration", AccelerationPool, accelerations_pool, ROHR_DYNAMIC)
#undef LOAD_VEC2

    value = yyjson_obj_get(components, "force");
    if(value != NULL) {
        if(!state_vec2(value, &vector)) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        result = physics_force_component_set(entity, vector);
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }

#define LOAD_SCALAR(Key, Pool, Table, Bit) \
    value = yyjson_obj_get(components, Key); \
    if(value != NULL) { \
        if(!yyjson_is_num(value)) return error_result_error(ERROR_ENGINE_STATE_INVALID); \
        number = yyjson_get_num(value); \
        if(Pool##_store_at(&Table##_pool, index, (float)number).kind == ERROR_RESULT_ERROR) \
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED); \
        entity_mask[index] |= Bit; \
    }

    LOAD_SCALAR("mass", MassPool, mass, ROHR_MASS)
    LOAD_SCALAR("orientation", OrientationPool, orientations, ROHR_NONE)
    LOAD_SCALAR("angular_velocity", AngularVelocityPool, angular_velocities, ROHR_DYNAMIC)
    LOAD_SCALAR("friction", FrictionPool, frictions, ROHR_NONE)
    LOAD_SCALAR("restitution", RestitutionPool, restitutions, ROHR_NONE)
#undef LOAD_SCALAR

    value = yyjson_obj_get(components, "angular_acceleration");
    if(value != NULL) {
        if(!yyjson_is_num(value)) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        result = physics_angular_acceleration_set(
            entity,
            (AngularAcceleration)yyjson_get_num(value)
        );
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }

    value = yyjson_obj_get(components, "torque");
    if(value != NULL) {
        if(!yyjson_is_num(value)) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        result = physics_torque_component_set(
            entity,
            (Torque)yyjson_get_num(value)
        );
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }

    value = yyjson_obj_get(components, "hit_box");
    if(value != NULL) {
        result = state_shape_load(index, value);
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }

    value = yyjson_obj_get(components, "target");
    if(value != NULL) {
        Entity target;
        result = state_resolve_name(value, &target);
        if(result.kind == ERROR_RESULT_ERROR) return result;
        result = physics_target_set(entity, target);
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }

    value = yyjson_obj_get(components, "parent");
    if(value != NULL) {
        Entity parent;
        result = state_resolve_name(value, &parent);
        if(result.kind == ERROR_RESULT_ERROR) return result;
        result = entity_parent_set(entity, parent);
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }

    value = yyjson_obj_get(components, "lifetime");
    if(value != NULL) {
        double expiry_time = 0.0;
        uint64_t expiry_tick = 0;
        yyjson_val *tick;
        if(!yyjson_is_obj(value)) return error_result_error(ERROR_ENGINE_STATE_INVALID);
        if(yyjson_obj_get(value, "time") != NULL && !state_number(value, "time", &expiry_time))
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        tick = yyjson_obj_get(value, "tick");
        if(tick != NULL) {
            if(!yyjson_is_uint(tick)) return error_result_error(ERROR_ENGINE_STATE_INVALID);
            expiry_tick = yyjson_get_uint(tick);
        }
        result = entity_life_time_set(entity, expiry_time, expiry_tick);
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }

    value = yyjson_obj_get(components, "angle_lock");
    if(value != NULL) {
        double min;
        double max;
        if(!yyjson_is_obj(value) || !state_number(value, "min", &min)
                || !state_number(value, "max", &max))
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        if(AngleLockPool_store_at(&angle_locks_pool, index, (AngleLock){(float)min, (float)max}).kind == ERROR_RESULT_ERROR)
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        entity_mask[index] |= ROHR_ANGLE_LOCK;
    }

    value = yyjson_obj_get(components, "axis_lock");
    if(value != NULL) {
        AxisLock lock;
        if(!yyjson_is_obj(value)
                || !state_vec2(yyjson_obj_get(value, "axis"), &lock.axis)
                || !state_vec2(yyjson_obj_get(value, "point"), &lock.point_on_axis))
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        if(AxisLockPool_store_at(&axis_locks_pool, index, lock).kind == ERROR_RESULT_ERROR)
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        entity_mask[index] |= ROHR_AXIS_LOCK;
    }

    value = yyjson_obj_get(components, "transform_lock");
    if(value != NULL) {
        TransformLock lock = {0};
        yyjson_val *driver = yyjson_obj_get(value, "driver");
        if(!yyjson_is_obj(value)
                || state_resolve_name(driver, &lock.driver).kind == ERROR_RESULT_ERROR
                || !state_vec2(yyjson_obj_get(value, "local_offset"), &lock.local_offset)
                || !state_number(value, "local_angle", &number)
                || !state_boolean(value, "lock_position", &lock.lock_position)
                || !state_boolean(value, "lock_orientation", &lock.lock_orientation)
                || !state_boolean(value, "inherit_velocity", &lock.inherit_velocity))
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        lock.local_angle = (float)number;
        if(TransformLockPool_store_at(&transform_locks_pool, index, lock).kind == ERROR_RESULT_ERROR)
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        entity_mask[index] |= ROHR_TRANSFORM_LOCK;
    }

    value = yyjson_obj_get(components, "joint");
    if(value != NULL) {
        Joint joint = {0};
        yyjson_val *type = yyjson_obj_get(value, "type");
        if(!yyjson_is_obj(value) || !yyjson_is_str(type)
                || state_resolve_name(yyjson_obj_get(value, "a"), &joint.a).kind == ERROR_RESULT_ERROR
                || state_resolve_name(yyjson_obj_get(value, "b"), &joint.b).kind == ERROR_RESULT_ERROR
                || !state_vec2(yyjson_obj_get(value, "local_anchor_a"), &joint.local_anchor_a)
                || !state_vec2(yyjson_obj_get(value, "local_anchor_b"), &joint.local_anchor_b)
                || !state_number(value, "rest_length", &number))
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        joint.rest_length = (float)number;
        if(strcmp(yyjson_get_str(type), "spring") == 0) joint.type = JOINT_SPRING;
        else if(strcmp(yyjson_get_str(type), "weld") == 0) joint.type = JOINT_WELD;
        else if(strcmp(yyjson_get_str(type), "pin") == 0) joint.type = JOINT_PIN;
        else return error_result_error(ERROR_ENGINE_STATE_INVALID);
#define JOINT_NUMBER(Key, Field) \
        if(!state_number(value, Key, &number)) return error_result_error(ERROR_ENGINE_STATE_INVALID); \
        joint.Field = (float)number
        JOINT_NUMBER("stiffness", stiffness);
        JOINT_NUMBER("damping", damping);
        JOINT_NUMBER("rest_angle", rest_angle);
        JOINT_NUMBER("angular_stiffness", angular_stiffness);
        JOINT_NUMBER("angular_damping", angular_damping);
#undef JOINT_NUMBER
        if(!state_boolean(value, "lock_angle", &joint.lock_angle))
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        result = physics_joint_component_set(entity, joint);
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }

    value = yyjson_obj_get(components, "animated_sprite");
    if(value != NULL) {
        yyjson_val *animation_name = yyjson_obj_get(value, "animation");
        yyjson_val *variation = yyjson_obj_get(value, "variation");
        yyjson_val *field;
        yyjson_val *base_value;
        StateAnimation *animation;
        Scale scale;
        Vec2D scale_value;
        AnimatedSprite sprite;
        double time_per_frame;
        double generated_value;
        Tick ticks_per_frame;
        int start_frame;
        if(!yyjson_is_obj(value)
                || !yyjson_is_str(animation_name)
                || yyjson_get_len(animation_name) == 0
                || yyjson_get_len(animation_name) >= STATE_ASSET_NAME_MAX
                || !state_vec2(yyjson_obj_get(value, "scale"), &scale_value)
                || (variation != NULL && !yyjson_is_obj(variation))) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        animation = state_find_animation(yyjson_get_str(animation_name));
        if(animation == NULL) {
            return error_result_error(
                ERROR_ENGINE_STATE_ASSET_REFERENCE_NOT_FOUND
            );
        }
        base_value = yyjson_obj_get(value, "time_per_frame");
        if(base_value != NULL) {
            if(!yyjson_is_num(base_value)) {
                return error_result_error(ERROR_ENGINE_STATE_INVALID);
            }
            time_per_frame = yyjson_get_num(base_value);
        } else {
            time_per_frame = animation->asset.time_per_frame;
        }
        base_value = yyjson_obj_get(value, "ticks_per_frame");
        if(base_value != NULL) {
            if(!yyjson_is_uint(base_value)) {
                return error_result_error(ERROR_ENGINE_STATE_INVALID);
            }
            ticks_per_frame = yyjson_get_uint(base_value);
        } else {
            ticks_per_frame = animation->asset.ticks_per_frame;
        }
        base_value = yyjson_obj_get(value, "start_frame");
        if(base_value != NULL) {
            if(!yyjson_is_uint(base_value)
                    || yyjson_get_uint(base_value) > INT_MAX) {
                return error_result_error(ERROR_ENGINE_STATE_INVALID);
            }
            start_frame = (int)yyjson_get_uint(base_value);
        } else {
            start_frame = 0;
        }
        if(variation != NULL) {
            field = yyjson_obj_get(variation, "scale");
            if(field != NULL && !state_variation_vec2(
                    field,
                    instance,
                    instance_count,
                    UINT64_C(0x7363616c65),
                    &scale_value
                )) {
                return error_result_error(ERROR_ENGINE_STATE_INVALID);
            }
            field = yyjson_obj_get(variation, "time_per_frame");
            if(field != NULL) {
                if(!state_variation_scalar(
                        field,
                        instance,
                        instance_count,
                        UINT64_C(0x74696d655f726174),
                        &time_per_frame
                    )) {
                    return error_result_error(ERROR_ENGINE_STATE_INVALID);
                }
            }
            field = yyjson_obj_get(variation, "ticks_per_frame");
            if(field != NULL) {
                if(!state_variation_scalar(
                        field,
                        instance,
                        instance_count,
                        UINT64_C(0x7469636b5f726174),
                        &generated_value
                    )
                        || !isfinite(generated_value)
                        || generated_value < 0.0
                        || generated_value > (double)UINT64_MAX
                        || floor(generated_value) != generated_value) {
                    return error_result_error(ERROR_ENGINE_STATE_INVALID);
                }
                ticks_per_frame = (Tick)generated_value;
            }
            field = yyjson_obj_get(variation, "start_frame");
            if(field != NULL) {
                if(!state_variation_scalar(
                        field,
                        instance,
                        instance_count,
                        UINT64_C(0x73746172745f6672),
                        &generated_value
                    )
                        || !isfinite(generated_value)
                        || generated_value < 0.0
                        || generated_value > INT_MAX
                        || floor(generated_value) != generated_value) {
                    return error_result_error(ERROR_ENGINE_STATE_INVALID);
                }
                start_frame = (int)generated_value;
            }
        }
        if(!isfinite(scale_value.x)
                || !isfinite(scale_value.y)
                || scale_value.x <= 0.0f
                || scale_value.y <= 0.0f
                || !isfinite(time_per_frame)
                || time_per_frame < 0.0
                || start_frame < 0
                || start_frame >= animation->asset.texture_list.amount) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        scale = (Scale){scale_value.x, scale_value.y};
        sprite = graphics_animated_sprite_create(animation->asset, scale);
        sprite.animation.time_per_frame = time_per_frame;
        sprite.animation.ticks_per_frame = ticks_per_frame;
        sprite.animation_frame = start_frame;
        result = graphics_animated_sprite_add(entity, sprite);
        if(result.kind == ERROR_RESULT_ERROR) return result;
        state_sprite_references[index] = (StateSpriteReference){
            .used = true,
            .scale = scale,
            .time_per_frame = time_per_frame,
            .ticks_per_frame = ticks_per_frame,
            .start_frame = start_frame
        };
        memcpy(
            state_sprite_references[index].animation,
            yyjson_get_str(animation_name),
            yyjson_get_len(animation_name) + 1
        );
    }

    value = yyjson_obj_get(components, "groups");
    if(value != NULL) {
        yyjson_val *group_name;
        size_t group_index;
        size_t group_count;
        if(!yyjson_is_arr(value)) {
            return error_result_error(ERROR_ENGINE_STATE_INVALID);
        }
        yyjson_arr_foreach(value, group_index, group_count, group_name) {
            GroupIdResult group_result;
            if(!yyjson_is_str(group_name)) {
                return error_result_error(ERROR_ENGINE_STATE_INVALID);
            }
            group_result = entity_group_by_name_get(yyjson_get_str(group_name));
            if(group_result.kind == ERROR_RESULT_ERROR) {
                return error_result_error(ERROR_ENGINE_STATE_REFERENCE_NOT_FOUND);
            }
            result = entity_group_add(group_result.result.value, entity);
            if(result.kind == ERROR_RESULT_ERROR) return result;
        }
    }
    return error_result_value(true);
}

static void state_rollback(Entity *created, size_t created_count) {
    while(created_count > 0) {
        created_count -= 1;
        (void)entity_delete(created[created_count]);
    }
}

EngineResult game_state_files_load(const char *const *paths, size_t path_count) {
    StateDocument *documents;
    Entity *created;
    StateLoadedEntity *loaded;
    GroupId *created_groups;
    size_t document_index;
    size_t created_count = 0;
    size_t total_created = 0;
    size_t created_group_count = 0;
    size_t camera_definition_count = 0;
    size_t initial_animation_count = state_animation_count;
    size_t initial_ui_button_count = state_ui_button_count;
    size_t initial_ui_font_count = state_ui_font_count;
    size_t initial_ui_label_count = state_ui_label_count;
    size_t initial_ui_slider_count = state_ui_slider_count;
    EngineResult result = error_result_value(true);

    if(paths == NULL || path_count == 0 || path_count > MAX_ENTITIES) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    if(path_count
            > GAME_STATE_MAX_TEMPLATE_DOCUMENTS - state_template_document_count) {
        return error_result_error(
            ERROR_ENGINE_STATE_TEMPLATE_DOCUMENT_LIMIT_EXCEEDED
        );
    }
    documents = calloc(path_count, sizeof(*documents));
    created = calloc(MAX_ENTITIES, sizeof(*created));
    loaded = calloc(MAX_ENTITIES, sizeof(*loaded));
    created_groups = calloc(MAX_GROUPS, sizeof(*created_groups));
    if(documents == NULL || created == NULL || loaded == NULL || created_groups == NULL) {
        free(documents);
        free(created);
        free(loaded);
        free(created_groups);
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }

    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_val *root;
        yyjson_val *version;
        yyjson_val *assets;
        yyjson_val *ui;
        yyjson_read_err read_error;
        if(paths[document_index] == NULL) {
            result = error_result_error(ERROR_ENGINE_STATE_INVALID);
            goto cleanup;
        }
        documents[document_index].document = yyjson_read_file(
            paths[document_index],
            YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS,
            NULL,
            &read_error
        );
        if(documents[document_index].document == NULL) {
            result = error_result_error(ERROR_ENGINE_STATE_IO_FAILED);
            goto cleanup;
        }
        root = yyjson_doc_get_root(documents[document_index].document);
        version = yyjson_obj_get(root, "version");
        documents[document_index].entities = yyjson_obj_get(root, "entities");
        documents[document_index].groups = yyjson_obj_get(root, "groups");
        documents[document_index].camera = yyjson_obj_get(root, "camera");
        ui = yyjson_obj_get(root, "ui");
        documents[document_index].ui_buttons = ui == NULL
            ? NULL
            : yyjson_obj_get(ui, "buttons");
        documents[document_index].ui_fonts = ui == NULL
            ? NULL
            : yyjson_obj_get(ui, "fonts");
        documents[document_index].ui_labels = ui == NULL
            ? NULL
            : yyjson_obj_get(ui, "labels");
        documents[document_index].ui_sliders = ui == NULL
            ? NULL
            : yyjson_obj_get(ui, "sliders");
        assets = yyjson_obj_get(root, "assets");
        documents[document_index].animations = assets == NULL
            ? NULL
            : yyjson_obj_get(assets, "animations");
        if(!yyjson_is_obj(root) || !yyjson_is_uint(version)
                || yyjson_get_uint(version) != GAME_STATE_VERSION
                || !yyjson_is_arr(documents[document_index].entities)
                || (documents[document_index].groups != NULL
                    && !yyjson_is_arr(documents[document_index].groups))
                || (documents[document_index].camera != NULL
                    && !yyjson_is_obj(documents[document_index].camera))
                || (assets != NULL && !yyjson_is_obj(assets))
                || (ui != NULL && !yyjson_is_obj(ui))
                || (documents[document_index].ui_buttons != NULL
                    && !yyjson_is_arr(documents[document_index].ui_buttons))
                || (documents[document_index].ui_fonts != NULL
                    && !yyjson_is_arr(documents[document_index].ui_fonts))
                || (documents[document_index].ui_labels != NULL
                    && !yyjson_is_arr(documents[document_index].ui_labels))
                || (documents[document_index].ui_sliders != NULL
                    && !yyjson_is_arr(documents[document_index].ui_sliders))
                || (documents[document_index].animations != NULL
                    && !yyjson_is_arr(documents[document_index].animations))) {
            result = error_result_error(ERROR_ENGINE_STATE_INVALID);
            goto cleanup;
        }
        if(documents[document_index].camera != NULL) {
            camera_definition_count += 1;
            if(camera_definition_count > 1
                    || state_template_camera_retained) {
                result = error_result_error(ERROR_ENGINE_STATE_INVALID);
                goto cleanup;
            }
        }
    }

    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_val *definition;
        size_t font_index;
        size_t font_count;
        if(documents[document_index].ui_fonts == NULL) continue;
        yyjson_arr_foreach(
            documents[document_index].ui_fonts,
            font_index,
            font_count,
            definition
        ) {
            result = state_ui_font_definition_load(definition);
            if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
        }
    }

    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_val *definition;
        size_t slider_index;
        size_t slider_count;
        if(documents[document_index].ui_sliders == NULL) continue;
        yyjson_arr_foreach(
            documents[document_index].ui_sliders,
            slider_index,
            slider_count,
            definition
        ) {
            result = state_ui_slider_definition_load(definition);
            if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
        }
    }

    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_val *definition;
        size_t label_index;
        size_t label_count;
        if(documents[document_index].ui_labels == NULL) continue;
        yyjson_arr_foreach(
            documents[document_index].ui_labels,
            label_index,
            label_count,
            definition
        ) {
            result = state_ui_label_definition_load(definition);
            if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
        }
    }

    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_val *definition;
        size_t button_index;
        size_t button_count;
        if(documents[document_index].ui_buttons == NULL) continue;
        yyjson_arr_foreach(
            documents[document_index].ui_buttons,
            button_index,
            button_count,
            definition
        ) {
            result = state_ui_button_definition_load(definition);
            if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
        }
    }

    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_val *definition;
        size_t group_index;
        size_t group_count;
        if(documents[document_index].groups == NULL) continue;
        yyjson_arr_foreach(
            documents[document_index].groups,
            group_index,
            group_count,
            definition
        ) {
            yyjson_val *name = yyjson_obj_get(definition, "name");
            GroupIdResult group_result;
            if(!yyjson_is_obj(definition)
                    || !yyjson_is_str(name)
                    || yyjson_get_len(name) == 0
                    || yyjson_get_len(name) >= GROUP_NAME_MAX) {
                result = error_result_error(ERROR_ENGINE_STATE_INVALID);
                goto cleanup;
            }
            group_result = entity_group_create();
            if(group_result.kind == ERROR_RESULT_ERROR) {
                result = error_result_error(group_result.result.error);
                goto cleanup;
            }
            created_groups[created_group_count++] = group_result.result.value;
            result = entity_group_name_set(
                group_result.result.value,
                yyjson_get_str(name)
            );
            if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
        }
    }

    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_val *definition;
        size_t animation_index;
        size_t animation_count;
        if(documents[document_index].animations == NULL) continue;
        yyjson_arr_foreach(
            documents[document_index].animations,
            animation_index,
            animation_count,
            definition
        ) {
            result = state_animation_definition_load(definition);
            if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
        }
    }

    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_val *description;
        size_t entity_index;
        size_t entity_count;
        yyjson_arr_foreach(documents[document_index].entities, entity_index, entity_count, description) {
            yyjson_val *name = yyjson_obj_get(description, "name");
            uint64_t instance;
            uint64_t instance_count;
            if(!yyjson_is_obj(description) || !yyjson_is_str(name)
                    || yyjson_get_len(name) == 0
                    || yyjson_get_len(name) >= ENTITY_NAME_MAX
                    || !state_entity_count(description, &instance_count)
                    || instance_count > MAX_ENTITIES - total_created) {
                result = error_result_error(ERROR_ENGINE_STATE_INVALID);
                goto cleanup;
            }
            for(instance = 0; instance < instance_count; instance += 1) {
                char generated_name[ENTITY_NAME_MAX];
                EntityResult entity_result;
                result = state_make_entity_name(
                    yyjson_get_str(name),
                    instance,
                    generated_name
                );
                if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
                entity_result = entity_add();
                if(entity_result.kind == ERROR_RESULT_ERROR) {
                    result = error_result_error(entity_result.result.error);
                    goto cleanup;
                }
                created[created_count] = entity_result.result.value;
                loaded[created_count] = (StateLoadedEntity){
                    .entity = entity_result.result.value,
                    .description = description,
                    .instance = instance,
                    .instance_count = instance_count
                };
                created_count += 1;
                total_created = created_count;
                result = entity_name_set(
                    entity_result.result.value,
                    generated_name
                );
                if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
            }
        }
    }

    for(created_count = 0; created_count < total_created; created_count += 1) {
        yyjson_val *components = yyjson_obj_get(
            loaded[created_count].description,
            "components"
        );
        if(!yyjson_is_obj(components)) {
            result = error_result_error(ERROR_ENGINE_STATE_INVALID);
            goto cleanup;
        }
        result = state_components_load(
            loaded[created_count].entity,
            components,
            loaded[created_count].instance,
            loaded[created_count].instance_count
        );
        if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
        result = state_placement_apply(&loaded[created_count]);
        if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
    }

    for(document_index = 0; document_index < path_count; document_index += 1) {
        if(documents[document_index].camera == NULL) continue;
        result = state_camera_load(documents[document_index].camera);
        if(result.kind == ERROR_RESULT_ERROR) goto cleanup;
    }

cleanup:
    if(result.kind == ERROR_RESULT_ERROR) state_rollback(created, total_created);
    if(result.kind == ERROR_RESULT_ERROR) {
        while(created_group_count > 0) {
            created_group_count -= 1;
            (void)entity_group_destroy(created_groups[created_group_count]);
        }
    }
    if(result.kind == ERROR_RESULT_ERROR) {
        while(state_ui_slider_count > initial_ui_slider_count) {
            state_ui_slider_count -= 1;
            state_ui_sliders[state_ui_slider_count] = (StateUISlider){0};
        }
        while(state_ui_label_count > initial_ui_label_count) {
            state_ui_label_count -= 1;
            state_ui_labels[state_ui_label_count] = (StateUILabel){0};
        }
        while(state_ui_font_count > initial_ui_font_count) {
            state_ui_font_count -= 1;
            state_ui_fonts[state_ui_font_count] = (StateUIFont){0};
        }
        while(state_ui_button_count > initial_ui_button_count) {
            state_ui_button_count -= 1;
            state_ui_buttons[state_ui_button_count] = (StateUIButton){0};
        }
    }
    if(result.kind == ERROR_RESULT_ERROR) {
        while(state_animation_count > initial_animation_count) {
            state_animation_count -= 1;
            state_animations[state_animation_count] = (StateAnimation){0};
        }
    }
    if(result.kind == ERROR_RESULT_VALUE) {
        if(camera_definition_count > 0) {
            state_template_camera_retained = true;
        }
        for(document_index = 0; document_index < path_count; document_index += 1) {
            state_template_documents[state_template_document_count] =
                documents[document_index].document;
            state_template_document_count += 1;
            documents[document_index].document = NULL;
        }
    }
    for(document_index = 0; document_index < path_count; document_index += 1) {
        yyjson_doc_free(documents[document_index].document);
    }
    free(created);
    free(loaded);
    free(created_groups);
    free(documents);
    return result;
}

EngineResult game_state_file_load(const char *path) {
    return game_state_files_load(&path, 1);
}

static yyjson_mut_val *state_vec2_write(yyjson_mut_doc *document, Vec2D value) {
    yyjson_mut_val *object = yyjson_mut_obj(document);
    yyjson_mut_obj_add_real(document, object, "x", value.x);
    yyjson_mut_obj_add_real(document, object, "y", value.y);
    return object;
}

static yyjson_mut_val *state_color_write(
        yyjson_mut_doc *document,
        Color color
) {
    yyjson_mut_val *object = yyjson_mut_obj(document);
    yyjson_mut_obj_add_uint(document, object, "red", color.red);
    yyjson_mut_obj_add_uint(document, object, "green", color.green);
    yyjson_mut_obj_add_uint(document, object, "blue", color.blue);
    yyjson_mut_obj_add_uint(document, object, "alpha", color.alpha);
    return object;
}

static yyjson_mut_val *state_ui_button_write(
        yyjson_mut_doc *document,
        const UIButtonDefinition *button
) {
    yyjson_mut_val *definition;
    yyjson_mut_val *bounds;
    yyjson_mut_val *style;

    if(document == NULL || button == NULL) return NULL;
    definition = yyjson_mut_obj(document);
    bounds = yyjson_mut_obj(document);
    style = yyjson_mut_obj(document);
    if(definition == NULL || bounds == NULL || style == NULL) return NULL;
    yyjson_mut_obj_add_strcpy(document, definition, "name", button->name);
    yyjson_mut_obj_add_strcpy(document, definition, "label", button->label);
    if(button->font[0] != '\0') {
        yyjson_mut_obj_add_strcpy(document, definition, "font", button->font);
        yyjson_mut_obj_add_val(document, definition, "text_color", state_color_write(document, button->text_color));
    }
    yyjson_mut_obj_add_real(document, bounds, "x", button->bounds.x);
    yyjson_mut_obj_add_real(document, bounds, "y", button->bounds.y);
    yyjson_mut_obj_add_real(document, bounds, "width", button->bounds.width);
    yyjson_mut_obj_add_real(document, bounds, "height", button->bounds.height);
    yyjson_mut_obj_add_val(document, definition, "bounds", bounds);
    yyjson_mut_obj_add_val(document, style, "idle", state_color_write(document, button->style.idle));
    yyjson_mut_obj_add_val(document, style, "hovered", state_color_write(document, button->style.hovered));
    yyjson_mut_obj_add_val(document, style, "pressed", state_color_write(document, button->style.pressed));
    yyjson_mut_obj_add_val(document, style, "disabled", state_color_write(document, button->style.disabled));
    yyjson_mut_obj_add_val(document, definition, "style", style);
    return definition;
}

static yyjson_mut_val *state_ui_font_write(
        yyjson_mut_doc *document,
        const UIFontDefinition *font
) {
    yyjson_mut_val *definition = yyjson_mut_obj(document);
    if(definition == NULL || font == NULL) return NULL;
    yyjson_mut_obj_add_strcpy(document, definition, "name", font->name);
    yyjson_mut_obj_add_strcpy(document, definition, "file", font->file);
    yyjson_mut_obj_add_real(document, definition, "point_size", font->point_size);
    return definition;
}

static yyjson_mut_val *state_ui_label_write(
        yyjson_mut_doc *document,
        const UILabelDefinition *label
) {
    yyjson_mut_val *definition;
    yyjson_mut_val *bounds;
    if(document == NULL || label == NULL) return NULL;
    definition = yyjson_mut_obj(document);
    bounds = yyjson_mut_obj(document);
    if(definition == NULL || bounds == NULL) return NULL;
    yyjson_mut_obj_add_strcpy(document, definition, "name", label->name);
    yyjson_mut_obj_add_strcpy(document, definition, "text", label->text);
    yyjson_mut_obj_add_strcpy(document, definition, "font", label->font);
    yyjson_mut_obj_add_val(document, definition, "color", state_color_write(document, label->color));
    yyjson_mut_obj_add_real(document, bounds, "x", label->bounds.x);
    yyjson_mut_obj_add_real(document, bounds, "y", label->bounds.y);
    yyjson_mut_obj_add_real(document, bounds, "width", label->bounds.width);
    yyjson_mut_obj_add_real(document, bounds, "height", label->bounds.height);
    yyjson_mut_obj_add_val(document, definition, "bounds", bounds);
    return definition;
}

static yyjson_mut_val *state_ui_slider_write(
        yyjson_mut_doc *document,
        const UISliderDefinition *slider
) {
    yyjson_mut_val *definition;
    yyjson_mut_val *center;
    yyjson_mut_val *range;
    yyjson_mut_val *style;
    if(document == NULL || slider == NULL) return NULL;
    definition = yyjson_mut_obj(document);
    center = yyjson_mut_obj(document);
    range = yyjson_mut_obj(document);
    style = yyjson_mut_obj(document);
    if(definition == NULL || center == NULL || range == NULL || style == NULL) return NULL;
    yyjson_mut_obj_add_strcpy(document, definition, "name", slider->name);
    yyjson_mut_obj_add_strcpy(document, definition, "label", slider->label);
    yyjson_mut_obj_add_strcpy(document, definition, "font", slider->font);
    yyjson_mut_obj_add_strcpy(document, definition, "value_format", slider->value_format);
    yyjson_mut_obj_add_val(document, definition, "text_color",
        state_color_write(document, slider->text_color));
    yyjson_mut_obj_add_real(document, center, "x", slider->config.center.x);
    yyjson_mut_obj_add_real(document, center, "y", slider->config.center.y);
    yyjson_mut_obj_add_val(document, definition, "center", center);
    yyjson_mut_obj_add_real(document, definition, "length", slider->config.length);
    yyjson_mut_obj_add_real(document, definition, "angle", slider->config.angle);
    yyjson_mut_obj_add_real(document, range, "min", slider->config.min_value);
    yyjson_mut_obj_add_real(document, range, "max", slider->config.max_value);
    yyjson_mut_obj_add_val(document, definition, "range", range);
    yyjson_mut_obj_add_real(document, definition, "step", slider->config.step);
    yyjson_mut_obj_add_real(document, definition, "initial_value", slider->initial_value);
    yyjson_mut_obj_add_val(document, style, "track", state_color_write(document, slider->config.style.track));
    yyjson_mut_obj_add_val(document, style, "fill", state_color_write(document, slider->config.style.fill));
    yyjson_mut_obj_add_val(document, style, "handle", state_color_write(document, slider->config.style.handle));
    yyjson_mut_obj_add_val(document, style, "handle_hovered", state_color_write(document, slider->config.style.handle_hovered));
    yyjson_mut_obj_add_val(document, style, "handle_pressed", state_color_write(document, slider->config.style.handle_pressed));
    yyjson_mut_obj_add_real(document, style, "track_thickness", slider->config.style.track_thickness);
    yyjson_mut_obj_add_real(document, style, "handle_width", slider->config.style.handle_width);
    yyjson_mut_obj_add_real(document, style, "handle_height", slider->config.style.handle_height);
    yyjson_mut_obj_add_real(document, style, "step_button_size", slider->config.style.step_button_size);
    yyjson_mut_obj_add_real(document, style, "step_button_gap", slider->config.style.step_button_gap);
    yyjson_mut_obj_add_val(document, definition, "style", style);
    return definition;
}

static void state_named_reference_write(
        yyjson_mut_doc *document,
        yyjson_mut_val *components,
        const char *key,
        Entity entity
) {
    EntityNameResult name = entity_name_get(entity);
    if(name.kind == ERROR_RESULT_VALUE)
        yyjson_mut_obj_add_strcpy(document, components, key, name.result.value.value);
}

EngineResult game_state_file_save(const char *path) {
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *entity_array;
    yyjson_mut_val *group_array;
    yyjson_mut_val *assets;
    yyjson_mut_val *animation_array;
    yyjson_mut_val *ui;
    yyjson_mut_val *ui_buttons;
    yyjson_mut_val *ui_fonts;
    yyjson_mut_val *ui_labels;
    yyjson_mut_val *ui_sliders;
    uint32_t position;
    yyjson_write_err write_error;
    bool success;

    if(path == NULL) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    document = yyjson_mut_doc_new(NULL);
    if(document == NULL) return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    root = yyjson_mut_obj(document);
    entity_array = yyjson_mut_arr(document);
    group_array = yyjson_mut_arr(document);
    assets = yyjson_mut_obj(document);
    animation_array = yyjson_mut_arr(document);
    ui = yyjson_mut_obj(document);
    ui_buttons = yyjson_mut_arr(document);
    ui_fonts = yyjson_mut_arr(document);
    ui_labels = yyjson_mut_arr(document);
    ui_sliders = yyjson_mut_arr(document);
    yyjson_mut_doc_set_root(document, root);
    yyjson_mut_obj_add_uint(document, root, "version", GAME_STATE_VERSION);
    yyjson_mut_obj_add_val(document, root, "groups", group_array);
    yyjson_mut_obj_add_val(document, assets, "animations", animation_array);
    yyjson_mut_obj_add_val(document, root, "assets", assets);
    yyjson_mut_obj_add_val(document, root, "entities", entity_array);
    yyjson_mut_obj_add_val(document, ui, "buttons", ui_buttons);
    yyjson_mut_obj_add_val(document, ui, "fonts", ui_fonts);
    yyjson_mut_obj_add_val(document, ui, "labels", ui_labels);
    yyjson_mut_obj_add_val(document, ui, "sliders", ui_sliders);
    yyjson_mut_obj_add_val(document, root, "ui", ui);

    {
        yyjson_mut_val *camera = yyjson_mut_obj(document);
        CameraAttachment attachment;
        if(graphics_camera_attachment_get(&attachment)) {
            EntityNameResult entity_name = entity_name_get(attachment.entity);
            if(entity_name.kind == ERROR_RESULT_VALUE) {
                yyjson_mut_val *attachment_value = yyjson_mut_obj(document);
                yyjson_mut_obj_add_strcpy(
                    document,
                    attachment_value,
                    "entity",
                    entity_name.result.value.value
                );
                yyjson_mut_obj_add_val(
                    document,
                    attachment_value,
                    "position_offset",
                    state_vec2_write(document, attachment.position_offset)
                );
                yyjson_mut_obj_add_real(
                    document,
                    attachment_value,
                    "orientation_offset",
                    attachment.orientation_offset
                );
                yyjson_mut_obj_add_bool(
                    document,
                    attachment_value,
                    "follow_position",
                    attachment.follow_position
                );
                yyjson_mut_obj_add_bool(
                    document,
                    attachment_value,
                    "follow_orientation",
                    attachment.follow_orientation
                );
                yyjson_mut_obj_add_val(
                    document,
                    camera,
                    "attachment",
                    attachment_value
                );
            }
        }
        if(yyjson_mut_obj_size(camera) == 0) {
            CameraResult camera_result = graphics_camera_get(
                graphics_camera_active_get()
            );
            Camera camera_value;
            if(camera_result.kind == ERROR_RESULT_ERROR) {
                yyjson_mut_doc_free(document);
                return error_result_error(camera_result.result.error);
            }
            camera_value = camera_result.result.value;
            yyjson_mut_val *transform = yyjson_mut_obj(document);
            yyjson_mut_obj_add_val(
                document,
                transform,
                "position",
                state_vec2_write(document, camera_value.position)
            );
            yyjson_mut_obj_add_real(
                document,
                transform,
                "orientation",
                camera_value.orientation
            );
            yyjson_mut_obj_add_val(document, camera, "transform", transform);
        }
        yyjson_mut_obj_add_val(document, root, "camera", camera);
    }

    for(position = 1; position <= MAX_GROUPS; position += 1) {
        GroupNameResult name = entity_group_name_get((GroupId)position);
        if(name.kind == ERROR_RESULT_VALUE) {
            yyjson_mut_val *definition = yyjson_mut_obj(document);
            yyjson_mut_obj_add_strcpy(
                document,
                definition,
                "name",
                name.result.value.value
            );
            yyjson_mut_arr_add_val(group_array, definition);
        }
    }

    for(position = 0; position < state_animation_count; position += 1) {
        StateAnimation *animation = &state_animations[position];
        yyjson_mut_val *definition = yyjson_mut_obj(document);
        yyjson_mut_val *frames = yyjson_mut_arr(document);
        uint8_t frame_index;

        yyjson_mut_obj_add_strcpy(document, definition, "name", animation->name);
        yyjson_mut_obj_add_uint(
            document,
            definition,
            "ticks_per_frame",
            animation->descriptor.ticks_per_frame
        );
        yyjson_mut_obj_add_real(
            document,
            definition,
            "time_per_frame",
            animation->descriptor.time_per_frame
        );
        for(frame_index = 0;
                frame_index < animation->descriptor.amount_of_descriptors;
                frame_index += 1) {
            TextureDescriptor *texture =
                &animation->descriptor.texture_descriptors[frame_index];
            yyjson_mut_val *frame = yyjson_mut_obj(document);
            yyjson_mut_obj_add_strcpy(document, frame, "file", texture->file);
            yyjson_mut_obj_add_val(
                document,
                frame,
                "size",
                state_vec2_write(
                    document,
                    (Vec2D){texture->size.x, texture->size.y}
                )
            );
            yyjson_mut_arr_add_val(frames, frame);
        }
        yyjson_mut_obj_add_val(document, definition, "frames", frames);
        yyjson_mut_arr_add_val(animation_array, definition);
    }

    for(position = 0; position < state_ui_button_count; position += 1) {
        yyjson_mut_val *definition = state_ui_button_write(
            document,
            &state_ui_buttons[position].definition
        );
        if(definition == NULL) {
            yyjson_mut_doc_free(document);
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        }
        yyjson_mut_arr_add_val(ui_buttons, definition);
    }
    for(position = 0; position < state_ui_font_count; position += 1) {
        yyjson_mut_arr_add_val(
            ui_fonts,
            state_ui_font_write(document, &state_ui_fonts[position].definition)
        );
    }
    for(position = 0; position < state_ui_label_count; position += 1) {
        yyjson_mut_arr_add_val(
            ui_labels,
            state_ui_label_write(document, &state_ui_labels[position].definition)
        );
    }
    for(position = 0; position < state_ui_slider_count; position += 1) {
        yyjson_mut_arr_add_val(
            ui_sliders,
            state_ui_slider_write(document, &state_ui_sliders[position].definition)
        );
    }

    for(position = 0; position < entity_alive_count_get(); position += 1) {
        EntityResult entity_result = entity_alive_at_get(position);
        EntityIndex index;
        EntityNameResult name;
        yyjson_mut_val *description;
        yyjson_mut_val *components;
        yyjson_mut_val *flags;
        RohrComponentMask mask;
        size_t i;

        if(entity_result.kind == ERROR_RESULT_ERROR
                || !entity_index_get(entity_result.result.value, &index)) continue;
        name = entity_name_get(entity_result.result.value);
        if(name.kind == ERROR_RESULT_ERROR) continue;
        mask = entity_mask[index];
        description = yyjson_mut_obj(document);
        components = yyjson_mut_obj(document);
        flags = yyjson_mut_arr(document);
        yyjson_mut_obj_add_strcpy(document, description, "name", name.result.value.value);
        yyjson_mut_obj_add_val(document, description, "components", components);
        yyjson_mut_arr_add_val(entity_array, description);
        yyjson_mut_obj_add_uint(document, components, "mask", mask & ~ROHR_ENTITY_NAME);
        if(mask & ROHR_STATIC) yyjson_mut_arr_add_str(document, flags, "static");
        if(mask & ROHR_DYNAMIC) yyjson_mut_arr_add_str(document, flags, "dynamic");
        if(mask & ROHR_COLLISION) yyjson_mut_arr_add_str(document, flags, "collision");
        if(mask & ROHR_TARGETABLE) yyjson_mut_arr_add_str(document, flags, "targetable");
        if(mask & ROHR_PARTICLE) yyjson_mut_arr_add_str(document, flags, "particle");
        if(mask & ROHR_HOLD) yyjson_mut_arr_add_str(document, flags, "hold");
        if(yyjson_mut_arr_size(flags) > 0) yyjson_mut_obj_add_val(document, components, "flags", flags);

        if(positions_pool.used[index]) yyjson_mut_obj_add_val(document, components, "position", state_vec2_write(document, positions[index]));
        if(velocities_pool.used[index]) yyjson_mut_obj_add_val(document, components, "velocity", state_vec2_write(document, velocities[index]));
        if(accelerations_pool.used[index]) yyjson_mut_obj_add_val(document, components, "acceleration", state_vec2_write(document, accelerations[index]));
        if(forces_pool.used[index]) yyjson_mut_obj_add_val(document, components, "force", state_vec2_write(document, forces[index]));
        if(mass_pool.used[index]) yyjson_mut_obj_add_real(document, components, "mass", mass[index]);
        if(orientations_pool.used[index]) yyjson_mut_obj_add_real(document, components, "orientation", orientations[index]);
        if(angular_velocities_pool.used[index]) yyjson_mut_obj_add_real(document, components, "angular_velocity", angular_velocities[index]);
        if(angular_accelerations_pool.used[index]) yyjson_mut_obj_add_real(document, components, "angular_acceleration", angular_accelerations[index]);
        if(torques_pool.used[index]) yyjson_mut_obj_add_real(document, components, "torque", torques[index]);
        if(frictions_pool.used[index]) yyjson_mut_obj_add_real(document, components, "friction", frictions[index]);
        if(restitutions_pool.used[index]) yyjson_mut_obj_add_real(document, components, "restitution", restitutions[index]);
        if(hit_boxes_pool.used[index]) {
            yyjson_mut_val *vertices = yyjson_mut_arr(document);
            for(i = 0; i < hit_boxes[index].amount_of_vertices; i += 1)
                yyjson_mut_arr_add_val(vertices, state_vec2_write(document, hit_boxes[index].vertices[i]));
            yyjson_mut_obj_add_val(document, components, "hit_box", vertices);
        }
        if(targets_pool.used[index]) state_named_reference_write(document, components, "target", targets[index]);
        if(parents_pool.used[index]) state_named_reference_write(document, components, "parent", parents[index]);
        if(life_times_pool.used[index]) {
            yyjson_mut_val *lifetime = yyjson_mut_obj(document);
            yyjson_mut_obj_add_real(document, lifetime, "time", life_times[index].expirey_time);
            yyjson_mut_obj_add_uint(document, lifetime, "tick", life_times[index].expirey_tick);
            yyjson_mut_obj_add_val(document, components, "lifetime", lifetime);
        }
        if(angle_locks_pool.used[index]) {
            yyjson_mut_val *lock = yyjson_mut_obj(document);
            yyjson_mut_obj_add_real(document, lock, "min", angle_locks[index].min);
            yyjson_mut_obj_add_real(document, lock, "max", angle_locks[index].max);
            yyjson_mut_obj_add_val(document, components, "angle_lock", lock);
        }
        if(axis_locks_pool.used[index]) {
            yyjson_mut_val *lock = yyjson_mut_obj(document);
            yyjson_mut_obj_add_val(document, lock, "axis", state_vec2_write(document, axis_locks[index].axis));
            yyjson_mut_obj_add_val(document, lock, "point", state_vec2_write(document, axis_locks[index].point_on_axis));
            yyjson_mut_obj_add_val(document, components, "axis_lock", lock);
        }
        if(transform_locks_pool.used[index]) {
            yyjson_mut_val *lock = yyjson_mut_obj(document);
            EntityNameResult driver = entity_name_get(transform_locks[index].driver);
            if(driver.kind == ERROR_RESULT_VALUE) {
                yyjson_mut_obj_add_strcpy(document, lock, "driver", driver.result.value.value);
                yyjson_mut_obj_add_val(document, lock, "local_offset", state_vec2_write(document, transform_locks[index].local_offset));
                yyjson_mut_obj_add_real(document, lock, "local_angle", transform_locks[index].local_angle);
                yyjson_mut_obj_add_bool(document, lock, "lock_position", transform_locks[index].lock_position);
                yyjson_mut_obj_add_bool(document, lock, "lock_orientation", transform_locks[index].lock_orientation);
                yyjson_mut_obj_add_bool(document, lock, "inherit_velocity", transform_locks[index].inherit_velocity);
                yyjson_mut_obj_add_val(document, components, "transform_lock", lock);
            }
        }
        if(joints_pool.used[index]) {
            EntityNameResult a = entity_name_get(joints[index].a);
            EntityNameResult b = entity_name_get(joints[index].b);
            if(a.kind == ERROR_RESULT_VALUE && b.kind == ERROR_RESULT_VALUE) {
                const char *type = joints[index].type == JOINT_SPRING ? "spring"
                    : joints[index].type == JOINT_WELD ? "weld" : "pin";
                yyjson_mut_val *joint = yyjson_mut_obj(document);
                yyjson_mut_obj_add_str(document, joint, "type", type);
                yyjson_mut_obj_add_strcpy(document, joint, "a", a.result.value.value);
                yyjson_mut_obj_add_strcpy(document, joint, "b", b.result.value.value);
                yyjson_mut_obj_add_val(document, joint, "local_anchor_a", state_vec2_write(document, joints[index].local_anchor_a));
                yyjson_mut_obj_add_val(document, joint, "local_anchor_b", state_vec2_write(document, joints[index].local_anchor_b));
                yyjson_mut_obj_add_real(document, joint, "rest_length", joints[index].rest_length);
                yyjson_mut_obj_add_real(document, joint, "stiffness", joints[index].stiffness);
                yyjson_mut_obj_add_real(document, joint, "damping", joints[index].damping);
                yyjson_mut_obj_add_bool(document, joint, "lock_angle", joints[index].lock_angle);
                yyjson_mut_obj_add_real(document, joint, "rest_angle", joints[index].rest_angle);
                yyjson_mut_obj_add_real(document, joint, "angular_stiffness", joints[index].angular_stiffness);
                yyjson_mut_obj_add_real(document, joint, "angular_damping", joints[index].angular_damping);
                yyjson_mut_obj_add_val(document, components, "joint", joint);
            }
        }
        if(state_sprite_references[index].used
                && (entity_mask[index] & ROHR_ANIMATED_SPRITE) != 0) {
            StateSpriteReference *reference = &state_sprite_references[index];
            yyjson_mut_val *sprite = yyjson_mut_obj(document);
            yyjson_mut_obj_add_strcpy(
                document,
                sprite,
                "animation",
                reference->animation
            );
            yyjson_mut_obj_add_val(
                document,
                sprite,
                "scale",
                state_vec2_write(
                    document,
                    (Vec2D){reference->scale.x, reference->scale.y}
                )
            );
            yyjson_mut_obj_add_real(
                document,
                sprite,
                "time_per_frame",
                reference->time_per_frame
            );
            yyjson_mut_obj_add_uint(
                document,
                sprite,
                "ticks_per_frame",
                reference->ticks_per_frame
            );
            yyjson_mut_obj_add_int(
                document,
                sprite,
                "start_frame",
                reference->start_frame
            );
            yyjson_mut_obj_add_val(
                document,
                components,
                "animated_sprite",
                sprite
            );
        }
        if((entity_mask[index] & ROHR_GROUP) != 0) {
            EntityGroupMembershipResult membership =
                entity_groups_get(entity_result.result.value);
            if(membership.kind == ERROR_RESULT_VALUE) {
                yyjson_mut_val *groups = yyjson_mut_arr(document);
                GroupIdPool *group_ids = &membership.result.value.groups;
                size_t group_index;
                for(group_index = 0;
                        group_index < group_ids->capacity;
                        group_index += 1) {
                    GroupNameResult group_name;
                    if(group_ids->used[group_index] == 0) continue;
                    group_name = entity_group_name_get(
                        group_ids->objects[group_index]
                    );
                    if(group_name.kind == ERROR_RESULT_VALUE) {
                        yyjson_mut_arr_add_strcpy(
                            document,
                            groups,
                            group_name.result.value.value
                        );
                    }
                }
                if(yyjson_mut_arr_size(groups) > 0) {
                    yyjson_mut_obj_add_val(
                        document,
                        components,
                        "groups",
                        groups
                    );
                }
            }
        }
    }
    success = yyjson_mut_write_file(path, document, YYJSON_WRITE_PRETTY, NULL, &write_error);
    yyjson_mut_doc_free(document);
    return success ? error_result_value(true) : error_result_error(ERROR_ENGINE_STATE_IO_FAILED);
}

static bool state_template_copy_array(
        yyjson_mut_doc *output_document,
        yyjson_mut_val *output_array,
        yyjson_val *input_array
) {
    yyjson_val *input_value;
    size_t input_index;
    size_t input_count;

    if(input_array == NULL) return true;
    if(!yyjson_is_arr(input_array)) return false;
    yyjson_arr_foreach(input_array, input_index, input_count, input_value) {
        yyjson_mut_val *output_value = yyjson_val_mut_copy(
            output_document,
            input_value
        );
        if(output_value == NULL
                || !yyjson_mut_arr_add_val(output_array, output_value)) {
            return false;
        }
    }
    return true;
}

EngineResult game_state_template_file_save(const char *path) {
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *groups;
    yyjson_mut_val *assets;
    yyjson_mut_val *animations;
    yyjson_mut_val *entities;
    yyjson_mut_val *ui;
    yyjson_mut_val *ui_buttons;
    yyjson_mut_val *ui_fonts;
    yyjson_mut_val *ui_labels;
    yyjson_mut_val *ui_sliders;
    bool camera_copied = false;
    yyjson_write_err write_error;
    size_t document_index;
    bool success = true;

    if(path == NULL || state_template_document_count == 0) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    document = yyjson_mut_doc_new(NULL);
    if(document == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    root = yyjson_mut_obj(document);
    groups = yyjson_mut_arr(document);
    assets = yyjson_mut_obj(document);
    animations = yyjson_mut_arr(document);
    entities = yyjson_mut_arr(document);
    ui = yyjson_mut_obj(document);
    ui_buttons = yyjson_mut_arr(document);
    ui_fonts = yyjson_mut_arr(document);
    ui_labels = yyjson_mut_arr(document);
    ui_sliders = yyjson_mut_arr(document);
    if(root == NULL || groups == NULL || assets == NULL
            || animations == NULL || entities == NULL
            || ui == NULL || ui_buttons == NULL
            || ui_fonts == NULL || ui_labels == NULL || ui_sliders == NULL) {
        yyjson_mut_doc_free(document);
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    yyjson_mut_doc_set_root(document, root);
    yyjson_mut_obj_add_uint(document, root, "version", GAME_STATE_VERSION);
    yyjson_mut_obj_add_val(document, root, "groups", groups);
    yyjson_mut_obj_add_val(document, assets, "animations", animations);
    yyjson_mut_obj_add_val(document, root, "assets", assets);
    yyjson_mut_obj_add_val(document, root, "entities", entities);
    yyjson_mut_obj_add_val(document, ui, "buttons", ui_buttons);
    yyjson_mut_obj_add_val(document, ui, "fonts", ui_fonts);
    yyjson_mut_obj_add_val(document, ui, "labels", ui_labels);
    yyjson_mut_obj_add_val(document, ui, "sliders", ui_sliders);
    yyjson_mut_obj_add_val(document, root, "ui", ui);

    for(document_index = 0;
            document_index < state_template_document_count && success;
            document_index += 1) {
        yyjson_val *input_root = yyjson_doc_get_root(
            state_template_documents[document_index]
        );
        yyjson_val *input_assets = yyjson_obj_get(input_root, "assets");
        yyjson_val *input_camera = yyjson_obj_get(input_root, "camera");
        yyjson_val *input_ui = yyjson_obj_get(input_root, "ui");
        success = state_template_copy_array(
                document,
                groups,
                yyjson_obj_get(input_root, "groups")
            )
            && state_template_copy_array(
                document,
                animations,
                input_assets == NULL
                    ? NULL
                    : yyjson_obj_get(input_assets, "animations")
            )
            && state_template_copy_array(
                document,
                entities,
                yyjson_obj_get(input_root, "entities")
            )
            && state_template_copy_array(
                document,
                ui_buttons,
                input_ui == NULL
                    ? NULL
                    : yyjson_obj_get(input_ui, "buttons")
            )
            && state_template_copy_array(
                document,
                ui_fonts,
                input_ui == NULL
                    ? NULL
                    : yyjson_obj_get(input_ui, "fonts")
            )
            && state_template_copy_array(
                document,
                ui_labels,
                input_ui == NULL
                    ? NULL
                    : yyjson_obj_get(input_ui, "labels")
            )
            && state_template_copy_array(
                document,
                ui_sliders,
                input_ui == NULL
                    ? NULL
                    : yyjson_obj_get(input_ui, "sliders")
            );
        if(success && input_camera != NULL) {
            yyjson_mut_val *output_camera;
            if(camera_copied) {
                success = false;
                continue;
            }
            output_camera = yyjson_val_mut_copy(document, input_camera);
            success = output_camera != NULL
                && yyjson_mut_obj_add_val(
                    document,
                    root,
                    "camera",
                    output_camera
                );
            camera_copied = success;
        }
    }
    if(success) {
        success = yyjson_mut_write_file(
            path,
            document,
            YYJSON_WRITE_PRETTY,
            NULL,
            &write_error
        );
    }
    yyjson_mut_doc_free(document);
    return success
        ? error_result_value(true)
        : error_result_error(ERROR_ENGINE_STATE_IO_FAILED);
}
