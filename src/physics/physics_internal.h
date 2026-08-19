#ifndef ROHR_PHYSICS_INTERNAL_H
#define ROHR_PHYSICS_INTERNAL_H

#include "physics.h"

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

#endif
