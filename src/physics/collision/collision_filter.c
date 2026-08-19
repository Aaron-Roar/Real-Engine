#include "physics.h"

#include "physics/physics_internal.h"

CollisionFilterConfig physics_collision_filter_config_default_get(void) {
    return (CollisionFilterConfig){
        .category = ROHR_COLLISION_CATEGORY_DEFAULT,
        .collides_with = ROHR_COLLISION_CATEGORY_ALL
    };
}

EngineResult physics_collision_filter_set(Entity entity,
        CollisionFilterConfig config) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!entity_index_components_check(index, ROHR_HIT_BOX))
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    if(CollisionFilterConfigPool_store_at(
            &collision_filters_pool, index, config).kind == ERROR_RESULT_ERROR)
        return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED);
    entity_mask[index] |= ROHR_COLLISION_FILTER;
    return error_result_value(true);
}

CollisionFilterConfigResult physics_collision_filter_get(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(
            CollisionFilterConfigResult, result.result.error);
    if(!entity_index_components_check(index, ROHR_HIT_BOX))
        return ERROR_RESULT_MAKE_ERROR(
            CollisionFilterConfigResult, ERROR_ENGINE_COMPONENT_MISSING);
    if(!entity_index_components_check(index, ROHR_COLLISION_FILTER))
        return ERROR_RESULT_MAKE_VALUE(CollisionFilterConfigResult,
            physics_collision_filter_config_default_get());
    return ERROR_RESULT_MAKE_VALUE(
        CollisionFilterConfigResult, collision_filters[index]);
}

EngineResult physics_collision_category_set(Entity entity,
        RohrCollisionCategoryMask category) {
    CollisionFilterConfigResult result = physics_collision_filter_get(entity);

    if(result.kind == ERROR_RESULT_ERROR)
        return error_result_error(result.result.error);
    result.result.value.category = category;
    return physics_collision_filter_set(entity, result.result.value);
}

EngineResult physics_collision_with_set(Entity entity,
        RohrCollisionCategoryMask categories) {
    CollisionFilterConfigResult result = physics_collision_filter_get(entity);

    if(result.kind == ERROR_RESULT_ERROR)
        return error_result_error(result.result.error);
    result.result.value.collides_with = categories;
    return physics_collision_filter_set(entity, result.result.value);
}

EngineResult physics_collision_with_all_set(Entity entity) {
    return physics_collision_with_set(entity, ROHR_COLLISION_CATEGORY_ALL);
}

EngineResult physics_collision_with_none_set(Entity entity) {
    return physics_collision_with_set(entity, ROHR_COLLISION_CATEGORY_NONE);
}

bool physics_collision_between_check(Entity entity_1, Entity entity_2) {
    CollisionFilterConfigResult first = physics_collision_filter_get(entity_1);
    CollisionFilterConfigResult second = physics_collision_filter_get(entity_2);

    if(first.kind == ERROR_RESULT_ERROR || second.kind == ERROR_RESULT_ERROR)
        return false;
    return (first.result.value.collides_with & second.result.value.category) != 0 &&
        (second.result.value.collides_with & first.result.value.category) != 0;
}
