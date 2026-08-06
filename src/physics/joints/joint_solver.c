#include "physics/physics_step_internal.h"
#include "systems.h"
#include "core/engine_internal.h"
#include "math2d.h"
#include <math.h>

static Velocity system_point_velocity(Entity entity, Vec2D world_offset) {
    EntityIndex index;

    if(!entity_index_get(entity, &index) || !entity_index_alive_check(index)) {
        return (Velocity){0};
    }
    if(!physics_entity_movable_get(index)) {
        return (Velocity){0};
    }
    Vec2D angular_part = math_angular_velocity_cross_vec(
        angular_velocities[index],
        world_offset
    );

    return (Velocity){
        .x = velocities[index].x + angular_part.x,
        .y = velocities[index].y + angular_part.y
    };
}
static void system_joint_force_for_one_tick_add(Entity target, Force force) {
    EntityIndex target_index;

    if(!entity_index_get(target, &target_index) || !entity_index_alive_check(target_index)) {
        return;
    }
    if(!physics_entity_movable_get(target_index)) {
        return;
    }
    if(!entity_index_components_check(target_index, MASS) || mass[target_index] == 0.0f) {
        return;
    }
    //Entity force_entity = set_force(target, force);
    force_accelerations[target_index].x += force.x/mass[target_index];
    force_accelerations[target_index].y += force.y/mass[target_index];

    //if(force_entity == 0) {
    //    return;
    //}

    //set_life_time(
    //    force_entity,
    //    0.0,
    //    engine_tick_get() + 1
    //);
}
static void system_joint_torque_for_one_tick_add(Entity target, Torque torque) {
    EntityIndex target_index;

    if(!entity_index_get(target, &target_index) || !entity_index_alive_check(target_index)) {
        return;
    }
    if(!physics_entity_movable_get(target_index)) {
        return;
    }
    if(!entity_index_components_check(target_index, MASS) || mass[target_index] == 0.0f) {
        return;
    }
    torque_angular_accelerations[target_index] += torque/physics_polygon_moment_of_inertia(hit_boxes[target_index], mass[target_index]);
    //Entity torque_entity = set_torque(target, torque);

    //if(torque_entity == 0) {
    //    return;
    //}

    //set_life_time(
    //    torque_entity,
    //    0.0,
    //    engine_tick_get() + 1
    //);
}

static void system_joint_force_at_point_for_one_tick_add(Entity target, Position world_point, Force force) {
    EntityIndex target_index;

    if(!entity_index_get(target, &target_index) || !entity_index_alive_check(target_index)) {
        return;
    }

    if(!physics_entity_movable_get(target_index)) {
        return;
    }

    EntityResult force_result = physics_force_create(target, force);

    if(force_result.kind == ERROR_RESULT_VALUE) {
        entity_life_time_set(force_result.result.value, 0.0, engine_tick_get() + 1);
    }

    Vec2D r = {
        .x = world_point.x - positions[target_index].x,
        .y = world_point.y - positions[target_index].y
    };

    Torque torque = math_cross_2d(r, force);

    EntityResult torque_result = physics_torque_create(target, torque);

    if(torque_result.kind == ERROR_RESULT_VALUE) {
        entity_life_time_set(torque_result.result.value, 0.0, engine_tick_get() + 1);
    }
}

static float system_joint_inverse_inertia(EntityIndex index) {
    float inertia;

    if(!physics_entity_movable_get(index) ||
            !entity_index_components_check(index, MASS | HIT_BOX) || mass[index] <= 0.0f) return 0.0f;
    inertia = physics_polygon_moment_of_inertia(hit_boxes[index], mass[index]);
    return inertia > 0.0f ? 1.0f / inertia : 0.0f;
}

