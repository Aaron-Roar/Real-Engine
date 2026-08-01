#include "rohr.h"
#include "example_runtime.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "elderfly_descriptors.h"

const Color background_color = (Color){255,255,255,255};
const int amount_of_entities = 20;
AnimationAsset animation = {0};
AnimatedSprite sprite = {0};
const Time demo_duration_seconds = 30.0;
const Mass ball_mass = 5000.0f;
const float ball_control_acceleration = 240.0f;
const Torque ball_control_torque = 2000000.0f;

#define PRINT_ENGINE_ERROR(engine_result) \
    fprintf(stderr, "%s\n", rohr_error_default_message((engine_result).result.error))

int main(void) {
    if(!example_use_executable_directory()) return 1;
    GroupId children_group = GROUP_INVALID;
    KeyboardState keyboard = {0};

    {
        EngineResult init_result = rohr_engine_init();
        if(rohr_error_check(init_result)) {
            PRINT_ENGINE_ERROR(init_result);
            return 1;
        }
    }
    if(rohr_error_check(rohr_engine_set_time_per_tick(1.0 / 120.0))) return 1;
    {
        EngineResult graphics_result = rohr_graphics_start();
        if(rohr_error_check(graphics_result)) {
            PRINT_ENGINE_ERROR(graphics_result);
            rohr_engine_shutdown();
            return 1;
        }
    }

    AnimationAssetResult animation_result = rohr_graphics_load_animation(elderfly_fly);
    if(rohr_error_check(animation_result)) {
        PRINT_ENGINE_ERROR(animation_result);
        goto fail;
    }
    animation = animation_result.result.value;
    sprite = rohr_graphics_create_animated_sprite(animation, (Scale){3,3});

    EntityResult ball_result = rohr_entity_add();
    if(rohr_error_check(ball_result)) {
        PRINT_ENGINE_ERROR(ball_result);
        goto fail;
    }
    Entity ball = ball_result.result.value;
    rohr_physics_set_position(ball, (Position){.x = 0, .y = 100});
    rohr_physics_set_orientation(ball, 0);
    rohr_physics_set_mass(ball, ball_mass);
    rohr_physics_set_velocity(ball, (Velocity){0, 0});
    //set_angular_velocity(ball, 3);
    rohr_physics_set_acceleration(ball, (Acceleration){0, 0});
    rohr_physics_set_restitution(ball, 0.7);
    Shape ball_shape = rohr_math_create_circle(50, 4);
    rohr_physics_set_hitbox(ball, ball_shape);
    rohr_physics_set_friction(ball, 0.4);
    rohr_physics_set_dynamic(ball);
    //set_angle_lock(ball, 0, 0);

    time_t seed = 1003463;
    srand(seed);
    for(int i = 0; i < amount_of_entities; i += 1) {
        EntityResult small_fly_result = rohr_entity_add();
        if(rohr_error_check(small_fly_result)) {
            PRINT_ENGINE_ERROR(small_fly_result);
            goto fail;
        }
        Entity small_fly = small_fly_result.result.value;
        rohr_physics_set_position(small_fly, (Position){.x = rohr_tools_random_range(100, 400), .y = rohr_tools_random_range(0, 300)});
        rohr_physics_set_orientation(small_fly, rohr_tools_random_range(0, 2*PI_F));
        rohr_physics_set_mass(small_fly, 10);
        rohr_physics_set_velocity(small_fly, (Velocity){.x = rohr_tools_random_range(-10, 10), .y = rohr_tools_random_range(0, 100)});
        rohr_physics_set_acceleration(small_fly, (Acceleration){0, 0});
        rohr_physics_set_restitution(small_fly, 0.1);
        float size = rohr_tools_random_range_float(10, 20);
        Shape small_fly_shape = rohr_math_create_circle(size, 5);
        rohr_physics_set_hitbox(small_fly, small_fly_shape);
        rohr_physics_set_friction(small_fly, 0.4);
        rohr_physics_set_dynamic(small_fly);
        EngineResult parent_result = rohr_entity_set_parent(small_fly, ball);
        if(rohr_error_check(parent_result)) {
            PRINT_ENGINE_ERROR(parent_result);
            goto fail;
        }
        sprite = rohr_graphics_create_animated_sprite(animation, (Scale){size/10, size/10});
        sprite.animation.time_per_frame = rohr_tools_random_range_float(0.005, 0.5);
        rohr_graphics_add_animated_sprite(small_fly, sprite);
        rohr_entity_add_components(small_fly, PARTICLE);
    }

    ChildrenResult children_result = rohr_entity_get_children(ball);
    if(rohr_error_check(children_result)) {
        PRINT_ENGINE_ERROR(children_result);
        goto fail;
    }
    children_group = children_result.result.value;

    //Game Loop
    rohr_engine_reset_clock();
    rohr_graphics_recording_start("flies_around_ball_recording.mp4", 60);
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
        Time time = rohr_engine_get_time();
        Time phase_time = time - ((int)(time / 4.0) * 4.0);
        float acceleration_magnitude = 50.0f + (float)time * 22.0f;
        Vec2D move_axis = rohr_controller_wasd_axis(&keyboard);
        Vec2D turn_axis = rohr_controller_axis_from_keycodes(&keyboard, SDLK_UNKNOWN, SDLK_LEFT, SDLK_UNKNOWN, SDLK_RIGHT);

        if(move_axis.x != 0.0f || move_axis.y != 0.0f) {
            EngineResult force_result = rohr_physics_apply_force_for_one_tick(ball, (Force){
                .x = -move_axis.x * ball_mass * ball_control_acceleration,
                .y = -move_axis.y * ball_mass * ball_control_acceleration
            });
            if(rohr_error_check(force_result)) {
                PRINT_ENGINE_ERROR(force_result);
                goto fail;
            }
        }
        if(turn_axis.x != 0.0f) {
            EngineResult torque_result = rohr_physics_apply_torque_for_one_tick(ball, -turn_axis.x * ball_control_torque);
            if(rohr_error_check(torque_result)) {
                PRINT_ENGINE_ERROR(torque_result);
                goto fail;
            }
        }

        EngineResult acceleration_result;
        if(phase_time < 3.0) {
            acceleration_result = rohr_physics_group_set_acceleration_toward_entity(children_group, acceleration_magnitude, ball);
        } else {
            acceleration_result = rohr_physics_group_set_acceleration_away_from_entity(children_group, acceleration_magnitude, ball);
        }
        if(rohr_error_check(acceleration_result)) {
            PRINT_ENGINE_ERROR(acceleration_result);
            goto fail;
        }

        //physics
        Tick ticks_advanced = rohr_engine_update_tick();
        rohr_physics_update(ticks_advanced);

        //render
        rohr_graphics_draw_background(background_color);
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
            rohr_graphics_draw_local_origins();
        }
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
