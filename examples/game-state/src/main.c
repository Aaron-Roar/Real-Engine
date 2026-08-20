/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include <stdio.h>
#include "rohr.h"
#include "example_runtime.h"

#define PRINT_ENGINE_ERROR(result_value) \
    fprintf(stderr, "error %d: %s\n", (int)(result_value).result.error, \
        rohr_error_message_get(result_value))

int main(void) {
    if(!example_use_executable_directory()) return 1;
    const char *paths[] = {
        "assets/game-state/world.json",
        "assets/game-state/relationships.json"
    };
    EntityIndex seeker_index;
    CameraAttachment camera_attachment;

    {
        EngineResult init_result = rohr_engine_init();
        if(rohr_error_check(init_result)) {
            PRINT_ENGINE_ERROR(init_result);
            return 1;
        }
    }

    {
        EngineResult load_result = rohr_game_state_files_load(paths, 2);
        if(rohr_error_check(load_result)) {
            PRINT_ENGINE_ERROR(load_result);
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
            PRINT_ENGINE_ERROR(index_result);
        }
        if(rohr_error_check(attachment_result)) {
            PRINT_ENGINE_ERROR(attachment_result);
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

    EngineResult save_result = rohr_game_state_file_save("saved_game_state.json");
    if(rohr_error_check(save_result)) {
        PRINT_ENGINE_ERROR(save_result);
        rohr_engine_shutdown();
        return 1;
    }
    EngineResult template_result = rohr_game_state_template_file_save(
        "saved_game_state_template.json"
    );
    if(rohr_error_check(template_result)) {
        PRINT_ENGINE_ERROR(template_result);
    }
    rohr_engine_shutdown();
    return rohr_error_check(template_result) ? 1 : 0;
}
