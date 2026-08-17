#include "rohr.h"
#include "game_components.h"
#include "example_runtime.h"
#include <stdio.h>

#define PRINT_ENGINE_ERROR(result_value) \
    fprintf(stderr, "error %d: %s\n", (int)(result_value).result.error, \
        rohr_error_message_get(result_value))

static const Color background_color = {18, 22, 30, 255};
static const Color foreground_color = {235, 240, 245, 255};
static const Color left_color = {70, 170, 255, 255};
static const Color right_color = {255, 105, 120, 255};
static const Color fire_color = {255, 105, 20, 255};
static const Time fire_duration = 0.2;
static const Time normal_physics_dt = 1.0 / 120.0;
static const Time slow_motion_physics_dt = 1.0 / 480.0;
static const float paddle_speed = 280.0f;
static const float goal_y = 330.0f;
static const float field_camera_center_y = 170.0f;
static const float paddle_min_x = -190.0f;
static const float paddle_max_x = 190.0f;
static const float left_paddle_min_y = 20.0f;
static const float left_paddle_max_y = 312.0f;
static const float right_paddle_min_y = -312.0f;
static const float right_paddle_max_y = -20.0f;
static const Time camera_approach_duration = 0.5;
static const Time camera_return_duration = 0.25;
static const float camera_ball_scale = 1.5f;
static const RohrCollisionCategoryMask pong_collision_category_paddle = UINT64_C(1) << 1;
static const RohrCollisionCategoryMask pong_collision_category_ball = UINT64_C(1) << 2;

typedef enum PongCameraState {
    PONG_CAMERA_HOME,
    PONG_CAMERA_MOVING_TO_BALL,
    PONG_CAMERA_FOLLOWING_BALL,
    PONG_CAMERA_RETURNING_HOME,
} PongCameraState;

typedef struct PongRenderContext {
    Entity wall_bottom;
    Entity wall_top;
    Entity center_line;
    Entity paddle_left;
    Entity paddle_right;
    Entity ball;
    bool ball_on_fire;
} PongRenderContext;

static void pong_draw_field(
    Entity wall_bottom,
    Entity wall_top,
    Entity center_line,
    Entity paddle_left,
    Entity paddle_right,
    Entity ball,
    bool ball_on_fire
) {
    rohr_graphics_hit_box_colored_draw(center_line, GRAPHICS_FILLED, foreground_color);
    rohr_graphics_hit_box_colored_draw(wall_bottom, GRAPHICS_FILLED, foreground_color);
    rohr_graphics_hit_box_colored_draw(wall_top, GRAPHICS_FILLED, foreground_color);
    rohr_graphics_hit_box_colored_draw(paddle_left, GRAPHICS_FILLED, left_color);
    rohr_graphics_hit_box_colored_draw(paddle_right, GRAPHICS_FILLED, right_color);
    rohr_graphics_hit_box_colored_draw(
        ball,
        GRAPHICS_FILLED,
        ball_on_fire ? fire_color : foreground_color
    );
}

static void pong_render_camera(CameraId camera, void *context_value) {
    PongRenderContext *context = context_value;
    (void)camera;
    rohr_graphics_background_draw(background_color);
    pong_draw_field(
        context->wall_bottom,
        context->wall_top,
        context->center_line,
        context->paddle_left,
        context->paddle_right,
        context->ball,
        context->ball_on_fire
    );
    rohr_graphics_aabb_tree_draw();
}

