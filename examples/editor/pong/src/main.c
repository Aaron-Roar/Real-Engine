#include "rohr_editor.h"
#include "game_components.h"
#include "example_runtime.h"
#include <stdio.h>

static const Color background_color = {18, 22, 30, 255};
static const Color foreground_color = {235, 240, 245, 255};
static const Color left_color = {70, 170, 255, 255};
static const Color right_color = {255, 105, 120, 255};
static const Color fire_color = {255, 105, 20, 255};
static const Time fire_duration = 0.2;
static const float paddle_speed = 280.0f;
static const float goal_y = 330.0f;
static const float paddle_min_x = -190.0f;
static const float paddle_max_x = 190.0f;
static const float left_paddle_min_y = 20.0f;
static const float left_paddle_max_y = 312.0f;
static const float right_paddle_min_y = -312.0f;
static const float right_paddle_max_y = -20.0f;

static EngineResult pong_reset_ball(Entity ball, int serve_direction) {
    EngineResult position_result = rohr_physics_set_position(ball, (Position){0.0f, 0.0f});
    if(rohr_error_check(position_result)) return position_result;
    EngineResult orientation_result = rohr_physics_set_orientation(ball, 0.0f);
    if(rohr_error_check(orientation_result)) return orientation_result;
    EngineResult angular_velocity_result = rohr_physics_set_angular_velocity(ball, 0.0f);
    if(rohr_error_check(angular_velocity_result)) return angular_velocity_result;
    return rohr_physics_set_velocity(ball, (Velocity){
        .x = serve_direction * 25.0f,
        .y = serve_direction * 45.0f
    });
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
    EntityIndexResult index_result = rohr_entity_get_index(paddle);

    if(rohr_error_check(index_result)) {
        return rohr_error_result_error(index_result.result.error);
    }
    index = index_result.result.value;
    if(!positions_pool.used[index]
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
        EngineResult position_result = rohr_physics_set_position(paddle, position);
        if(rohr_error_check(position_result)) return position_result;
    }
    return rohr_physics_set_velocity(paddle, velocity);
}

