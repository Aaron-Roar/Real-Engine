#include <stdio.h>
#include "rohr.h"
#include "example_runtime.h"

int main(void) {
    if(!example_use_executable_directory()) return 1;
    const char *paths[] = {
        "assets/game-state/world.json",
        "assets/game-state/relationships.json"
    };
    EntityIndex seeker_index;
    CameraAttachment camera_attachment;

    if(rohr_error_check(rohr_engine_init())) return 1;

    {
        EngineResult load_result = rohr_game_state_load_files(paths, 2);
        if(rohr_error_check(load_result)) {
            fprintf(stderr, "%s\n", rohr_error_default_message(load_result.result.error));
            rohr_engine_shutdown();
            return 1;
        }
    }

    EntityResult seeker_result = rohr_entity_by_name_get("seeker");
    EntityResult player_result = rohr_entity_by_name_get("player");
    if(rohr_error_check(seeker_result) || rohr_error_check(player_result)) {
        rohr_engine_shutdown();
        return 1;
    }
    Entity seeker = seeker_result.result.value;
    Entity player = player_result.result.value;
    EntityIndexResult index_result = rohr_entity_index_get(seeker);
    CameraAttachmentResult attachment_result = rohr_graphics_camera_attachment_get();
    if(rohr_error_check(index_result) || rohr_error_check(attachment_result)) {
        if(rohr_error_check(index_result)) {
            rohr_error_print_stderr(index_result.result.error);
        }
        if(rohr_error_check(attachment_result)) {
            rohr_error_print_stderr(attachment_result.result.error);
        }
        rohr_engine_shutdown();
        return 1;
    }
    seeker_index = index_result.result.value;
    camera_attachment = attachment_result.result.value;
    if(targets[seeker_index] != player
            || camera_attachment.entity != player
            || camera_attachment.position_offset.x != 10.0f
            || camera_attachment.position_offset.y != -20.0f
            || camera_attachment.orientation_offset != 0.25f) {
        fprintf(stderr, "Loaded entity relationships do not match\n");
        rohr_engine_shutdown();
        return 1;
    }

    EngineResult save_result = rohr_game_state_save_file("saved_game_state.json");
    if(rohr_error_check(save_result)) {
        rohr_error_print_stderr(save_result.result.error);
        rohr_engine_shutdown();
        return 1;
    }
    EngineResult template_result = rohr_game_state_save_template_file(
        "saved_game_state_template.json"
    );
    if(rohr_error_check(template_result)) {
        rohr_error_print_stderr(template_result.result.error);
    }
    rohr_engine_shutdown();
    return rohr_error_check(template_result) ? 1 : 0;
}
