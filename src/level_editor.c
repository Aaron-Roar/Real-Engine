#include "entity_components.h"
#include "systems.h"
#include "tools.h"
#include "console.h"
#include "engine.h"
#include "math2d.h"
#include "graphics.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

Entity selection = 0;
Entity selected_entity = 0;
//Selection Info

#define SCALE_INCREMENT 0.1
#define MOVE_INCREMENT 5
typedef enum {
    EDITOR_NONE,

    EDITOR_SELECT,

    EDITOR_MOVE,
    EDITOR_MOVE_MOUSE,
    EDITOR_MOVE_UP,
    EDITOR_MOVE_DOWN,
    EDITOR_MOVE_LEFT,
    EDITOR_MOVE_RIGHT,

    EDITOR_ROTATE,
    EDITOR_ROTATE_CW,
    EDITOR_ROTATE_CCW,
    EDITOR_ROTATE_UP,
    EDITOR_ROTATE_DOWN,
    EDITOR_ROTATE_LEFT,
    EDITOR_ROTATE_RIGHT,


    EDITOR_SCALE,
    EDITOR_SCALE_ALL_UP,
    EDITOR_SCALE_ALL_DOWN,
    EDITOR_SCALE_X_UP,
    EDITOR_SCALE_Y_UP,
    EDITOR_SCALE_X_DOWN,
    EDITOR_SCALE_Y_DOWN,
    EDITOR_SCALE_LOCK,

    EDITOR_SHAPE,
    EDITOR_SHAPE_ADD_VERTEX,
    EDITOR_SHAPE_REMOVE_VERTEX,


    EDITOR_PAUSE_ENGINE,
} LevelEditorMode;

bool has_selection = false;
LevelEditorMode prev_mode = EDITOR_NONE;
LevelEditorMode current_mode = EDITOR_NONE;

typedef enum {
    EDITOR_KEY_NONE,
    EDITOR_KEY_0,
    EDITOR_KEY_1,
    EDITOR_KEY_2,
    EDITOR_KEY_3,
    EDITOR_KEY_4,
    EDITOR_KEY_5,
    EDITOR_KEY_6,
    EDITOR_KEY_7,
    EDITOR_KEY_8,
    EDITOR_KEY_9,

    EDITOR_KEY_A,
    EDITOR_KEY_B,
    EDITOR_KEY_C,
    EDITOR_KEY_D,
    EDITOR_KEY_E,
    EDITOR_KEY_F,
    EDITOR_KEY_G,
    EDITOR_KEY_H,
    EDITOR_KEY_I,
    EDITOR_KEY_J,
    EDITOR_KEY_K,
    EDITOR_KEY_L,
    EDITOR_KEY_M,
    EDITOR_KEY_N,
    EDITOR_KEY_O,
    EDITOR_KEY_P,
    EDITOR_KEY_Q,
    EDITOR_KEY_R,
    EDITOR_KEY_S,
    EDITOR_KEY_T,
    EDITOR_KEY_U,
    EDITOR_KEY_V,
    EDITOR_KEY_W,
    EDITOR_KEY_X,
    EDITOR_KEY_Y,
    EDITOR_KEY_Z,

    EDITOR_KEY_BACKSPACE,
    EDITOR_KEY_DELETE,
    EDITOR_KEY_ENTER,
    EDITOR_KEY_EQUALS,
    EDITOR_KEY_ESCAPE,

    EDITOR_KEY_ALT,
    EDITOR_KEY_CTRL,
    EDITOR_KEY_SHIFT,

    EDITOR_KEY_UP,
    EDITOR_KEY_DOWN,
    EDITOR_KEY_LEFT,
    EDITOR_KEY_RIGHT,

    EDITOR_KEY_MINUS,
    EDITOR_KEY_PLUS,

    EDITOR_KEY_MOUSE_LEFT,
    EDITOR_KEY_MOUSE_MIDDLE,
    EDITOR_KEY_MOUSE_RIGHT,

    EDITOR_KEY_SPACE,
    EDITOR_KEY_TAB,
    EDITOR_KEY_SHIFT_TAB,
    EDITOR_KEY_SHIFT_0,
    EDITOR_KEY_SHIFT_1,
    EDITOR_KEY_SHIFT_2,
    EDITOR_KEY_SHIFT_3,
    EDITOR_KEY_SHIFT_4,
    EDITOR_KEY_SHIFT_5,
    EDITOR_KEY_SHIFT_6,
    EDITOR_KEY_SHIFT_7,
    EDITOR_KEY_SHIFT_8,
    EDITOR_KEY_SHIFT_9,

    EDITOR_KEY_SHIFT_A,
    EDITOR_KEY_SHIFT_B,
    EDITOR_KEY_SHIFT_C,
    EDITOR_KEY_SHIFT_D,
    EDITOR_KEY_SHIFT_E,
    EDITOR_KEY_SHIFT_F,
    EDITOR_KEY_SHIFT_G,
    EDITOR_KEY_SHIFT_H,
    EDITOR_KEY_SHIFT_I,
    EDITOR_KEY_SHIFT_J,
    EDITOR_KEY_SHIFT_K,
    EDITOR_KEY_SHIFT_L,
    EDITOR_KEY_SHIFT_M,
    EDITOR_KEY_SHIFT_N,
    EDITOR_KEY_SHIFT_O,
    EDITOR_KEY_SHIFT_P,
    EDITOR_KEY_SHIFT_Q,
    EDITOR_KEY_SHIFT_R,
    EDITOR_KEY_SHIFT_S,
    EDITOR_KEY_SHIFT_T,
    EDITOR_KEY_SHIFT_U,
    EDITOR_KEY_SHIFT_V,
    EDITOR_KEY_SHIFT_W,
    EDITOR_KEY_SHIFT_X,
    EDITOR_KEY_SHIFT_Y,
    EDITOR_KEY_SHIFT_Z,

    EDITOR_KEY_SHIFT_BACKSPACE,
    EDITOR_KEY_SHIFT_DELETE,
    EDITOR_KEY_SHIFT_ENTER,
    EDITOR_KEY_SHIFT_EQUALS,
    EDITOR_KEY_SHIFT_ESCAPE,

    EDITOR_KEY_SHIFT_ALT,
    EDITOR_KEY_SHIFT_CTRL,

    EDITOR_KEY_SHIFT_UP,
    EDITOR_KEY_SHIFT_DOWN,
    EDITOR_KEY_SHIFT_LEFT,
    EDITOR_KEY_SHIFT_RIGHT,

    EDITOR_KEY_SHIFT_MINUS,
    EDITOR_KEY_SHIFT_PLUS,

    EDITOR_KEY_SHIFT_MOUSE_LEFT,
    EDITOR_KEY_SHIFT_MOUSE_MIDDLE,
    EDITOR_KEY_SHIFT_MOUSE_RIGHT,

    EDITOR_KEY_SHIFT_SPACE,
} EditorInputKey;

