#ifndef CONTROLLER_H
#define CONTROLLER_H
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "math2d.h"

/** Per-key state tracked across frames. */
typedef enum {
    /** Key is not held. */
    KEY_STATE_UP = 0,
    /** Key is held. */
    KEY_STATE_DOWN,
    /** Key was pressed this frame. */
    KEY_STATE_PRESSED,
    /** Key was released this frame. */
    KEY_STATE_RELEASED,
    /** No valid key state. */
    KEY_STATE_NONE,
} KeyState;

/** Current state table for all keyboard keys. */
typedef struct {
    /** Key states indexed by SDL_Scancode. */
    KeyState key_states[SDL_SCANCODE_COUNT];
} KeyboardState;

/** One keyboard input event. */
typedef struct {
    /** SDL keycode involved in the event. */
    SDL_Keycode keycode;
    /** SDL scancode used to index KeyboardState. */
    SDL_Scancode scancode;
    /** New key state from the event. */
    KeyState state;
} KeyboardEvent;

/** Engine mouse button identifiers. */
typedef enum {
    /** No mouse button. */
    MOUSE_BUTTON_NONE = 0,
    /** Left mouse button. */
    MOUSE_BUTTON_LEFT,
    /** Right mouse button. */
    MOUSE_BUTTON_RIGHT,
    /** Middle mouse button. */
    MOUSE_BUTTON_MIDDLE,
    /** Number of mouse button entries. */
    MOUSE_BUTTON_COUNT,
} MouseButton;

/** Per-button mouse state tracked across frames. */
typedef enum {
    /** Button is not held. */
    MOUSE_BUTTON_STATE_UP = 0,
    /** Button is held. */
    MOUSE_BUTTON_STATE_DOWN,
    /** Button was pressed this frame. */
    MOUSE_BUTTON_STATE_PRESSED,
    /** Button was released this frame. */
    MOUSE_BUTTON_STATE_RELEASED,
    /** No valid button state. */
    MOUSE_BUTTON_STATE_NONE,
} MouseButtonState;

/** Mouse position in logical screen coordinates. */
typedef Vec2D MousePosition;

/** Current mouse state. */
typedef struct {
    /** Button states indexed by MouseButton. */
    MouseButtonState button_states[MOUSE_BUTTON_COUNT];
    /** Current mouse position in logical screen coordinates. */
    MousePosition position;
} MouseState;

/** One mouse input event. */
typedef struct {
    /** Button involved in the event. */
    MouseButton button;
    /** New button state from the event. */
    MouseButtonState state;
    /** Mouse position in logical screen coordinates at event capture time. */
    MousePosition position;
} MouseEvent;

/** Advance transient key states to held/up states. */
void update_key_states(KeyboardState *keyboard);

/** Apply one keyboard event to keyboard state. */
void add_key_event(KeyboardState *keyboard, KeyboardEvent key_event);

/** Convert an SDL event to an engine keyboard event. */
KeyboardEvent capture_keyboard_event(const SDL_Event *sdl_event);
/** Check whether an SDL keycode is currently held or was pressed this frame. */
bool controller_key_down(const KeyboardState *keyboard, SDL_Keycode keycode);
/** Check whether an SDL keycode was pressed this frame. */
bool controller_key_pressed(const KeyboardState *keyboard, SDL_Keycode keycode);
/** Check whether an SDL keycode was released this frame. */
bool controller_key_released(const KeyboardState *keyboard, SDL_Keycode keycode);
/** Return normalized movement input from supplied up/left/down/right SDL keycodes. Opposing directions cancel. */
Vec2D controller_axis_from_keycodes(
        const KeyboardState *keyboard,
        SDL_Keycode up,
        SDL_Keycode left,
        SDL_Keycode down,
        SDL_Keycode right
);
/** Return normalized movement input from W/A/S/D. */
Vec2D controller_wasd_axis(const KeyboardState *keyboard);
/** Return normalized movement input from arrow keys. */
Vec2D controller_arrow_axis(const KeyboardState *keyboard);

/** Print a mouse event to the console. */
void print_mouse_event(MouseEvent event);

/** Advance transient mouse button states to held/up states. */
void update_mouse_states(MouseState *mouse);

/** Apply one mouse event to mouse state. */
void add_mouse_event(MouseState *mouse, MouseEvent mouse_event);

/** Convert an SDL event to an engine mouse event. */
MouseEvent capture_mouse_event(const SDL_Event *sdl_event);
#endif
