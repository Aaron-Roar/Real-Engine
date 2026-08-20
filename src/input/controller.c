/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "controller.h"
#include "console.h"
#include "graphics.h"
#include <string.h>

static bool controller_scancode_check(SDL_Scancode scancode) {
    return scancode > SDL_SCANCODE_UNKNOWN && scancode < SDL_SCANCODE_COUNT;
}

static bool controller_scancode_state_check(const KeyboardState *keyboard, SDL_Scancode scancode, KeyState state) {
    if(keyboard == NULL || !controller_scancode_check(scancode)) {
        return false;
    }
    return keyboard->key_states[scancode] == state;
}

static bool controller_scancode_down_get(const KeyboardState *keyboard, SDL_Scancode scancode) {
    if(keyboard == NULL || !controller_scancode_check(scancode)) {
        return false;
    }
    return keyboard->key_states[scancode] == KEY_STATE_DOWN || keyboard->key_states[scancode] == KEY_STATE_PRESSED;
}

static SDL_Scancode controller_keycode_to_scancode_get(SDL_Keycode keycode) {
    SDL_Scancode scancode = SDL_GetScancodeFromKey(keycode, NULL);

    if(!controller_scancode_check(scancode)) {
        return SDL_SCANCODE_UNKNOWN;
    }
    return scancode;
}

Vec2D controller_axis_from_keycodes_get(
        const KeyboardState *keyboard,
        SDL_Keycode up,
        SDL_Keycode left,
        SDL_Keycode down,
        SDL_Keycode right
        ) {
    Vec2D axis = {0};

    if(controller_key_down_get(keyboard, right)) {
        axis.x += 1.0f;
    }
    if(controller_key_down_get(keyboard, left)) {
        axis.x -= 1.0f;
    }
    if(controller_key_down_get(keyboard, up)) {
        axis.y += 1.0f;
    }
    if(controller_key_down_get(keyboard, down)) {
        axis.y -= 1.0f;
    }
    /* Opposing direction keys cancel each other before diagonal normalization. */
    if(axis.x == 0.0f && axis.y == 0.0f) {
        return axis;
    }
    return math_vector_normalize(axis);
}

KeyboardEvent controller_keyboard_event_capture(const SDL_Event *sdl_event) {
    KeyboardEvent key_event = {
        .keycode = SDLK_UNKNOWN,
        .scancode = SDL_SCANCODE_UNKNOWN,
        .state = KEY_STATE_NONE,
    };

    if (sdl_event == NULL) {
        return key_event;
    }
    if (sdl_event->type != SDL_EVENT_KEY_DOWN &&
        sdl_event->type != SDL_EVENT_KEY_UP) {
        return key_event;
    }

    SDL_Scancode scancode = sdl_event->key.scancode;
    if(!controller_scancode_check(scancode)) {
        return key_event;
    }
    key_event.keycode = sdl_event->key.key;
    key_event.scancode = scancode;

    if(sdl_event->type == SDL_EVENT_KEY_DOWN) {
        if(sdl_event->key.repeat) {
            return key_event;
        }
        key_event.state = KEY_STATE_PRESSED;
    }
    else if(sdl_event->type == SDL_EVENT_KEY_UP) {
        key_event.state = KEY_STATE_RELEASED;
    }

    return key_event;
}

bool controller_key_down_get(const KeyboardState *keyboard, SDL_Keycode keycode) {
    return controller_scancode_down_get(keyboard, controller_keycode_to_scancode_get(keycode));
}

bool controller_key_pressed_get(const KeyboardState *keyboard, SDL_Keycode keycode) {
    return controller_scancode_state_check(keyboard, controller_keycode_to_scancode_get(keycode), KEY_STATE_PRESSED);
}

bool controller_key_released_get(const KeyboardState *keyboard, SDL_Keycode keycode) {
    return controller_scancode_state_check(keyboard, controller_keycode_to_scancode_get(keycode), KEY_STATE_RELEASED);
}

Vec2D controller_wasd_axis_get(const KeyboardState *keyboard) {
    return controller_axis_from_keycodes_get(keyboard, SDLK_W, SDLK_A, SDLK_S, SDLK_D);
}