typedef struct {
    EditorInputKey key;
    Vec2D mouse_position;
    bool pressed;
    bool shift;
    bool ctrl;
    bool alt;
    bool super;
} EditorInput;

Vec2D get_mouse_coordinates(void) {
    Position position = graphics_mouse_screen_position_get();

    return (Vec2D) {
        .x = position.x,
        .y = position.y,
    };
}

EditorInput sdl_event_to_editor_input(SDL_Event event)
{
    EditorInput input = {
        .key = EDITOR_KEY_NONE,
        .mouse_position = get_mouse_coordinates(),
        .pressed = false,
        .shift = false,
        .ctrl = false,
        .alt = false,
        .super = false,
    };

    /*
     * Keyboard input
     */
    if (/*event.type == SDL_EVENT_EDITOR_KEY_DOWN ||*/
        event.type == SDL_EVENT_KEY_UP) {

        input.pressed = (event.type == SDL_EVENT_KEY_DOWN);

        input.shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
        input.ctrl  = (event.key.mod & SDL_KMOD_CTRL)  != 0;
        input.alt   = (event.key.mod & SDL_KMOD_ALT)   != 0;
        input.super = (event.key.mod & SDL_KMOD_GUI)   != 0;

        if(input.shift) {
            switch (event.key.scancode) {
                case SDL_SCANCODE_0: input.key = EDITOR_KEY_0; break;
                case SDL_SCANCODE_1: input.key = EDITOR_KEY_1; break;
                case SDL_SCANCODE_2: input.key = EDITOR_KEY_2; break;
                case SDL_SCANCODE_3: input.key = EDITOR_KEY_3; break;
                case SDL_SCANCODE_4: input.key = EDITOR_KEY_4; break;
                case SDL_SCANCODE_5: input.key = EDITOR_KEY_5; break;
                case SDL_SCANCODE_6: input.key = EDITOR_KEY_6; break;
                case SDL_SCANCODE_7: input.key = EDITOR_KEY_7; break;
                case SDL_SCANCODE_8: input.key = EDITOR_KEY_8; break;
                case SDL_SCANCODE_9: input.key = EDITOR_KEY_9; break;

                case SDL_SCANCODE_A: input.key = EDITOR_KEY_SHIFT_A; break;
                case SDL_SCANCODE_B: input.key = EDITOR_KEY_SHIFT_B; break;
                case SDL_SCANCODE_C: input.key = EDITOR_KEY_SHIFT_C; break;
                case SDL_SCANCODE_D: input.key = EDITOR_KEY_SHIFT_D; break;
                case SDL_SCANCODE_E: input.key = EDITOR_KEY_SHIFT_E; break;
                case SDL_SCANCODE_F: input.key = EDITOR_KEY_SHIFT_F; break;
                case SDL_SCANCODE_G: input.key = EDITOR_KEY_SHIFT_G; break;
                case SDL_SCANCODE_H: input.key = EDITOR_KEY_SHIFT_H; break;
                case SDL_SCANCODE_I: input.key = EDITOR_KEY_SHIFT_I; break;
                case SDL_SCANCODE_J: input.key = EDITOR_KEY_SHIFT_J; break;
                case SDL_SCANCODE_K: input.key = EDITOR_KEY_SHIFT_K; break;
                case SDL_SCANCODE_L: input.key = EDITOR_KEY_SHIFT_L; break;
                case SDL_SCANCODE_M: input.key = EDITOR_KEY_SHIFT_M; break;
                case SDL_SCANCODE_N: input.key = EDITOR_KEY_SHIFT_N; break;
                case SDL_SCANCODE_O: input.key = EDITOR_KEY_SHIFT_O; break;
                case SDL_SCANCODE_P: input.key = EDITOR_KEY_SHIFT_P; break;
                case SDL_SCANCODE_Q: input.key = EDITOR_KEY_SHIFT_Q; break;
                case SDL_SCANCODE_R: input.key = EDITOR_KEY_SHIFT_R; break;
                case SDL_SCANCODE_S: input.key = EDITOR_KEY_SHIFT_S; break;
                case SDL_SCANCODE_T: input.key = EDITOR_KEY_SHIFT_T; break;
                case SDL_SCANCODE_U: input.key = EDITOR_KEY_SHIFT_U; break;
                case SDL_SCANCODE_V: input.key = EDITOR_KEY_SHIFT_V; break;
                case SDL_SCANCODE_W: input.key = EDITOR_KEY_SHIFT_W; break;
                case SDL_SCANCODE_X: input.key = EDITOR_KEY_SHIFT_X; break;
                case SDL_SCANCODE_Y: input.key = EDITOR_KEY_SHIFT_Y; break;
                case SDL_SCANCODE_Z: input.key = EDITOR_KEY_SHIFT_Z; break;

                case SDL_SCANCODE_BACKSPACE:
                    input.key = EDITOR_KEY_BACKSPACE;
                    break;

                case SDL_SCANCODE_DELETE:
                    input.key = EDITOR_KEY_DELETE;
                    break;

                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                    input.key = EDITOR_KEY_ENTER;
                    break;


                case SDL_SCANCODE_ESCAPE:
                    input.key = EDITOR_KEY_ESCAPE;
                    break;

                case SDL_SCANCODE_LALT:
                case SDL_SCANCODE_RALT:
                    input.key = EDITOR_KEY_ALT;
                    break;

                case SDL_SCANCODE_LCTRL:
                case SDL_SCANCODE_RCTRL:
                    input.key = EDITOR_KEY_CTRL;
                    break;

                case SDL_SCANCODE_LSHIFT:
                case SDL_SCANCODE_RSHIFT:
                    input.key = EDITOR_KEY_SHIFT;
                    break;

                case SDL_SCANCODE_UP:
                    input.key = EDITOR_KEY_SHIFT_UP;
                    break;

                case SDL_SCANCODE_DOWN:
                    input.key = EDITOR_KEY_SHIFT_DOWN;
                    break;

                case SDL_SCANCODE_LEFT:
                    input.key = EDITOR_KEY_SHIFT_LEFT;
                    break;

                case SDL_SCANCODE_RIGHT:
                    input.key = EDITOR_KEY_SHIFT_RIGHT;
                    break;

                case SDL_SCANCODE_MINUS:
                case SDL_SCANCODE_KP_MINUS:
                    input.key = EDITOR_KEY_MINUS;
                    break;

                case SDL_SCANCODE_KP_PLUS:
                    input.key = EDITOR_KEY_PLUS;
                    break;

                case SDL_SCANCODE_EQUALS:
                    input.key = EDITOR_KEY_EQUALS;
                    break;

                case SDL_SCANCODE_SPACE:
                    input.key = EDITOR_KEY_SHIFT_SPACE;
                    break;

                case SDL_SCANCODE_TAB:
                    input.key = EDITOR_KEY_TAB;
                    break;

                default:
                    input.key = EDITOR_KEY_NONE;
                    break;
            }

        }
        else {
            switch (event.key.scancode) {
                case SDL_SCANCODE_0: input.key = EDITOR_KEY_0; break;
                case SDL_SCANCODE_1: input.key = EDITOR_KEY_1; break;
                case SDL_SCANCODE_2: input.key = EDITOR_KEY_2; break;
                case SDL_SCANCODE_3: input.key = EDITOR_KEY_3; break;
                case SDL_SCANCODE_4: input.key = EDITOR_KEY_4; break;
                case SDL_SCANCODE_5: input.key = EDITOR_KEY_5; break;
                case SDL_SCANCODE_6: input.key = EDITOR_KEY_6; break;
                case SDL_SCANCODE_7: input.key = EDITOR_KEY_7; break;
                case SDL_SCANCODE_8: input.key = EDITOR_KEY_8; break;
                case SDL_SCANCODE_9: input.key = EDITOR_KEY_9; break;

                case SDL_SCANCODE_A: input.key = EDITOR_KEY_A; break;
                case SDL_SCANCODE_B: input.key = EDITOR_KEY_B; break;
                case SDL_SCANCODE_C: input.key = EDITOR_KEY_C; break;
                case SDL_SCANCODE_D: input.key = EDITOR_KEY_D; break;
                case SDL_SCANCODE_E: input.key = EDITOR_KEY_E; break;
                case SDL_SCANCODE_F: input.key = EDITOR_KEY_F; break;
                case SDL_SCANCODE_G: input.key = EDITOR_KEY_G; break;
                case SDL_SCANCODE_H: input.key = EDITOR_KEY_H; break;
                case SDL_SCANCODE_I: input.key = EDITOR_KEY_I; break;
                case SDL_SCANCODE_J: input.key = EDITOR_KEY_J; break;
                case SDL_SCANCODE_K: input.key = EDITOR_KEY_K; break;
                case SDL_SCANCODE_L: input.key = EDITOR_KEY_L; break;
                case SDL_SCANCODE_M: input.key = EDITOR_KEY_M; break;
                case SDL_SCANCODE_N: input.key = EDITOR_KEY_N; break;
                case SDL_SCANCODE_O: input.key = EDITOR_KEY_O; break;
                case SDL_SCANCODE_P: input.key = EDITOR_KEY_P; break;
                case SDL_SCANCODE_Q: input.key = EDITOR_KEY_Q; break;
                case SDL_SCANCODE_R: input.key = EDITOR_KEY_R; break;
                case SDL_SCANCODE_S: input.key = EDITOR_KEY_S; break;
                case SDL_SCANCODE_T: input.key = EDITOR_KEY_T; break;
                case SDL_SCANCODE_U: input.key = EDITOR_KEY_U; break;
                case SDL_SCANCODE_V: input.key = EDITOR_KEY_V; break;
                case SDL_SCANCODE_W: input.key = EDITOR_KEY_W; break;
                case SDL_SCANCODE_X: input.key = EDITOR_KEY_X; break;
                case SDL_SCANCODE_Y: input.key = EDITOR_KEY_Y; break;
                case SDL_SCANCODE_Z: input.key = EDITOR_KEY_Z; break;

                case SDL_SCANCODE_BACKSPACE:
                    input.key = EDITOR_KEY_BACKSPACE;
                    break;

                case SDL_SCANCODE_DELETE:
                    input.key = EDITOR_KEY_DELETE;
                    break;

                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                    input.key = EDITOR_KEY_ENTER;
                    break;

                case SDL_SCANCODE_EQUALS:
                    input.key = EDITOR_KEY_EQUALS;
                    break;

                case SDL_SCANCODE_ESCAPE:
                    input.key = EDITOR_KEY_ESCAPE;
                    break;

                case SDL_SCANCODE_LALT:
                case SDL_SCANCODE_RALT:
                    input.key = EDITOR_KEY_ALT;
                    break;

                case SDL_SCANCODE_LCTRL:
                case SDL_SCANCODE_RCTRL:
                    input.key = EDITOR_KEY_CTRL;
                    break;

                case SDL_SCANCODE_LSHIFT:
                case SDL_SCANCODE_RSHIFT:
                    input.key = EDITOR_KEY_SHIFT;
                    break;

                case SDL_SCANCODE_UP:
                    input.key = EDITOR_KEY_UP;
                    break;

                case SDL_SCANCODE_DOWN:
                    input.key = EDITOR_KEY_DOWN;
                    break;

                case SDL_SCANCODE_LEFT:
                    input.key = EDITOR_KEY_LEFT;
                    break;

                case SDL_SCANCODE_RIGHT:
                    input.key = EDITOR_KEY_RIGHT;
                    break;

                case SDL_SCANCODE_MINUS:
                case SDL_SCANCODE_KP_MINUS:
                    input.key = EDITOR_KEY_MINUS;
                    break;

                case SDL_SCANCODE_KP_PLUS:
                    input.key = EDITOR_KEY_PLUS;
                    break;

                case SDL_SCANCODE_SPACE:
                    input.key = EDITOR_KEY_SPACE;
                    break;

                case SDL_SCANCODE_TAB:
                    input.key = EDITOR_KEY_TAB;
                    break;

                default:
                    input.key = EDITOR_KEY_NONE;
                    break;
            }

        }

        return input;
    }

    /*
     * Mouse input
     */
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event.type == SDL_EVENT_MOUSE_BUTTON_UP) {

        input.pressed = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);

        switch (event.button.button) {
            case SDL_BUTTON_LEFT:
                input.key = EDITOR_KEY_MOUSE_LEFT;
                break;

            case SDL_BUTTON_MIDDLE:
                input.key = EDITOR_KEY_MOUSE_MIDDLE;
                break;

            case SDL_BUTTON_RIGHT:
                input.key = EDITOR_KEY_MOUSE_RIGHT;
                break;

            default:
                input.key = EDITOR_KEY_NONE;
                break;
        }

        /*
         * Mouse events don't contain keyboard modifier information,
         * so query the current modifier state.
         */
        SDL_Keymod modifiers = SDL_GetModState();

        input.shift = (modifiers & SDL_KMOD_SHIFT) != 0;
        input.ctrl  = (modifiers & SDL_KMOD_CTRL)  != 0;
        input.alt   = (modifiers & SDL_KMOD_ALT)   != 0;
        input.super = (modifiers & SDL_KMOD_GUI)   != 0;

        return input;
    }

    return input;
}

