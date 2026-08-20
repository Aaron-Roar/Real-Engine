/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_PHYSICS_INTERACTION_QUERY_H
#define ROHR_PHYSICS_INTERACTION_QUERY_H

#include "physics.h"
#include "physics/collision/interaction_set.h"

bool physics_interaction_query_entity_valid(Entity entity);
bool physics_interaction_query_check(Entity entity, Entity target,
    PhysicsInteractionFlags flags);
bool physics_interaction_query_get(Entity entity, Entity target,
    PhysicsInteractionFlags flags, PhysicsInteraction *interaction);
bool physics_interaction_query_entered_check(Entity entity, Entity target,
    PhysicsInteractionFlags flags);
bool physics_interaction_query_stayed_check(Entity entity, Entity target,
    PhysicsInteractionFlags flags);
bool physics_interaction_query_exited_check(Entity entity, Entity target,
    PhysicsInteractionFlags flags);

#endif
