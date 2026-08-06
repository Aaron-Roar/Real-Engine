#include "rohr.h"
#include "engine_internal.h"

#include <stdio.h>

int main(void) {
    const RohrCollisionCategoryMask player = UINT64_C(1) << 1;
    const RohrCollisionCategoryMask enemy = UINT64_C(1) << 2;
    EntityResult first;
    EntityResult second;
    CollisionFilterConfigResult filter;
    ContactInfo contact;
    EngineResult result = rohr_engine_init();
    if(rohr_error_check(result)) {
        fprintf(stderr, "%s\n", rohr_error_default_message_get(result.result.error));
        return 1;
    }
    if(rohr_physics_solver_iterations_get() != PHYSICS_SOLVER_ITERATIONS_DEFAULT ||
            !rohr_error_check(rohr_physics_solver_iterations_set(0)) ||
            rohr_error_check(rohr_physics_solver_iterations_set(12)) ||
            rohr_physics_solver_iterations_get() != 12) {
        rohr_engine_shutdown();
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
    if(rohr_error_check(rohr_physics_position_set(first.result.value, (Position){-0.25f, 0.0f})) ||
            rohr_error_check(rohr_physics_position_set(second.result.value, (Position){0.25f, 0.0f})) ||
            rohr_error_check(rohr_physics_velocity_set(first.result.value, (Velocity){1.0f, 0.0f})) ||
            rohr_error_check(rohr_physics_acceleration_set(first.result.value, (Acceleration){0})) ||
            rohr_error_check(rohr_physics_mass_set(first.result.value, 1.0f)) ||
            rohr_error_check(rohr_physics_dynamic_set(first.result.value)) ||
            rohr_error_check(rohr_physics_static_set(second.result.value)) ||
            rohr_error_check(rohr_physics_restitution_set(first.result.value, 0.0f)) ||
            rohr_error_check(rohr_physics_restitution_set(second.result.value, 0.0f)) ||
            rohr_error_check(rohr_entity_components_add(first.result.value, COLLISION)) ||
            rohr_error_check(rohr_entity_components_add(second.result.value, COLLISION))) {
        rohr_engine_shutdown();
        return 1;
    }
    rohr_system_physics_update(0.0);
    contact = rohr_physics_contact_get(first.result.value, second.result.value);
    if(!rohr_physics_overlap_check(first.result.value, second.result.value) ||
            !rohr_physics_overlap_get(first.result.value, second.result.value).detected ||
            !rohr_physics_overlap_entered_check(first.result.value, second.result.value) ||
            rohr_physics_overlap_stayed_check(first.result.value, second.result.value) ||
            rohr_physics_overlap_exited_check(first.result.value, second.result.value) ||
            !rohr_physics_contact_check(first.result.value, second.result.value) ||
            !contact.detected || contact.relative_velocity.x >= 0.0f ||
            contact.normal_impulse.x <= 0.0f ||
            rohr_physics_contact_total_impulse_get(contact).x <= 0.0f ||
            !rohr_physics_contact_entered_check(first.result.value, second.result.value) ||
            rohr_physics_contact_stayed_check(first.result.value, second.result.value) ||
            rohr_physics_contact_exited_check(first.result.value, second.result.value) ||
            !physics_interaction_current_check(
                first.result.value,
                second.result.value,
                PHYSICS_INTERACTION_CONTACT
            )) {
        rohr_engine_shutdown();
        return 1;
    }
    rohr_engine_shutdown();
    return 0;
}