static EngineResult pong_update_camera(
    CameraId camera,
    Entity ball,
    bool ball_behind_paddle,
    Position home,
    PongCameraState *state
) {
    EngineResult result;
    if(state == NULL) return rohr_error_result_error(ERROR_ENGINE_STATE_INVALID);
    if(ball_behind_paddle) {
        if(*state == PONG_CAMERA_HOME || *state == PONG_CAMERA_RETURNING_HOME) {
            result = rohr_camera_zoom_set(
                camera,
                camera_ball_scale,
                camera_approach_duration
            );
            if(rohr_error_check(result)) return result;
            result = rohr_camera_position_from_entity_set(camera, ball, camera_approach_duration);
            if(rohr_error_check(result)) return result;
            *state = PONG_CAMERA_MOVING_TO_BALL;
        } else if(*state == PONG_CAMERA_MOVING_TO_BALL) {
            result = rohr_camera_moving_get(camera);
            if(rohr_error_check(result)) return result;
            if(!result.result.value) {
                result = rohr_camera_entity_attachment_set(camera, ball);
                if(rohr_error_check(result)) return result;
                *state = PONG_CAMERA_FOLLOWING_BALL;
            }
        }
    } else if(*state == PONG_CAMERA_MOVING_TO_BALL
            || *state == PONG_CAMERA_FOLLOWING_BALL) {
        result = rohr_camera_zoom_set(camera, 1.0f, camera_return_duration);
        if(rohr_error_check(result)) return result;
        result = rohr_camera_position_set(camera, home, camera_return_duration);
        if(rohr_error_check(result)) return result;
        *state = PONG_CAMERA_RETURNING_HOME;
    } else if(*state == PONG_CAMERA_RETURNING_HOME) {
        result = rohr_camera_moving_get(camera);
        if(rohr_error_check(result)) return result;
        if(!result.result.value) *state = PONG_CAMERA_HOME;
    }
    return rohr_error_result_value(true);
}