Vec2D controller_arrow_axis_get(const KeyboardState *keyboard) {
    return controller_axis_from_keycodes_get(keyboard, SDLK_UP, SDLK_LEFT, SDLK_DOWN, SDLK_RIGHT);
}

Controller controller_default_get(void) {
    return (Controller){
        .enabled = true,
    };
}

Controller controller_wasd_default_get(void) {
    Controller controller = controller_default_get();
    (void)controller_axis_add(
        &controller,
        "movement",
        (ControllerAxisBinding){
            .positive_x = SDLK_D,
            .negative_x = SDLK_A,
            .positive_y = SDLK_W,
            .negative_y = SDLK_S,
        }
    );
    return controller;
}

Controller controller_arrows_default_get(void) {
    Controller controller = controller_default_get();
    (void)controller_axis_add(
        &controller,
        "movement",
        (ControllerAxisBinding){
            .positive_x = SDLK_RIGHT,
            .negative_x = SDLK_LEFT,
            .positive_y = SDLK_UP,
            .negative_y = SDLK_DOWN,
        }
    );
    return controller;
}

void controller_axis_binding_set(Controller *controller, ControllerAxisBinding binding) {
    (void)controller_axis_add(controller, "movement", binding);
}

Vec2D controller_default_axis_get(const KeyboardState *keyboard, const Controller *controller) {
    return controller_axis_get(keyboard, controller, "movement");
}

bool controller_axis_add(
        Controller *controller,
        const char *name,
        ControllerAxisBinding binding
        ) {
    size_t i;
    if(controller == NULL || name == NULL || name[0] == '\0') {
        return false;
    }
    for(i = 0; i < controller->axis_count; i += 1) {
        if(strcmp(controller->axes[i].name, name) == 0) {
            controller->axes[i].binding = binding;
            return true;
        }
    }
    if(controller->axis_count >= CONTROLLER_AXIS_LIMIT) {
        return false;
    }
    controller->axes[controller->axis_count] = (ControllerAxis){
        .name = name,
        .binding = binding,
    };
    controller->axis_count += 1;
    return true;
}

bool controller_button_add(
        Controller *controller,
        const char *name,
        SDL_Keycode keycode
        ) {
    size_t i;
    if(controller == NULL || name == NULL || name[0] == '\0' || keycode == SDLK_UNKNOWN) {
        return false;
    }
    for(i = 0; i < controller->button_count; i += 1) {
        if(strcmp(controller->buttons[i].name, name) == 0) {
            controller->buttons[i].keycode = keycode;
            return true;
        }
    }
    if(controller->button_count >= CONTROLLER_BUTTON_LIMIT) {
        return false;
    }
    controller->buttons[controller->button_count] = (ControllerButton){
        .name = name,
        .keycode = keycode,
    };
    controller->button_count += 1;
    return true;
}

static const ControllerAxis *controller_find_axis(
        const Controller *controller,
        const char *name
        ) {
    size_t i;
    if(controller == NULL || name == NULL) {
        return NULL;
    }
    for(i = 0; i < controller->axis_count; i += 1) {
        if(strcmp(controller->axes[i].name, name) == 0) {
            return &controller->axes[i];
        }
    }
    return NULL;
}

static const ControllerButton *controller_find_button(
        const Controller *controller,
        const char *name
        ) {
    size_t i;
    if(controller == NULL || name == NULL) {
        return NULL;
    }
    for(i = 0; i < controller->button_count; i += 1) {
        if(strcmp(controller->buttons[i].name, name) == 0) {
            return &controller->buttons[i];
        }
    }
    return NULL;
}

Vec2D controller_axis_get(
        const KeyboardState *keyboard,
        const Controller *controller,
        const char *name
        ) {
    const ControllerAxis *axis = controller_find_axis(controller, name);
    if(controller == NULL || !controller->enabled || axis == NULL) {
        return (Vec2D){0};
    }
    return controller_axis_from_keycodes_get(
        keyboard,
        axis->binding.positive_y,
        axis->binding.negative_x,
        axis->binding.negative_y,
        axis->binding.positive_x
    );
}