LevelEditorMode editor_input_to_mode(LevelEditorMode current_mode, EditorInput input) {

    //Check MODE DEDICATED EDITOR_KEYS
    switch(input.key) {
        case EDITOR_KEY_R:
            return EDITOR_ROTATE;
        case EDITOR_KEY_S:
            return EDITOR_SCALE;
        case EDITOR_KEY_M:
            return EDITOR_MOVE;
        case EDITOR_KEY_A:
            return EDITOR_SHAPE;
        case EDITOR_KEY_SPACE:
            return EDITOR_PAUSE_ENGINE;
        default:
            break;
    }

    //Check MODE dependent keys
    switch(current_mode) {
        case EDITOR_PAUSE_ENGINE:
            switch (input.key) {
                default:
                    return EDITOR_SELECT;
            }

        case EDITOR_SELECT:
            switch (input.key) {
                case EDITOR_KEY_M:
                    return EDITOR_MOVE;

                case EDITOR_KEY_R:
                    return EDITOR_ROTATE;

                case EDITOR_KEY_S:
                    return EDITOR_SCALE;

                case EDITOR_KEY_A:
                    return EDITOR_SHAPE;

                default:
                    return EDITOR_SELECT;
            }

        case EDITOR_MOVE:
            switch(input.key) {
                case EDITOR_KEY_ESCAPE:
                    return EDITOR_SELECT;
                case EDITOR_KEY_UP:
                    return EDITOR_MOVE_UP;
                case EDITOR_KEY_DOWN:
                    return EDITOR_MOVE_DOWN;
                case EDITOR_KEY_LEFT:
                    return EDITOR_MOVE_LEFT;
                case EDITOR_KEY_RIGHT:
                    return EDITOR_MOVE_RIGHT;

                case EDITOR_KEY_MOUSE_LEFT:
                    return EDITOR_MOVE_MOUSE;
                case EDITOR_KEY_R:
                    return EDITOR_ROTATE;
                case EDITOR_KEY_S:
                    return EDITOR_SCALE;
                case EDITOR_KEY_A:
                    return EDITOR_SHAPE;
                default:
                    return EDITOR_MOVE;
            }
        case EDITOR_MOVE_MOUSE:
            switch(input.key) {
                case EDITOR_KEY_ESCAPE:
                    return EDITOR_MOVE;
                default:
                    return EDITOR_MOVE_MOUSE;
            }
        case EDITOR_MOVE_UP:
            switch(input.key) {
                default:
                    return EDITOR_MOVE;
            }
        case EDITOR_MOVE_DOWN:
            switch(input.key) {
                default:
                    return EDITOR_MOVE;
            }
        case EDITOR_MOVE_LEFT:
            switch(input.key) {
                default:
                    return EDITOR_MOVE;
            }
        case EDITOR_MOVE_RIGHT:
            switch(input.key) {
                default:
                    return EDITOR_MOVE;
            }


        case EDITOR_ROTATE:
            switch(input.key) {
                case EDITOR_KEY_ESCAPE:
                    return EDITOR_SELECT;
                case EDITOR_KEY_LEFT:
                    return EDITOR_ROTATE_CCW;
                case EDITOR_KEY_RIGHT:
                    return EDITOR_ROTATE_CW;
                case EDITOR_KEY_SHIFT_UP:
                    return EDITOR_ROTATE_UP;
                case EDITOR_KEY_SHIFT_DOWN:
                    return EDITOR_ROTATE_DOWN;
                case EDITOR_KEY_SHIFT_RIGHT:
                    return EDITOR_ROTATE_RIGHT;
                case EDITOR_KEY_SHIFT_LEFT:
                    return EDITOR_ROTATE_LEFT;

                case EDITOR_KEY_M:
                    return EDITOR_MOVE;
                case EDITOR_KEY_R:
                    return EDITOR_ROTATE;
                case EDITOR_KEY_S:
                    return EDITOR_SCALE;
                case EDITOR_KEY_A:
                    return EDITOR_SHAPE;

                default:
                    return EDITOR_ROTATE;
            }
        case EDITOR_ROTATE_CW:
            switch(input.key) {
                default:
                    return EDITOR_ROTATE;
            }
        case EDITOR_ROTATE_CCW:
            switch(input.key) {
                default:
                    return EDITOR_ROTATE;
            }
        case EDITOR_ROTATE_UP:
            switch(input.key) {
                default:
                    return EDITOR_ROTATE;
            }
        case EDITOR_ROTATE_DOWN:
            switch(input.key) {
                default:
                    return EDITOR_ROTATE;
            }
        case EDITOR_ROTATE_LEFT:
            switch(input.key) {
                default:
                    return EDITOR_ROTATE;
            }
        case EDITOR_ROTATE_RIGHT:
            switch(input.key) {
                default:
                    return EDITOR_ROTATE;
            }

        case EDITOR_SCALE:
            switch(input.key) {
                case EDITOR_KEY_ESCAPE:
                    return EDITOR_SELECT;
                case EDITOR_KEY_UP:
                    return EDITOR_SCALE_Y_UP;
                case EDITOR_KEY_DOWN:
                    return EDITOR_SCALE_Y_UP;
                case EDITOR_KEY_LEFT:
                    return EDITOR_SCALE_X_UP;
                case EDITOR_KEY_RIGHT:
                    return EDITOR_SCALE_X_UP;
                case EDITOR_KEY_SHIFT_UP:
                    return EDITOR_SCALE_Y_DOWN;
                case EDITOR_KEY_SHIFT_DOWN:
                    return EDITOR_SCALE_Y_DOWN;
                case EDITOR_KEY_SHIFT_LEFT:
                    return EDITOR_SCALE_X_DOWN;
                case EDITOR_KEY_SHIFT_RIGHT:
                    return EDITOR_SCALE_X_DOWN;

                case EDITOR_KEY_L:
                    return EDITOR_SCALE_LOCK;

                case EDITOR_KEY_M:
                    return EDITOR_MOVE;
                case EDITOR_KEY_R:
                    return EDITOR_ROTATE;
                case EDITOR_KEY_S:
                    return EDITOR_SCALE;
                case EDITOR_KEY_A:
                    return EDITOR_SHAPE;

                default:
                    return EDITOR_SCALE;
            }
        case EDITOR_SCALE_X_UP:
            switch(input.key) {
                default:
                    return EDITOR_SCALE;
            }
        case EDITOR_SCALE_X_DOWN:
            switch(input.key) {
                default:
                    return EDITOR_SCALE;
            }
        case EDITOR_SCALE_Y_UP:
            switch(input.key) {
                default:
                    return EDITOR_SCALE;
            }
        case EDITOR_SCALE_Y_DOWN:
            switch(input.key) {
                default:
                    return EDITOR_SCALE;
            }
        case EDITOR_SCALE_LOCK:
            switch(input.key) {
                case EDITOR_KEY_ESCAPE:
                    return EDITOR_SCALE;
                case EDITOR_KEY_UP:
                    return EDITOR_SCALE_ALL_UP;
                case EDITOR_KEY_DOWN:
                    return EDITOR_SCALE_ALL_UP;
                case EDITOR_KEY_LEFT:
                    return EDITOR_SCALE_ALL_UP;
                case EDITOR_KEY_RIGHT:
                    return EDITOR_SCALE_ALL_UP;
                case EDITOR_KEY_SHIFT_UP:
                    return EDITOR_SCALE_ALL_DOWN;
                case EDITOR_KEY_SHIFT_DOWN:
                    return EDITOR_SCALE_ALL_DOWN;
                case EDITOR_KEY_SHIFT_LEFT:
                    return EDITOR_SCALE_ALL_DOWN;
                case EDITOR_KEY_SHIFT_RIGHT:
                    return EDITOR_SCALE_ALL_DOWN;

                case EDITOR_KEY_M:
                    return EDITOR_MOVE;
                case EDITOR_KEY_R:
                    return EDITOR_ROTATE;
                case EDITOR_KEY_S:
                    return EDITOR_SCALE;
                case EDITOR_KEY_A:
                    return EDITOR_SHAPE;

                default:
                    return EDITOR_SCALE_LOCK;

            }
        case EDITOR_SCALE_ALL_UP:
            switch(input.key) {
                default:
                    return EDITOR_SCALE_LOCK;
            }
        case EDITOR_SCALE_ALL_DOWN:
            switch(input.key) {
                default:
                    return EDITOR_SCALE_LOCK;
            }

        case EDITOR_SHAPE:
            switch(input.key) {
                case EDITOR_KEY_ESCAPE:
                    return EDITOR_SELECT;
                case EDITOR_KEY_UP:
                    return EDITOR_SHAPE_ADD_VERTEX;
                case EDITOR_KEY_DOWN:
                    return EDITOR_SHAPE_REMOVE_VERTEX;
                default:
                    return EDITOR_SHAPE;
            }
        case EDITOR_SHAPE_ADD_VERTEX:
            switch(input.key) {
                default:
                    return EDITOR_SHAPE;
            }
        case EDITOR_SHAPE_REMOVE_VERTEX:
            switch(input.key) {
                default:
                    return EDITOR_SHAPE;
            }


        default:
            return EDITOR_SELECT;
    }
    return current_mode;
}



