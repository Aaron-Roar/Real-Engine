#include "rohr.h"
#include "example_runtime.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PRINT_ENGINE_ERROR(engine_result) \
    fprintf(stderr, "%s\n", rohr_error_default_message_get((engine_result).result.error))

typedef struct ObstacleRecord {
    Entity entity;
    Color color;
    bool active;
} ObstacleRecord;

enum {
    MAX_OBSTACLE_RECORDS = 256
};

const Color background_color = {245, 247, 250, 255};
const Color finish_line_color = {40, 190, 110, 255};
const Color wall_color = {90, 100, 115, 255};
const Color small_obstacle_color = {245, 220, 60, 255};
const Color small_medium_obstacle_color = {45, 120, 225, 255};
const Color tiny_fast_obstacle_color = {245, 130, 35, 255};
const Color massive_slow_obstacle_color = {210, 45, 55, 255};

const float screen_left = -320.0f;
const float screen_right = 320.0f;
const float spawn_y = 310.0f;
const Time demo_duration_seconds = 45.0;

const float player_thrust_acceleration = 240.0f;
const float player_brake_acceleration = 900.0f;
const Torque player_control_torque = 300000.0f;

const Time spawn_interval_seconds = 0.12;
const Time player_control_delay_seconds = 2.5f;

static Vec2D player_forward(Orientation orientation) {
    return rohr_math_vector_rotate((Vec2D){0.0f, 1.0f}, orientation);
}

static Color obstacle_color_for_size(float size, float speed) {
    if(size <= 8.0f && speed >= 180.0f) {
        return tiny_fast_obstacle_color;
    }
    if(size <= 12.0f) {
        return small_obstacle_color;
    }
    if(size <= 24.0f) {
        return small_medium_obstacle_color;
    }
    return massive_slow_obstacle_color;
}

static EngineResult spawn_obstacle(ObstacleRecord records[], size_t *next_record) {
    float size;
    float speed;
    float mass_value;
    int kind = rohr_tools_random_range(0, 3);

    if(records == NULL || next_record == NULL) {
        return rohr_error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }

    if(kind == 0) {
        size = rohr_tools_random_range_float(8.0f, 12.0f);
        speed = rohr_tools_random_range_float(85.0f, 125.0f);
        mass_value = 700.0f;
    } else if(kind == 1) {
        size = rohr_tools_random_range_float(16.0f, 24.0f);
        speed = rohr_tools_random_range_float(65.0f, 100.0f);
        mass_value = 1200.0f;
    } else if(kind == 2) {
        size = rohr_tools_random_range_float(4.0f, 7.0f);
        speed = rohr_tools_random_range_float(190.0f, 260.0f);
        mass_value = 900.0f;
    } else {
        size = rohr_tools_random_range_float(34.0f, 52.0f);
        speed = rohr_tools_random_range_float(35.0f, 70.0f);
        mass_value = 5000.0f;
    }

    EntityResult entity_result = rohr_entity_add();
    if(rohr_error_check(entity_result)) {
        return rohr_error_result_error(entity_result.result.error);
    }
    Entity obstacle = entity_result.result.value;
    rohr_physics_position_set(obstacle, (Position){
        .x = rohr_tools_random_range_float(screen_left + size, screen_right - size),
        .y = spawn_y
    });
    rohr_physics_orientation_set(obstacle, rohr_tools_random_range_float(0.0f, 2.0f * PI_F));
    rohr_physics_mass_set(obstacle, mass_value);
    rohr_physics_velocity_set(obstacle, (Velocity){.x = 0.0f, .y = -speed});
    rohr_physics_acceleration_set(obstacle, (Acceleration){0});
    rohr_physics_restitution_set(obstacle, 0.15f);
    rohr_physics_friction_set(obstacle, 0.45f);
    rohr_physics_hitbox_set(obstacle, rohr_math_circle_create(size, 8));
    rohr_physics_dynamic_set(obstacle);
    EngineResult lifetime_result = rohr_entity_life_time_set(
        obstacle,
        rohr_engine_time_get() + 12.0,
        0
    );
    if(rohr_error_check(lifetime_result)) {
        rohr_entity_delete(obstacle);
        return lifetime_result;
    }

    if(records[*next_record].active && rohr_entity_alive_check(records[*next_record].entity)) {
        (void)rohr_entity_delete(records[*next_record].entity);
    }

    records[*next_record] = (ObstacleRecord){
        .entity = obstacle,
        .color = obstacle_color_for_size(size, speed),
        .active = true
    };
    *next_record = (*next_record + 1) % MAX_OBSTACLE_RECORDS;
    return rohr_error_result_value(true);
}

