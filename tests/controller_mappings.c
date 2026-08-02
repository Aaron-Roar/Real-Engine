#include "rohr.h"

int main(void) {
    KeyboardState keyboard = {0};
    Controller first = rohr_controller_wasd_default_get();
    Controller second = rohr_controller_arrows_default_get();
    Vec2D first_axis;
    Vec2D second_axis;

    if(rohr_error_check(rohr_engine_init())) {
        return 1;
    }
    if(!rohr_controller_axis_add(
            &first,
            "aim",
            (ControllerAxisBinding){
                .positive_x = SDLK_D,
                .negative_x = SDLK_A,
                .positive_y = SDLK_W,
                .negative_y = SDLK_S,
            }) ||
            !rohr_controller_button_add(&first, "fire", SDLK_SPACE) ||
            !rohr_controller_button_add(&second, "fire", SDLK_RETURN)) {
        rohr_engine_shutdown();
        return 1;
    }

    rohr_controller_key_event_add(
        &keyboard,
        (KeyboardEvent){
            .keycode = SDLK_D,
            .scancode = SDL_SCANCODE_D,
            .state = KEY_STATE_PRESSED,
        }
    );
    rohr_controller_key_event_add(
        &keyboard,
        (KeyboardEvent){
            .keycode = SDLK_SPACE,
            .scancode = SDL_SCANCODE_SPACE,
            .state = KEY_STATE_PRESSED,
        }
    );
    rohr_controller_key_event_add(
        &keyboard,
        (KeyboardEvent){
            .keycode = SDLK_UP,
            .scancode = SDL_SCANCODE_UP,
            .state = KEY_STATE_PRESSED,
        }
    );

    first_axis = rohr_controller_default_axis_get(&keyboard, &first);
    second_axis = rohr_controller_default_axis_get(&keyboard, &second);
    if(first_axis.x != 1.0f || first_axis.y != 0.0f ||
            second_axis.x != 0.0f || second_axis.y != 1.0f) {
        rohr_engine_shutdown();
        return 1;
    }
    first_axis = rohr_controller_axis_get(&keyboard, &first, "aim");
    if(first_axis.x != 1.0f || first_axis.y != 0.0f ||
            !rohr_controller_button_down_get(&keyboard, &first, "fire") ||
            !rohr_controller_button_pressed_get(&keyboard, &first, "fire") ||
            rohr_controller_button_down_get(&keyboard, &second, "fire") ||
            rohr_controller_button_down_get(&keyboard, &first, "missing")) {
        rohr_engine_shutdown();
        return 1;
    }

    rohr_controller_axis_binding_set(
        &first,
        (ControllerAxisBinding){
            .positive_x = SDLK_W,
            .negative_x = SDLK_S,
            .positive_y = SDLK_A,
            .negative_y = SDLK_D,
        }
    );
    first_axis = rohr_controller_default_axis_get(&keyboard, &first);
    if(first_axis.x != 0.0f || first_axis.y != -1.0f) {
        rohr_engine_shutdown();
        return 1;
    }

    first.enabled = false;
    first_axis = rohr_controller_default_axis_get(&keyboard, &first);
    if(first_axis.x != 0.0f || first_axis.y != 0.0f) {
        rohr_engine_shutdown();
        return 1;
    }

    rohr_engine_shutdown();
    return 0;
}
