#ifndef ENTITY_PAIR_SET_H
#define ENTITY_PAIR_SET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "entity_components.h"
#include "error.h"

typedef struct EntityPair {
    Entity first;
    Entity second;
} EntityPair;

typedef struct EntityPairSet {
    EntityPair *entries;
    uint8_t *occupied;
    size_t capacity;
    size_t count;
} EntityPairSet;

EngineResult entity_pair_set_init(EntityPairSet *set, size_t capacity);
EngineResult entity_pair_set_reserve(EntityPairSet *set, size_t capacity);
EngineResult entity_pair_set_insert(EntityPairSet *set, Entity first, Entity second);
EngineResult entity_pair_set_remove(EntityPairSet *set, Entity first, Entity second);
bool entity_pair_set_contains(const EntityPairSet *set, Entity first, Entity second);
void entity_pair_set_clear(EntityPairSet *set);
void entity_pair_set_destroy(EntityPairSet *set);

#endif
