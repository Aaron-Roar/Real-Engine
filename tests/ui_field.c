#include "rohr.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void key_add_mod(SDL_Keycode key, SDL_Keymod modifiers) {
    SDL_Event event = {0};

    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = key;
    event.key.mod = modifiers;
    rohr_ui_field_event_add(&event);
}

static void key_add(SDL_Keycode key) {
    key_add_mod(key, SDL_KMOD_NONE);
}

static void repeated_key_add(SDL_Keycode key) {
    SDL_Event event = {0};

    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = key;
    event.key.repeat = true;
    rohr_ui_field_event_add(&event);
}

int main(void) {
    UIRect bounds = {0.0f, 0.0f, 100.0f, 30.0f};
    float number = 0.0f;
    char string[32] = "a";
    const TextAsset *dropdown_options[2] = {NULL, NULL};
    const TextAsset *long_dropdown_options[10] = {0};

    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED});
    (void)rohr_ui_interaction("primitive", bounds);
    rohr_ui_surface(bounds, (Color){20, 30, 40, 255});
    rohr_ui_border(bounds, 2.0f, (Color){0, 0, 0, 255});
    rohr_ui_content(NULL, bounds);
    rohr_ui_quad((Position){50.0f, 15.0f}, 10.0f, 10.0f, 0.0f,
        (Color){255, 255, 255, 255});
    if(rohr_ui_clip_begin(bounds)) rohr_ui_clip_end();
    rohr_ui_frame_end();
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_RELEASED});
    if(!rohr_ui_interaction("primitive", bounds).clicked) return 1;
    rohr_ui_frame_end();

    snprintf(string, sizeof(string), "abcd");
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED});
    (void)rohr_ui_field("repeated-delete-string", (UIFieldBinding){
        .kind = UI_FIELD_STRING, .string = string,
        .string_capacity = sizeof(string)
    }, NULL, bounds, NULL);
    rohr_ui_frame_end();
    repeated_key_add(SDLK_BACKSPACE);
    repeated_key_add(SDLK_BACKSPACE);
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f}});
    if(!rohr_ui_field("repeated-delete-string", (UIFieldBinding){
            .kind = UI_FIELD_STRING, .string = string,
            .string_capacity = sizeof(string)
        }, NULL, bounds, NULL).changed || strcmp(string, "ab") != 0) {
        fprintf(stderr, "repeated backspace produced '%s' instead of 'ab'\n", string);
        return 1;
    }
    rohr_ui_frame_end();
    key_add(SDLK_HOME);
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f}});
    (void)rohr_ui_field("repeated-delete-string", (UIFieldBinding){
        .kind = UI_FIELD_STRING, .string = string,
        .string_capacity = sizeof(string)
    }, NULL, bounds, NULL);
    rohr_ui_frame_end();
    repeated_key_add(SDLK_DELETE);
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f}});
    if(!rohr_ui_field("repeated-delete-string", (UIFieldBinding){
            .kind = UI_FIELD_STRING, .string = string,
            .string_capacity = sizeof(string)
        }, NULL, bounds, NULL).changed || strcmp(string, "b") != 0) {
        fprintf(stderr, "repeated delete produced '%s' instead of 'b'\n", string);
        return 1;
    }
    rohr_ui_frame_end();

    snprintf(string, sizeof(string), "a b");
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED});
    (void)rohr_ui_field("whitespace-cursor-string", (UIFieldBinding){
        .kind = UI_FIELD_STRING, .string = string,
        .string_capacity = sizeof(string)
    }, NULL, bounds, NULL);
    rohr_ui_frame_end();
    key_add(SDLK_HOME);
    key_add(SDLK_RIGHT);
    key_add(SDLK_RIGHT);
    key_add('X');
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f}});
    if(!rohr_ui_field("whitespace-cursor-string", (UIFieldBinding){
            .kind = UI_FIELD_STRING, .string = string,
            .string_capacity = sizeof(string)
        }, NULL, bounds, NULL).changed || strcmp(string, "a Xb") != 0) {
        fprintf(stderr, "whitespace cursor produced '%s' instead of 'a Xb'\n", string);
        return 1;
    }
    rohr_ui_frame_end();

    snprintf(string, sizeof(string), "a");
    rohr_ui_frame_begin((UIInput){
        .pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED
    });
    (void)rohr_ui_field("number", (UIFieldBinding){
        .kind = UI_FIELD_FLOAT,
        .number = &number
    }, NULL, bounds, NULL);
    rohr_ui_frame_end();
    key_add('1'); key_add('2'); key_add(','); key_add('.'); key_add('3'); key_add('4');
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f}});
    if(!rohr_ui_field("number", (UIFieldBinding){
            .kind = UI_FIELD_FLOAT, .number = &number
        }, NULL, bounds, NULL).changed || fabsf(number - 12.3f) > 0.001f) {
        fprintf(stderr, "numeric field produced %.3f instead of 12.3\n", number);
        return 1;
    }
    rohr_ui_frame_end();

    rohr_ui_frame_begin((UIInput){
        .pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED
    });
    (void)rohr_ui_field("string", (UIFieldBinding){
        .kind = UI_FIELD_STRING,
        .string = string,
        .string_capacity = sizeof(string)
    }, NULL, bounds, NULL);
    rohr_ui_frame_end();
    key_add('b');
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f}});
    if(!rohr_ui_field("string", (UIFieldBinding){
            .kind = UI_FIELD_STRING,
            .string = string,
            .string_capacity = sizeof(string)
        }, NULL, bounds, NULL).changed || strcmp(string, "ab") != 0) {
        fprintf(stderr, "string field produced '%s' instead of 'ab'\n", string);
        return 1;
    }
    rohr_ui_frame_end();

    key_add_mod(SDLK_A, SDL_KMOD_CTRL);
    key_add('z');
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f}});
    if(!rohr_ui_field("string", (UIFieldBinding){
            .kind = UI_FIELD_STRING, .string = string,
            .string_capacity = sizeof(string)
        }, NULL, bounds, NULL).changed || strcmp(string, "z") != 0) {
        fprintf(stderr, "Ctrl+A replacement produced '%s' instead of 'z'\n", string);
        return 1;
    }
    rohr_ui_frame_end();

    key_add_mod(SDLK_A, SDL_KMOD_CTRL);
    key_add(SDLK_DELETE);
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f}});
    if(!rohr_ui_field("string", (UIFieldBinding){
            .kind = UI_FIELD_STRING, .string = string,
            .string_capacity = sizeof(string)
        }, NULL, bounds, NULL).changed || string[0] != '\0') {
        fprintf(stderr, "Ctrl+A delete produced '%s' instead of empty text\n", string);
        return 1;
    }
    rohr_ui_frame_end();

    snprintf(string, sizeof(string), "abcd");
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED});
    (void)rohr_ui_field("cursor-string", (UIFieldBinding){
        .kind = UI_FIELD_STRING, .string = string,
        .string_capacity = sizeof(string)
    }, NULL, bounds, NULL);
    rohr_ui_frame_end();
    key_add(SDLK_LEFT);
    key_add(SDLK_LEFT);
    key_add('X');
    key_add(SDLK_DELETE);
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f}});
    if(!rohr_ui_field("cursor-string", (UIFieldBinding){
            .kind = UI_FIELD_STRING, .string = string,
            .string_capacity = sizeof(string)
        }, NULL, bounds, NULL).changed || strcmp(string, "abXd") != 0) {
        fprintf(stderr, "cursor editing produced '%s' instead of 'abXd'\n", string);
        return 1;
    }
    rohr_ui_frame_end();
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_RELEASED});
    (void)rohr_ui_field("cursor-string", (UIFieldBinding){
        .kind = UI_FIELD_STRING, .string = string,
        .string_capacity = sizeof(string)
    }, NULL, bounds, NULL);
    rohr_ui_frame_end();

    rohr_ui_frame_begin((UIInput){
        .pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED
    });
    (void)rohr_ui_button("double-click", NULL, bounds, NULL);
    rohr_ui_frame_end();
    rohr_ui_frame_begin((UIInput){
        .pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_RELEASED
    });
    if(!rohr_ui_button("double-click", NULL, bounds, NULL).clicked) {
        fprintf(stderr, "first button click failed\n");
        return 1;
    }
    rohr_ui_frame_end();
    rohr_ui_frame_begin((UIInput){
        .pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED
    });
    (void)rohr_ui_button("double-click", NULL, bounds, NULL);
    rohr_ui_frame_end();
    rohr_ui_frame_begin((UIInput){
        .pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_RELEASED
    });
    if(!rohr_ui_button("double-click", NULL, bounds, NULL).double_clicked) {
        fprintf(stderr, "double button click failed\n");
        return 1;
    }
    rohr_ui_frame_end();

    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED});
    (void)rohr_ui_dropdown("dropdown", dropdown_options, 2, 0, bounds, NULL);
    rohr_ui_frame_end();
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_RELEASED});
    if(!rohr_ui_dropdown("dropdown", dropdown_options, 2, 0, bounds, NULL).open) return 1;
    rohr_ui_frame_end();
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 70.0f},
        .primary_button = MOUSE_BUTTON_STATE_UP});
    {
        UIDropdownResult result = rohr_ui_dropdown(
            "dropdown", dropdown_options, 2, 0, bounds, NULL);
        if(!result.open || result.hovered_index != 1) return 1;
    }
    rohr_ui_frame_end();
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 70.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED});
    (void)rohr_ui_dropdown("dropdown", dropdown_options, 2, 0, bounds, NULL);
    rohr_ui_frame_end();
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 70.0f},
        .primary_button = MOUSE_BUTTON_STATE_RELEASED});
    {
        UIDropdownResult result = rohr_ui_dropdown(
            "dropdown", dropdown_options, 2, 0, bounds, NULL);
        if(!result.changed || result.selected_index != 1 || result.open) return 1;
    }
    rohr_ui_frame_end();

    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED});
    (void)rohr_ui_dropdown(
        "keyboard-dropdown", dropdown_options, 2, 0, bounds, NULL);
    rohr_ui_frame_end();
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_RELEASED});
    if(!rohr_ui_dropdown("keyboard-dropdown", dropdown_options,
            2, 0, bounds, NULL).open) return 1;
    rohr_ui_frame_end();
    if(!rohr_ui_navigation_move(UI_NAVIGATION_DOWN) ||
            !rohr_ui_navigation_move(UI_NAVIGATION_DOWN) ||
            !rohr_ui_navigation_activate()) return 1;
    rohr_ui_frame_begin((UIInput){0});
    {
        UIDropdownResult result = rohr_ui_dropdown(
            "keyboard-dropdown", dropdown_options, 2, 0, bounds, NULL);
        if(result.open || !result.changed || result.selected_index != 1) return 1;
    }
    rohr_ui_frame_end();

    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED});
    (void)rohr_ui_dropdown("long-dropdown", long_dropdown_options, 10, 0, bounds, NULL);
    rohr_ui_frame_end();
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 10.0f},
        .primary_button = MOUSE_BUTTON_STATE_RELEASED});
    if(!rohr_ui_dropdown("long-dropdown", long_dropdown_options,
            10, 0, bounds, NULL).open) return 1;
    rohr_ui_frame_end();
    {
        SDL_Event wheel = {0};
        UIDropdownResult result;
        wheel.type = SDL_EVENT_MOUSE_WHEEL;
        wheel.wheel.y = -1.0f;
        rohr_ui_field_event_add(&wheel);
        rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 40.0f}});
        result = rohr_ui_dropdown("long-dropdown", long_dropdown_options,
            10, 0, bounds, NULL);
        if(!result.open || result.hovered_index != 1) return 1;
        rohr_ui_frame_end();
    }
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 40.0f},
        .primary_button = MOUSE_BUTTON_STATE_PRESSED});
    (void)rohr_ui_dropdown("long-dropdown", long_dropdown_options, 10, 0, bounds, NULL);
    rohr_ui_frame_end();
    rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 40.0f},
        .primary_button = MOUSE_BUTTON_STATE_RELEASED});
    {
        UIDropdownResult result = rohr_ui_dropdown(
            "long-dropdown", long_dropdown_options, 10, 0, bounds, NULL);
        if(result.open || !result.changed || result.selected_index != 1) return 1;
    }
    rohr_ui_frame_end();

    {
        SDL_Event wheel = {0};
        UIScrollRegionResult scroll;
        wheel.type = SDL_EVENT_MOUSE_WHEEL;
        wheel.wheel.y = -1.0f;
        rohr_ui_field_event_add(&wheel);
        rohr_ui_frame_begin((UIInput){.pointer = {10.0f, 35.0f}});
        scroll = rohr_ui_scroll_region_begin("scroll", (UIRect){0.0f, 0.0f,
            100.0f, 50.0f}, 100.0f, 0.0f, 10.0f);
        if(!scroll.changed || fabsf(scroll.offset - 10.0f) > 0.001f ||
                !rohr_ui_button("scrolled-button", NULL,
                    (UIRect){0.0f, 40.0f, 100.0f, 20.0f}, NULL).hovered ||
                rohr_ui_button("clipped-button", NULL,
                    (UIRect){0.0f, 100.0f, 100.0f, 20.0f}, NULL).hovered) return 1;
        rohr_ui_scroll_region_end();
        rohr_ui_frame_end();
    }

    rohr_ui_frame_begin((UIInput){0});
    (void)rohr_ui_button("nav-a", NULL, (UIRect){0.0f, 0.0f, 80.0f, 20.0f}, NULL);
    (void)rohr_ui_button("nav-b", NULL, (UIRect){120.0f, 0.0f, 80.0f, 20.0f}, NULL);
    (void)rohr_ui_button("nav-c", NULL, (UIRect){0.0f, 40.0f, 80.0f, 20.0f}, NULL);
    rohr_ui_frame_end();
    {
        UIRect focused;
        if(!rohr_ui_navigation_move(UI_NAVIGATION_DOWN) ||
                !rohr_ui_navigation_focus_bounds_get(&focused) ||
                fabsf(focused.x) > 0.001f ||
                !rohr_ui_navigation_move(UI_NAVIGATION_RIGHT) ||
                !rohr_ui_navigation_focus_bounds_get(&focused) ||
                fabsf(focused.x - 120.0f) > 0.001f ||
                !rohr_ui_navigation_activate()) return 1;
    }
    rohr_ui_frame_begin((UIInput){0});
    (void)rohr_ui_button("nav-a", NULL, (UIRect){0.0f, 0.0f, 80.0f, 20.0f}, NULL);
    {
        UIButtonResult activated = rohr_ui_button(
            "nav-b", NULL, (UIRect){120.0f, 0.0f, 80.0f, 20.0f}, NULL);
        if(!activated.clicked || !activated.double_clicked ||
                !activated.keyboard_activated || !activated.focused ||
                !activated.focus_changed) return 1;
    }
    (void)rohr_ui_button("nav-c", NULL, (UIRect){0.0f, 40.0f, 80.0f, 20.0f}, NULL);
    rohr_ui_frame_end();
    return 0;
}
