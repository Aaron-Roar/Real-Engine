#include "rohr.h"

int main(void) {
    CameraConfig config = rohr_camera_default_config();
    CameraId original;
    CameraIdResult first_result;
    CameraIdResult second_result;
    CameraResult camera_result;
    Position screen;

    if(rohr_error_check(rohr_engine_init())) return 1;
    original = rohr_camera_get_active();
    config.position = (Position){10.0f, 20.0f};
    config.scale = 2.0f;
    config.viewport = (CameraViewport){0.0f, 0.0f, 320.0f, 240.0f};
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
            screen.x != 161.0f || screen.y != 120.0f ||
            !rohr_error_check(rohr_camera_destroy(first_result.result.value)) ||
            rohr_error_check(rohr_camera_set_active(second_result.result.value)) ||
            rohr_error_check(rohr_camera_destroy(first_result.result.value)) ||
            rohr_error_check(rohr_camera_set_active(original))) {
        rohr_engine_shutdown();
        return 1;
    }
    rohr_engine_shutdown();
    return 0;
}
