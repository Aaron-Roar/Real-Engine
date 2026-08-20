/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics.h"

#include "console.h"
#include "physics/physics_internal.h"

#include <math.h>
#include <string.h>

static bool kinematic_explicit[MAX_ENTITIES];

void physics_body_state_table_init(void) {
    memset(kinematic_explicit, 0, sizeof(kinematic_explicit));
}

void physics_body_state_entity_clear(EntityIndex index) {
    if(index < MAX_ENTITIES) kinematic_explicit[index] = false;
}

void physics_body_state_sync(EntityIndex index) {
    bool dynamic;
    bool positive_mass;

    if(!entity_index_alive_check(index)) return;
    dynamic = entity_index_components_check(index, ROHR_DYNAMIC) &&
        !entity_index_components_check(index, ROHR_STATIC);
    positive_mass = entity_index_components_check(index, ROHR_MASS) &&
        mass[index] > 0.0f;
    if(dynamic && (kinematic_explicit[index] || !positive_mass))
        entity_mask[index] |= ROHR_KINEMATIC_DRIVEN;
    else
        entity_mask[index] &= ~ROHR_KINEMATIC_DRIVEN;
}

bool physics_entity_held_get(EntityIndex index) {
    return entity_index_alive_check(index) &&
        entity_index_components_check(index, ROHR_HOLD);
}

bool physics_entity_movable_get(EntityIndex index) {
    return entity_index_alive_check(index) &&
        entity_index_components_check(index, ROHR_DYNAMIC) &&
        !entity_index_components_check(index, ROHR_STATIC) &&
        !physics_entity_held_get(index);
}

bool physics_entity_simulated_get(EntityIndex index) {
    return physics_entity_movable_get(index) &&
        !entity_index_components_check(index, ROHR_KINEMATIC_DRIVEN) &&
        entity_index_components_check(index, ROHR_MASS) && mass[index] > 0.0f;
}

EngineResult physics_mass_set(Entity entity, Mass mass_value) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!isfinite(mass_value) || mass_value < 0.0f)
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    entity_mask[index] |= ROHR_MASS;
    (void)MassPool_store_at(&mass_pool, index, mass_value);
    physics_body_state_sync(index);
    console_debug_write(LOG_ENGINE, "Set Entity: %d Mass: %f\n",
        entity, mass_value);
    return error_result_value(true);
}

EngineResult physics_mass_remove(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(index < mass_pool.capacity && mass_pool.used[index])
        (void)MassPool_release_at(&mass_pool, index);
    result = entity_components_delete(entity, ROHR_MASS);
    if(result.kind == ERROR_RESULT_VALUE) physics_body_state_sync(index);
    return result;
}

bool physics_mass_check(Entity entity) {
    return entity_components_check(entity, ROHR_MASS);
}

EngineResult physics_kinematic_driven_set(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    kinematic_explicit[index] = true;
    physics_body_state_sync(index);
    return error_result_value(true);
}

EngineResult physics_kinematic_driven_remove(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(entity_index_components_check(index, ROHR_DYNAMIC) &&
            (!entity_index_components_check(index, ROHR_MASS) ||
                mass[index] <= 0.0f))
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    kinematic_explicit[index] = false;
    physics_body_state_sync(index);
    return error_result_value(true);
}

bool physics_kinematic_driven_check(Entity entity) {
    return entity_components_check(entity, ROHR_KINEMATIC_DRIVEN);
}

EngineResult physics_dynamic_set(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = entity_components_add(entity, ROHR_DYNAMIC);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = entity_components_delete(entity, ROHR_STATIC);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    physics_body_state_sync(index);
    console_debug_write(LOG_ENGINE, "Set Entity: %d to ROHR_DYNAMIC\n", entity);
    return error_result_value(true);
}

EngineResult physics_static_set(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = entity_components_add(entity, ROHR_STATIC);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = entity_components_delete(entity, ROHR_DYNAMIC);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    physics_body_state_sync(index);
    console_debug_write(LOG_ENGINE, "Set Entity: %d to ROHR_STATIC\n", entity);
    return error_result_value(true);
}

EngineResult physics_entity_hold(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = entity_components_add(entity, ROHR_HOLD);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    console_debug_write(LOG_ENGINE, "Hold Entity: %d\n", entity);
    return error_result_value(true);
}

EngineResult physics_entity_unhold(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    result = entity_components_delete(entity, ROHR_HOLD);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    console_debug_write(LOG_ENGINE, "Unhold Entity: %d\n", entity);
    return error_result_value(true);
}

EngineResult physics_group_entities_hold(GroupId group) {
    return physics_group_entity_apply(group, physics_entity_hold);
}

EngineResult physics_group_entities_unhold(GroupId group) {
    return physics_group_entity_apply(group, physics_entity_unhold);
}
