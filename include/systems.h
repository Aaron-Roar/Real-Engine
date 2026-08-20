/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef SYSTEMS_H
#define SYSTEMS_H
#include "entity_components.h"
#include "physics.h"

/** Advance engine time and clear entities whose lifetimes have expired. */
Tick system_tick_update(void);

/**
 * Run the physics pipeline for one frame.
 *
 * The current pipeline applies spring forces, integrates movement, solves
 * rigid joint constraints, applies locks, updates hitboxes/AABBs, then
 * resolves collisions.
 *
 * @param dt Delta time in seconds.
 */
void system_physics_update(double dt);
PhysicsDebugStats system_physics_debug_stats_get(void);
void system_physics_debug_stats_enabled_set(bool enabled);

/**
 * Delete entities whose lifetime has expired.
 */
void system_entities_past_lifetime_clean(void);
#endif
