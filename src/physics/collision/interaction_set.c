#include "interaction_set.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PHYSICS_INTERACTION_SET_MIN_CAPACITY 8

static void physics_contact_orientation_reverse(ContactInfo *contact) {
    if(contact == NULL) return;
    contact->normal.x = -contact->normal.x;
    contact->normal.y = -contact->normal.y;
    for(uint8_t i = 0; i < contact->point_count; i += 1) {
        contact->points[i].relative_velocity.x =
            -contact->points[i].relative_velocity.x;
        contact->points[i].relative_velocity.y =
            -contact->points[i].relative_velocity.y;
        contact->points[i].normal_impulse.x =
            -contact->points[i].normal_impulse.x;
        contact->points[i].normal_impulse.y =
            -contact->points[i].normal_impulse.y;
        contact->points[i].friction_impulse.x =
            -contact->points[i].friction_impulse.x;
        contact->points[i].friction_impulse.y =
            -contact->points[i].friction_impulse.y;
    }
}

static EntityPair physics_interaction_pair_make(Entity first, Entity second) {
    if(first < second) return (EntityPair){first, second};
    return (EntityPair){second, first};
}

static uint64_t physics_interaction_pair_hash(EntityPair pair) {
    uint64_t value = ((uint64_t)pair.first << 32) | pair.second;

    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return value;
}

static bool physics_interaction_pair_equal(EntityPair first, EntityPair second) {
    return first.first == second.first && first.second == second.second;
}

static size_t physics_interaction_slot_get(
    const PhysicsInteraction *entries,
    const uint8_t *occupied,
    size_t capacity,
    EntityPair pair,
    bool *found
) {
    size_t slot = (size_t)(
        physics_interaction_pair_hash(pair) & (uint64_t)(capacity - 1)
    );

    while(occupied[slot] != 0) {
        if(physics_interaction_pair_equal(entries[slot].pair, pair)) {
            *found = true;
            return slot;
        }
        slot = (slot + 1) & (capacity - 1);
    }
    *found = false;
    return slot;
}

static bool physics_interaction_capacity_get(size_t requested, size_t *capacity) {
    size_t value = PHYSICS_INTERACTION_SET_MIN_CAPACITY;

    if(capacity == NULL) return false;
    while(value < requested) {
        if(value > SIZE_MAX / 2) return false;
        value *= 2;
    }
    *capacity = value;
    return true;
}