bool controller_button_down_get(
        const KeyboardState *keyboard,
        const Controller *controller,
        const char *name
        ) {
    const ControllerButton *button = controller_find_button(controller, name);
    return controller != NULL && controller->enabled && button != NULL &&
        controller_key_down_get(keyboard, button->keycode);
}

bool controller_button_pressed_get(
        const KeyboardState *keyboard,
        const Controller *controller,
        const char *name
        ) {
    const ControllerButton *button = controller_find_button(controller, name);
    return controller != NULL && controller->enabled && button != NULL &&
        controller_key_pressed_get(keyboard, button->keycode);
}

bool controller_button_released_get(
        const KeyboardState *keyboard,
        const Controller *controller,
        const char *name
        ) {
    const ControllerButton *button = controller_find_button(controller, name);
    return controller != NULL && controller->enabled && button != NULL &&
        controller_key_released_get(keyboard, button->keycode);
}

void controller_key_event_add(KeyboardState *keyboard, KeyboardEvent key_event) {
    if(keyboard == NULL) {
        return;
    }
    if(
        !controller_scancode_check(key_event.scancode) ||
        key_event.state == KEY_STATE_NONE ||
        key_event.keycode == SDLK_UNKNOWN) {
        return;
    }

    KeyState current_key_state = keyboard->key_states[key_event.scancode];
    if(key_event.state == KEY_STATE_PRESSED) {
        if(current_key_state == KEY_STATE_DOWN || current_key_state == KEY_STATE_PRESSED) {
            return;
        }
        keyboard->key_states[key_event.scancode] = KEY_STATE_PRESSED;
    }
    if(key_event.state == KEY_STATE_RELEASED) {
        if(current_key_state == KEY_STATE_UP || current_key_state == KEY_STATE_RELEASED) {
            return;
        }
        keyboard->key_states[key_event.scancode] = KEY_STATE_RELEASED;
    }
}

void controller_key_states_update(KeyboardState *keyboard) {
    if(keyboard == NULL) {
        return;
    }

    for(int i = 0; i < SDL_SCANCODE_COUNT; i += 1) {
        if(keyboard->key_states[i] == KEY_STATE_RELEASED) {
            keyboard->key_states[i] = KEY_STATE_UP;
        }
        else if(keyboard->key_states[i] == KEY_STATE_PRESSED) {
            keyboard->key_states[i] = KEY_STATE_DOWN;
        }
    }
}

MouseEvent controller_mouse_event_capture(const SDL_Event *sdl_event) {
    MouseEvent mouse_event = {
        .button = MOUSE_BUTTON_NONE,
        .state = MOUSE_BUTTON_STATE_NONE,
        .position = {0,0},
    };

    if (sdl_event == NULL) {
        return mouse_event;
    }
    if (sdl_event->type != SDL_EVENT_MOUSE_BUTTON_DOWN &&
        sdl_event->type != SDL_EVENT_MOUSE_BUTTON_UP) {
        return mouse_event;
    }

    if(sdl_event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        switch(sdl_event->button.button) {
            case SDL_BUTTON_LEFT:
                mouse_event.button = MOUSE_BUTTON_LEFT;
                mouse_event.state = MOUSE_BUTTON_STATE_PRESSED;
                break;
            case SDL_BUTTON_RIGHT:
                mouse_event.button = MOUSE_BUTTON_RIGHT;
                mouse_event.state = MOUSE_BUTTON_STATE_PRESSED;
                break;
            case SDL_BUTTON_MIDDLE:
                mouse_event.button = MOUSE_BUTTON_MIDDLE;
                mouse_event.state = MOUSE_BUTTON_STATE_PRESSED;
                break;
            default:
                break;
        }

    }
    else if(sdl_event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        switch(sdl_event->button.button) {
            case SDL_BUTTON_LEFT:
                mouse_event.button = MOUSE_BUTTON_LEFT;
                mouse_event.state = MOUSE_BUTTON_STATE_RELEASED;
                break;
            case SDL_BUTTON_RIGHT:
                mouse_event.button = MOUSE_BUTTON_RIGHT;
                mouse_event.state = MOUSE_BUTTON_STATE_RELEASED;
                break;
            case SDL_BUTTON_MIDDLE:
                mouse_event.button = MOUSE_BUTTON_MIDDLE;
                mouse_event.state = MOUSE_BUTTON_STATE_RELEASED;
                break;
            default:
                break;
        }
    }
    mouse_event.position = graphics_window_to_screen_get((Position){
        .x = sdl_event->button.x,
        .y = sdl_event->button.y,
    });

    return mouse_event;
}