static EngineResult reset_level(
        Entity player,
        Position player_start_position,
        Orientation player_start_orientation,
        ObstacleRecord records[],
        size_t *next_record,
        Time *next_spawn_time,
        Time *level_start_time,
        bool *reached_finish
        ) {
    if(records == NULL || next_record == NULL || next_spawn_time == NULL ||
            level_start_time == NULL || reached_finish == NULL) {
        return rohr_error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }

    for(size_t i = 0; i < MAX_OBSTACLE_RECORDS; i += 1) {
        if(records[i].active && rohr_entity_alive_check(records[i].entity)) {
            EngineResult delete_result = rohr_entity_delete(records[i].entity);
            if(rohr_error_check(delete_result)) {
                return delete_result;
            }
        }
        records[i] = (ObstacleRecord){0};
    }

    EngineResult position_result = rohr_physics_position_set(player, player_start_position);
    if(rohr_error_check(position_result)) { return position_result; }
    EngineResult orientation_result = rohr_physics_orientation_set(player, player_start_orientation);
    if(rohr_error_check(orientation_result)) { return orientation_result; }
    EngineResult velocity_result = rohr_physics_velocity_set(player, (Velocity){0});
    if(rohr_error_check(velocity_result)) { return velocity_result; }
    EngineResult acceleration_result = rohr_physics_acceleration_set(player, (Acceleration){0});
    if(rohr_error_check(acceleration_result)) { return acceleration_result; }
    EngineResult angular_velocity_result = rohr_physics_angular_velocity_set(player, 0.0f);
    if(rohr_error_check(angular_velocity_result)) { return angular_velocity_result; }

    *next_record = 0;
    *level_start_time = rohr_engine_time_get();
    *next_spawn_time = *level_start_time;
    *reached_finish = false;
    return rohr_error_result_value(true);
}

