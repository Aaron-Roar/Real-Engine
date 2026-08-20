/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics.h"

#include "core/engine_internal.h"
#include "physics/collision/interaction_query.h"

bool physics_overlap_check(Entity entity, Entity target) {
    return physics_interaction_query_check(
        entity, target, PHYSICS_INTERACTION_OVERLAP);
}

OverlapInfo physics_overlap_get(Entity entity, Entity target) {
    PhysicsInteraction interaction;

    if(!physics_interaction_query_get(entity, target,
            PHYSICS_INTERACTION_OVERLAP, &interaction)) {
        return (OverlapInfo){.detected = false};
    }
    return interaction.overlap;
}

bool physics_overlap_entered_check(Entity entity, Entity target) {
    return physics_interaction_query_entered_check(
        entity, target, PHYSICS_INTERACTION_OVERLAP);
}

bool physics_overlap_stayed_check(Entity entity, Entity target) {
    return physics_interaction_query_stayed_check(
        entity, target, PHYSICS_INTERACTION_OVERLAP);
}

bool physics_overlap_exited_check(Entity entity, Entity target) {
    return physics_interaction_query_exited_check(
        entity, target, PHYSICS_INTERACTION_OVERLAP);
}

size_t physics_overlap_count_get(Entity entity) {
    if(!physics_interaction_query_entity_valid(entity)) return 0;
    return physics_interaction_current_count_get(
        entity, PHYSICS_INTERACTION_OVERLAP);
}

size_t physics_overlaps_get(Entity entity, EntityInteraction *results,
        size_t capacity) {
    if(!physics_interaction_query_entity_valid(entity)) return 0;
    return physics_interaction_current_entities_get(
        entity, PHYSICS_INTERACTION_OVERLAP, results, capacity);
}