EngineResult editor_select(EditorInput input) {
    if(input.pressed) {
        EntityIndex selection_index;

        if(!entity_index_get(selection, &selection_index)) {
            return error_result_value(true);
        }
        //Do something here??
        console_write(LOG_APP, "Pressed\n");
        for(EntityIndex i = 0; i < MAX_ENTITIES; i += 1) {
            if(i == selection_index) {
                continue;
            }
            if(collision_reports[selection_index].collisions[i]) {
                EntityResult selected_result = entity_from_index(i);
                if(selected_result.kind == ERROR_RESULT_VALUE) {
                    selected_entity = selected_result.result.value;
                    console_write(LOG_APP, "Selected Entity: %d\n", i);
                }
            }
        }
    }
    return error_result_value(true);
}

EngineResult editor_move_mouse(EditorInput input) {
    EntityIndex selection_index;

    if(!entity_index_get(selection, &selection_index)) {
        return error_result_value(true);
    }
    return physics_position_set(selected_entity, positions[selection_index]);
}

//Rotation
EngineResult editor_rotate_cw(EditorInput input) {
    EntityIndex selected_index;
    if(entity_index_get(selected_entity, &selected_index)) {
        return physics_orientation_set(selected_entity, orientations[selected_index] - 1*2*PI_F/360);
    }
    return error_result_value(true);
}
EngineResult editor_rotate_ccw(EditorInput input) {
    EntityIndex selected_index;
    if(entity_index_get(selected_entity, &selected_index)) {
        return physics_orientation_set(selected_entity, orientations[selected_index] + 1*2*PI_F/360);
    }
    return error_result_value(true);
}
EngineResult editor_rotate_up(EditorInput input) {
    EntityIndex selected_index;
    if(entity_index_get(selected_entity, &selected_index)) {
        return physics_orientation_set(selected_entity, orientations[selected_index] - 180*2*PI_F/360);
    }
    return error_result_value(true);
}
EngineResult editor_rotate_down(EditorInput input) {
    EntityIndex selected_index;
    if(entity_index_get(selected_entity, &selected_index)) {
        return physics_orientation_set(selected_entity, orientations[selected_index] + 180*2*PI_F/360);
    }
    return error_result_value(true);
}
EngineResult editor_rotate_right(EditorInput input) {
    EntityIndex selected_index;
    if(entity_index_get(selected_entity, &selected_index)) {
        return physics_orientation_set(selected_entity, orientations[selected_index] - 90*2*PI_F/360);
    }
    return error_result_value(true);
}
EngineResult editor_rotate_left(EditorInput input) {
    EntityIndex selected_index;
    if(entity_index_get(selected_entity, &selected_index)) {
        return physics_orientation_set(selected_entity, orientations[selected_index] + 90*2*PI_F/360);
    }
    return error_result_value(true);
}

