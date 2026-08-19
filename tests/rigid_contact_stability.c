#include "rohr.h"
#include "physics/physics_internal.h"

#include <math.h>
#include <stdio.h>

static Shape concave_body_shape_get(void) {
    return (Shape){
        .amount_of_vertices = 8,
        .vertices = {
            {-20.0f, -20.0f}, {-5.0f, -20.0f}, {-5.0f, -5.0f},
            {5.0f, -5.0f}, {5.0f, -20.0f}, {20.0f, -20.0f},
            {20.0f, 20.0f}, {-20.0f, 20.0f}
        }
    };
}

static bool entity_configure(
    Entity entity,
    Shape shape,
    Position position,
    bool dynamic
) {
    if(rohr_error_check(rohr_physics_hitbox_set(entity, shape)) ||
            rohr_error_check(rohr_physics_position_set(entity, position)) ||
            rohr_error_check(rohr_physics_restitution_set(entity, 0.0f)) ||
            rohr_error_check(rohr_physics_friction_set(entity, 0.8f)) ||
            rohr_error_check(rohr_entity_components_add(
                entity, ROHR_COLLISION))) return false;
    if(dynamic) {
        return !rohr_error_check(rohr_physics_mass_set(entity, 5.0f)) &&
            !rohr_error_check(rohr_physics_dynamic_set(entity)) &&
            !rohr_error_check(rohr_physics_gravity_enable(entity));
    }
    return !rohr_error_check(rohr_physics_static_set(entity));
}

int main(void) {
    EntityResult floor_result;
    EntityResult body_result;
    EntityIndexResult body_index;
    PositionResult position;
    ContactInfo resting_contact;
    float settled_x;
    EngineResult result = rohr_engine_init();

    if(rohr_error_check(result)) {
        fprintf(stderr, "%s\n", rohr_error_message_get(result));
        return 1;
    }
    floor_result = rohr_entity_add();
    body_result = rohr_entity_add();
    if(rohr_error_check(floor_result) || rohr_error_check(body_result) ||
            !entity_configure(floor_result.result.value,
                rohr_math_square_create(400.0f, 20.0f),
                (Position){0.0f, 100.0f}, false) ||
            !entity_configure(body_result.result.value,
                concave_body_shape_get(), (Position){0.0f, 20.0f}, true)) {
        rohr_engine_shutdown();
        return 2;
    }

    for(size_t tick = 0; tick < 360; tick += 1)
        rohr_system_physics_update(1.0 / 60.0);
    position = rohr_physics_position_get(body_result.result.value);
    if(rohr_error_check(position)) {
        rohr_engine_shutdown();
        return 3;
    }
    settled_x = position.result.value.x;
    for(size_t tick = 0; tick < 360; tick += 1)
        rohr_system_physics_update(1.0 / 60.0);
    position = rohr_physics_position_get(body_result.result.value);
    body_index = rohr_entity_index_get(body_result.result.value);
    resting_contact = rohr_physics_contact_get(
        body_result.result.value, floor_result.result.value);
    if(rohr_error_check(position) || rohr_error_check(body_index) ||
            fabsf(position.result.value.x - settled_x) > 0.1f ||
            fabsf(position.result.value.x) > 1.0f ||
            rohr_physics_contact_count_get(body_result.result.value) != 1 ||
            !resting_contact.detected || resting_contact.depth > 0.1f ||
            fabsf(velocities[body_index.result.value].x) > 0.1f ||
            fabsf(velocities[body_index.result.value].y) > 0.1f) {
        fprintf(stderr,
            "concave resting body: x=%f settled_x=%f y=%f\n",
            position.result.value.x, settled_x, position.result.value.y);
        if(!rohr_error_check(body_index))
            fprintf(stderr, "velocity=(%f,%f) angular=%f\n",
                velocities[body_index.result.value].x,
                velocities[body_index.result.value].y,
                angular_velocities[body_index.result.value]);
        rohr_engine_shutdown();
        return 4;
    }
    if(rohr_error_check(rohr_physics_impulse_apply(
            body_result.result.value, (Vec2D){20.0f, 0.0f}))) {
        rohr_engine_shutdown();
        return 5;
    }
    for(size_t tick = 0; tick < 180; tick += 1)
        rohr_system_physics_update(1.0 / 60.0);
    if(fabsf(velocities[body_index.result.value].x) > 0.2f) {
        fprintf(stderr, "friction failed to stop body: vx=%f\n",
            velocities[body_index.result.value].x);
        rohr_engine_shutdown();
        return 6;
    }
    rohr_engine_shutdown();
    return 0;
}
