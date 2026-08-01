#include "rohr.h"
#include "example_runtime.h"
#include <stdio.h>

const Color background_color = (Color){255,255,255,255};
const Time demo_duration_seconds = 30.0;
const Mass large_fly_mass = 50.0f;
const float large_fly_control_acceleration = 240.0f;
const Torque large_fly_control_torque = 2000000.0f;

#define PRINT_ENGINE_ERROR(engine_result) \
    fprintf(stderr, "%s\n", rohr_error_default_message((engine_result).result.error))

int main(void) {
    if(!example_use_executable_directory()) return 1;
    KeyboardState keyboard = {0};

    {
        EngineResult init_result = rohr_engine_init();
        if(rohr_error_check(init_result)) {
            PRINT_ENGINE_ERROR(init_result);
            return 1;
        }
    }
    if(rohr_error_check(rohr_engine_time_per_tick_set(1.0 / 120.0))) return 1;
    {
        EngineResult graphics_result = rohr_graphics_start();
        if(rohr_error_check(graphics_result)) {
            PRINT_ENGINE_ERROR(graphics_result);
            rohr_engine_shutdown();
            return 1;
        }
    }

    EngineResult load_result = rohr_game_state_load_file(
        "assets/flies-in-pit/game.json"
    );
    if(rohr_error_check(load_result)) {
        PRINT_ENGINE_ERROR(load_result);
        goto fail;
    }

    EntityResult wall_1_result = rohr_entity_by_name_get("wall_1");
    if(rohr_error_check(wall_1_result)) {
        PRINT_ENGINE_ERROR(wall_1_result);
        goto fail;
    }
    Entity wall_1 = wall_1_result.result.value;
    EntityResult wall_2_result = rohr_entity_by_name_get("wall_2");
    if(rohr_error_check(wall_2_result)) {
        PRINT_ENGINE_ERROR(wall_2_result);
        goto fail;
    }
    Entity wall_2 = wall_2_result.result.value;
    EntityResult wall_3_result = rohr_entity_by_name_get("wall_3");
    if(rohr_error_check(wall_3_result)) {
        PRINT_ENGINE_ERROR(wall_3_result);
        goto fail;
    }
    Entity wall_3 = wall_3_result.result.value;
    EntityResult large_fly_result = rohr_entity_by_name_get("large_fly");
    if(rohr_error_check(large_fly_result)) {
        PRINT_ENGINE_ERROR(large_fly_result);
        goto fail;
    }
    Entity large_fly = large_fly_result.result.value;
    CameraAttachmentResult attachment_result = rohr_graphics_camera_attachment_get();
    if(rohr_error_check(attachment_result)
            || attachment_result.result.value.entity != large_fly
            || !attachment_result.result.value.follow_position
            || attachment_result.result.value.follow_orientation) {
        fprintf(stderr, "Camera is not attached to large_fly\n");
        goto fail;
    }

    rohr_engine_reset_clock();
    //Game Loop
    rohr_graphics_recording_start("flies_in_pit_recording.mp4",60);
    bool phase_1 = false;
    bool phase_2 = false;
    bool phase_3 = false;
    while (rohr_engine_time_get() < demo_duration_seconds) {
        rohr_system_clean_entities_past_lifetime();
        SDL_Event event = rohr_engine_poll_event();
        if(event.type == SDL_EVENT_QUIT) {
            break;
        }
        KeyboardEvent key_event = rohr_controller_capture_keyboard_event(&event);
        rohr_controller_update_key_states(&keyboard);
        rohr_controller_add_key_event(&keyboard, key_event);
        if(!phase_1 && rohr_engine_time_get() > 3) {
            phase_1 = true;
        }
        if(!phase_2 && rohr_engine_time_get() > 5) {
            phase_2 = true;
        }
        if(!phase_3 && rohr_engine_time_get() > 7) {
            phase_3 = true;
        }

        //Game Code
        Vec2D move_axis = rohr_controller_wasd_axis_get(&keyboard);
        Vec2D turn_axis = rohr_controller_axis_from_keycodes(&keyboard, SDLK_UNKNOWN, SDLK_LEFT, SDLK_UNKNOWN, SDLK_RIGHT);
        if(move_axis.x != 0.0f || move_axis.y != 0.0f) {
            EngineResult force_result = rohr_physics_apply_force_for_one_tick(large_fly, (Force){
                .x = move_axis.x * large_fly_mass * large_fly_control_acceleration,
                .y = move_axis.y * large_fly_mass * large_fly_control_acceleration
            });
            if(rohr_error_check(force_result)) {
                PRINT_ENGINE_ERROR(force_result);
                goto fail;
            }
        }
        if(turn_axis.x != 0.0f) {
            EngineResult torque_result = rohr_physics_apply_torque_for_one_tick(large_fly, -turn_axis.x * large_fly_control_torque);
            if(rohr_error_check(torque_result)) {
                PRINT_ENGINE_ERROR(torque_result);
                goto fail;
            }
        }

        //physics
        Tick ticks_advanced = rohr_engine_update_tick();
        rohr_physics_update(ticks_advanced);

        //render
        rohr_graphics_draw_background(background_color);
        rohr_graphics_draw_hit_box(wall_1, GRAPHICS_FILLED);
        rohr_graphics_draw_hit_box(wall_2, GRAPHICS_FILLED);
        rohr_graphics_draw_hit_box(wall_3, GRAPHICS_FILLED);
        rohr_graphics_update_sprite_frames(rohr_engine_tick_get(), rohr_engine_time_get());
        rohr_graphics_draw_animated_sprites();
        if(phase_1) {
            rohr_graphics_draw_hit_boxes();
        }
        if(phase_2) {
            rohr_graphics_draw_particles();
        }
        if(phase_3) {
            rohr_graphics_draw_grid();
        }
        rohr_graphics_draw_local_origins();
        rohr_graphics_show();

    }
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;

fail:
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