static void system_rigid_anchor_axis_solve(
        EntityIndex a,
        EntityIndex b,
        Vec2D offset_a,
        Vec2D offset_b,
        Vec2D position_error,
        Vec2D velocity_error,
        Vec2D axis
) {
    float inverse_mass_a = physics_entity_movable_get(a) && mass[a] > 0.0f ? 1.0f / mass[a] : 0.0f;
    float inverse_mass_b = physics_entity_movable_get(b) && mass[b] > 0.0f ? 1.0f / mass[b] : 0.0f;
    float inverse_inertia_a = system_joint_inverse_inertia(a);
    float inverse_inertia_b = system_joint_inverse_inertia(b);
    float lever_a = math_cross_2d(offset_a, axis);
    float lever_b = math_cross_2d(offset_b, axis);
    float effective_inverse_mass = inverse_mass_a + inverse_mass_b +
        lever_a * lever_a * inverse_inertia_a + lever_b * lever_b * inverse_inertia_b;
    float position_impulse;
    float velocity_impulse;

    if(effective_inverse_mass <= 0.0f) return;
    position_impulse = math_dot_product(position_error, axis) / effective_inverse_mass;
    velocity_impulse = math_dot_product(velocity_error, axis) / effective_inverse_mass;
    positions[a].x += axis.x * position_impulse * inverse_mass_a;
    positions[a].y += axis.y * position_impulse * inverse_mass_a;
    orientations[a] += lever_a * position_impulse * inverse_inertia_a;
    positions[b].x -= axis.x * position_impulse * inverse_mass_b;
    positions[b].y -= axis.y * position_impulse * inverse_mass_b;
    orientations[b] -= lever_b * position_impulse * inverse_inertia_b;
    velocities[a].x += axis.x * velocity_impulse * inverse_mass_a;
    velocities[a].y += axis.y * velocity_impulse * inverse_mass_a;
    angular_velocities[a] += lever_a * velocity_impulse * inverse_inertia_a;
    velocities[b].x -= axis.x * velocity_impulse * inverse_mass_b;
    velocities[b].y -= axis.y * velocity_impulse * inverse_mass_b;
    angular_velocities[b] -= lever_b * velocity_impulse * inverse_inertia_b;
}

static void system_pin_joint_apply(Entity joint_entity) {
    Joint joint = joints[joint_entity];

    Entity a = joint.a;
    Entity b = joint.b;
    EntityIndex a_index;
    EntityIndex b_index;

    if(!entity_index_get(a, &a_index) || !entity_index_alive_check(a_index) || !entity_index_get(b, &b_index) || !entity_index_alive_check(b_index)) {
        physics_step_entity_by_index_delete(joint_entity);
        return;
    }

    Vec2D offset_a;
    Vec2D offset_b;
    Position world_anchor_a;
    Position world_anchor_b;

    if(joint.anchor_a != JOINT_ANCHOR_INVALID && joint.anchor_b != JOINT_ANCHOR_INVALID) {
        JointAnchorPositionResult anchor_a = physics_joint_anchor_world_position_get(joint.anchor_a);
        JointAnchorPositionResult anchor_b = physics_joint_anchor_world_position_get(joint.anchor_b);
        if(anchor_a.kind == ERROR_RESULT_ERROR || anchor_b.kind == ERROR_RESULT_ERROR) {
            physics_step_entity_by_index_delete(joint_entity);
            return;
        }
        world_anchor_a = anchor_a.result.value;
        world_anchor_b = anchor_b.result.value;
        offset_a = math_vector_subtract(world_anchor_a, positions[a_index]);
        offset_b = math_vector_subtract(world_anchor_b, positions[b_index]);
    } else {
        offset_a = math_vector_rotate(joint.local_anchor_a, orientations[a_index]);
        offset_b = math_vector_rotate(joint.local_anchor_b, orientations[b_index]);
        world_anchor_a = (Position){positions[a_index].x + offset_a.x, positions[a_index].y + offset_a.y};
        world_anchor_b = (Position){positions[b_index].x + offset_b.x, positions[b_index].y + offset_b.y};
    }

    Vec2D error = {
        .x = world_anchor_b.x - world_anchor_a.x,
        .y = world_anchor_b.y - world_anchor_a.y
    };

    Velocity velocity_a = system_point_velocity(a, offset_a);
    Velocity velocity_b = system_point_velocity(b, offset_b);

    Vec2D relative_velocity = {
        .x = velocity_b.x - velocity_a.x,
        .y = velocity_b.y - velocity_a.y
    };

    if(joint.anchor_a != JOINT_ANCHOR_INVALID && joint.anchor_b != JOINT_ANCHOR_INVALID) {
        system_rigid_anchor_axis_solve(a_index, b_index, offset_a, offset_b,
            error, relative_velocity, (Vec2D){1.0f, 0.0f});
        system_rigid_anchor_axis_solve(a_index, b_index, offset_a, offset_b,
            error, relative_velocity, (Vec2D){0.0f, 1.0f});
        return;
    }

    Force force_on_a = {
        .x = joint.stiffness * error.x + joint.damping * relative_velocity.x,
        .y = joint.stiffness * error.y + joint.damping * relative_velocity.y
    };

    Force force_on_b = {
        .x = -force_on_a.x,
        .y = -force_on_a.y
    };

    system_joint_force_at_point_for_one_tick_add(
        a,
        world_anchor_a,
        force_on_a
    );

    system_joint_force_at_point_for_one_tick_add(
        b,
        world_anchor_b,
        force_on_b
    );
}