void controller_mouse_event_add(MouseState *mouse, MouseEvent mouse_event) {
    if(mouse == NULL) {
        return;
    }
    if(
        mouse_event.button <= MOUSE_BUTTON_NONE ||
        mouse_event.state == MOUSE_BUTTON_STATE_NONE ||
        mouse_event.button >= MOUSE_BUTTON_COUNT) {
        return;
    }

    MouseButtonState current_mouse_button_state = mouse->button_states[mouse_event.button];
    if(mouse_event.state == MOUSE_BUTTON_STATE_PRESSED) {
        if(current_mouse_button_state == MOUSE_BUTTON_STATE_DOWN || current_mouse_button_state == MOUSE_BUTTON_STATE_PRESSED) {
            return;
        }
        mouse->button_states[mouse_event.button] = MOUSE_BUTTON_STATE_PRESSED;
    }
    if(mouse_event.state == MOUSE_BUTTON_STATE_RELEASED) {
        if(current_mouse_button_state == MOUSE_BUTTON_STATE_UP || current_mouse_button_state == MOUSE_BUTTON_STATE_RELEASED) {
            return;
        }
        mouse->button_states[mouse_event.button] = MOUSE_BUTTON_STATE_RELEASED;
    }
    mouse->position = mouse_event.position;
}

void controller_mouse_states_update(MouseState *mouse) {
    if(mouse == NULL) {
        return;
    }

    for(int i = 0; i < MOUSE_BUTTON_COUNT; i += 1) {
        if(mouse->button_states[i] == MOUSE_BUTTON_STATE_RELEASED) {
            mouse->button_states[i] = MOUSE_BUTTON_STATE_UP;
        }
        else if(mouse->button_states[i] == MOUSE_BUTTON_STATE_PRESSED) {
            mouse->button_states[i] = MOUSE_BUTTON_STATE_DOWN;
        }
    }
    mouse->position = graphics_mouse_screen_position_get();
}
const char *mouse_button_name(MouseButton button)
{
    switch (button) {
        case MOUSE_BUTTON_NONE:
            return "ROHR_NONE";

        case MOUSE_BUTTON_LEFT:
            return "LEFT";

        case MOUSE_BUTTON_MIDDLE:
            return "MIDDLE";

        case MOUSE_BUTTON_RIGHT:
            return "RIGHT";

        default:
            return "INVALID_BUTTON";
    }
}

const char *mouse_button_state_name(MouseButtonState state)
{
    switch (state) {
        case MOUSE_BUTTON_STATE_NONE:
            return "ROHR_NONE";

        case MOUSE_BUTTON_STATE_UP:
            return "UP";

        case MOUSE_BUTTON_STATE_DOWN:
            return "DOWN";

        case MOUSE_BUTTON_STATE_PRESSED:
            return "PRESSED";

        case MOUSE_BUTTON_STATE_RELEASED:
            return "RELEASED";

        default:
            return "INVALID_STATE";
    }
}

void controller_mouse_event_print(MouseEvent event)
{
    if (event.button == MOUSE_BUTTON_NONE ||
        event.state == MOUSE_BUTTON_STATE_NONE) {
        return;
    }

    console_write(
        LOG_ENGINE,
        "MouseEvent { button: %s, state: %s, position: { x: %.2f, y: %.2f } }\n",
        mouse_button_name(event.button),
        mouse_button_state_name(event.state),
        event.position.x,
        event.position.y
    );
}
