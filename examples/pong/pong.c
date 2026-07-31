#include "rohr.h"
#include <stdio.h>

#define PRINT_ENGINE_ERROR(engine_result) \
    fprintf(stderr, "%s\n", rohr_error_default_message((engine_result).result.error))

static const Color background_color = {18, 22, 30, 255};
static const Color foreground_color = {235, 240, 245, 255};
static const Color left_color = {70, 170, 255, 255};
static const Color right_color = {255, 105, 120, 255};
static const float paddle_speed = 280.0f;
static const float goal_y = 330.0f;
static const float paddle_min_x = -190.0f;
static const float paddle_max_x = 190.0f;
static const float left_paddle_min_y = 20.0f;
static const float left_paddle_max_y = 312.0f;
static const float right_paddle_min_y = -312.0f;
static const float right_paddle_max_y = -20.0f;

static EngineResult pong_find_entity(const char *name, Entity *entity) {
    EntityResult result;

    if(name == NULL || entity == NULL) {
        return rohr_error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    result = rohr_entity_find_by_name(name);
    if(rohr_error_check(result)) {
        return rohr_error_result_error(result.result.error);
    }
    *entity = result.result.value;
    return rohr_error_result_value(true);
}

static EngineResult pong_reset_ball(Entity ball, int serve_direction) {
    EngineResult result;

    result = rohr_physics_set_position(ball, (Position){0.0f, 0.0f});
    if(rohr_error_check(result)) return result;
    result = rohr_physics_set_orientation(ball, 0.0f);
    if(rohr_error_check(result)) return result;
    result = rohr_physics_set_angular_velocity(ball, 0.0f);
    if(rohr_error_check(result)) return result;
    return rohr_physics_set_velocity(ball, (Velocity){
        .x = serve_direction * 25.0f,
        .y = serve_direction * 45.0f
    });
}

static Vec2D pong_screen_axis_to_world(Vec2D screen_axis) {
    Camera camera = rohr_graphics_get_camera();
    return rohr_math_rotate_vector(screen_axis, camera.orientation);
}

static EngineResult pong_constrain_paddle(
    Entity paddle,
    float min_y,
    float max_y
) {
    EntityIndex index;
    Position position;
    Velocity velocity;
    bool position_changed = false;

    if(!rohr_entity_get_index(paddle, &index)
            || !positions_pool.used[index]
            || !velocities_pool.used[index]) {
        return rohr_error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    position = positions[index];
    velocity = velocities[index];
    if(position.x < paddle_min_x) {
        position.x = paddle_min_x;
        velocity.x = 0.0f;
        position_changed = true;
    } else if(position.x > paddle_max_x) {
        position.x = paddle_max_x;
        velocity.x = 0.0f;
        position_changed = true;
    }
    if(position.y < min_y) {
        position.y = min_y;
        velocity.y = 0.0f;
        position_changed = true;
    } else if(position.y > max_y) {
        position.y = max_y;
        velocity.y = 0.0f;
        position_changed = true;
    }
    if(!position_changed) return rohr_error_result_value(true);
    {
        EngineResult result = rohr_physics_set_position(paddle, position);
        if(rohr_error_check(result)) return result;
    }
    return rohr_physics_set_velocity(paddle, velocity);
}

int main(void) {
    EngineResult result;
    KeyboardState keyboard = {0};
    Entity wall_bottom;
    Entity wall_top;
    Entity center_line;
    Entity paddle_left;
    Entity paddle_right;
    Entity ball;
    int left_score = 0;
    int right_score = 0;
    int serve_direction = 1;

    if(rohr_error_check(result = rohr_engine_init())) {
        PRINT_ENGINE_ERROR(result);
        return 1;
    }
    rohr_engine_set_dt(1.0f / 120.0f);
    if(rohr_error_check(result = rohr_graphics_start())) {
        PRINT_ENGINE_ERROR(result);
        rohr_engine_shutdown();
        return 1;
    }
    if(rohr_error_check(result = rohr_game_state_load_file(
            "examples/pong/pong.json"))
            || rohr_error_check(result = pong_find_entity("wall_bottom", &wall_bottom))
            || rohr_error_check(result = pong_find_entity("wall_top", &wall_top))
            || rohr_error_check(result = pong_find_entity("center_line", &center_line))
            || rohr_error_check(result = pong_find_entity("paddle_left", &paddle_left))
            || rohr_error_check(result = pong_find_entity("paddle_right", &paddle_right))
            || rohr_error_check(result = pong_find_entity("ball", &ball))) {
        PRINT_ENGINE_ERROR(result);
        goto fail;
    }

    rohr_engine_reset_clock();
    while(true) {
        SDL_Event event = rohr_engine_poll_event();
        Vec2D left_axis;
        Vec2D right_axis;
        EntityIndex ball_index;

        if(event.type == SDL_EVENT_QUIT) break;
        rohr_controller_update_key_states(&keyboard);
        rohr_controller_add_key_event(
            &keyboard,
            rohr_controller_capture_keyboard_event(&event)
        );
        left_axis = pong_screen_axis_to_world(
            rohr_controller_wasd_axis(&keyboard)
        );
        right_axis = pong_screen_axis_to_world(
            rohr_controller_arrow_axis(&keyboard)
        );
        result = rohr_physics_set_velocity(
            paddle_left,
            (Velocity){
                left_axis.x * paddle_speed,
                left_axis.y * paddle_speed
            }
        );
        if(rohr_error_check(result)) {
            PRINT_ENGINE_ERROR(result);
            goto fail;
        }
        result = rohr_physics_set_velocity(
            paddle_right,
            (Velocity){
                right_axis.x * paddle_speed,
                right_axis.y * paddle_speed
            }
        );
        if(rohr_error_check(result)) {
            PRINT_ENGINE_ERROR(result);
            goto fail;
        }

        rohr_engine_update_time();
        rohr_engine_update_tick();
        rohr_system_update_physics(rohr_engine_get_dt());
        result = pong_constrain_paddle(
            paddle_left,
            left_paddle_min_y,
            left_paddle_max_y
        );
        if(rohr_error_check(result)) {
            PRINT_ENGINE_ERROR(result);
            goto fail;
        }
        result = pong_constrain_paddle(
            paddle_right,
            right_paddle_min_y,
            right_paddle_max_y
        );
        if(rohr_error_check(result)) {
            PRINT_ENGINE_ERROR(result);
            goto fail;
        }

        if(!rohr_entity_get_index(ball, &ball_index)) goto fail;
        if(positions[ball_index].y > goal_y) {
            right_score += 1;
            serve_direction = -1;
            printf("Left: %d  Right: %d\n", left_score, right_score);
            result = pong_reset_ball(ball, serve_direction);
            if(rohr_error_check(result)) {
                PRINT_ENGINE_ERROR(result);
                goto fail;
            }
        } else if(positions[ball_index].y < -goal_y) {
            left_score += 1;
            serve_direction = 1;
            printf("Left: %d  Right: %d\n", left_score, right_score);
            result = pong_reset_ball(ball, serve_direction);
            if(rohr_error_check(result)) {
                PRINT_ENGINE_ERROR(result);
                goto fail;
            }
        }

        rohr_graphics_draw_background(background_color);
        rohr_graphics_draw_hit_box_colored(
            center_line,
            GRAPHICS_FILLED,
            foreground_color
        );
        rohr_graphics_draw_hit_box_colored(
            wall_bottom,
            GRAPHICS_FILLED,
            foreground_color
        );
        rohr_graphics_draw_hit_box_colored(
            wall_top,
            GRAPHICS_FILLED,
            foreground_color
        );
        rohr_graphics_draw_hit_box_colored(
            paddle_left,
            GRAPHICS_FILLED,
            left_color
        );
        rohr_graphics_draw_hit_box_colored(
            paddle_right,
            GRAPHICS_FILLED,
            right_color
        );
        rohr_graphics_draw_hit_box_colored(
            ball,
            GRAPHICS_FILLED,
            foreground_color
        );
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
