/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics.h"

#include "console.h"
#include "physics/physics_internal.h"

EngineResult physics_restitution_set(Entity entity, Restitution restitution) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(restitution < 0) restitution = 0;
    else if(restitution > 1) restitution = 1;
    (void)RestitutionPool_store_at(&restitutions_pool, index, restitution);
    console_debug_write(LOG_ENGINE, "Set Entity: %d Restitution: %f\n",
        entity, restitution);
    return entity_components_add(entity, ROHR_COLLISION);
}

EngineResult physics_friction_set(Entity entity, float friction) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(friction < 0)
        (void)FrictionPool_store_at(&frictions_pool, index, 0);
    else if(friction >= 0)
        (void)FrictionPool_store_at(&frictions_pool, index, friction);
    return error_result_value(true);
}
