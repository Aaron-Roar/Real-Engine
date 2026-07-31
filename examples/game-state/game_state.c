#include <stdio.h>
#include "rohr.h"

int main(void) {
    const char *paths[] = {
        "examples/game-state/world.json",
        "examples/game-state/relationships.json"
    };
    EngineResult result;
    EntityResult seeker;
    EntityResult player;
    EntityIndex seeker_index;
    CameraAttachment camera_attachment;

    result = rohr_engine_init();
    if(rohr_error_check(result)) return 1;

    result = rohr_game_state_load_files(paths, 2);
    if(rohr_error_check(result)) {
        fprintf(stderr, "%s\n", rohr_error_default_message(result.result.error));
        rohr_engine_shutdown();
        return 1;
    }

    seeker = rohr_entity_find_by_name("seeker");
    player = rohr_entity_find_by_name("player");
    if(rohr_error_check(seeker) || rohr_error_check(player)
            || !rohr_entity_get_index(seeker.result.value, &seeker_index)
            || targets[seeker_index] != player.result.value
            || !rohr_graphics_get_camera_attachment(&camera_attachment)
            || camera_attachment.entity != player.result.value
            || camera_attachment.position_offset.x != 10.0f
            || camera_attachment.position_offset.y != -20.0f
            || camera_attachment.orientation_offset != 0.25f) {
        rohr_engine_shutdown();
        return 1;
    }

    result = rohr_game_state_save_file("build/examples/saved_game_state.json");
    if(!rohr_error_check(result)) {
        result = rohr_game_state_save_template_file(
            "build/examples/saved_game_state_template.json"
        );
    }
    rohr_engine_shutdown();
    return rohr_error_check(result) ? 1 : 0;
}
