#include "physics.h"

#include "console.h"
#include "physics/physics_internal.h"

Vec2D physics_direction_between_positions(Position from, Position to) {
    Vec2D delta = {
        .x = to.x - from.x,
        .y = to.y - from.y
    };
    float distance = math_vector_magnitude(delta);

    if(distance <= 0.00001f) return (Vec2D){0};
    return (Vec2D){
        .x = delta.x / distance,
        .y = delta.y / distance
    };
}

EngineResult physics_group_entity_target_apply(
        GroupId group,
        float magnitude,
        Entity target,
        PhysicsGroupEntityTargetFn fn) {
    EntityGroupResult group_result;
    EntityGroup group_storage;
    size_t i;

    if(fn == NULL) return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    group_result = entity_group_get(group);
    if(group_result.kind == ERROR_RESULT_ERROR)
        return error_result_error(group_result.result.error);
    group_storage = group_result.result.value;
    if(group_storage.entities.objects == NULL || group_storage.entities.used == NULL)
        return error_result_value(true);
    for(i = 0; i < group_storage.entities.capacity; i += 1) {
        EngineResult result;
        Entity entity;

        if(group_storage.entities.used[i] == 0) continue;
        entity = group_storage.entities.objects[i];
        if(!entity_alive_check(entity)) continue;
        result = fn(entity, magnitude, target);
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }
    return error_result_value(true);
}

EngineResult physics_velocity_set(Entity entity, Velocity velocity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    (void)VelocityPool_store_at(&velocities_pool, index, velocity);
    console_debug_write(LOG_ENGINE, "Set Entity: %d Velocity: {x: %f, y: %f}\n",
        entity, velocity.x, velocity.y);
    return error_result_value(true);
}

EngineResult physics_velocity_toward_position_set(
        Entity entity, float speed, Position position) {
    EntityIndex index;
    Vec2D direction;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    direction = physics_direction_between_positions(positions[index], position);
    return physics_velocity_set(entity, (Velocity){
        .x = direction.x * speed,
        .y = direction.y * speed
    });
}

EngineResult physics_velocity_toward_entity_set(
        Entity entity, float speed, Entity target) {
    EntityIndex target_index;
    EngineResult result = physics_live_index_get(target, &target_index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    return physics_velocity_toward_position_set(entity, speed, positions[target_index]);
}

EngineResult physics_velocity_away_from_position_set(
        Entity entity, float speed, Position position) {
    return physics_velocity_toward_position_set(entity, -speed, position);
}

EngineResult physics_velocity_away_from_entity_set(
        Entity entity, float speed, Entity target) {
    return physics_velocity_toward_entity_set(entity, -speed, target);
}

EngineResult physics_group_velocity_toward_entity_set(
        GroupId group, float speed, Entity target) {
    return physics_group_entity_target_apply(
        group, speed, target, physics_velocity_toward_entity_set);
}

EngineResult physics_group_velocity_away_from_entity_set(
        GroupId group, float speed, Entity target) {
    return physics_group_entity_target_apply(
        group, speed, target, physics_velocity_away_from_entity_set);
}

EngineResult physics_entity_stop(Entity entity) {
    return physics_velocity_set(entity, (Velocity){0});
}

EngineResult physics_group_entities_stop(GroupId group) {
    return physics_group_entity_apply(group, physics_entity_stop);
}

EngineResult physics_position_set(Entity entity, Position position) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    (void)PositionPool_store_at(&positions_pool, index, position);
    console_debug_write(LOG_ENGINE, "Set Entity: %d Position: {x: %f, y: %f}\n",
        entity, position.x, position.y);
    return error_result_value(true);
}

PositionResult physics_position_get(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(PositionResult, result.result.error);
    if(!positions_pool.used[index])
        return ERROR_RESULT_MAKE_ERROR(
            PositionResult, ERROR_ENGINE_COMPONENT_MISSING);
    return ERROR_RESULT_MAKE_VALUE(PositionResult, positions[index]);
}

EngineResult physics_orientation_set(Entity entity, Orientation orientation) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    (void)OrientationPool_store_at(&orientations_pool, index, orientation);
    console_debug_write(LOG_ENGINE, "Set Entity: %d Orientation: %f\n",
        entity, orientation);
    return error_result_value(true);
}

EngineResult physics_angular_velocity_set(
        Entity entity, AngularVelocity velocity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = physics_dynamic_set(entity);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    (void)AngularVelocityPool_store_at(
        &angular_velocities_pool, index, velocity);
    console_debug_write(LOG_ENGINE, "Set Entity: %d Angular Velocity: %f\n",
        entity, velocity);
    return error_result_value(true);
}

AngularVelocityResult physics_angular_velocity_get(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(
            AngularVelocityResult, result.result.error);
    if(!angular_velocities_pool.used[index])
        return ERROR_RESULT_MAKE_ERROR(
            AngularVelocityResult, ERROR_ENGINE_COMPONENT_MISSING);
    return ERROR_RESULT_MAKE_VALUE(
        AngularVelocityResult, angular_velocities[index]);
}

EngineResult physics_angular_velocity_maximum_set(
        Entity entity, AngularVelocity maximum) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(maximum < 0.0f) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    if(AngularVelocityPool_store_at(
            &angular_velocity_maximums_pool, index, maximum).kind
            == ERROR_RESULT_ERROR)
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    return error_result_value(true);
}

AngularVelocityResult physics_angular_velocity_maximum_get(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(
            AngularVelocityResult, result.result.error);
    if(!angular_velocity_maximums_pool.used[index])
        return ERROR_RESULT_MAKE_ERROR(
            AngularVelocityResult, ERROR_ENGINE_COMPONENT_MISSING);
    return ERROR_RESULT_MAKE_VALUE(
        AngularVelocityResult, angular_velocity_maximums[index]);
}

EngineResult physics_angular_acceleration_set(
        Entity entity, AngularAcceleration acceleration) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = physics_dynamic_set(entity);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(AngularAccelerationPool_store_at(
            &angular_accelerations_pool, index, acceleration).kind
            == ERROR_RESULT_ERROR)
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    return error_result_value(true);
}