EngineResult editor_scale_all_up(EditorInput input) {
    EntityIndex selected_index;
    if(!entity_index_get(selected_entity, &selected_index)) { return error_result_value(true); }
    Shape shape = math_scale_shape(hit_boxes[selected_index], 1 + SCALE_INCREMENT);
    EngineResult result = physics_hitbox_set(selected_entity, shape);
    if(result.kind == ERROR_RESULT_ERROR) { return result; }
    graphics_scale_textures(selected_entity, (Scale){1 + SCALE_INCREMENT,1 + SCALE_INCREMENT});
    return error_result_value(true);
}
EngineResult editor_scale_all_down(EditorInput input) {
    EntityIndex selected_index;
    if(!entity_index_get(selected_entity, &selected_index)) { return error_result_value(true); }
    Shape shape = math_scale_shape(hit_boxes[selected_index], 1 - SCALE_INCREMENT);
    EngineResult result = physics_hitbox_set(selected_entity, shape);
    if(result.kind == ERROR_RESULT_ERROR) { return result; }
    graphics_scale_textures(selected_entity, (Scale){1 - SCALE_INCREMENT,1 - SCALE_INCREMENT});
    return error_result_value(true);
}
EngineResult editor_scale_x_up(EditorInput input) {
    EntityIndex selected_index;
    if(!entity_index_get(selected_entity, &selected_index)) { return error_result_value(true); }
    Shape shape = math_scale_shape_x(hit_boxes[selected_index], 1 + SCALE_INCREMENT);
    EngineResult result = physics_hitbox_set(selected_entity, shape);
    if(result.kind == ERROR_RESULT_ERROR) { return result; }
    graphics_scale_textures(selected_entity, (Scale){1 + SCALE_INCREMENT,1});
    return error_result_value(true);
}
EngineResult editor_scale_x_down(EditorInput input) {
    EntityIndex selected_index;
    if(!entity_index_get(selected_entity, &selected_index)) { return error_result_value(true); }
    Shape shape = math_scale_shape_x(hit_boxes[selected_index], 1 - SCALE_INCREMENT);
    EngineResult result = physics_hitbox_set(selected_entity, shape);
    if(result.kind == ERROR_RESULT_ERROR) { return result; }
    graphics_scale_textures(selected_entity, (Scale){1 - SCALE_INCREMENT,1});
    return error_result_value(true);
}
EngineResult editor_scale_y_up(EditorInput input) {
    EntityIndex selected_index;
    if(!entity_index_get(selected_entity, &selected_index)) { return error_result_value(true); }
    Shape shape = math_scale_shape_y(hit_boxes[selected_index], 1 + SCALE_INCREMENT);
    EngineResult result = physics_hitbox_set(selected_entity, shape);
    if(result.kind == ERROR_RESULT_ERROR) { return result; }
    graphics_scale_textures(selected_entity, (Scale){1,1 + SCALE_INCREMENT});
    return error_result_value(true);
}
EngineResult editor_scale_y_down(EditorInput input) {
    EntityIndex selected_index;
    if(!entity_index_get(selected_entity, &selected_index)) { return error_result_value(true); }
    Shape shape = math_scale_shape_y(hit_boxes[selected_index], 1 - SCALE_INCREMENT);
    EngineResult result = physics_hitbox_set(selected_entity, shape);
    if(result.kind == ERROR_RESULT_ERROR) { return result; }
    graphics_scale_textures(selected_entity, (Scale){1,1 - SCALE_INCREMENT});
    return error_result_value(true);
}

