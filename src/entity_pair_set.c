#include "entity_pair_set.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ENTITY_PAIR_SET_MIN_CAPACITY 8

static EntityPair entity_pair_make(Entity first, Entity second) {
    if(first < second) {
        return (EntityPair){first, second};
    }
    return (EntityPair){second, first};
}

static uint64_t entity_pair_hash(EntityPair pair) {
    uint64_t value = ((uint64_t)pair.first << 32) | pair.second;

    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return value;
}

static bool entity_pair_equal(EntityPair first, EntityPair second) {
    return first.first == second.first && first.second == second.second;
}

static size_t entity_pair_set_slot_get(
    const EntityPair *entries,
    const uint8_t *occupied,
    size_t capacity,
    EntityPair pair,
    bool *found
) {
    size_t slot = (size_t)(entity_pair_hash(pair) & (uint64_t)(capacity - 1));

    while(occupied[slot] != 0) {
        if(entity_pair_equal(entries[slot], pair)) {
            *found = true;
            return slot;
        }
        slot = (slot + 1) & (capacity - 1);
    }
    *found = false;
    return slot;
}

static bool entity_pair_set_capacity_get(size_t requested, size_t *capacity) {
    size_t value = ENTITY_PAIR_SET_MIN_CAPACITY;

    if(capacity == NULL) return false;
    while(value < requested) {
        if(value > SIZE_MAX / 2) return false;
        value *= 2;
    }
    *capacity = value;
    return true;
}

EngineResult entity_pair_set_reserve(EntityPairSet *set, size_t requested) {
    EntityPair *entries;
    uint8_t *occupied;
    size_t capacity;
    size_t index;

    if(set == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    if(requested <= set->capacity) return error_result_value(true);
    if(!entity_pair_set_capacity_get(requested, &capacity)
            || capacity > SIZE_MAX / sizeof(*entries)) {
        return error_result_error(ERROR_MEMORY_POOL_CAPACITY_OVERFLOW);
    }
    entries = calloc(capacity, sizeof(*entries));
    occupied = calloc(capacity, sizeof(*occupied));
    if(entries == NULL || occupied == NULL) {
        free(entries);
        free(occupied);
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }

    for(index = 0; index < set->capacity; index += 1) {
        if(set->occupied[index] != 0) {
            bool found;
            size_t slot = entity_pair_set_slot_get(
                entries, occupied, capacity, set->entries[index], &found
            );
            entries[slot] = set->entries[index];
            occupied[slot] = 1;
        }
    }
    free(set->entries);
    free(set->occupied);
    set->entries = entries;
    set->occupied = occupied;
    set->capacity = capacity;
    return error_result_value(true);
}

EngineResult entity_pair_set_init(EntityPairSet *set, size_t capacity) {
    if(set == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    *set = (EntityPairSet){0};
    if(capacity == 0) return error_result_value(true);
    return entity_pair_set_reserve(set, capacity);
}

EngineResult entity_pair_set_insert(EntityPairSet *set, Entity first, Entity second) {
    EntityPair pair;
    EngineResult result;
    bool found;
    size_t slot;

    if(set == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    if(first == ENTITY_INVALID || second == ENTITY_INVALID || first == second) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(set->capacity == 0
            || set->count + 1 > set->capacity - set->capacity / 4) {
        size_t capacity;
        if(set->capacity > SIZE_MAX / 2) {
            return error_result_error(ERROR_MEMORY_POOL_CAPACITY_OVERFLOW);
        }
        capacity = set->capacity == 0
            ? ENTITY_PAIR_SET_MIN_CAPACITY
            : set->capacity * 2;
        result = entity_pair_set_reserve(set, capacity);
        if(error_check(result)) return result;
    }

    pair = entity_pair_make(first, second);
    slot = entity_pair_set_slot_get(
        set->entries, set->occupied, set->capacity, pair, &found
    );
    if(found) return error_result_value(false);
    set->entries[slot] = pair;
    set->occupied[slot] = 1;
    set->count += 1;
    return error_result_value(true);
}

EngineResult entity_pair_set_remove(EntityPairSet *set, Entity first, Entity second) {
    EntityPair pair;
    bool found;
    size_t slot;
    size_t next;

    if(set == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    if(first == ENTITY_INVALID || second == ENTITY_INVALID || first == second) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(set->capacity == 0) return error_result_value(false);

    pair = entity_pair_make(first, second);
    slot = entity_pair_set_slot_get(
        set->entries, set->occupied, set->capacity, pair, &found
    );
    if(!found) return error_result_value(false);

    set->entries[slot] = (EntityPair){0};
    set->occupied[slot] = 0;
    set->count -= 1;

    next = (slot + 1) & (set->capacity - 1);
    while(set->occupied[next] != 0) {
        EntityPair moved = set->entries[next];
        size_t moved_slot;

        set->entries[next] = (EntityPair){0};
        set->occupied[next] = 0;
        moved_slot = entity_pair_set_slot_get(
            set->entries, set->occupied, set->capacity, moved, &found
        );
        set->entries[moved_slot] = moved;
        set->occupied[moved_slot] = 1;
        next = (next + 1) & (set->capacity - 1);
    }
    return error_result_value(true);
}

bool entity_pair_set_contains(const EntityPairSet *set, Entity first, Entity second) {
    bool found;

    if(set == NULL || set->capacity == 0 || first == ENTITY_INVALID
            || second == ENTITY_INVALID || first == second) {
        return false;
    }
    (void)entity_pair_set_slot_get(
        set->entries,
        set->occupied,
        set->capacity,
        entity_pair_make(first, second),
        &found
    );
    return found;
}

void entity_pair_set_clear(EntityPairSet *set) {
    if(set == NULL) return;
    if(set->capacity > 0) {
        memset(set->entries, 0, set->capacity * sizeof(*set->entries));
        memset(set->occupied, 0, set->capacity * sizeof(*set->occupied));
    }
    set->count = 0;
}

void entity_pair_set_destroy(EntityPairSet *set) {
    if(set == NULL) return;
    free(set->entries);
    free(set->occupied);
    *set = (EntityPairSet){0};
}
