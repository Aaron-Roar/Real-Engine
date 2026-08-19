#include "physics.h"

#include "console.h"
#include "physics/physics_internal.h"

#include <math.h>

static Acceleration physics_gravity = ROHR_PHYSICS_GRAVITY_DEFAULT;

void physics_force_state_init(void) {
    physics_gravity = ROHR_PHYSICS_GRAVITY_DEFAULT;
}

EngineResult physics_impulse_apply(Entity entity, Vec2D impulse) {
    EntityIndex index;
    Velocity velocity;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!physics_entity_simulated_get(index)) return error_result_value(true);
    if(!entity_index_components_check(index, ROHR_MASS))
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    if(mass[index] == 0.0f) return error_result_value(true);
    velocity = (Velocity){
        .x = velocities[index].x + impulse.x / mass[index],
        .y = velocities[index].y + impulse.y / mass[index]
    };
    (void)VelocityPool_store_at(&velocities_pool, index, velocity);
    console_debug_write(
        LOG_ENGINE,
        "Apply Entity: %d Impulse: {x: %f, y: %f} Velocity: {x: %f, y: %f}\n",
        entity, impulse.x, impulse.y, velocity.x, velocity.y);
    return error_result_value(true);
}

EntityResult physics_force_create(Entity entity, Force force) {
    EntityIndex index;
    EntityResult force_result;
    EngineResult result = physics_live_index_get(entity, &index);
    Entity force_entity;
    EntityIndex force_index;

    if(result.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(EntityResult, result.result.error);
    force_result = entity_add();
    if(force_result.kind == ERROR_RESULT_ERROR) return force_result;
    force_entity = force_result.result.value;
    if(!(entity_index_get(force_entity, &force_index) &&
            entity_index_alive_check(force_index)))
        return ERROR_RESULT_MAKE_ERROR(
            EntityResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    (void)ForcePool_store_at(&forces_pool, force_index, force);
    (void)TargetPool_store_at(&targets_pool, force_index, entity);
    entity_mask[force_index] |= ROHR_TARGETABLE | ROHR_FORCE;
    console_debug_write(LOG_ENGINE, "Set Entity: %d Force: {x: %f, y: %f}\n",
        entity, force.x, force.y);
    return ERROR_RESULT_MAKE_VALUE(EntityResult, force_entity);
}

EngineResult physics_force_component_set(Entity entity, Force force) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(ForcePool_store_at(&forces_pool, index, force).kind
            == ERROR_RESULT_ERROR)
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    entity_mask[index] |= ROHR_FORCE;
    return error_result_value(true);
}

EngineResult physics_force_for_one_tick_apply(Entity entity, Force force) {
    EntityResult force_result = physics_force_create(entity, force);
    EngineResult result;

    if(force_result.kind == ERROR_RESULT_ERROR)
        return error_result_error(force_result.result.error);
    result = entity_life_time_set(
        force_result.result.value, 0.0, engine_tick_get() + 1);
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)entity_delete(force_result.result.value);
        return result;
    }
    return error_result_value(true);
}

EngineResult physics_acceleration_set(
        Entity entity, Acceleration acceleration) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    (void)AccelerationPool_store_at(
        &accelerations_pool, index, acceleration);
    console_debug_write(
        LOG_ENGINE,
        "Set Entity: %d Acceleration: {x: %f, y: %f}\n",
        entity, acceleration.x, acceleration.y);
    return error_result_value(true);
}

EngineResult physics_acceleration_toward_position_set(
        Entity entity,
        float acceleration_magnitude,
        Position position) {
    EntityIndex index;
    Vec2D direction;
    Acceleration acceleration;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    direction = physics_direction_between_positions(positions[index], position);
    acceleration = (Acceleration){
        .x = direction.x * acceleration_magnitude,
        .y = direction.y * acceleration_magnitude
    };
    (void)AccelerationPool_store_at(
        &accelerations_pool, index, acceleration);
    console_debug_write(
        LOG_ENGINE,
        "Set Entity: %d Acceleration Toward Position: {x: %f, y: %f} Magnitude: %f Acceleration: {x: %f, y: %f}\n",
        entity, position.x, position.y, acceleration_magnitude,
        acceleration.x, acceleration.y);
    return error_result_value(true);
}

