#include "rohr.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void key_add(SDL_Keycode key) {
    SDL_Event event = {0};

    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = key;
    rohr_ui_field_event_add(&event);
}

int main(void) {
    UIRect bounds = {0.0f, 0.0f, 100.0f, 30.0f};
    float number = 0.0f;
    char string[32] = "a";

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
    if(!rohr_ui_button("double-click", NULL, bounds, NULL).clicked) return 1;
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
    if(!rohr_ui_button("double-click", NULL, bounds, NULL).double_clicked) return 1;
    rohr_ui_frame_end();
    return 0;
}
