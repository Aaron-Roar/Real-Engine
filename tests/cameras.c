#include "rohr.h"

static void count_camera_render(CameraId camera, void *context) {
    int *count = context;
    (void)camera;
    *count += 1;
    rohr_graphics_background_draw((Color){0, 0, 0, 255});
}

int main(void) {
    CameraConfig config = rohr_camera_config_default_get();
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
    rohr_graphics_layer_set(27);
    if(rohr_graphics_layer_get() != 27) {
        rohr_graphics_end();
        rohr_engine_shutdown();
        return 1;
    }
    {
        GraphicsWindowPresentationConfig presentation =
            rohr_graphics_window_presentation_default_get();
        GraphicsWindowPresentationConfig active =
            rohr_graphics_window_presentation_get();
        if(active.mode != GRAPHICS_WINDOW_MODE_WINDOWED ||
                active.logical_width != WINDOW_WIDTH ||
                active.logical_height != WINDOW_HEIGHT) {
            rohr_graphics_end();
            rohr_engine_shutdown();
            return 1;
        }
        presentation.window_width = 640;
        presentation.window_height = 480;
        presentation.logical_width = 800;
        presentation.logical_height = 600;
        if(rohr_error_check(rohr_graphics_window_presentation_set(presentation))) {
            rohr_graphics_end();
            rohr_engine_shutdown();
            return 1;
        }
        active = rohr_graphics_window_presentation_get();
        if(active.mode != GRAPHICS_WINDOW_MODE_WINDOWED ||
                active.window_width <= 0 || active.window_height <= 0 ||
                active.logical_width != 800 || active.logical_height != 600 ||
                active.aspect_ratio_auto ||
                rohr_error_check(rohr_graphics_window_presentation_set(
                    rohr_graphics_window_presentation_default_get()))) {
            rohr_graphics_end();
            rohr_engine_shutdown();
            return 1;
        }
    }
    {
        EngineResult frame_limit_result = rohr_graphics_frame_limit_set(-1);
        if(!rohr_error_check(frame_limit_result) ||
                frame_limit_result.result.error != ERROR_ENGINE_INVALID_FRAME_LIMIT ||
                rohr_error_check(rohr_graphics_frame_limit_set(0))) {
            rohr_graphics_end();
            rohr_engine_shutdown();
            return 1;
        }
    }
    original = rohr_camera_active_get();
    config.position = (Position){10.0f, 20.0f};
    config.zoom = 2.0f;
    first_result = rohr_camera_create(config);
    second_result = rohr_camera_create(rohr_camera_config_default_get());
    if(rohr_error_check(first_result) || rohr_error_check(second_result) ||
            rohr_error_check(rohr_camera_active_set(first_result.result.value))) {
        rohr_engine_shutdown();
        return 1;
    }
    camera_result = rohr_camera_get(first_result.result.value);
    screen = rohr_graphics_world_to_screen_get((Position){11.0f, 20.0f});
    if(rohr_error_check(camera_result) || camera_result.result.value.zoom != 2.0f ||
            screen.x != WINDOW_WIDTH * 0.5f + 2.0f ||
            screen.y != WINDOW_HEIGHT * 0.5f ||
            !rohr_error_check(rohr_camera_destroy(first_result.result.value)) ||
            rohr_error_check(rohr_camera_active_set(second_result.result.value)) ||
            rohr_error_check(rohr_camera_destroy(first_result.result.value)) ||
            rohr_error_check(rohr_camera_active_set(original))) {
        rohr_graphics_end();
        rohr_engine_shutdown();
        return 1;
    }
    target_entity_result = rohr_entity_add();
    if(rohr_error_check(target_entity_result)
            || rohr_error_check(rohr_physics_position_set(
                target_entity_result.result.value,
                (Position){20.0f, 30.0f}
            ))
            || rohr_error_check(rohr_camera_position_from_entity_set(
                original,
                target_entity_result.result.value,
                0.0
            ))
            || rohr_graphics_camera_attachment_get().kind != ERROR_RESULT_ERROR
            || rohr_error_check(rohr_camera_entity_attachment_set(
                original,
                target_entity_result.result.value
            ))
            || rohr_graphics_camera_attachment_get().kind == ERROR_RESULT_ERROR
            || rohr_error_check(rohr_camera_position_move(
                original,
                (Vec2D){5.0f, -5.0f},
                -1.0
            ))
            || rohr_graphics_camera_attachment_get().kind != ERROR_RESULT_ERROR) {
        rohr_graphics_end();
        rohr_engine_shutdown();
        return 1;
    }
    camera_result = rohr_camera_get(original);
    if(rohr_error_check(camera_result)
            || camera_result.result.value.position.x != 25.0f
            || camera_result.result.value.position.y != 25.0f
            || rohr_error_check(rohr_camera_position_set(
                original,
                (Position){0.0f, 0.0f},
                0.0
            ))) {
        rohr_graphics_end();
        rohr_engine_shutdown();
        return 1;
    }
    {
        EngineResult moving_result;
        CameraZoomResult zoom_result;
        if(rohr_error_check(rohr_camera_position_set(
                original,
                (Position){10.0f, 10.0f},
                0.5
            ))) {
            rohr_graphics_end();
            rohr_engine_shutdown();
            return 1;
        }
        moving_result = rohr_camera_moving_get(original);
        if(rohr_error_check(moving_result) || !moving_result.result.value
                || rohr_error_check(rohr_camera_position_move(
                    original,
                    (Vec2D){0.0f, 0.0f},
                    0.0
                ))) {
            rohr_graphics_end();
            rohr_engine_shutdown();
            return 1;
        }
        moving_result = rohr_camera_moving_get(original);
        if(rohr_error_check(moving_result) || moving_result.result.value
            || rohr_error_check(rohr_camera_zoom_set(original, 1.5f, -1.0))) {
            rohr_graphics_end();
            rohr_engine_shutdown();
            return 1;
        }
        zoom_result = rohr_camera_zoom_get(original);
        if(rohr_error_check(zoom_result) || zoom_result.result.value != 1.5f
            || !rohr_error_check(rohr_camera_zoom_set(original, 0.0f, 0.0))
            || rohr_error_check(rohr_entity_delete(target_entity_result.result.value))) {
            rohr_graphics_end();
            rohr_engine_shutdown();
            return 1;
        }
    }
    {
        ViewportConfig viewport_config = rohr_viewport_config_default_get();
        viewport_result = rohr_viewport_create(viewport_config);
    }
    if(rohr_error_check(viewport_result)
            || rohr_viewport_config_default_get().fit != SCREEN_FIT_CONTAIN
            || rohr_error_check(rohr_viewport_camera_set(
                viewport_result.result.value,
                original
            ))
            || rohr_error_check(rohr_camera_render_callback_set(
                original,
                count_camera_render,
                &render_count
            ))
            || rohr_error_check(rohr_viewport_enable_set(viewport_result.result.value))
            || (rohr_graphics_show(), render_count != 1)
            || rohr_graphics_layer_get() != 0
            || rohr_error_check(rohr_camera_disable_set(original))
            || (rohr_graphics_show(), render_count != 1)
            || rohr_error_check(rohr_camera_enable_set(original))
            || rohr_error_check(rohr_camera_pause_with_engine_set(original))
            || (rohr_engine_pause(), rohr_graphics_show(), render_count != 1)
            || rohr_error_check(rohr_camera_render_when_paused_set(original))
            || (rohr_graphics_show(), render_count != 2)
            || (rohr_engine_resume(), false)
            || rohr_error_check(rohr_viewport_disable_set(viewport_result.result.value))
            || (rohr_graphics_show(), render_count != 2)
            || rohr_error_check(rohr_viewport_camera_clear(viewport_result.result.value))
            || rohr_error_check(rohr_viewport_destroy(viewport_result.result.value))) {
        rohr_graphics_end();
        rohr_engine_shutdown();
        return 1;
    }
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;
}
