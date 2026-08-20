/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_PHYSICS_INTERNAL_H
#define ROHR_PHYSICS_INTERNAL_H

#include "physics.h"

void physics_tables_destroy(void);
EngineResult physics_live_index_get(Entity entity, EntityIndex *index);
typedef EngineResult (*PhysicsGroupEntityFn)(Entity entity);
typedef EngineResult (*PhysicsGroupEntityTargetFn)(
    Entity entity,
    float magnitude,
    Entity target
);
EngineResult physics_group_entity_apply(GroupId group, PhysicsGroupEntityFn fn);
EngineResult physics_group_entity_target_apply(
    GroupId group,
    float magnitude,
    Entity target,
    PhysicsGroupEntityTargetFn fn
);
Vec2D physics_direction_between_positions(Position from, Position to);
void physics_body_state_table_init(void);
void physics_body_state_entity_clear(EntityIndex index);
void physics_body_state_sync(EntityIndex index);
void physics_force_state_init(void);
void physics_joint_state_init(void);
void physics_joint_entity_clear(Entity entity, EntityIndex index);
void physics_soft_body_entity_clear(Entity entity, EntityIndex index);
EngineResult physics_interaction_state_init(void);
void physics_interaction_state_destroy(void);
void physics_config_init(void);
Position physics_particle_world_origin_by_index_get(EntityIndex index);
float physics_particle_radius_by_index_get(EntityIndex index);
OverlapInfo physics_particle_entities_overlap_get(
    EntityIndex first,
    EntityIndex second
);

#endif