static EngineResult physics_interaction_set_reserve(
    PhysicsInteractionSet *set,
    size_t requested
) {
    PhysicsInteraction *entries;
    uint8_t *occupied;
    size_t capacity;
    size_t index;

    if(set == NULL) return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    if(requested <= set->capacity) return error_result_value(true);
    if(!physics_interaction_capacity_get(requested, &capacity) ||
            capacity > SIZE_MAX / sizeof(*entries)) {
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
            size_t slot = physics_interaction_slot_get(
                entries, occupied, capacity, set->entries[index].pair, &found
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

EngineResult physics_interaction_set_init(PhysicsInteractionSet *set, size_t capacity) {
    if(set == NULL) return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    *set = (PhysicsInteractionSet){0};
    if(capacity == 0) return error_result_value(true);
    return physics_interaction_set_reserve(set, capacity);
}

EngineResult physics_interaction_set_record(
    PhysicsInteractionSet *set,
    Entity first,
    Entity second,
    OverlapInfo overlap,
    ContactInfo contact,
    PhysicsInteractionFlags flags
) {
    EntityPair pair;
    EngineResult result;
    bool found;
    size_t slot;

    if(set == NULL) return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    if(first == ENTITY_INVALID || second == ENTITY_INVALID || first == second) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(flags == PHYSICS_INTERACTION_NONE) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    if((flags & PHYSICS_INTERACTION_CONTACT) != 0) {
        flags |= PHYSICS_INTERACTION_OVERLAP;
    }
    if(set->capacity == 0 ||
            set->count + 1 > set->capacity - set->capacity / 4) {
        size_t capacity;
        if(set->capacity > SIZE_MAX / 2) {
            return error_result_error(ERROR_MEMORY_POOL_CAPACITY_OVERFLOW);
        }
        capacity = set->capacity == 0
            ? PHYSICS_INTERACTION_SET_MIN_CAPACITY
            : set->capacity * 2;
        result = physics_interaction_set_reserve(set, capacity);
        if(error_check(result)) return result;
    }

    pair = physics_interaction_pair_make(first, second);
    if(first != pair.first) {
        overlap.normal.x = -overlap.normal.x;
        overlap.normal.y = -overlap.normal.y;
        physics_contact_orientation_reverse(&contact);
    }
    slot = physics_interaction_slot_get(
        set->entries, set->occupied, set->capacity, pair, &found
    );
    if(found) {
        set->entries[slot].flags |= flags;
        set->entries[slot].overlap = overlap;
        if((flags & PHYSICS_INTERACTION_CONTACT) != 0) {
            set->entries[slot].contact = contact;
        }
        return error_result_value(false);
    }
    set->entries[slot] = (PhysicsInteraction){
        .pair = pair,
        .overlap = overlap,
        .contact = contact,
        .flags = flags
    };
    set->occupied[slot] = 1;
    set->count += 1;
    return error_result_value(true);
}

EngineResult physics_interaction_set_remove(
    PhysicsInteractionSet *set,
    Entity first,
    Entity second
) {
    EntityPair pair;
    bool found;
    size_t slot;
    size_t next;

    if(set == NULL) return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    if(first == ENTITY_INVALID || second == ENTITY_INVALID || first == second) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(set->capacity == 0) return error_result_value(false);
    pair = physics_interaction_pair_make(first, second);
    slot = physics_interaction_slot_get(
        set->entries, set->occupied, set->capacity, pair, &found
    );
    if(!found) return error_result_value(false);
    set->entries[slot] = (PhysicsInteraction){0};
    set->occupied[slot] = 0;
    set->count -= 1;

    next = (slot + 1) & (set->capacity - 1);
    while(set->occupied[next] != 0) {
        PhysicsInteraction moved = set->entries[next];
        size_t moved_slot;

        set->entries[next] = (PhysicsInteraction){0};
        set->occupied[next] = 0;
        moved_slot = physics_interaction_slot_get(
            set->entries, set->occupied, set->capacity, moved.pair, &found
        );
        set->entries[moved_slot] = moved;
        set->occupied[moved_slot] = 1;
        next = (next + 1) & (set->capacity - 1);
    }
    return error_result_value(true);
}

bool physics_interaction_set_check(
    const PhysicsInteractionSet *set,
    Entity first,
    Entity second,
    PhysicsInteractionFlags flags
) {
    bool found;
    size_t slot;

    if(set == NULL || set->capacity == 0 || first == ENTITY_INVALID ||
            second == ENTITY_INVALID || first == second) return false;
    slot = physics_interaction_slot_get(
        set->entries,
        set->occupied,
        set->capacity,
        physics_interaction_pair_make(first, second),
        &found
    );
    return found && (set->entries[slot].flags & flags) == flags;
}

bool physics_interaction_set_get(
    const PhysicsInteractionSet *set,
    Entity first,
    Entity second,
    PhysicsInteraction *interaction
) {
    bool found;
    size_t slot;

    if(interaction == NULL || set == NULL || set->capacity == 0 ||
            first == ENTITY_INVALID || second == ENTITY_INVALID || first == second) {
        return false;
    }
    slot = physics_interaction_slot_get(
        set->entries,
        set->occupied,
        set->capacity,
        physics_interaction_pair_make(first, second),
        &found
    );
    if(!found) return false;
    *interaction = set->entries[slot];
    if(interaction->pair.first != first) {
        interaction->pair = (EntityPair){first, second};
        interaction->overlap.normal.x = -interaction->overlap.normal.x;
        interaction->overlap.normal.y = -interaction->overlap.normal.y;
        physics_contact_orientation_reverse(&interaction->contact);
    }
    return true;
}

size_t physics_interaction_set_entity_count_get(
    const PhysicsInteractionSet *set,
    Entity entity,
    PhysicsInteractionFlags flags
) {
    size_t count = 0;

    if(set == NULL || entity == ENTITY_INVALID || flags == PHYSICS_INTERACTION_NONE) {
        return 0;
    }
    for(size_t index = 0; index < set->capacity; index += 1) {
        const PhysicsInteraction *interaction = &set->entries[index];

        if(set->occupied[index] == 0 ||
                (interaction->flags & flags) != flags) continue;
        if(interaction->pair.first == entity || interaction->pair.second == entity) {
            count += 1;
        }
    }
    return count;
}

size_t physics_interaction_set_entities_get(
    const PhysicsInteractionSet *set,
    Entity entity,
    PhysicsInteractionFlags flags,
    EntityInteraction *results,
    size_t capacity
) {
    size_t count = 0;

    if(set == NULL || entity == ENTITY_INVALID || flags == PHYSICS_INTERACTION_NONE ||
            results == NULL || capacity == 0) return 0;
    for(size_t index = 0; index < set->capacity && count < capacity; index += 1) {
        const PhysicsInteraction *interaction = &set->entries[index];
        Entity target;
        OverlapInfo overlap;

        if(set->occupied[index] == 0 ||
                (interaction->flags & flags) != flags) continue;
        if(interaction->pair.first == entity) {
            target = interaction->pair.second;
            overlap = interaction->overlap;
        } else if(interaction->pair.second == entity) {
            target = interaction->pair.first;
            overlap = interaction->overlap;
            overlap.normal.x = -overlap.normal.x;
            overlap.normal.y = -overlap.normal.y;
        } else {
            continue;
        }
        results[count++] = (EntityInteraction){
            .target = target,
            .overlap = overlap
        };
    }
    return count;
}

size_t physics_interaction_set_contacts_get(
    const PhysicsInteractionSet *set,
    Entity entity,
    EntityContact *results,
    size_t capacity
) {
    size_t count = 0;

    if(set == NULL || entity == ENTITY_INVALID || results == NULL || capacity == 0) {
        return 0;
    }
    for(size_t index = 0; index < set->capacity && count < capacity; index += 1) {
        const PhysicsInteraction *interaction = &set->entries[index];
        Entity target;
        ContactInfo contact;

        if(set->occupied[index] == 0 ||
                (interaction->flags & PHYSICS_INTERACTION_CONTACT) == 0) continue;
        contact = interaction->contact;
        if(interaction->pair.first == entity) {
            target = interaction->pair.second;
        } else if(interaction->pair.second == entity) {
            target = interaction->pair.first;
            physics_contact_orientation_reverse(&contact);
        } else {
            continue;
        }
        results[count++] = (EntityContact){
            .target = target,
            .contact = contact
        };
    }
    return count;
}

void physics_interaction_set_clear(PhysicsInteractionSet *set) {
    if(set == NULL) return;
    if(set->capacity > 0) {
        memset(set->entries, 0, set->capacity * sizeof(*set->entries));
        memset(set->occupied, 0, set->capacity * sizeof(*set->occupied));
    }
    set->count = 0;
}

void physics_interaction_set_destroy(PhysicsInteractionSet *set) {
    if(set == NULL) return;
    free(set->entries);
    free(set->occupied);
    *set = (PhysicsInteractionSet){0};
}
