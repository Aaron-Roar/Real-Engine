#include "rohr.h"

#include <math.h>
#include <stdio.h>

static const RohrCollisionCategoryMask body_category = UINT64_C(1) << 1;
static const RohrCollisionCategoryMask sensor_category = UINT64_C(1) << 2;

int main(void) {
    EntityResult body_result;
    EntityResult sensor_result;
    EntityIndexResult body_index;
    PositionResult body_position;
    Entity body;
    Entity sensor;

    if(rohr_error_check(rohr_engine_init())) return 1;
    body_result = rohr_entity_add();
    sensor_result = rohr_entity_add();
    if(rohr_error_check(body_result) || rohr_error_check(sensor_result)) goto fail;
    body = body_result.result.value;
    sensor = sensor_result.result.value;

    if(rohr_error_check(rohr_physics_position_set(body, (Position){-15.0f, 0.0f})) ||
            rohr_error_check(rohr_physics_velocity_set(body, (Velocity){12.0f, 0.0f})) ||
            rohr_error_check(rohr_physics_acceleration_set(body, (Acceleration){0})) ||
            rohr_error_check(rohr_physics_mass_set(body, 1.0f)) ||
            rohr_error_check(rohr_physics_hitbox_set(
                body, rohr_math_square_create(2.0f, 2.0f))) ||
            rohr_error_check(rohr_physics_dynamic_set(body)) ||
            rohr_error_check(rohr_physics_restitution_set(body, 0.0f)) ||
            rohr_error_check(rohr_physics_collision_category_set(
                body, body_category)) ||
            rohr_error_check(rohr_physics_collision_with_set(
                body, sensor_category))) goto fail;

    if(rohr_error_check(rohr_physics_position_set(sensor, (Position){0.0f, 0.0f})) ||
            rohr_error_check(rohr_physics_hitbox_set(
                sensor, rohr_math_square_create(10.0f, 10.0f))) ||
            rohr_error_check(rohr_physics_static_set(sensor)) ||
            rohr_error_check(rohr_physics_collision_category_set(
                sensor, sensor_category)) ||
            rohr_error_check(rohr_physics_collision_with_set(
                sensor, body_category))) goto fail;
    if(rohr_entity_components_check(sensor, COLLISION)) goto fail;

    rohr_system_physics_update(1.0);
    body_position = rohr_physics_position_get(body);
    body_index = rohr_entity_index_get(body);
    if(rohr_error_check(body_position) || rohr_error_check(body_index) ||
            fabsf(body_position.result.value.x - -3.0f) > 0.0001f ||
            fabsf(body_position.result.value.y) > 0.0001f ||
            fabsf(velocities[body_index.result.value].x - 12.0f) > 0.0001f ||
            fabsf(velocities[body_index.result.value].y) > 0.0001f ||
            !rohr_physics_collision_report_get(body, sensor)) {
        fprintf(stderr, "sensor overlap step: position=(%f,%f) velocity=(%f,%f) overlap=%d\n",
            body_position.result.value.x,
            body_position.result.value.y,
            velocities[body_index.result.value].x,
            velocities[body_index.result.value].y,
            rohr_physics_collision_report_get(body, sensor));
        goto fail;
    }

    rohr_system_physics_update(1.0);
    body_position = rohr_physics_position_get(body);
    if(rohr_error_check(body_position) ||
            fabsf(body_position.result.value.x - 9.0f) > 0.0001f ||
            fabsf(velocities[body_index.result.value].x - 12.0f) > 0.0001f ||
            rohr_physics_collision_report_get(body, sensor)) {
        fprintf(stderr, "sensor exit step: position=(%f,%f) velocity=(%f,%f) overlap=%d\n",
            body_position.result.value.x,
            body_position.result.value.y,
            velocities[body_index.result.value].x,
            velocities[body_index.result.value].y,
            rohr_physics_collision_report_get(body, sensor));
        goto fail;
    }

    rohr_engine_shutdown();
    return 0;

fail:
    rohr_engine_shutdown();
    return 1;
}
