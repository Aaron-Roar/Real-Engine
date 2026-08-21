/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics.h"

#include "console.h"
#include "physics/physics_internal.h"
#include "physics/physics_step_internal.h"
#include "shape_decomposition.h"

#include <stdlib.h>

static EngineResult physics_hitbox_active_cache_sync(
        EntityIndex index, HitboxVariantList *variants) {
    if(variants == NULL || variants->count == 0 ||
            variants->active_index >= variants->count) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    if(ShapePool_store_at(&hit_boxes_pool, index,
            variants->values[variants->active_index]).kind == ERROR_RESULT_ERROR ||
            ShapePool_store_at(&world_hit_boxes_pool, index,
                variants->values[variants->active_index]).kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[index] |= ROHR_HIT_BOX;
    physics_step_hitbox_dirty_add(index);
    return error_result_value(true);
}

static HitboxVariantList *physics_hitbox_variants_get(EntityIndex index) {
    if(index >= hitbox_variants_pool.capacity ||
            !hitbox_variants_pool.used[index]) return NULL;
    return &hitbox_variants_pool.objects[index];
}

EngineResult physics_hitbox_add(Entity entity, Shape hitbox) {
    EntityIndex index;
    Shape prepared;
    HitboxVariantList *variants;
    Shape *values;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!physics_shape_collision_prepare(hitbox, &prepared))
        return error_result_error(ERROR_ENGINE_INVALID_SHAPE);
    variants = physics_hitbox_variants_get(index);
    if(variants == NULL) {
        HitboxVariantList empty = {0};
        if(HitboxVariantListPool_store_at(&hitbox_variants_pool, index,
                empty).kind == ERROR_RESULT_ERROR)
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        variants = &hitbox_variants_pool.objects[index];
    }
    if(variants->count == variants->capacity) {
        size_t capacity = variants->capacity == 0 ? 1 : variants->capacity * 2;
        values = realloc(variants->values, capacity * sizeof(*values));
        if(values == NULL)
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        variants->values = values;
        variants->capacity = capacity;
    }
    variants->values[variants->count++] = prepared;
    if(variants->count == 1) variants->active_index = 0;
    result = physics_hitbox_active_cache_sync(index, variants);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    console_debug_write(LOG_ENGINE, "Added hitbox variant to Entity: %d\n", entity);
    return error_result_value(true);
}

EngineResult physics_hitbox_set(Entity entity, Shape hitbox) {
    EntityIndex index;
    HitboxVariantList *variants;
    Shape prepared;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    variants = physics_hitbox_variants_get(index);
    if(variants == NULL || variants->count == 0)
        return physics_hitbox_add(entity, hitbox);
    if(!physics_shape_collision_prepare(hitbox, &prepared))
        return error_result_error(ERROR_ENGINE_INVALID_SHAPE);
    variants->values[variants->active_index] = prepared;
    return physics_hitbox_active_cache_sync(index, variants);
}

ShapeResult physics_hitbox_get(Entity entity) {
    EntityIndex index;
    HitboxVariantList *variants;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(ShapeResult, result.result.error);
    variants = physics_hitbox_variants_get(index);
    if(variants == NULL || variants->count == 0)
        return ERROR_RESULT_MAKE_ERROR(ShapeResult, ERROR_ENGINE_COMPONENT_MISSING);
    return ERROR_RESULT_MAKE_VALUE(ShapeResult,
        variants->values[variants->active_index]);
}

ShapeResult physics_hitbox_at_get(Entity entity, size_t variant_index) {
    EntityIndex index;
    HitboxVariantList *variants;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(ShapeResult, result.result.error);
    variants = physics_hitbox_variants_get(index);
    if(variants == NULL)
        return ERROR_RESULT_MAKE_ERROR(ShapeResult, ERROR_ENGINE_COMPONENT_MISSING);
    if(variant_index >= variants->count)
        return ERROR_RESULT_MAKE_ERROR(ShapeResult, ERROR_ENGINE_INDEX_OUT_OF_RANGE);
    return ERROR_RESULT_MAKE_VALUE(ShapeResult, variants->values[variant_index]);
}

