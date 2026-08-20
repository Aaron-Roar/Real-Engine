/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics.h"
#include "physics/physics_internal.h"

MEMORY_DEFINE_OBJECT_POOL(PositionPool, Position)
MEMORY_DEFINE_OBJECT_POOL(ParticleGeometryPool, ParticleGeometry)
MEMORY_DEFINE_OBJECT_POOL(VelocityPool, Velocity)
MEMORY_DEFINE_OBJECT_POOL(AccelerationPool, Acceleration)
MEMORY_DEFINE_OBJECT_POOL(MassPool, float)
MEMORY_DEFINE_OBJECT_POOL(ForcePool, Force)
MEMORY_DEFINE_OBJECT_POOL(ShapePool, Shape)
MEMORY_DEFINE_OBJECT_POOL(CollisionFilterConfigPool, CollisionFilterConfig)
MEMORY_DEFINE_OBJECT_POOL(OrientationPool, Orientation)
MEMORY_DEFINE_OBJECT_POOL(AngularVelocityPool, AngularVelocity)
MEMORY_DEFINE_OBJECT_POOL(AngularAccelerationPool, AngularAcceleration)
MEMORY_DEFINE_OBJECT_POOL(TorquePool, Torque)
MEMORY_DEFINE_OBJECT_POOL(FrictionPool, Friction)
MEMORY_DEFINE_OBJECT_POOL(RestitutionPool, Restitution)
MEMORY_DEFINE_OBJECT_POOL(AngleLockPool, AngleLock)
MEMORY_DEFINE_OBJECT_POOL(AxisLockPool, AxisLock)
MEMORY_DEFINE_OBJECT_POOL(TransformLockPool, TransformLock)
MEMORY_DEFINE_OBJECT_POOL(JointPool, Joint)
MEMORY_DEFINE_OBJECT_POOL(SoftBodyPool, SoftBody)
MEMORY_DEFINE_OBJECT_POOL(SoftBodyNodePool, SoftBodyNode)
MEMORY_DEFINE_OBJECT_POOL(SoftBodyBeamPool, SoftBodyBeam)
MEMORY_DEFINE_OBJECT_POOL(SoftBodyTrianglePool, SoftBodyTriangle)

PositionPool positions_pool = {0};
ParticleGeometryPool particle_geometries_pool = {0};
OrientationPool orientations_pool = {0};
VelocityPool velocities_pool = {0};
AccelerationPool accelerations_pool = {0};
AccelerationPool force_accelerations_pool = {0};
MassPool mass_pool = {0};
ForcePool forces_pool = {0};
ShapePool hit_boxes_pool = {0};
ShapePool world_hit_boxes_pool = {0};
CollisionFilterConfigPool collision_filters_pool = {0};
AngularVelocityPool angular_velocities_pool = {0};
AngularVelocityPool angular_velocity_maximums_pool = {0};
AngularAccelerationPool angular_accelerations_pool = {0};
AngularVelocityPool torque_angular_accelerations_pool = {0};
TorquePool torques_pool = {0};
FrictionPool frictions_pool = {0};
RestitutionPool restitutions_pool = {0};
AngleLockPool angle_locks_pool = {0};
AxisLockPool axis_locks_pool = {0};
TransformLockPool transform_locks_pool = {0};
JointPool joints_pool = {0};
SoftBodyPool soft_bodies_pool = {0};
SoftBodyNodePool soft_body_nodes_pool = {0};
SoftBodyBeamPool soft_body_beams_pool = {0};
SoftBodyTrianglePool soft_body_triangles_pool = {0};