EngineResult physics_acceleration_toward_entity_set(
        Entity entity, float acceleration_magnitude, Entity target) {
    EntityIndex target_index;
    EngineResult result = physics_live_index_get(target, &target_index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    return physics_acceleration_toward_position_set(
        entity, acceleration_magnitude, positions[target_index]);
}

EngineResult physics_acceleration_away_from_position_set(
        Entity entity, float acceleration_magnitude, Position position) {
    return physics_acceleration_toward_position_set(
        entity, -acceleration_magnitude, position);
}

EngineResult physics_acceleration_away_from_entity_set(
        Entity entity, float acceleration_magnitude, Entity target) {
    return physics_acceleration_toward_entity_set(
        entity, -acceleration_magnitude, target);
}

EngineResult physics_group_acceleration_toward_entity_set(
        GroupId group, float acceleration_magnitude, Entity target) {
    return physics_group_entity_target_apply(
        group, acceleration_magnitude, target,
        physics_acceleration_toward_entity_set);
}

EngineResult physics_group_acceleration_away_from_entity_set(
        GroupId group, float acceleration_magnitude, Entity target) {
    return physics_group_entity_target_apply(
        group, acceleration_magnitude, target,
        physics_acceleration_away_from_entity_set);
}

EntityResult physics_torque_create(Entity entity, Torque torque) {
    EntityIndex index;
    EntityResult torque_result;
    EngineResult result = physics_live_index_get(entity, &index);
    Entity torque_entity;
    EntityIndex torque_index;

    if(result.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(EntityResult, result.result.error);
    torque_result = entity_add();
    if(torque_result.kind == ERROR_RESULT_ERROR) return torque_result;
    torque_entity = torque_result.result.value;
    if(!(entity_index_get(torque_entity, &torque_index) &&
            entity_index_alive_check(torque_index)))
        return ERROR_RESULT_MAKE_ERROR(
            EntityResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    (void)TorquePool_store_at(&torques_pool, torque_index, torque);
    (void)TargetPool_store_at(&targets_pool, torque_index, entity);
    entity_mask[torque_index] |= ROHR_TARGETABLE | ROHR_TORQUE;
    console_debug_write(LOG_ENGINE, "Set Entity: %d Torque: %f\n",
        entity, torque);
    return ERROR_RESULT_MAKE_VALUE(EntityResult, torque_entity);
}

EngineResult physics_torque_component_set(Entity entity, Torque torque) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(TorquePool_store_at(&torques_pool, index, torque).kind
            == ERROR_RESULT_ERROR)
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    entity_mask[index] |= ROHR_TORQUE;
    return error_result_value(true);
}

EngineResult physics_torque_for_one_tick_apply(Entity entity, Torque torque) {
    EntityResult torque_result = physics_torque_create(entity, torque);
    EngineResult result;

    if(torque_result.kind == ERROR_RESULT_ERROR)
        return error_result_error(torque_result.result.error);
    result = entity_life_time_set(
        torque_result.result.value, 0.0, engine_tick_get() + 1);
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)entity_delete(torque_result.result.value);
        return result;
    }
    return error_result_value(true);
}

EngineResult physics_gravity_set(Acceleration gravity) {
    if(!isfinite(gravity.x) || !isfinite(gravity.y))
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    physics_gravity = gravity;
    return error_result_value(true);
}

Acceleration physics_gravity_get(void) {
    return physics_gravity;
}

EngineResult physics_gravity_enable(Entity entity) {
    return entity_components_add(entity, ROHR_GRAVITY);
}

EngineResult physics_gravity_disable(Entity entity) {
    return entity_components_delete(entity, ROHR_GRAVITY);
}

bool physics_gravity_check(Entity entity) {
    return entity_components_check(entity, ROHR_GRAVITY);
}
