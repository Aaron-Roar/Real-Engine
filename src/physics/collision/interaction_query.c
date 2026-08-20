/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics/collision/interaction_query.h"

#include "core/engine_internal.h"

bool physics_interaction_query_entity_valid(Entity entity) {
    EntityIndex index;

    return entity_index_get(entity, &index) && entity_index_alive_check(index);
}

static bool entities_valid(Entity entity, Entity target) {
    return physics_interaction_query_entity_valid(entity) &&
        physics_interaction_query_entity_valid(target);
}

bool physics_interaction_query_check(Entity entity, Entity target,
        PhysicsInteractionFlags flags) {
    return entities_valid(entity, target) &&
        physics_interaction_current_check(entity, target, flags);
}

bool physics_interaction_query_get(Entity entity, Entity target,
        PhysicsInteractionFlags flags, PhysicsInteraction *interaction) {
    return interaction != NULL &&
        physics_interaction_query_check(entity, target, flags) &&
        physics_interaction_current_get(entity, target, interaction);
}

bool physics_interaction_query_entered_check(Entity entity, Entity target,
        PhysicsInteractionFlags flags) {
    return entities_valid(entity, target) &&
        physics_interaction_current_check(entity, target, flags) &&
        !physics_interaction_previous_check(entity, target, flags);
}

bool physics_interaction_query_stayed_check(Entity entity, Entity target,
        PhysicsInteractionFlags flags) {
    return entities_valid(entity, target) &&
        physics_interaction_current_check(entity, target, flags) &&
        physics_interaction_previous_check(entity, target, flags);
}

bool physics_interaction_query_exited_check(Entity entity, Entity target,
        PhysicsInteractionFlags flags) {
    return entities_valid(entity, target) &&
        !physics_interaction_current_check(entity, target, flags) &&
        physics_interaction_previous_check(entity, target, flags);
}
