#include <stddef.h>
#include <stdint.h>

#include "entity/entity_pair_set.h"

static Entity entity_handle(uint16_t generation, uint16_t slot) {
    return ((Entity)generation << 16) | slot;
}

int main(void) {
    EntityPairSet set;
    Entity first = entity_handle(1, 1);
    Entity second = entity_handle(1, 2);
    Entity reused = entity_handle(2, 1);
    EngineResult result;
    size_t original_capacity;
    size_t i;

    if(error_check(entity_pair_set_init(&set, 2)) || set.capacity < 2) return 1;

    result = entity_pair_set_insert(&set, first, second);
    if(error_check(result) || !result.result.value || set.count != 1
            || !entity_pair_set_contains(&set, first, second)
            || !entity_pair_set_contains(&set, second, first)) goto fail;

    result = entity_pair_set_insert(&set, second, first);
    if(error_check(result) || result.result.value || set.count != 1) goto fail;

    if(entity_pair_set_contains(&set, first, reused)) goto fail;
    result = entity_pair_set_insert(&set, first, reused);
    if(error_check(result) || !result.result.value
            || !entity_pair_set_contains(&set, reused, first)) goto fail;

    for(i = 3; i < 200; i += 1) {
        result = entity_pair_set_insert(&set, first, entity_handle(1, (uint16_t)i));
        if(error_check(result) || !result.result.value) goto fail;
    }
    for(i = 3; i < 200; i += 1) {
        if(!entity_pair_set_contains(&set, entity_handle(1, (uint16_t)i), first)) {
            goto fail;
        }
    }
    result = entity_pair_set_remove(&set, first, entity_handle(1, 100));
    if(error_check(result) || !result.result.value ||
            entity_pair_set_contains(&set, first, entity_handle(1, 100)) ||
            !entity_pair_set_contains(&set, first, entity_handle(1, 99)) ||
            !entity_pair_set_contains(&set, first, entity_handle(1, 101))) goto fail;
    result = entity_pair_set_remove(&set, first, entity_handle(1, 100));
    if(error_check(result) || result.result.value) goto fail;

    original_capacity = set.capacity;
    result = entity_pair_set_reserve(&set, SIZE_MAX);
    if(!error_check(result)
            || result.result.error != ERROR_MEMORY_POOL_CAPACITY_OVERFLOW
            || set.capacity != original_capacity
            || !entity_pair_set_contains(&set, first, second)) goto fail;

    result = entity_pair_set_insert(&set, ENTITY_INVALID, second);
    if(!error_check(result) || result.result.error != ERROR_ENGINE_INVALID_ENTITY) goto fail;
    result = entity_pair_set_insert(&set, first, first);
    if(!error_check(result) || result.result.error != ERROR_ENGINE_INVALID_ENTITY) goto fail;

    entity_pair_set_clear(&set);
    if(set.count != 0 || set.capacity != original_capacity
            || entity_pair_set_contains(&set, first, second)) goto fail;
    result = entity_pair_set_insert(&set, first, second);
    if(error_check(result) || !entity_pair_set_contains(&set, first, second)) goto fail;

    entity_pair_set_destroy(&set);
    if(set.entries != NULL || set.occupied != NULL
            || set.capacity != 0 || set.count != 0) return 1;
    return 0;

fail:
    entity_pair_set_destroy(&set);
    return 1;
}
