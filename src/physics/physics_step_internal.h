/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_PHYSICS_STEP_INTERNAL_H
#define ROHR_PHYSICS_STEP_INTERNAL_H

#include "entity_components.h"
#include "physics/broadphase/aabb_tree.h"
#include "physics/collision/contact_constraint.h"
#include "physics/collision/interaction_set.h"
#include "physics/joints/joint_constraint.h"

extern AABBTree physics_broadphase_tree;
extern PhysicsDebugStats physics_step_debug_stats;
extern bool physics_step_debug_stats_enabled;
extern ContactConstraintList physics_step_contact_constraints;
extern JointConstraintList physics_step_joint_constraints;

void physics_interactions_step_begin(void);
bool physics_step_entity_from_index_get(EntityIndex index, Entity *entity);
bool physics_step_alive_index_at(uint32_t alive_position, EntityIndex *index);
void physics_step_interaction_by_index_record(
    EntityIndex entity_1,
    EntityIndex entity_2,
    OverlapInfo overlap,
    ContactInfo contact,
    PhysicsInteractionFlags flags
);
void physics_step_hitbox_dirty_add(EntityIndex index);
void physics_step_hitbox_dirty_flush(void);
void physics_step_entity_by_index_delete(EntityIndex index);
void physics_step_transform_lock_by_index_remove(EntityIndex index);
double physics_step_elapsed_ms(uint64_t start);
void physics_pipeline_contact_constraints_solve(
    ContactConstraintList *constraints,
    float position_fraction,
    void *context
);
void physics_pipeline_contact_constraints_finalize(
    ContactConstraintList *constraints,
    void *context
);
void physics_pipeline_joint_constraints_solve(void *context);

void physics_rigid_integrate(double dt);
void physics_rigid_accelerations_clear(void);
void physics_rigid_gravity_apply(Acceleration gravity);
void physics_rigid_constraints_gather(void);
void physics_rigid_contact_constraint_solve(
    SystemContactConstraint *constraint,
    float position_fraction
);
void physics_rigid_contact_constraint_finalize(
    const SystemContactConstraint *constraint
);
void physics_rigid_contact_point_impulses_accumulate(
    ContactInfo *current,
    const ContactInfo *previous
);

void physics_joint_spring_forces_apply(void);
void physics_joint_constraints_gather(void);
void physics_joint_constraints_solve(void);

void physics_soft_body_beams_apply(void);
void physics_soft_body_constraints_gather(void);
void physics_soft_body_constraint_solve(
    SystemContactConstraint *constraint,
    float position_fraction
);
void physics_soft_body_constraint_finalize(
    const SystemContactConstraint *constraint
);

#endif
