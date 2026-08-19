#include "physics.h"

#include "physics/physics_internal.h"

EngineResult physics_live_index_get(Entity entity, EntityIndex *index) {
    if(index == NULL) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(!entity_index_get(entity, index)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(!entity_index_alive_check(*index)) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    return error_result_value(true);
}

EngineResult physics_group_entity_apply(GroupId group, PhysicsGroupEntityFn fn) {
    EntityGroupResult group_result;
    EntityGroup group_storage;
    size_t i;

    if(fn == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    group_result = entity_group_get(group);
    if(group_result.kind == ERROR_RESULT_ERROR) {
        return error_result_error(group_result.result.error);
    }
    group_storage = group_result.result.value;
    if(group_storage.entities.objects == NULL || group_storage.entities.used == NULL) {
        return error_result_value(true);
    }
    for(i = 0; i < group_storage.entities.capacity; i += 1) {
        EngineResult result;
        Entity entity;

        if(group_storage.entities.used[i] == 0) {
            continue;
        }
        entity = group_storage.entities.objects[i];
        if(!entity_alive_check(entity)) {
            continue;
        }
        result = fn(entity);
        if(result.kind == ERROR_RESULT_ERROR) {
            return result;
        }
    }
    return error_result_value(true);
}

EngineResult physics_target_set(Entity entity, Entity target) {
    EntityIndex index;
    EntityIndex target_index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = physics_live_index_get(target, &target_index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(TargetPool_store_at(&targets_pool, index, target).kind
            == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[index] |= ROHR_TARGETABLE;
    return error_result_value(true);
}