EngineResult physics_tables_init(void) {
    physics_config_init();
    physics_force_state_init();
    physics_joint_state_init();
    physics_body_state_table_init();
    if(error_check(physics_interaction_state_init())) { goto fail; }
    if(PositionPool_init(&positions_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(ParticleGeometryPool_init(&particle_geometries_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(OrientationPool_init(&orientations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(VelocityPool_init(&velocities_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AccelerationPool_init(&accelerations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AccelerationPool_init(&force_accelerations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(MassPool_init(&mass_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(ForcePool_init(&forces_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(ShapePool_init(&hit_boxes_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(ShapePool_init(&world_hit_boxes_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(CollisionFilterConfigPool_init(&collision_filters_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngularVelocityPool_init(&angular_velocities_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngularVelocityPool_init(&angular_velocity_maximums_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngularAccelerationPool_init(&angular_accelerations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngularVelocityPool_init(&torque_angular_accelerations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(TorquePool_init(&torques_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(FrictionPool_init(&frictions_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(RestitutionPool_init(&restitutions_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngleLockPool_init(&angle_locks_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AxisLockPool_init(&axis_locks_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(TransformLockPool_init(&transform_locks_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(JointPool_init(&joints_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(SoftBodyPool_init(&soft_bodies_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(SoftBodyNodePool_init(&soft_body_nodes_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(SoftBodyBeamPool_init(&soft_body_beams_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(SoftBodyTrianglePool_init(&soft_body_triangles_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    return error_result_value(true);

fail:
    physics_tables_destroy();
    return error_result_error(ERROR_ENGINE_PHYSICS_TABLES_INIT_FAILED);
}

EngineResult physics_tables_ensure_capacity(size_t capacity) {
    size_t new_capacity;

    if(capacity > MAX_ENTITIES) {
        return error_result_error(ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
    }
    if(capacity <= positions_pool.capacity) {
        return error_result_value(true);
    }
    new_capacity = positions_pool.capacity == 0 ? 16 : positions_pool.capacity;
    while(new_capacity < capacity) {
        new_capacity *= 2;
    }
    if(new_capacity > MAX_ENTITIES) {
        new_capacity = MAX_ENTITIES;
    }
    if(new_capacity > positions_pool.capacity && PositionPool_expand(&positions_pool, new_capacity - positions_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > particle_geometries_pool.capacity && ParticleGeometryPool_expand(&particle_geometries_pool, new_capacity - particle_geometries_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > orientations_pool.capacity && OrientationPool_expand(&orientations_pool, new_capacity - orientations_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > velocities_pool.capacity && VelocityPool_expand(&velocities_pool, new_capacity - velocities_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > accelerations_pool.capacity && AccelerationPool_expand(&accelerations_pool, new_capacity - accelerations_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > force_accelerations_pool.capacity && AccelerationPool_expand(&force_accelerations_pool, new_capacity - force_accelerations_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > mass_pool.capacity && MassPool_expand(&mass_pool, new_capacity - mass_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > forces_pool.capacity && ForcePool_expand(&forces_pool, new_capacity - forces_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > hit_boxes_pool.capacity && ShapePool_expand(&hit_boxes_pool, new_capacity - hit_boxes_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > world_hit_boxes_pool.capacity && ShapePool_expand(&world_hit_boxes_pool, new_capacity - world_hit_boxes_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > collision_filters_pool.capacity && CollisionFilterConfigPool_expand(&collision_filters_pool, new_capacity - collision_filters_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > angular_velocities_pool.capacity && AngularVelocityPool_expand(&angular_velocities_pool, new_capacity - angular_velocities_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > angular_velocity_maximums_pool.capacity && AngularVelocityPool_expand(&angular_velocity_maximums_pool, new_capacity - angular_velocity_maximums_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > angular_accelerations_pool.capacity && AngularAccelerationPool_expand(&angular_accelerations_pool, new_capacity - angular_accelerations_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > torque_angular_accelerations_pool.capacity && AngularVelocityPool_expand(&torque_angular_accelerations_pool, new_capacity - torque_angular_accelerations_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > torques_pool.capacity && TorquePool_expand(&torques_pool, new_capacity - torques_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > frictions_pool.capacity && FrictionPool_expand(&frictions_pool, new_capacity - frictions_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > restitutions_pool.capacity && RestitutionPool_expand(&restitutions_pool, new_capacity - restitutions_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > angle_locks_pool.capacity && AngleLockPool_expand(&angle_locks_pool, new_capacity - angle_locks_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > axis_locks_pool.capacity && AxisLockPool_expand(&axis_locks_pool, new_capacity - axis_locks_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > transform_locks_pool.capacity && TransformLockPool_expand(&transform_locks_pool, new_capacity - transform_locks_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > joints_pool.capacity && JointPool_expand(&joints_pool, new_capacity - joints_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > soft_bodies_pool.capacity && SoftBodyPool_expand(&soft_bodies_pool, new_capacity - soft_bodies_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > soft_body_nodes_pool.capacity && SoftBodyNodePool_expand(&soft_body_nodes_pool, new_capacity - soft_body_nodes_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > soft_body_beams_pool.capacity && SoftBodyBeamPool_expand(&soft_body_beams_pool, new_capacity - soft_body_beams_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > soft_body_triangles_pool.capacity && SoftBodyTrianglePool_expand(&soft_body_triangles_pool, new_capacity - soft_body_triangles_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    return error_result_value(true);
}

void physics_tables_destroy(void) {
    physics_interaction_state_destroy();
    (void)PositionPool_destroy(&positions_pool);
    (void)ParticleGeometryPool_destroy(&particle_geometries_pool);
    (void)OrientationPool_destroy(&orientations_pool);
    (void)VelocityPool_destroy(&velocities_pool);
    (void)AccelerationPool_destroy(&accelerations_pool);
    (void)AccelerationPool_destroy(&force_accelerations_pool);
    (void)MassPool_destroy(&mass_pool);
    (void)ForcePool_destroy(&forces_pool);
    (void)ShapePool_destroy(&hit_boxes_pool);
    (void)ShapePool_destroy(&world_hit_boxes_pool);
    (void)CollisionFilterConfigPool_destroy(&collision_filters_pool);
    (void)AngularVelocityPool_destroy(&angular_velocities_pool);
    (void)AngularVelocityPool_destroy(&angular_velocity_maximums_pool);
    (void)AngularAccelerationPool_destroy(&angular_accelerations_pool);
    (void)AngularVelocityPool_destroy(&torque_angular_accelerations_pool);
    (void)TorquePool_destroy(&torques_pool);
    (void)FrictionPool_destroy(&frictions_pool);
    (void)RestitutionPool_destroy(&restitutions_pool);
    (void)AngleLockPool_destroy(&angle_locks_pool);
    (void)AxisLockPool_destroy(&axis_locks_pool);
    (void)TransformLockPool_destroy(&transform_locks_pool);
    (void)JointPool_destroy(&joints_pool);
    (void)SoftBodyPool_destroy(&soft_bodies_pool);
    (void)SoftBodyNodePool_destroy(&soft_body_nodes_pool);
    (void)SoftBodyBeamPool_destroy(&soft_body_beams_pool);
    (void)SoftBodyTrianglePool_destroy(&soft_body_triangles_pool);
}

void physics_entity_clear(Entity entity, EntityIndex index) {
    physics_body_state_entity_clear(index);
    if(index < particle_geometries_pool.capacity &&
            particle_geometries_pool.used[index])
        (void)ParticleGeometryPool_release_at(&particle_geometries_pool, index);
    if(index < angular_velocity_maximums_pool.capacity &&
            angular_velocity_maximums_pool.used[index]) {
        (void)AngularVelocityPool_release_at(&angular_velocity_maximums_pool, index);
    }
    physics_soft_body_entity_clear(entity, index);
    physics_joint_entity_clear(entity, index);
}
