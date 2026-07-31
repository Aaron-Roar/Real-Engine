#include "rohr.h"
#include <stdio.h>

const Color background_color = (Color){255,255,255,255};
const Time demo_duration_seconds = 30.0;
const Mass large_fly_mass = 50.0f;
const float large_fly_control_acceleration = 240.0f;
const Torque large_fly_control_torque = 2000000.0f;

#define PRINT_ENGINE_ERROR(engine_result) \
    fprintf(stderr, "%s\n", rohr_error_default_message((engine_result).result.error))

int main(void) {
    EngineResult result;
    EntityResult entity_result;
    KeyboardState keyboard = {0};

    if(rohr_error_check(result = rohr_engine_init())) {
        PRINT_ENGINE_ERROR(result);
        return 1;
    }
    rohr_engine_set_dt(1/(float)120);
    if(rohr_error_check(result = rohr_graphics_start())) {
        PRINT_ENGINE_ERROR(result);
        rohr_engine_shutdown();
        return 1;
    }

    if(rohr_error_check(result = rohr_game_state_load_file(
            "examples/flies-in-pit/flies_in_pit.json"))) {
        PRINT_ENGINE_ERROR(result);
        goto fail;
    }

    if(rohr_error_check(entity_result = rohr_entity_find_by_name("wall_1"))) {
        PRINT_ENGINE_ERROR(entity_result);
        goto fail;
    }
    Entity wall_1 = entity_result.result.value;
    if(rohr_error_check(entity_result = rohr_entity_find_by_name("wall_2"))) {
        PRINT_ENGINE_ERROR(entity_result);
        goto fail;
    }
    Entity wall_2 = entity_result.result.value;
    if(rohr_error_check(entity_result = rohr_entity_find_by_name("wall_3"))) {
        PRINT_ENGINE_ERROR(entity_result);
        goto fail;
    }
    Entity wall_3 = entity_result.result.value;
    if(rohr_error_check(entity_result = rohr_entity_find_by_name("large_fly"))) {
        PRINT_ENGINE_ERROR(entity_result);
        goto fail;
    }
    Entity large_fly = entity_result.result.value;
    CameraAttachment camera_attachment;
    if(!rohr_graphics_get_camera_attachment(&camera_attachment)
            || camera_attachment.entity != large_fly
            || !camera_attachment.follow_position
            || camera_attachment.follow_orientation) {
        fprintf(stderr, "Camera is not attached to large_fly\n");
        goto fail;
    }

    rohr_engine_reset_clock();
    //Game Loop
    rohr_graphics_recording_start("examples/flies-in-pit/recording.mp4",60);
    bool phase_1 = false;
    bool phase_2 = false;
    bool phase_3 = false;
    while (rohr_engine_get_time() < demo_duration_seconds) {
        rohr_system_clean_entities_past_lifetime();
        SDL_Event event = rohr_engine_poll_event();
        if(event.type == SDL_EVENT_QUIT) {
            break;
        }
        KeyboardEvent key_event = rohr_controller_capture_keyboard_event(&event);
        rohr_controller_update_key_states(&keyboard);
        rohr_controller_add_key_event(&keyboard, key_event);
        if(!phase_1 && rohr_engine_get_time() > 3) {
            phase_1 = true;
        }
        if(!phase_2 && rohr_engine_get_time() > 5) {
            phase_2 = true;
        }
        if(!phase_3 && rohr_engine_get_time() > 7) {
            phase_3 = true;
        }

        //Game Code
        Vec2D move_axis = rohr_controller_wasd_axis(&keyboard);
        Vec2D turn_axis = rohr_controller_axis_from_keycodes(&keyboard, SDLK_UNKNOWN, SDLK_LEFT, SDLK_UNKNOWN, SDLK_RIGHT);
        if(move_axis.x != 0.0f || move_axis.y != 0.0f) {
            result = rohr_physics_apply_force_for_one_tick(large_fly, (Force){
                .x = move_axis.x * large_fly_mass * large_fly_control_acceleration,
                .y = move_axis.y * large_fly_mass * large_fly_control_acceleration
            });
            if(rohr_error_check(result)) {
                PRINT_ENGINE_ERROR(result);
                goto fail;
            }
        }
        if(turn_axis.x != 0.0f) {
            result = rohr_physics_apply_torque_for_one_tick(large_fly, -turn_axis.x * large_fly_control_torque);
            if(rohr_error_check(result)) {
                PRINT_ENGINE_ERROR(result);
                goto fail;
            }
        }

        //physics
        rohr_engine_update_time();
        rohr_engine_update_tick();
        rohr_system_update_physics(rohr_engine_get_dt());

        //render
        rohr_graphics_draw_background(background_color);
        rohr_graphics_draw_hit_box(wall_1, GRAPHICS_FILLED);
        rohr_graphics_draw_hit_box(wall_2, GRAPHICS_FILLED);
        rohr_graphics_draw_hit_box(wall_3, GRAPHICS_FILLED);
        rohr_graphics_update_sprite_frames(rohr_engine_get_tick(), rohr_engine_get_time());
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