EngineResult editor_shape_add_vertex(EditorInput input) {
    EntityIndex selected_index;
    if(!entity_index_get(selected_entity, &selected_index)) { return error_result_value(true); }
    Shape new_shape = math_add_vertex(hit_boxes[selected_index]);
    return physics_hitbox_set(selected_entity, new_shape);
}
EngineResult editor_shape_remove_vertex(EditorInput input) {
    EntityIndex selected_index;
    if(!entity_index_get(selected_entity, &selected_index)) { return error_result_value(true); }
    Shape new_shape = math_delete_vertex(hit_boxes[selected_index]);
    return physics_hitbox_set(selected_entity, new_shape);
}
EngineResult editor_move_up(EditorInput input) {
    EntityIndex selected_index;
    if(entity_index_get(selected_entity, &selected_index)) {
        return physics_position_set(selected_entity, (Position){positions[selected_index].x, positions[selected_index].y + MOVE_INCREMENT});
    }
    return error_result_value(true);
}
EngineResult editor_move_down(EditorInput input) {
    EntityIndex selected_index;
    if(entity_index_get(selected_entity, &selected_index)) {
        return physics_position_set(selected_entity, (Position){positions[selected_index].x, positions[selected_index].y - MOVE_INCREMENT});
    }
    return error_result_value(true);
}
EngineResult editor_move_left(EditorInput input) {
    EntityIndex selected_index;
    if(entity_index_get(selected_entity, &selected_index)) {
        return physics_position_set(selected_entity, (Position){positions[selected_index].x - MOVE_INCREMENT, positions[selected_index].y});
    }
    return error_result_value(true);
}
EngineResult editor_move_right(EditorInput input) {
    EntityIndex selected_index;
    if(entity_index_get(selected_entity, &selected_index)) {
        return physics_position_set(selected_entity, (Position){positions[selected_index].x + MOVE_INCREMENT, positions[selected_index].y});
    }
    return error_result_value(true);
}