int main(void) {
    if(!example_use_executable_directory()) return 1;
    KeyboardState keyboard = {0};
    Controller left_controller = rohr_controller_default_wasd();
    Controller right_controller = rohr_controller_default_arrows();
    Entity wall_bottom;
    Entity wall_top;
    Entity center_line;
    Entity paddle_left;
    Entity paddle_right;
    Entity ball;
    int left_score = 0;
    int right_score = 0;
    int serve_direction = 1;
    Time fire_expires_at = 0.0;

    if(!rohr_controller_add_axis(
        &left_controller,
        "movement",
        (ControllerAxisBinding){
            .positive_x = SDLK_W,
            .negative_x = SDLK_S,
            .positive_y = SDLK_A,
            .negative_y = SDLK_D,
        }
    )) return 1;
    if(!rohr_controller_add_axis(
        &right_controller,
        "movement",
        (ControllerAxisBinding){
            .positive_x = SDLK_UP,
            .negative_x = SDLK_DOWN,
            .positive_y = SDLK_LEFT,
            .negative_y = SDLK_RIGHT,
        }
    )) return 1;

    {
        EngineResult init_result = rohr_engine_init();
        if(rohr_error_check(init_result)) {
            rohr_error_print_stderr(init_result.result.error);
            return 1;
        }
    }
    if(rohr_error_check(rohr_engine_set_time_per_tick(1.0 / 120.0))) return 1;
    {
        EngineResult graphics_result = rohr_graphics_start();
        if(rohr_error_check(graphics_result)) {
            rohr_error_print_stderr(graphics_result.result.error);
            rohr_engine_shutdown();
            return 1;
        }
    }
    if(!game_components_init()) {
        goto fail;
    }
    EngineResult load_result = rohr_game_state_load_file("assets/pong/pong.json");
    if(rohr_error_check(load_result)) {
        rohr_error_print_stderr(load_result.result.error);
        goto fail;
    }
    EntityResult wall_bottom_result = RE_entity_find_by_name("wall_bottom");
    if(rohr_error_check(wall_bottom_result)) {
        rohr_error_print_stderr(wall_bottom_result.result.error);
        goto fail;
    }
    wall_bottom = wall_bottom_result.result.value;
    EntityResult wall_top_result = RE_entity_find_by_name("wall_top");
    if(rohr_error_check(wall_top_result)) {
        rohr_error_print_stderr(wall_top_result.result.error);
        goto fail;
    }
    wall_top = wall_top_result.result.value;
    EntityResult center_line_result = RE_entity_find_by_name("center_line");
    if(rohr_error_check(center_line_result)) {
        rohr_error_print_stderr(center_line_result.result.error);
        goto fail;
    }
    center_line = center_line_result.result.value;
    EntityResult paddle_left_result = RE_entity_find_by_name("paddle_left");
    if(rohr_error_check(paddle_left_result)) {
        rohr_error_print_stderr(paddle_left_result.result.error);
        goto fail;
    }
    paddle_left = paddle_left_result.result.value;
    EntityResult paddle_right_result = RE_entity_find_by_name("paddle_right");
    if(rohr_error_check(paddle_right_result)) {
        rohr_error_print_stderr(paddle_right_result.result.error);
        goto fail;
    }
    paddle_right = paddle_right_result.result.value;
    EntityResult ball_result = RE_entity_find_by_name("ball");
    if(rohr_error_check(ball_result)) {
        rohr_error_print_stderr(ball_result.result.error);
        goto fail;
    }
    ball = ball_result.result.value;

    rohr_engine_reset_clock();
    while(true) {
        SDL_Event event = rohr_engine_poll_event();
        Vec2D left_axis;
        Vec2D right_axis;
        EntityIndex ball_index;
        Tick ticks_advanced;

        if(event.type == SDL_EVENT_QUIT) break;
        rohr_controller_update_key_states(&keyboard);
        rohr_controller_add_key_event(
            &keyboard,
            rohr_controller_capture_keyboard_event(&event)
        );
        left_axis = rohr_controller_get_axis(&keyboard, &left_controller, "movement");
        right_axis = rohr_controller_get_axis(&keyboard, &right_controller, "movement");
        EngineResult left_velocity_result = rohr_physics_set_velocity(
            paddle_left,
            (Velocity){
                left_axis.x * paddle_speed,
                left_axis.y * paddle_speed
            }
        );
        if(rohr_error_check(left_velocity_result)) {
            rohr_error_print_stderr(left_velocity_result.result.error);
            goto fail;
        }
        EngineResult right_velocity_result = rohr_physics_set_velocity(
            paddle_right,
            (Velocity){
                right_axis.x * paddle_speed,
                right_axis.y * paddle_speed
            }
        );
        if(rohr_error_check(right_velocity_result)) {
            rohr_error_print_stderr(right_velocity_result.result.error);
            goto fail;
        }

        ticks_advanced = rohr_engine_update_tick();
        rohr_physics_update(ticks_advanced);
        if(rohr_physics_get_collision_report(ball, paddle_left) ||
                rohr_physics_get_collision_report(ball, paddle_right)) {
            if(!game_ball_on_fire_set(ball, true)) {
                goto fail;
            }
            fire_expires_at = rohr_engine_get_time() + fire_duration;
        }
        {
            GameBallOnFireResult fire_result = game_ball_on_fire_get(ball);
            if(!rohr_error_check(fire_result) && fire_result.result.value &&
                    rohr_engine_get_time() >= fire_expires_at &&
                    !game_ball_on_fire_set(ball, false)) {
                goto fail;
            }
        }
        EngineResult left_constraint_result = pong_constrain_paddle(
            paddle_left,
            left_paddle_min_y,
            left_paddle_max_y
        );
        if(rohr_error_check(left_constraint_result)) {
            rohr_error_print_stderr(left_constraint_result.result.error);
            goto fail;
        }
        EngineResult right_constraint_result = pong_constrain_paddle(
            paddle_right,
            right_paddle_min_y,
            right_paddle_max_y
        );
        if(rohr_error_check(right_constraint_result)) {
            rohr_error_print_stderr(right_constraint_result.result.error);
            goto fail;
        }

        {
            EntityIndexResult ball_index_result = rohr_entity_get_index(ball);
            if(rohr_error_check(ball_index_result)) goto fail;
            ball_index = ball_index_result.result.value;
        }
        if(positions[ball_index].y > goal_y) {
            right_score += 1;
            serve_direction = -1;
            printf("Left: %d  Right: %d\n", left_score, right_score);
            EngineResult reset_result = pong_reset_ball(ball, serve_direction);
            if(rohr_error_check(reset_result)) {
                rohr_error_print_stderr(reset_result.result.error);
                goto fail;
            }
        } else if(positions[ball_index].y < -goal_y) {
            left_score += 1;
            serve_direction = 1;
            printf("Left: %d  Right: %d\n", left_score, right_score);
            EngineResult reset_result = pong_reset_ball(ball, serve_direction);
            if(rohr_error_check(reset_result)) {
                rohr_error_print_stderr(reset_result.result.error);
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
        {
            GameBallOnFireResult fire_result = game_ball_on_fire_get(ball);
            BallOnFire ball_on_fire = rohr_error_check(fire_result)
                ? false
                : fire_result.result.value;
            rohr_graphics_draw_hit_box_colored(
                ball,
                GRAPHICS_FILLED,
                ball_on_fire ? fire_color : foreground_color
            );
        }
        rohr_graphics_show();
    }

    game_components_clear(ball);
    game_components_shutdown();
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;

fail:
    game_components_shutdown();
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
