#ifndef SYSTEMS_H
#define SYSTEMS_H
#include "entity_components.h"
#include "physics.h"

/** Advance engine time and clear entities whose lifetimes have expired. */
Tick system_tick_update(void);

/**
 * Run the physics pipeline for one frame.
 *
 * The current pipeline applies forces and joints, integrates movement,
 * applies locks, updates hitboxes/AABBs, then resolves collisions.
 *
 * @param dt Delta time in seconds.
 */
void system_physics_update(double dt);

/**
 * Delete entities whose lifetime has expired.
 */
void system_entities_past_lifetime_clean(void);
#endif