int main(void) {
    if(!example_use_executable_directory()) return 1;
    KeyboardState keyboard = {0};
    ObstacleRecord obstacle_records[MAX_OBSTACLE_RECORDS] = {0};
    size_t next_obstacle_record = 0;
    Time next_spawn_time = 0.0;
    Time level_start_time = 0.0;
    Entity player;
    Entity finish_line;
    Entity wall_mid;
    Entity walls[4];
    const char *wall_names[4] = {
        "wall_left",
        "wall_right",
        "wall_bottom",
        "wall_top"
    };
    EntityIndex player_index;
    Position player_start_position;
    Orientation player_start_orientation;
    Mass player_mass;
    bool reached_finish = false;

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

    EngineResult load_result = rohr_game_state_file_load(
        "assets/fly-to-finish/game.json"
    );
    if(rohr_error_check(load_result)) {
        PRINT_ENGINE_ERROR(load_result);
        goto fail;
    }

    EntityResult finish_line_result = rohr_entity_by_name_get("finish_line");
    if(rohr_error_check(finish_line_result)) {
        PRINT_ENGINE_ERROR(finish_line_result);
        goto fail;
    }
    finish_line = finish_line_result.result.value;

    for(size_t wall_index = 0; wall_index < 4; wall_index += 1) {
        EntityResult wall_result = rohr_entity_by_name_get(wall_names[wall_index]);
        if(rohr_error_check(wall_result)) {
            PRINT_ENGINE_ERROR(wall_result);
            goto fail;
        }
        walls[wall_index] = wall_result.result.value;
    }

    EntityResult player_result = rohr_entity_by_name_get("player");
    if(rohr_error_check(player_result)) {
        PRINT_ENGINE_ERROR(player_result);
        goto fail;
    }
    player = player_result.result.value;
    EntityResult wall_mid_result = rohr_entity_by_name_get("wall_mid");
    if(rohr_error_check(wall_mid_result)) {
        PRINT_ENGINE_ERROR(wall_mid_result);
        goto fail;
    }
    wall_mid = wall_mid_result.result.value;
    EntityIndexResult player_index_result = rohr_entity_index_get(player);
    if(rohr_error_check(player_index_result)) {
        fprintf(stderr, "Invalid player state\n");
        goto fail;
    }
    player_index = player_index_result.result.value;
    if(!positions_pool.used[player_index]
            || !orientations_pool.used[player_index]
            || !mass_pool.used[player_index]) {
        fprintf(stderr, "Invalid player state\n");
        goto fail;
    }
    player_start_position = positions[player_index];
    player_start_orientation = orientations[player_index];
    player_mass = mass[player_index];

    srand((unsigned int)time(NULL));
    rohr_engine_clock_reset();
    level_start_time = rohr_engine_time_get();
    while(true) {
        SDL_Event event;
        Vec2D thrust_axis;
        Vec2D turn_axis;
        EntityIndex player_index;
        Velocity player_velocity;
        float speed;
        bool level_active;
        bool player_control_enabled;
        size_t i;

        event = rohr_engine_event_poll();
        rohr_controller_key_states_update(&keyboard);
        rohr_controller_key_event_add(&keyboard, rohr_controller_keyboard_event_capture(&event));
        if(event.type == SDL_EVENT_QUIT ||
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) break;

        if(rohr_controller_key_pressed_get(&keyboard, SDLK_R)) {
            EngineResult reset_result = reset_level(
                    player,
                    player_start_position,
                    player_start_orientation,
                    obstacle_records,
                    &next_obstacle_record,
                    &next_spawn_time,
                    &level_start_time,
                    &reached_finish
            );
            if(rohr_error_check(reset_result)) {
                PRINT_ENGINE_ERROR(reset_result);
                goto fail;
            }
        }

        level_active = !reached_finish && rohr_engine_time_get() - level_start_time < demo_duration_seconds;
        player_control_enabled = level_active &&
            rohr_engine_time_get() - level_start_time >= player_control_delay_seconds;

        if(level_active && rohr_engine_time_get() >= next_spawn_time) {
            EngineResult spawn_result = spawn_obstacle(obstacle_records, &next_obstacle_record);
            if(rohr_error_check(spawn_result)) {
                PRINT_ENGINE_ERROR(spawn_result);
                goto fail;
            }
            next_spawn_time = rohr_engine_time_get() + spawn_interval_seconds;
        }

        EntityIndexResult player_index_result = rohr_entity_index_get(player);
        if(rohr_error_check(player_index_result)) {
            goto fail;
        }
        player_index = player_index_result.result.value;
        player_velocity = velocities[player_index];
        speed = rohr_math_vector_magnitude(player_velocity);

        thrust_axis = player_forward(orientations[player_index]);
        if(player_control_enabled && rohr_controller_key_down_get(&keyboard, SDLK_W)) {
            EngineResult thrust_result = rohr_physics_force_for_one_tick_apply(player, (Force){
                .x = thrust_axis.x * player_mass * player_thrust_acceleration,
                .y = thrust_axis.y * player_mass * player_thrust_acceleration
            });
            if(rohr_error_check(thrust_result)) {
                PRINT_ENGINE_ERROR(thrust_result);
                goto fail;
            }
        }
        if(player_control_enabled && rohr_controller_key_down_get(&keyboard, SDLK_S) && speed > 0.001f) {
            EngineResult brake_result = rohr_physics_force_for_one_tick_apply(player, (Force){
                .x = -(player_velocity.x / speed) * player_mass * player_brake_acceleration,
                .y = -(player_velocity.y / speed) * player_mass * player_brake_acceleration
            });
            if(rohr_error_check(brake_result)) {
                PRINT_ENGINE_ERROR(brake_result);
                goto fail;
            }
        }

        turn_axis = rohr_controller_axis_from_keycodes_get(&keyboard, SDLK_UNKNOWN, SDLK_A, SDLK_UNKNOWN, SDLK_D);
        if(player_control_enabled && turn_axis.x != 0.0f) {
            EngineResult torque_result = rohr_physics_torque_for_one_tick_apply(player, -turn_axis.x * player_control_torque);
            if(rohr_error_check(torque_result)) {
                PRINT_ENGINE_ERROR(torque_result);
                goto fail;
            }
        }

        if(animated_sprites_pool.used[player_index]) {
            animated_sprites[player_index].animation.time_per_frame = fmaxf(0.015f, 0.09f - speed * 0.0002f);
        }

        Tick ticks_advanced = rohr_system_tick_update();
        if(level_active) {
            rohr_physics_update(ticks_advanced);
        }

        if(level_active) {
            ShapeResult player_shape_result = rohr_physics_global_hit_box_get(player);
            ShapeResult finish_shape_result = rohr_physics_global_hit_box_get(finish_line);
            if(player_shape_result.kind == ERROR_RESULT_VALUE && finish_shape_result.kind == ERROR_RESULT_VALUE) {
                OverlapInfo finish_overlap = rohr_physics_sat_overlap_get(
                    player_shape_result.result.value,
                    finish_shape_result.result.value
                );
                reached_finish = finish_overlap.detected;
            }
        }

        rohr_graphics_background_draw(background_color);
        for(i = 0; i < 4; i += 1) {
            rohr_graphics_hit_box_colored_draw(
                walls[i],
                GRAPHICS_FILLED,
                wall_color
            );
        }
        rohr_graphics_hit_box_colored_draw(wall_mid, GRAPHICS_FILLED, obstacle_records[1].color);
        rohr_graphics_hit_box_colored_draw(finish_line, GRAPHICS_FILLED, finish_line_color);
        rohr_graphics_hit_box_colored_draw(player, GRAPHICS_OUTLINE, finish_line_color);
        for(i = 0; i < (size_t)MAX_OBSTACLE_RECORDS; i += 1) {
            if(obstacle_records[i].active && rohr_entity_alive_check(obstacle_records[i].entity)) {
                rohr_graphics_hit_box_colored_draw(obstacle_records[i].entity, GRAPHICS_FILLED, obstacle_records[i].color);
            }
        }
        rohr_graphics_sprite_frames_update(rohr_engine_tick_get(), rohr_engine_time_get());
        rohr_graphics_animated_sprites_draw();
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
