#include "rohr.h"

#include <math.h>

int main(void) {
    EntityResult body_a;
    EntityResult body_b;
    EntityResult joint;
    JointAnchorIdResult anchor_a;
    JointAnchorIdResult anchor_b;
    JointAnchorListResult anchors;
    EntityIndexResult index_a;
    EntityIndexResult index_b;

    if(rohr_error_check(rohr_engine_init())) return 1;
    body_a = rohr_entity_add();
    body_b = rohr_entity_add();
    joint = rohr_entity_add();
    if(rohr_error_check(body_a) || rohr_error_check(body_b) || rohr_error_check(joint)) goto fail;
    if(rohr_error_check(rohr_physics_position_set(body_a.result.value, (Position){0.0f, 0.0f})) ||
            rohr_error_check(rohr_physics_position_set(body_b.result.value, (Position){4.0f, 0.0f})) ||
            rohr_error_check(rohr_physics_mass_set(body_a.result.value, 1.0f)) ||
            rohr_error_check(rohr_physics_mass_set(body_b.result.value, 1.0f)) ||
            rohr_error_check(rohr_physics_dynamic_set(body_a.result.value)) ||
            rohr_error_check(rohr_physics_dynamic_set(body_b.result.value))) goto fail;

    anchor_a = rohr_physics_joint_anchor_create(body_a.result.value, (Vec2D){0.0f, 0.0f});
    anchor_b = rohr_physics_joint_anchor_create(body_b.result.value, (Vec2D){0.0f, 0.0f});
    if(rohr_error_check(anchor_a) || rohr_error_check(anchor_b)) goto fail;
    anchors = rohr_physics_joint_anchors_get(body_a.result.value);
    if(rohr_error_check(anchors) || anchors.result.value.count != 1 ||
            anchors.result.value.values[0] != anchor_a.result.value) goto fail;
    if(rohr_error_check(rohr_physics_joint_pin_set(
                joint.result.value,
                anchor_a.result.value,
                anchor_b.result.value
            ))) goto fail;

    rohr_system_physics_update(0.0);
    index_a = rohr_entity_index_get(body_a.result.value);
    index_b = rohr_entity_index_get(body_b.result.value);
    if(rohr_error_check(index_a) || rohr_error_check(index_b) ||
            fabsf(positions[index_a.result.value].x - positions[index_b.result.value].x) > 0.0001f) goto fail;
    if(rohr_error_check(rohr_physics_acceleration_set(
                body_a.result.value, (Acceleration){-10.0f, 0.0f})) ||
            rohr_error_check(rohr_physics_acceleration_set(
                body_b.result.value, (Acceleration){10.0f, 0.0f}))) goto fail;
    rohr_system_physics_update(0.1);
    if(fabsf(positions[index_a.result.value].x -
            positions[index_b.result.value].x) > 0.0001f) goto fail;

    if(rohr_error_check(rohr_physics_joint_anchor_remove(anchor_a.result.value)) ||
            rohr_entity_alive_check(joint.result.value)) goto fail;

    anchor_a = rohr_physics_joint_anchor_create(body_a.result.value, (Vec2D){1.0f, 0.0f});
    anchor_b = rohr_physics_joint_anchor_create(body_b.result.value, (Vec2D){-1.0f, 0.0f});
    joint = rohr_entity_add();
    if(rohr_error_check(anchor_a) || rohr_error_check(anchor_b) || rohr_error_check(joint) ||
            rohr_error_check(rohr_physics_joint_weld_set(
                joint.result.value,
                anchor_a.result.value,
                anchor_b.result.value
            ))) goto fail;
    {
        EntityIndexResult joint_index = rohr_entity_index_get(joint.result.value);
        if(rohr_error_check(joint_index) || joints[joint_index.result.value].type != JOINT_WELD ||
                rohr_error_check(rohr_physics_joint_spring_set(
                    joint.result.value,
                    anchor_a.result.value,
                    anchor_b.result.value,
                    2.0f,
                    8.0f,
                    0.5f
                )) || joints[joint_index.result.value].type != JOINT_SPRING ||
                joints[joint_index.result.value].rest_length != 2.0f) goto fail;
    }
    if(rohr_error_check(rohr_entity_delete(body_a.result.value)) ||
            rohr_entity_alive_check(joint.result.value) ||
            !rohr_error_check(rohr_physics_joint_anchor_position_get(anchor_a.result.value))) goto fail;
    rohr_engine_shutdown();
    return 0;

fail:
    rohr_engine_shutdown();
    return 1;
}
