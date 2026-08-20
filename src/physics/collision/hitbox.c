/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics.h"

#include "console.h"
#include "physics/physics_internal.h"
#include "shape_decomposition.h"

EngineResult physics_hitbox_set(Entity entity, Shape hitbox) {
    EntityIndex index;
    Shape prepared;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!physics_shape_collision_prepare(hitbox, &prepared))
        return error_result_error(ERROR_ENGINE_INVALID_SHAPE);
    entity_mask[index] |= ROHR_HIT_BOX;
    (void)ShapePool_store_at(&hit_boxes_pool, index, prepared);
    console_debug_write(LOG_ENGINE, "Set Entity: %d to have a hit box\n", entity);
    return error_result_value(true);
}

ShapeResult physics_global_hit_box_get(Entity entity) {
    RohrComponentMask filter = ROHR_HIT_BOX;
    EntityIndex index;

    if(entity_index_get(entity, &index) && entity_index_alive_check(index)) {
        if(entity_index_components_check(index, filter))
            return ERROR_RESULT_MAKE_VALUE(
                ShapeResult, world_hit_boxes[index]);
        return ERROR_RESULT_MAKE_ERROR(
            ShapeResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_ERROR(
        ShapeResult, ERROR_ENGINE_INVALID_ENTITY);
}
