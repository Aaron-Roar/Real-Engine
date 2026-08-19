#include "rohr.h"

#include <stdio.h>

static Shape lower_triangle_get(void) {
    return (Shape){
        .amount_of_vertices = 3,
        .vertices = {{0.0f, 0.0f}, {2.0f, 0.0f}, {0.0f, 2.0f}}
    };
}

static Shape upper_triangle_get(void) {
    return (Shape){
        .amount_of_vertices = 3,
        .vertices = {{2.0f, 2.0f}, {2.0f, 0.5f}, {0.5f, 2.0f}}
    };
}

int main(void) {
    EntityResult first_result;
    EntityResult second_result;
    Entity first;
    Entity second;

    if(rohr_physics_sat_overlap_get(
            lower_triangle_get(), upper_triangle_get()).detected) {
        fprintf(stderr, "test polygons unexpectedly overlap\n");
        return 1;
    }
    if(rohr_error_check(rohr_engine_init())) return 1;
    first_result = rohr_entity_add();
    second_result = rohr_entity_add();
    if(rohr_error_check(first_result) || rohr_error_check(second_result)) goto fail;
    first = first_result.result.value;
    second = second_result.result.value;
    if(rohr_error_check(rohr_physics_hitbox_set(first, lower_triangle_get())) ||
            rohr_error_check(rohr_physics_hitbox_set(second, upper_triangle_get())) ||
            rohr_error_check(rohr_physics_position_set(
                first, (Position){2.0f / 3.0f, 2.0f / 3.0f})) ||
            rohr_error_check(rohr_physics_position_set(
                second, (Position){1.5f, 1.5f})) ||
            rohr_error_check(rohr_physics_static_set(first)) ||
            rohr_error_check(rohr_physics_static_set(second)) ||
            rohr_error_check(rohr_entity_components_add(first, ROHR_PARTICLE)))
        goto fail;

    rohr_system_physics_update(0.0);
    if(rohr_physics_overlap_check(first, second)) {
        ShapeResult first_shape = rohr_physics_global_hit_box_get(first);
        ShapeResult second_shape = rohr_physics_global_hit_box_get(second);
        fprintf(stderr, "mixed particle/rigid pair used circle overlap: second_particle=%d\n",
            rohr_entity_components_check(second, ROHR_PARTICLE));
        if(!rohr_error_check(first_shape) && !rohr_error_check(second_shape))
            fprintf(stderr, "global sat=%d particle=%d\n",
                rohr_physics_sat_overlap_get(first_shape.result.value,
                    second_shape.result.value).detected,
                rohr_physics_particle_overlap_get(first_shape.result.value,
                    second_shape.result.value).detected);
        goto fail;
    }

    if(rohr_error_check(rohr_entity_components_add(second, ROHR_PARTICLE)))
        goto fail;
    rohr_system_physics_update(0.0);
    if(!rohr_physics_overlap_check(first, second)) {
        fprintf(stderr, "particle pair did not use circle overlap\n");
        goto fail;
    }

    rohr_engine_shutdown();
    return 0;

fail:
    rohr_engine_shutdown();
    return 1;
}
