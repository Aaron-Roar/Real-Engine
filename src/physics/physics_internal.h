#ifndef ROHR_PHYSICS_INTERNAL_H
#define ROHR_PHYSICS_INTERNAL_H

#include "physics.h"

EngineResult physics_live_index_get(Entity entity, EntityIndex *index);
typedef EngineResult (*PhysicsGroupEntityFn)(Entity entity);
EngineResult physics_group_entity_apply(GroupId group, PhysicsGroupEntityFn fn);
void physics_body_state_table_init(void);
void physics_body_state_entity_clear(EntityIndex index);
void physics_body_state_sync(EntityIndex index);

#endif