EngineResult physics_hitbox_at_set(Entity entity, size_t variant_index, Shape hitbox) {
    EntityIndex index;
    HitboxVariantList *variants;
    Shape prepared;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    variants = physics_hitbox_variants_get(index);
    if(variants == NULL) return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    if(variant_index >= variants->count)
        return error_result_error(ERROR_ENGINE_INDEX_OUT_OF_RANGE);
    if(!physics_shape_collision_prepare(hitbox, &prepared))
        return error_result_error(ERROR_ENGINE_INVALID_SHAPE);
    variants->values[variant_index] = prepared;
    if(variant_index == variants->active_index)
        return physics_hitbox_active_cache_sync(index, variants);
    return error_result_value(true);
}

EngineResult physics_hitbox_remove(Entity entity) {
    EntityIndex index;
    HitboxVariantList *variants;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    variants = physics_hitbox_variants_get(index);
    if(variants != NULL) {
        free(variants->values);
        (void)HitboxVariantListPool_release_at(&hitbox_variants_pool, index);
    }
    if(index < hit_boxes_pool.capacity && hit_boxes_pool.used[index])
        (void)ShapePool_release_at(&hit_boxes_pool, index);
    if(index < world_hit_boxes_pool.capacity && world_hit_boxes_pool.used[index])
        (void)ShapePool_release_at(&world_hit_boxes_pool, index);
    entity_mask[index] &= ~ROHR_HIT_BOX;
    return error_result_value(true);
}

EngineResult physics_hitbox_at_remove(Entity entity, size_t variant_index) {
    EntityIndex index;
    HitboxVariantList *variants;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    variants = physics_hitbox_variants_get(index);
    if(variants == NULL) return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    if(variant_index >= variants->count)
        return error_result_error(ERROR_ENGINE_INDEX_OUT_OF_RANGE);
    if(variants->count == 1) return physics_hitbox_remove(entity);
    for(size_t i = variant_index + 1; i < variants->count; i += 1)
        variants->values[i - 1] = variants->values[i];
    variants->count -= 1;
    if(variant_index < variants->active_index) variants->active_index -= 1;
    else if(variant_index == variants->active_index &&
            variants->active_index >= variants->count)
        variants->active_index = variants->count - 1;
    return physics_hitbox_active_cache_sync(index, variants);
}

HitboxIndexResult physics_hitbox_count_get(Entity entity) {
    EntityIndex index;
    HitboxVariantList *variants;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(HitboxIndexResult, result.result.error);
    variants = physics_hitbox_variants_get(index);
    return ERROR_RESULT_MAKE_VALUE(HitboxIndexResult,
        variants == NULL ? 0 : variants->count);
}

HitboxIndexResult physics_hitbox_active_index_get(Entity entity) {
    EntityIndex index;
    HitboxVariantList *variants;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(HitboxIndexResult, result.result.error);
    variants = physics_hitbox_variants_get(index);
    if(variants == NULL || variants->count == 0)
        return ERROR_RESULT_MAKE_ERROR(HitboxIndexResult,
            ERROR_ENGINE_COMPONENT_MISSING);
    return ERROR_RESULT_MAKE_VALUE(HitboxIndexResult, variants->active_index);
}

EngineResult physics_hitbox_active_index_set(Entity entity, size_t variant_index) {
    EntityIndex index;
    HitboxVariantList *variants;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    variants = physics_hitbox_variants_get(index);
    if(variants == NULL) return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    if(variant_index >= variants->count)
        return error_result_error(ERROR_ENGINE_INDEX_OUT_OF_RANGE);
    variants->active_index = variant_index;
    return physics_hitbox_active_cache_sync(index, variants);
}

ShapeResult physics_global_hit_box_get(Entity entity) {
    RohrComponentMask filter = ROHR_HIT_BOX;
    EntityIndex index;

    if(entity_index_get(entity, &index) && entity_index_alive_check(index)) {
        if(entity_index_components_check(index, filter))
            return ERROR_RESULT_MAKE_VALUE(ShapeResult, world_hit_boxes[index]);
        return ERROR_RESULT_MAKE_ERROR(ShapeResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_ERROR(ShapeResult, ERROR_ENGINE_INVALID_ENTITY);
}