static EngineResult pong_reset_ball(Entity ball, int serve_direction) {
    EngineResult position_result = rohr_physics_position_set(ball, (Position){0.0f, 0.0f});
    if(rohr_error_check(position_result)) return position_result;
    EngineResult orientation_result = rohr_physics_orientation_set(ball, 0.0f);
    if(rohr_error_check(orientation_result)) return orientation_result;
    EngineResult angular_velocity_result = rohr_physics_angular_velocity_set(ball, 0.0f);
    if(rohr_error_check(angular_velocity_result)) return angular_velocity_result;
    return rohr_physics_velocity_set(ball, (Velocity){
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
    EntityIndexResult index_result = rohr_entity_index_get(paddle);

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
        EngineResult position_result = rohr_physics_position_set(paddle, position);
        if(rohr_error_check(position_result)) return position_result;
    }
    return rohr_physics_velocity_set(paddle, velocity);
}

int main(void) {
    UIPhysicsDebugPanel debug_panel = {0};
    if(!example_use_executable_directory()) return 1;
    KeyboardState keyboard = {0};
    Controller left_controller = rohr_controller_wasd_default_get();
    Controller right_controller = rohr_controller_arrows_default_get();
    Entity wall_bottom;
    Entity wall_top;
    Entity center_line;
    Entity paddle_left;
    Entity paddle_right;
    Entity ball;
    CameraId left_camera = CAMERA_INVALID;
    CameraId right_camera = CAMERA_INVALID;
    ViewportId left_viewport = VIEWPORT_INVALID;
    ViewportId right_viewport = VIEWPORT_INVALID;
    PongRenderContext render_context = {0};
    bool broadphase_debug = true;
    bool ball_behind_left = false;
    bool ball_behind_right = false;
    PongCameraState left_camera_state = PONG_CAMERA_HOME;
    PongCameraState right_camera_state = PONG_CAMERA_HOME;
    int left_score = 0;
    int right_score = 0;
    int serve_direction = 1;
    Time fire_expires_at = 0.0;

    if(!rohr_controller_axis_add(
        &left_controller,
        "movement",
        (ControllerAxisBinding){
            .positive_x = SDLK_W,
            .negative_x = SDLK_S,
            .positive_y = SDLK_A,
            .negative_y = SDLK_D,
        }
    )) return 1;
    if(!rohr_controller_axis_add(
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
            PRINT_ENGINE_ERROR(init_result);
            return 1;
        }
    }
    {
        EngineResult tick_result = rohr_engine_time_per_tick_set(1.0 / 120.0);
        if(rohr_error_check(tick_result)) {
            PRINT_ENGINE_ERROR(tick_result);
            rohr_engine_shutdown();
            return 1;
        }
    }
    {
        EngineResult graphics_result = rohr_graphics_start();
        if(rohr_error_check(graphics_result)) {
            PRINT_ENGINE_ERROR(graphics_result);
            rohr_engine_shutdown();
            return 1;
        }
    }
    {
        EngineResult debug_result = rohr_ui_physics_debug_panel_init(&debug_panel,
            (FontDescriptor){"assets/debug/JetBrainsMono-BoldItalic.ttf", 11.0f});
        if(rohr_error_check(debug_result)) {
            PRINT_ENGINE_ERROR(debug_result);
            goto fail;
        }
    }
    rohr_graphics_aabb_tree_debug_set(broadphase_debug);
    if(!game_components_init()) {
        goto fail;
    }
    EngineResult load_result = rohr_game_state_file_load("assets/pong/pong.json");
    if(rohr_error_check(load_result)) {
        PRINT_ENGINE_ERROR(load_result);
        goto fail;
    }
    EntityResult wall_bottom_result = rohr_entity_by_name_get("wall_bottom");
    if(rohr_error_check(wall_bottom_result)) {
        PRINT_ENGINE_ERROR(wall_bottom_result);
        goto fail;
    }
    wall_bottom = wall_bottom_result.result.value;
    EntityResult wall_top_result = rohr_entity_by_name_get("wall_top");
    if(rohr_error_check(wall_top_result)) {
        PRINT_ENGINE_ERROR(wall_top_result);
        goto fail;
    }
    wall_top = wall_top_result.result.value;
    EntityResult center_line_result = rohr_entity_by_name_get("center_line");
    if(rohr_error_check(center_line_result)) {
        PRINT_ENGINE_ERROR(center_line_result);
        goto fail;
    }
    center_line = center_line_result.result.value;
    EntityResult paddle_left_result = rohr_entity_by_name_get("paddle_left");
    if(rohr_error_check(paddle_left_result)) {
        PRINT_ENGINE_ERROR(paddle_left_result);
        goto fail;
    }
    paddle_left = paddle_left_result.result.value;
    EntityResult paddle_right_result = rohr_entity_by_name_get("paddle_right");
    if(rohr_error_check(paddle_right_result)) {
        PRINT_ENGINE_ERROR(paddle_right_result);
        goto fail;
    }
    paddle_right = paddle_right_result.result.value;
    EntityResult ball_result = rohr_entity_by_name_get("ball");
    if(rohr_error_check(ball_result)) {
        PRINT_ENGINE_ERROR(ball_result);
        goto fail;
    }
    ball = ball_result.result.value;
    if(rohr_error_check(rohr_physics_collision_category_set(
                paddle_left,
                pong_collision_category_paddle
            )) ||
            rohr_error_check(rohr_physics_collision_with_set(
                paddle_left,
                pong_collision_category_ball
            )) ||
            rohr_error_check(rohr_physics_collision_category_set(
                paddle_right,
                pong_collision_category_paddle
            )) ||
            rohr_error_check(rohr_physics_collision_with_set(
                paddle_right,
                pong_collision_category_ball
            )) ||
            rohr_error_check(rohr_physics_collision_category_set(
                ball,
                pong_collision_category_ball
            )) ||
            rohr_error_check(rohr_physics_collision_with_set(
                ball,
                pong_collision_category_paddle | ROHR_COLLISION_CATEGORY_DEFAULT
            ))) {
        goto fail;
    }
    render_context = (PongRenderContext){
        .wall_bottom = wall_bottom,
        .wall_top = wall_top,
        .center_line = center_line,
        .paddle_left = paddle_left,
        .paddle_right = paddle_right,
        .ball = ball,
    };

    left_camera = rohr_camera_active_get();
    {
        Camera left_camera_value = {
            .position = {0.0f, field_camera_center_y},
            .orientation = -PI_F * 0.5f,
            .dimensions = {340.0f, 500.0f},
            .zoom = 1.0f,
        };
        EngineResult camera_result = rohr_camera_set(left_camera, left_camera_value);
        if(rohr_error_check(camera_result)) {
            PRINT_ENGINE_ERROR(camera_result);
            goto fail;
        }
    }
    {
        CameraConfig config = rohr_camera_config_default_get();
        CameraIdResult camera_result;
        config.position = (Position){0.0f, -field_camera_center_y};
        config.orientation = -PI_F * 0.5f;
        config.dimensions = (Vec2D){340.0f, 500.0f};
        camera_result = rohr_camera_create(config);
        if(rohr_error_check(camera_result)) {
            PRINT_ENGINE_ERROR(camera_result);
            goto fail;
        }
        right_camera = camera_result.result.value;
    }
    {
        ViewportConfig config = rohr_viewport_config_default_get();
        ViewportIdResult result;
        config.rectangle = (ViewportRectangle){0.0f, 0.0f, WINDOW_WIDTH * 0.5f, WINDOW_HEIGHT};
        result = rohr_viewport_create(config);
        if(rohr_error_check(result)) goto fail;
        left_viewport = result.result.value;
        config.rectangle.x = WINDOW_WIDTH * 0.5f;
        result = rohr_viewport_create(config);
        if(rohr_error_check(result)) goto fail;
        right_viewport = result.result.value;
        if(rohr_error_check(rohr_viewport_camera_set(left_viewport, left_camera))
                || rohr_error_check(rohr_viewport_camera_set(right_viewport, right_camera))
                || rohr_error_check(rohr_viewport_enable_set(left_viewport))
                || rohr_error_check(rohr_viewport_enable_set(right_viewport))) goto fail;
    }
    if(rohr_error_check(rohr_camera_render_callback_set(
            left_camera,
            pong_render_camera,
            &render_context
        )) || rohr_error_check(rohr_camera_render_callback_set(
            right_camera,
            pong_render_camera,
            &render_context
        ))) goto fail;

    rohr_engine_clock_reset();
    while(true) {
        SDL_Event event;
        Vec2D left_axis;
        Vec2D right_axis;
        EntityIndex ball_index;
        Tick ticks_advanced;
        bool exit_requested = false;

        rohr_controller_key_states_update(&keyboard);
        while((event = rohr_engine_event_poll()).type != 0) {
            KeyboardEvent key_event =
                rohr_controller_keyboard_event_capture(&event);
            rohr_controller_key_event_add(&keyboard, key_event);
            if(key_event.keycode == SDLK_B &&
                    key_event.state == KEY_STATE_PRESSED) {
                broadphase_debug = !broadphase_debug;
                rohr_graphics_aabb_tree_debug_set(broadphase_debug);
            }
            if(event.type == SDL_EVENT_QUIT) exit_requested = true;
        }
        if(exit_requested ||
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) break;
        left_axis = rohr_controller_axis_get(&keyboard, &left_controller, "movement");
        right_axis = rohr_controller_axis_get(&keyboard, &right_controller, "movement");
        EngineResult left_velocity_result = rohr_physics_velocity_set(
            paddle_left,
            (Velocity){
                left_axis.x * paddle_speed,
                left_axis.y * paddle_speed
            }
        );
        if(rohr_error_check(left_velocity_result)) {
            PRINT_ENGINE_ERROR(left_velocity_result);
            goto fail;
        }
        EngineResult right_velocity_result = rohr_physics_velocity_set(
            paddle_right,
            (Velocity){
                right_axis.x * paddle_speed,
                right_axis.y * paddle_speed
            }
        );
        if(rohr_error_check(right_velocity_result)) {
            PRINT_ENGINE_ERROR(right_velocity_result);
            goto fail;
        }

        ticks_advanced = rohr_system_tick_update();
        rohr_physics_update(ticks_advanced);
        if(rohr_physics_contact_check(ball, paddle_left) ||
                rohr_physics_contact_check(ball, paddle_right)) {
            if(!game_ball_on_fire_set(ball, true)) {
                goto fail;
            }
            fire_expires_at = rohr_engine_time_get() + fire_duration;
        }
        {
            GameBallOnFireResult fire_result = game_ball_on_fire_get(ball);
            if(!rohr_error_check(fire_result) && fire_result.result.value &&
                    rohr_engine_time_get() >= fire_expires_at &&
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
            PRINT_ENGINE_ERROR(left_constraint_result);
            goto fail;
        }
        EngineResult right_constraint_result = pong_constrain_paddle(
            paddle_right,
            right_paddle_min_y,
            right_paddle_max_y
        );
        if(rohr_error_check(right_constraint_result)) {
            PRINT_ENGINE_ERROR(right_constraint_result);
            goto fail;
        }

        {
            EntityIndexResult ball_index_result = rohr_entity_index_get(ball);
            if(rohr_error_check(ball_index_result)) goto fail;
            ball_index = ball_index_result.result.value;
        }
        if(positions[ball_index].y > goal_y) {
            right_score += 1;
            serve_direction = -1;
            printf("Left: %d  Right: %d\n", left_score, right_score);
            EngineResult reset_result = pong_reset_ball(ball, serve_direction);
            if(rohr_error_check(reset_result)) {
                PRINT_ENGINE_ERROR(reset_result);
                goto fail;
            }
        } else if(positions[ball_index].y < -goal_y) {
            left_score += 1;
            serve_direction = 1;
            printf("Left: %d  Right: %d\n", left_score, right_score);
            EngineResult reset_result = pong_reset_ball(ball, serve_direction);
            if(rohr_error_check(reset_result)) {
                PRINT_ENGINE_ERROR(reset_result);
                goto fail;
            }
        }

        {
            EntityIndexResult left_index_result = rohr_entity_index_get(paddle_left);
            EntityIndexResult right_index_result = rohr_entity_index_get(paddle_right);
            bool was_behind_left = ball_behind_left;
            bool was_behind_right = ball_behind_right;
            if(rohr_error_check(left_index_result) || rohr_error_check(right_index_result)) {
                goto fail;
            }
            ball_behind_left = positions[ball_index].y > positions[left_index_result.result.value].y;
            ball_behind_right = positions[ball_index].y < positions[right_index_result.result.value].y;
            if(ball_behind_left != was_behind_left || ball_behind_right != was_behind_right) {
                EngineResult dt_result = (ball_behind_left || ball_behind_right)
                    ? rohr_physics_dt_per_tick_set(slow_motion_physics_dt)
                    : rohr_physics_dt_per_tick_set(normal_physics_dt);
                if(rohr_error_check(dt_result)) {
                    PRINT_ENGINE_ERROR(dt_result);
                    goto fail;
                }
            }
            if(rohr_error_check(pong_update_camera(
                    left_camera,
                    ball,
                    ball_behind_left,
                    (Position){0.0f, field_camera_center_y},
                    &left_camera_state
                )) || rohr_error_check(pong_update_camera(
                    right_camera,
                    ball,
                    ball_behind_right,
                    (Position){0.0f, -field_camera_center_y},
                    &right_camera_state
                ))) goto fail;
        }

        {
            GameBallOnFireResult fire_result = game_ball_on_fire_get(ball);
            render_context.ball_on_fire = rohr_error_check(fire_result)
                ? false
                : fire_result.result.value;
        }
        rohr_ui_physics_debug_panel_draw(&debug_panel);
        rohr_graphics_show();
    }

    (void)rohr_viewport_destroy(right_viewport);
    (void)rohr_viewport_destroy(left_viewport);
    (void)rohr_camera_active_set(left_camera);
    (void)rohr_camera_destroy(right_camera);
    game_components_clear(ball);
    game_components_shutdown();
    rohr_graphics_end();
    rohr_ui_physics_debug_panel_destroy(&debug_panel);
    rohr_engine_shutdown();
    return 0;

fail:
    rohr_ui_physics_debug_panel_destroy(&debug_panel);
    if(right_viewport != VIEWPORT_INVALID) (void)rohr_viewport_destroy(right_viewport);
    if(left_viewport != VIEWPORT_INVALID) (void)rohr_viewport_destroy(left_viewport);
    if(left_camera != CAMERA_INVALID) (void)rohr_camera_active_set(left_camera);
    if(right_camera != CAMERA_INVALID) (void)rohr_camera_destroy(right_camera);
    game_components_shutdown();
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
