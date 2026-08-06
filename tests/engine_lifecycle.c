#include "rohr.h"

#include <stdio.h>

int main(void) {
    const RohrCollisionCategoryMask player = UINT64_C(1) << 1;
    const RohrCollisionCategoryMask enemy = UINT64_C(1) << 2;
    EntityResult first;
    EntityResult second;
    CollisionFilterConfigResult filter;
    EngineResult result = rohr_engine_init();
    if(rohr_error_check(result)) {
        fprintf(stderr, "%s\n", rohr_error_default_message_get(result.result.error));
        return 1;
    }

    first = rohr_entity_add();
    second = rohr_entity_add();
    if(first.kind == ERROR_RESULT_ERROR || second.kind == ERROR_RESULT_ERROR ||
            rohr_error_check(rohr_physics_hitbox_set(first.result.value, rohr_math_square_create(1.0f, 1.0f))) ||
            rohr_error_check(rohr_physics_hitbox_set(second.result.value, rohr_math_square_create(1.0f, 1.0f)))) {
        rohr_engine_shutdown();
        return 1;
    }
    filter = rohr_physics_collision_filter_get(first.result.value);
    if(filter.kind == ERROR_RESULT_ERROR ||
            filter.result.value.category != ROHR_COLLISION_CATEGORY_DEFAULT ||
            filter.result.value.collides_with != ROHR_COLLISION_CATEGORY_ALL ||
            !rohr_physics_collision_between_check(first.result.value, second.result.value)) {
        rohr_engine_shutdown();
        return 1;
    }
    if(rohr_error_check(rohr_physics_collision_category_set(first.result.value, player)) ||
            rohr_error_check(rohr_physics_collision_category_set(second.result.value, enemy)) ||
            rohr_error_check(rohr_physics_collision_with_set(first.result.value, enemy)) ||
            rohr_error_check(rohr_physics_collision_with_none_set(second.result.value)) ||
            rohr_physics_collision_between_check(first.result.value, second.result.value)) {
        rohr_engine_shutdown();
        return 1;
    }
    if(rohr_error_check(rohr_physics_collision_with_set(second.result.value, player)) ||
            !rohr_physics_collision_between_check(first.result.value, second.result.value) ||
            rohr_error_check(rohr_physics_collision_with_all_set(first.result.value)) ||
            !rohr_physics_collision_between_check(first.result.value, second.result.value)) {
        rohr_engine_shutdown();
        return 1;
    }
    if(rohr_error_check(rohr_physics_position_set(first.result.value, (Position){0.0f, 0.0f})) ||
            rohr_error_check(rohr_physics_position_set(second.result.value, (Position){0.0f, 0.0f}))) {
        rohr_engine_shutdown();
        return 1;
    }
    rohr_system_physics_update(0.0);
    if(!rohr_physics_contact_get(first.result.value, second.result.value)) {
        rohr_engine_shutdown();
        return 1;
    }
    if(rohr_error_check(rohr_physics_collision_report_set(
                first.result.value, second.result.value, false)) ||
            rohr_physics_contact_get(first.result.value, second.result.value) ||
            rohr_error_check(rohr_physics_collision_report_set(
                first.result.value, second.result.value, true)) ||
            !rohr_physics_contact_get(second.result.value, first.result.value)) {
        rohr_engine_shutdown();
        return 1;
    }

    rohr_engine_shutdown();
    return 0;
}