EngineResult editor_pause_engine(EditorInput input) {
    if(engine_paused_is()) {
        engine_resume();
    }
    else {
        engine_pause();
    }
    return error_result_value(true);
}

EngineResult resolve_mode(LevelEditorMode mode, EditorInput input) {
    switch(mode) {
        case EDITOR_NONE:
            return error_result_value(true);
        case EDITOR_PAUSE_ENGINE:
            return editor_pause_engine(input);
        case EDITOR_SELECT:
            return editor_select(input);
        case EDITOR_MOVE_MOUSE: {
            EngineResult result = editor_move_mouse(input);
            if(result.kind == ERROR_RESULT_ERROR) {
                return result;
            }
        }
        case EDITOR_MOVE_UP:
            return editor_move_up(input);
        case EDITOR_MOVE_DOWN:
            return editor_move_down(input);
        case EDITOR_MOVE_LEFT:
            return editor_move_left(input);
        case EDITOR_MOVE_RIGHT:
            return editor_move_right(input);
        case EDITOR_ROTATE_CW:
            return editor_rotate_cw(input);
        case EDITOR_ROTATE_CCW:
            return editor_rotate_ccw(input);
        case EDITOR_ROTATE_UP:
            return editor_rotate_up(input);
        case EDITOR_ROTATE_DOWN:
            return editor_rotate_down(input);
        case EDITOR_ROTATE_LEFT:
            return editor_rotate_left(input);
        case EDITOR_ROTATE_RIGHT:
            return editor_rotate_right(input);
        case EDITOR_SCALE_ALL_UP:
            return editor_scale_all_up(input);
        case EDITOR_SCALE_ALL_DOWN:
            return editor_scale_all_down(input);
        case EDITOR_SCALE_X_UP:
            return editor_scale_x_up(input);
        case EDITOR_SCALE_X_DOWN:
            return editor_scale_x_down(input);
        case EDITOR_SCALE_Y_UP:
            return editor_scale_y_up(input);
        case EDITOR_SCALE_Y_DOWN:
            return editor_scale_y_down(input);
        case EDITOR_SHAPE_ADD_VERTEX:
            return editor_shape_add_vertex(input);
        case EDITOR_SHAPE_REMOVE_VERTEX:
            return editor_shape_remove_vertex(input);
        default:
            return error_result_value(true);
    }
    return error_result_value(true);
}

EngineResult bind_selection_to_mouse(void) {
    return physics_position_set(selection, graphics_screen_to_world(get_mouse_coordinates()));
}

