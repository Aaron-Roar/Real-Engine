#include "rohr.h"
#include "example_runtime.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "elderfly_descriptors.h"

const Color background_color = {255,255,255,255};
AnimationAsset animation_elderfly = {0};
AnimatedSprite sprite_elderfly = {0};
const float camera_move_speed = 100.0f;
const float camera_turn_speed = PI_F * 0.5f;

#define PRINT_ENGINE_ERROR(engine_result) \
    fprintf(stderr, "%s\n", rohr_error_default_message_get((engine_result).result.error))

int main(void) {
    UIPhysicsDebugPanel debug_panel = {0};
    if(!example_use_executable_directory()) return 1;
    {
        EngineResult init_result = rohr_engine_init();
        if(rohr_error_check(init_result)) {
            PRINT_ENGINE_ERROR(init_result);
            return 1;
        }
    }
    KeyboardState keyboard = {0};
    MouseState mouse = {0};
    //rohr_level_editor_init();
    {
        EngineResult graphics_result = rohr_graphics_start();
        if(rohr_error_check(graphics_result)) {
            PRINT_ENGINE_ERROR(graphics_result);
            rohr_engine_shutdown();
            return 1;
        }
    }

    if(rohr_error_check(rohr_ui_physics_debug_panel_init(&debug_panel,
            (FontDescriptor){"assets/debug/JetBrainsMono-BoldItalic.ttf", 11.0f}))) goto fail;
    EntityResult water_smash_result = rohr_entity_add();
    if(rohr_error_check(water_smash_result)) {
        PRINT_ENGINE_ERROR(water_smash_result);
        goto fail;
    }
    Entity water_smash = water_smash_result.result.value;
    rohr_physics_position_set(water_smash, (Position){.x = 0, .y = 0});
    rohr_physics_orientation_set(water_smash, 0);
    rohr_physics_mass_set(water_smash, 50);
    rohr_physics_velocity_set(water_smash, (Velocity){0, 0});
    rohr_physics_restitution_set(water_smash, 0.1);
    Shape shape4 = rohr_math_square_create(150, 220);
    rohr_physics_hitbox_set(water_smash, shape4);
    rohr_physics_friction_set(water_smash, 0.4);
    rohr_physics_dynamic_set(water_smash);
    EngineResult camera_result = rohr_graphics_camera_attach(
            water_smash,
            (Vec2D){.x = 0.0f, .y = 100.0f},
            0.0f);
    if(rohr_error_check(camera_result)) {
        PRINT_ENGINE_ERROR(camera_result);
        goto fail;
    }
    AnimationAssetResult animation_result = rohr_graphics_animation_load(elderfly_fly);
    if(rohr_error_check(animation_result)) {
        PRINT_ENGINE_ERROR(animation_result);
        goto fail;
    }
    animation_elderfly = animation_result.result.value;
    sprite_elderfly = rohr_graphics_animated_sprite_create(animation_elderfly, (Scale){10,10});
    rohr_graphics_animated_sprite_add(water_smash, sprite_elderfly);

    rohr_engine_clock_reset();
    //Game Loop
    //rohr_graphics_recording_start("examples/engine-core/view-port/recording.mp4",60);
    while(true) {
        //rohr_level_editor_update(renderer);

        //physics
        Tick ticks_advanced = rohr_system_tick_update();
        Time tick_time = rohr_engine_time_per_tick_get() * (Time)ticks_advanced;
        rohr_physics_update(ticks_advanced);

        //render
        rohr_graphics_background_draw(background_color);
        rohr_graphics_sprite_frames_update(rohr_engine_tick_get(), rohr_engine_time_get());
        rohr_graphics_animated_sprites_draw();
        rohr_graphics_aabb_tree_debug_set(true);
        rohr_graphics_aabb_tree_draw();
        rohr_ui_physics_debug_panel_draw(&debug_panel);
        rohr_graphics_show();

        SDL_Event sdl_event = rohr_engine_event_poll();
        KeyboardEvent key_event = rohr_controller_keyboard_event_capture(&sdl_event);
        MouseEvent mouse_event = rohr_controller_mouse_event_capture(&sdl_event);

        rohr_controller_key_states_update(&keyboard);
        rohr_controller_key_event_add(&keyboard, key_event);
        if(sdl_event.type == SDL_EVENT_QUIT ||
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) break;
        Vec2D move_axis = rohr_controller_wasd_axis_get(&keyboard);
        Vec2D camera_move_axis = rohr_controller_axis_from_keycodes_get(
            &keyboard,
            SDLK_I,
            SDLK_J,
            SDLK_K,
            SDLK_L
        );
        Vec2D camera_turn_axis = rohr_controller_axis_from_keycodes_get(
            &keyboard,
            SDLK_UNKNOWN,
            SDLK_Q,
            SDLK_UNKNOWN,
            SDLK_E
        );
        rohr_physics_velocity_set(water_smash, (Velocity){
            .x = move_axis.x * 100.0f,
            .y = move_axis.y * 100.0f
        });
        rohr_graphics_camera_move((Vec2D){
            .x = camera_move_axis.x * camera_move_speed * tick_time,
            .y = camera_move_axis.y * camera_move_speed * tick_time
        });
        rohr_graphics_camera_rotate(
            camera_turn_axis.x * camera_turn_speed * tick_time
        );

        rohr_controller_mouse_states_update(&mouse);
        rohr_controller_mouse_event_add(&mouse, mouse_event);
        if(mouse.button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_DOWN) {
            rohr_physics_position_set(
                water_smash,
                rohr_controller_mouse_world_position_get(&mouse)
            );
        }
        if(mouse.button_states[MOUSE_BUTTON_RIGHT] == MOUSE_BUTTON_STATE_DOWN) {
            EntityIndexResult index_result = rohr_entity_index_get(water_smash);
            if(!rohr_error_check(index_result)) {
                rohr_physics_orientation_set(water_smash, orientations[index_result.result.value] + 10*(2*PI_F/360));
            }
        }


    }
    rohr_graphics_end();
    rohr_ui_physics_debug_panel_destroy(&debug_panel);
    rohr_engine_shutdown();
    return 0;

fail:
    rohr_ui_physics_debug_panel_destroy(&debug_panel);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