static void system_weld_joint_apply(Entity joint_entity) {
    Joint joint = joints[joint_entity];
    EntityIndex a_index;
    EntityIndex b_index;
    float inverse_inertia_a = 0.0f;
    float inverse_inertia_b = 0.0f;
    float inverse_inertia_sum;
    float angle_error;
    float angular_velocity_error;

    system_pin_joint_apply(joint_entity);
    if(!entity_index_get(joint.a, &a_index) || !entity_index_alive_check(a_index) ||
            !entity_index_get(joint.b, &b_index) || !entity_index_alive_check(b_index)) return;
    if(physics_entity_movable_get(a_index) && entity_index_components_check(a_index, MASS | HIT_BOX)) {
        float inertia = physics_polygon_moment_of_inertia(hit_boxes[a_index], mass[a_index]);
        if(inertia > 0.0f) inverse_inertia_a = 1.0f / inertia;
    }
    if(physics_entity_movable_get(b_index) && entity_index_components_check(b_index, MASS | HIT_BOX)) {
        float inertia = physics_polygon_moment_of_inertia(hit_boxes[b_index], mass[b_index]);
        if(inertia > 0.0f) inverse_inertia_b = 1.0f / inertia;
    }
    inverse_inertia_sum = inverse_inertia_a + inverse_inertia_b;
    if(inverse_inertia_sum <= 0.0f) return;
    angle_error = (orientations[b_index] - orientations[a_index]) - joint.rest_angle;
    angular_velocity_error = angular_velocities[b_index] - angular_velocities[a_index];
    orientations[a_index] += angle_error * inverse_inertia_a / inverse_inertia_sum;
    orientations[b_index] -= angle_error * inverse_inertia_b / inverse_inertia_sum;
    angular_velocities[a_index] += angular_velocity_error * inverse_inertia_a / inverse_inertia_sum;
    angular_velocities[b_index] -= angular_velocity_error * inverse_inertia_b / inverse_inertia_sum;
}

