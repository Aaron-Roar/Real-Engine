#ifndef PHYSICS_INTERACTION_SET_H
#define PHYSICS_INTERACTION_SET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "entity_pair_set.h"
#include "physics.h"

typedef uint8_t PhysicsInteractionFlags;

enum {
    PHYSICS_INTERACTION_NONE = 0,
    PHYSICS_INTERACTION_OVERLAP = 1 << 0,
    PHYSICS_INTERACTION_CONTACT = 1 << 1
};

typedef struct PhysicsInteraction {
    EntityPair pair;
    OverlapInfo overlap;
    PhysicsInteractionFlags flags;
} PhysicsInteraction;

typedef struct PhysicsInteractionSet {
    PhysicsInteraction *entries;
    uint8_t *occupied;
    size_t capacity;
    size_t count;
} PhysicsInteractionSet;

EngineResult physics_interaction_set_init(PhysicsInteractionSet *set, size_t capacity);
EngineResult physics_interaction_set_record(
    PhysicsInteractionSet *set,
    Entity first,
    Entity second,
    OverlapInfo overlap,
    PhysicsInteractionFlags flags
);
EngineResult physics_interaction_set_remove(
    PhysicsInteractionSet *set,
    Entity first,
    Entity second
);
bool physics_interaction_set_check(
    const PhysicsInteractionSet *set,
    Entity first,
    Entity second,
    PhysicsInteractionFlags flags
);
bool physics_interaction_set_get(
    const PhysicsInteractionSet *set,
    Entity first,
    Entity second,
    PhysicsInteraction *interaction
);
size_t physics_interaction_set_entity_count_get(
    const PhysicsInteractionSet *set,
    Entity entity,
    PhysicsInteractionFlags flags
);
size_t physics_interaction_set_entities_get(
    const PhysicsInteractionSet *set,
    Entity entity,
    PhysicsInteractionFlags flags,
    EntityInteraction *results,
    size_t capacity
);
void physics_interaction_set_clear(PhysicsInteractionSet *set);
void physics_interaction_set_destroy(PhysicsInteractionSet *set);

#endif