void print_mode(LevelEditorMode mode) {
    switch(mode) {
        case EDITOR_NONE:
            console_write(LOG_APP, "Mode is NONE\n");
            break;
        case EDITOR_SELECT:
            console_write(LOG_APP, "Mode is SELECT\n");
            break;
        case EDITOR_MOVE:
            console_write(LOG_APP, "Mode is MOVE\n");
            break;
        case EDITOR_MOVE_MOUSE:
            console_write(LOG_APP, "Mode is MOVE_MOUSE\n");
            break;
        case EDITOR_MOVE_UP:
            console_write(LOG_APP, "Mode is MOVE_UP\n");
            break;
        case EDITOR_MOVE_DOWN:
            console_write(LOG_APP, "Mode is MOVE_DOWN\n");
            break;
        case EDITOR_MOVE_LEFT:
            console_write(LOG_APP, "Mode is MOVE_LEFT\n");
            break;
        case EDITOR_MOVE_RIGHT:
            console_write(LOG_APP, "Mode is MOVE_RIGHT\n");
            break;
        case EDITOR_ROTATE:
            console_write(LOG_APP, "Mode is ROTATE\n");
            break;
        case EDITOR_ROTATE_CW:
            console_write(LOG_APP, "Mode is ROTATE_CW\n");
            break;
        case EDITOR_ROTATE_CCW:
            console_write(LOG_APP, "Mode is ROTATE_CCW\n");
            break;
        case EDITOR_SCALE:
            console_write(LOG_APP, "Mode is SCALE\n");
            break;
        case EDITOR_SCALE_X_UP:
            console_write(LOG_APP, "Mode is SCALE_X_UP\n");
            break;
        case EDITOR_SCALE_Y_UP:
            console_write(LOG_APP, "Mode is SCALE_Y_UP\n");
            break;
        case EDITOR_SCALE_X_DOWN:
            console_write(LOG_APP, "Mode is SCALE_X_DOWN\n");
            break;
        case EDITOR_SCALE_Y_DOWN:
            console_write(LOG_APP, "Mode is SCALE_Y_DOWN\n");
            break;
        case EDITOR_SCALE_LOCK:
            console_write(LOG_APP, "Mode is SCALE_LOCK\n");
            break;
        case EDITOR_SCALE_ALL_DOWN:
            console_write(LOG_APP, "Mode is SCALE_ALL_DOWN\n");
            break;
        case EDITOR_SCALE_ALL_UP:
            console_write(LOG_APP, "Mode is SCALE_ALL_UP\n");
            break;
        case EDITOR_ROTATE_UP:
            console_write(LOG_APP, "Mode is ROTATE_UP\n");
            break;
        case EDITOR_ROTATE_DOWN:
            console_write(LOG_APP, "Mode is ROTATE_DOWN\n");
            break;
        case EDITOR_ROTATE_LEFT:
            console_write(LOG_APP, "Mode is ROTATE_LEFT\n");
            break;
        case EDITOR_ROTATE_RIGHT:
            console_write(LOG_APP, "Mode is ROTATE_RIGHT\n");
            break;
        case EDITOR_PAUSE_ENGINE:
            console_write(LOG_APP, "Mode is PAUSE_ENGINE\n");
            break;
        case EDITOR_SHAPE:
            console_write(LOG_APP, "Mode is SHAPE\n");
            break;
        case EDITOR_SHAPE_ADD_VERTEX:
            console_write(LOG_APP, "Mode is SHAPE_ADD_VERTEX\n");
            break;
        case EDITOR_SHAPE_REMOVE_VERTEX:
            console_write(LOG_APP, "Mode is SHAPE_REMOVE_VERTEX\n");
            break;
        default:
            console_write(LOG_APP, "Mode is not working\n");
            break;

    }
}

void print_editor_controls(void)
{
    console_write(LOG_APP,
        "\n"
        "========================================\n"
        "          LEVEL EDITOR CONTROLS\n"
        "========================================\n"
        "\n"
        "GLOBAL MODES\n"
        "  M                 Move mode\n"
        "  R                 Rotate mode\n"
        "  S                 Scale mode\n"
        "  A                 Shape mode\n"
        "  Space             Pause / resume engine\n"
        "  Escape            Return to select mode\n"
        "\n"
        "SELECT MODE\n"
        "  Left Mouse        Select entity under cursor\n"
        "\n"
        "MOVE MODE\n"
        "  Left Mouse        Attach selected entity to mouse\n"
        "  Up Arrow          Move entity down by %d units\n"
        "  Down Arrow        Move entity up by %d units\n"
        "  Left Arrow        Move entity left by %d units\n"
        "  Right Arrow       Move entity right by %d units\n"
        "  Escape            Stop mouse movement / return\n"
        "\n"
        "ROTATE MODE\n"
        "  Left Arrow        Rotate counter-clockwise by 1 degree\n"
        "  Right Arrow       Rotate clockwise by 1 degree\n"
        "  Shift + Up        Rotate by +180 degrees\n"
        "  Shift + Down      Rotate by -180 degrees\n"
        "  Shift + Left      Rotate by -90 degrees\n"
        "  Shift + Right     Rotate by +90 degrees\n"
        "\n"
        "SCALE MODE\n"
        "  Up / Down         Increase Y scale by %.2f\n"
        "  Left / Right      Increase X scale by %.2f\n"
        "  Shift + Up/Down   Decrease Y scale by %.2f\n"
        "  Shift + Left/Right Decrease X scale by %.2f\n"
        "  L                 Toggle uniform scale mode\n"
        "\n"
        "UNIFORM SCALE MODE\n"
        "  Any Arrow         Increase total scale by %.2f\n"
        "  Shift + Arrow     Decrease total scale by %.2f\n"
        "  Escape            Return to normal scale mode\n"
        "\n"
        "SHAPE MODE\n"
        "  Up Arrow          Add a vertex\n"
        "  Down Arrow        Remove a vertex\n"
        "\n"
        "========================================\n"
        "\n",
        MOVE_INCREMENT,
        MOVE_INCREMENT,
        MOVE_INCREMENT,
        MOVE_INCREMENT,
        SCALE_INCREMENT,
        SCALE_INCREMENT,
        SCALE_INCREMENT,
        SCALE_INCREMENT,
        SCALE_INCREMENT,
        SCALE_INCREMENT
    );
}


EngineResult level_editor_init(void) {
    EntityResult selection_result = entity_add();
    EngineResult result;

    if(selection_result.kind == ERROR_RESULT_ERROR) {
        return error_result_error(selection_result.result.error);
    }
    selection = selection_result.result.value;
    result = physics_static_set(selection);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = physics_position_set(selection, (Vec2D){80, 390});
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    Shape selection_hit_box = math_create_circle(0.1, 5);
    result = physics_hitbox_set(selection, selection_hit_box);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_delete_components(selection, COLLISION);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    print_editor_controls();
    return error_result_value(true);
}

EngineResult level_editor_update(void) {
        SDL_Event event = engine_poll_event();
        EditorInput input = sdl_event_to_editor_input(event);
        EngineResult result;

        prev_mode = current_mode;
        current_mode = editor_input_to_mode(current_mode, input);
        //if(prev_mode != current_mode) {
        //    print_mode(current_mode);
        //}
        result = bind_selection_to_mouse();
        if(result.kind == ERROR_RESULT_ERROR) {
            return result;
        }
        return resolve_mode(current_mode, input);

}
