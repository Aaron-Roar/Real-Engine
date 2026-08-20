/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics.h"

#include "physics/physics_internal.h"

EngineResult physics_angle_lock_set(
        Entity entity, Orientation min, Orientation max) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = entity_components_add(entity, ROHR_ANGLE_LOCK);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    (void)AngleLockPool_store_at(&angle_locks_pool, index, (AngleLock){
        .min = min,
        .max = max
    });
    return error_result_value(true);
}

EngineResult physics_axis_lock_set(
        Entity entity, Axis axis, Position axis_point) {
    EntityIndex index;
    Axis normalized_axis;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = entity_components_add(entity, ROHR_AXIS_LOCK);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    normalized_axis = math_vector_normalize(axis);
    (void)AxisLockPool_store_at(&axis_locks_pool, index, (AxisLock){
        .axis = (Axis){
            .x = normalized_axis.x,
            .y = normalized_axis.y
        },
        .point_on_axis = (Position){
            .x = axis_point.x,
            .y = axis_point.y
        }
    });
    return error_result_value(true);
}

EngineResult physics_transform_lock_set(
        Entity driven,
        Entity driver,
        Vec2D local_offset,
        Orientation local_angle,
        bool lock_position,
        bool lock_orientation,
        bool inherit_velocity) {
    EntityIndex driven_index;
    EntityIndex driver_index;
    EngineResult result = physics_live_index_get(driven, &driven_index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = physics_live_index_get(driver, &driver_index);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = entity_components_add(driven, ROHR_TRANSFORM_LOCK);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    (void)TransformLockPool_store_at(
        &transform_locks_pool, driven_index, (TransformLock){
            .driver = driver,
            .local_offset = local_offset,
            .local_angle = local_angle,
            .lock_position = lock_position,
            .lock_orientation = lock_orientation,
            .inherit_velocity = inherit_velocity
        });
    return error_result_value(true);
}

EngineResult physics_transform_lock_remove(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = entity_components_delete(entity, ROHR_TRANSFORM_LOCK);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(index < transform_locks_pool.capacity && transform_locks_pool.used[index])
        (void)TransformLockPool_release_at(&transform_locks_pool, index);
    return error_result_value(true);
}

EngineResult physics_transform_lock_current_transform_set(
        Entity driven,
        Entity driver,
        bool lock_position,
        bool lock_orientation,
        bool inherit_velocity) {
    EntityIndex driven_index;
    EntityIndex driver_index;
    Vec2D world_offset;
    Vec2D local_offset;
    Orientation local_angle;
    EngineResult result = physics_live_index_get(driven, &driven_index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = physics_live_index_get(driver, &driver_index);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    world_offset = (Vec2D){
        .x = positions[driven_index].x - positions[driver_index].x,
        .y = positions[driven_index].y - positions[driver_index].y
    };
    local_offset = math_vector_rotate(
        world_offset, -orientations[driver_index]);
    local_angle = orientations[driven_index] - orientations[driver_index];
    return physics_transform_lock_set(
        driven,
        driver,
        local_offset,
        local_angle,
        lock_position,
        lock_orientation,
        inherit_velocity
    );
}