static void system_spring_joint_apply(Entity joint_entity) {
        Joint joint = joints[joint_entity];
        if(joint.type != JOINT_SPRING) {
            return;
        }

        Entity a = joint.a;
        Entity b = joint.b;
        EntityIndex a_index;
        EntityIndex b_index;

        if(!entity_index_get(a, &a_index) || !entity_index_alive_check(a_index) || !entity_index_get(b, &b_index) || !entity_index_alive_check(b_index)) {
            physics_step_entity_by_index_delete(joint_entity);
            return;
        }

        Vec2D offset_a;
        Vec2D offset_b;
        Position world_anchor_a;
        Position world_anchor_b;
        if(joint.anchor_a != JOINT_ANCHOR_INVALID && joint.anchor_b != JOINT_ANCHOR_INVALID) {
            JointAnchorPositionResult anchor_a = physics_joint_anchor_world_position_get(joint.anchor_a);
            JointAnchorPositionResult anchor_b = physics_joint_anchor_world_position_get(joint.anchor_b);
            if(anchor_a.kind == ERROR_RESULT_ERROR || anchor_b.kind == ERROR_RESULT_ERROR) {
                physics_step_entity_by_index_delete(joint_entity);
                return;
            }
            world_anchor_a = anchor_a.result.value;
            world_anchor_b = anchor_b.result.value;
            offset_a = math_vector_subtract(world_anchor_a, positions[a_index]);
            offset_b = math_vector_subtract(world_anchor_b, positions[b_index]);
        } else {
            offset_a = math_vector_rotate(joint.local_anchor_a, orientations[a_index]);
            offset_b = math_vector_rotate(joint.local_anchor_b, orientations[b_index]);
            world_anchor_a = (Position){positions[a_index].x + offset_a.x, positions[a_index].y + offset_a.y};
            world_anchor_b = (Position){positions[b_index].x + offset_b.x, positions[b_index].y + offset_b.y};
        }

        Vec2D delta = {
            .x = world_anchor_b.x - world_anchor_a.x,
            .y = world_anchor_b.y - world_anchor_a.y
        };

        float length = math_vector_magnitude(delta);

        if(length <= 0.0) {
            return;
        }

        Vec2D normal = {
            .x = delta.x / length,
            .y = delta.y / length
        };

        Velocity velocity_a = system_point_velocity(a, offset_a);
        Velocity velocity_b = system_point_velocity(b, offset_b);

        Vec2D relative_velocity = {
            .x = velocity_b.x - velocity_a.x,
            .y = velocity_b.y - velocity_a.y
        };

        float relative_speed = math_dot_product(relative_velocity, normal);

        float stretch = length - joint.rest_length;

        float force_magnitude =
            joint.stiffness * stretch +
            joint.damping * relative_speed;

        Force force_on_a = {
            .x = normal.x * force_magnitude,
            .y = normal.y * force_magnitude
        };

        Force force_on_b = {
            .x = -force_on_a.x,
            .y = -force_on_a.y
        };

        Vec2D r_a = {
            .x = world_anchor_a.x - positions[a_index].x,
            .y = world_anchor_a.y - positions[a_index].y
        };

        Vec2D r_b = {
            .x = world_anchor_b.x - positions[b_index].x,
            .y = world_anchor_b.y - positions[b_index].y
        };

        Torque torque_on_a = math_cross_2d(r_a, force_on_a);
        Torque torque_on_b = math_cross_2d(r_b, force_on_b);

        system_joint_force_for_one_tick_add(a, force_on_a);
        system_joint_force_for_one_tick_add(b, force_on_b);

        system_joint_torque_for_one_tick_add(a, torque_on_a);
        system_joint_torque_for_one_tick_add(b, torque_on_b);

}
static void system_joint_spring_forces_apply(void) {
    for(Entity joint_entity = 0; joint_entity < MAX_ENTITIES; joint_entity += 1) {
        if(!entity_index_alive_check(joint_entity) ||
                !entity_index_components_check(joint_entity, JOINT) ||
                joints[joint_entity].type != JOINT_SPRING) continue;
        system_spring_joint_apply(joint_entity);
    }
}

static void physics_step_joint_constraints_gather(void) {
    joint_constraint_list_clear(&physics_step_joint_constraints);
    for(EntityIndex joint_entity = 0;
            joint_entity < joints_pool.capacity;
            joint_entity += 1) {
        if(!joints_pool.used[joint_entity] ||
                !entity_index_alive_check(joint_entity) ||
                !entity_index_components_check(joint_entity, JOINT)) continue;
        if(joints[joint_entity].type == JOINT_PIN ||
                joints[joint_entity].type == JOINT_WELD) {
            (void)joint_constraint_list_append(
                &physics_step_joint_constraints,
                joint_entity);
        }
    }
}

static void physics_step_joint_constraints_apply(void) {
    for(size_t i = 0; i < physics_step_joint_constraints.count; i += 1) {
        EntityIndex joint_entity = physics_step_joint_constraints.values[i];

        if(!entity_index_alive_check(joint_entity) ||
                !entity_index_components_check(joint_entity, JOINT)) continue;
        if(joints[joint_entity].type == JOINT_PIN) {
            system_pin_joint_apply(joint_entity);
        } else if(joints[joint_entity].type == JOINT_WELD) {
            system_weld_joint_apply(joint_entity);
        }
    }
}

void physics_joint_spring_forces_apply(void) {
    system_joint_spring_forces_apply();
}

void physics_joint_constraints_gather(void) {
    physics_step_joint_constraints_gather();
}

void physics_joint_constraints_solve(void) {
    physics_step_joint_constraints_apply();
    for(size_t i = 0; i < physics_step_joint_constraints.count; i += 1) {
        EntityIndex joint_entity = physics_step_joint_constraints.values[i];
        EntityIndex index;

        if(!entity_index_alive_check(joint_entity)) continue;
        if(entity_index_get(joints[joint_entity].a, &index)) {
            physics_step_hitbox_dirty_add(index);
        }
        if(entity_index_get(joints[joint_entity].b, &index)) {
            physics_step_hitbox_dirty_add(index);
        }
    }
    physics_step_hitbox_dirty_flush();
}
