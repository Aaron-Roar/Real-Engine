#include "rohr.h"

static void count_camera_render(CameraId camera, void *context) {
    int *count = context;
    (void)camera;
    *count += 1;
    rohr_graphics_draw_background((Color){0, 0, 0, 255});
}

int main(void) {
    CameraConfig config = rohr_camera_default_config();
    CameraId original;
    CameraIdResult first_result;
    CameraIdResult second_result;
    CameraResult camera_result;
    ViewportIdResult viewport_result;
    Position screen;
    int render_count = 0;
    EntityResult target_entity_result;

    if(rohr_error_check(rohr_engine_init())) return 1;
    if(rohr_error_check(rohr_graphics_start())) {
        rohr_engine_shutdown();
        return 1;
    }
    original = rohr_camera_get_active();
    config.position = (Position){10.0f, 20.0f};
    config.scale = 2.0f;
    first_result = rohr_camera_create(config);
    second_result = rohr_camera_create(rohr_camera_default_config());
    if(rohr_error_check(first_result) || rohr_error_check(second_result) ||
            rohr_error_check(rohr_camera_set_active(first_result.result.value))) {
        rohr_engine_shutdown();
        return 1;
    }
    camera_result = rohr_camera_get(first_result.result.value);
    screen = rohr_graphics_world_to_screen((Position){11.0f, 20.0f});
    if(rohr_error_check(camera_result) || camera_result.result.value.scale != 2.0f ||
            screen.x != 322.0f || screen.y != 240.0f ||
            !rohr_error_check(rohr_camera_destroy(first_result.result.value)) ||
            rohr_error_check(rohr_camera_set_active(second_result.result.value)) ||
            rohr_error_check(rohr_camera_destroy(first_result.result.value)) ||
            rohr_error_check(rohr_camera_set_active(original))) {
        rohr_graphics_end();
        rohr_engine_shutdown();
        return 1;
    }
    target_entity_result = rohr_entity_add();
    if(rohr_error_check(target_entity_result)
            || rohr_error_check(rohr_physics_set_position(
                target_entity_result.result.value,
                (Position){20.0f, 30.0f}
            ))
            || rohr_error_check(rohr_camera_move_to_entity(
                original,
                target_entity_result.result.value,
                0.0
            ))
            || rohr_graphics_get_camera_attachment().kind != ERROR_RESULT_ERROR
            || rohr_error_check(rohr_camera_attach_to_entity(
                original,
                target_entity_result.result.value
            ))
            || rohr_graphics_get_camera_attachment().kind == ERROR_RESULT_ERROR
            || rohr_error_check(rohr_camera_move(
                original,
                (Vec2D){5.0f, -5.0f},
                -1.0
            ))
            || rohr_graphics_get_camera_attachment().kind != ERROR_RESULT_ERROR) {
        rohr_graphics_end();
        rohr_engine_shutdown();
        return 1;
    }
    camera_result = rohr_camera_get(original);
    if(rohr_error_check(camera_result)
            || camera_result.result.value.position.x != 25.0f
            || camera_result.result.value.position.y != 25.0f
            || rohr_error_check(rohr_camera_move_to(
                original,
                (Position){0.0f, 0.0f},
                0.0
            ))
            || rohr_error_check(rohr_entity_delete(target_entity_result.result.value))) {
        rohr_graphics_end();
        rohr_engine_shutdown();
        return 1;
    }
    {
        ViewportConfig viewport_config = rohr_viewport_default_config();
        viewport_result = rohr_viewport_create(viewport_config);
    }
    if(rohr_error_check(viewport_result)
            || rohr_viewport_default_config().fit != SCREEN_FIT_CONTAIN
            || rohr_error_check(rohr_viewport_set_camera(
                viewport_result.result.value,
                original
            ))
            || rohr_error_check(rohr_camera_set_render_callback(
                original,
                count_camera_render,
                &render_count
            ))
            || rohr_error_check(rohr_viewport_set_enable(viewport_result.result.value))
            || (rohr_graphics_show(), render_count != 1)
            || rohr_error_check(rohr_camera_set_disable(original))
            || (rohr_graphics_show(), render_count != 1)
            || rohr_error_check(rohr_camera_set_enable(original))
            || rohr_error_check(rohr_camera_set_pause_with_engine(original))
            || (rohr_engine_pause(), rohr_graphics_show(), render_count != 1)
            || rohr_error_check(rohr_camera_set_render_when_paused(original))
            || (rohr_graphics_show(), render_count != 2)
            || (rohr_engine_resume(), false)
            || rohr_error_check(rohr_viewport_set_disable(viewport_result.result.value))
            || (rohr_graphics_show(), render_count != 2)
            || rohr_error_check(rohr_viewport_clear_camera(viewport_result.result.value))
            || rohr_error_check(rohr_viewport_destroy(viewport_result.result.value))) {
        rohr_graphics_end();
        rohr_engine_shutdown();
        return 1;
    }
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;
}
