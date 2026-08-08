#ifndef PHYSICS_H
#define PHYSICS_H
#include <stddef.h>
#include "math2d.h"
#include "entity_components.h"

typedef struct PhysicsDebugStats {
    double total_ms;
    double broadphase_build_ms;
    double broadphase_query_ms;
    double narrowphase_ms;
    double response_ms;
    size_t collider_count;
    size_t tree_node_count;
    int tree_height;
    size_t candidate_pair_count;
    size_t narrowphase_test_count;
    size_t overlap_count;
    size_t contact_count;
} PhysicsDebugStats;

PhysicsDebugStats physics_debug_stats_get(void);
void physics_debug_stats_enabled_set(bool enabled);

#define PHYSICS_SOLVER_ITERATIONS_DEFAULT 8u
#define PHYSICS_SUBSTEPS_DEFAULT 1u
#define ROHR_PHYSICS_GRAVITY_DEFAULT ((Acceleration){0.0f, 980.0f})

EngineResult physics_solver_iterations_set(uint32_t iterations);
uint32_t physics_solver_iterations_get(void);
EngineResult physics_substeps_set(uint32_t substeps);
uint32_t physics_substeps_get(void);

/** Begin a complete physics step and advance interaction transition state. */
void physics_pipeline_step_begin(void);
/** Clear transient constraints before assembling one physics substep. */
void physics_pipeline_substep_begin(void);
/** Clear force-derived acceleration accumulated during the previous substep. */
void physics_pipeline_accelerations_clear(void);
/** Apply global gravity to opted-in movable entities. */
void physics_pipeline_gravity_apply(void);
/** Apply spring-joint and soft-body-beam forces for the current substep. */
void physics_pipeline_forces_apply(void);
/** Integrate rigid-body positions and velocities for one substep. */
void physics_pipeline_integrate(double dt);
/** Detect rigid and soft-body contacts and gather their constraints. */
void physics_pipeline_contacts_gather(void);
/** Gather active pin and weld joint constraints. */
void physics_pipeline_joints_gather(void);
/** Solve all currently gathered contact and joint constraints. */
void physics_pipeline_constraints_solve(uint32_t iterations);
/** Run the standard sequence for one substep. */
void physics_pipeline_substep(double dt);
/** Run the plug-and-play physics pipeline, including configured substeps. */
void physics_pipeline_update(double dt);
/** Result type for functions that return a Shape. */
ERROR_DECLARE_RESULT_TYPE(ShapeResult, Shape);

/** Geometric overlap information from narrow-phase shape tests. */
typedef struct OverlapInfo {
    /** true when shapes overlap. */
    bool detected;
    /** Normal pointing from the first shape toward the second. */
    Axis normal;
    /** Penetration depth along the overlap normal. */
    Vec1D depth;
} OverlapInfo;

/** One current interaction between a queried entity and another entity. */
typedef struct EntityInteraction {
    /** The other entity in the interaction. */
    Entity target;
    /** Geometry oriented from the queried entity toward target. */
    OverlapInfo overlap;
} EntityInteraction;

/** Bit mask describing one or more collision categories. */
typedef uint64_t RohrCollisionCategoryMask;

/** Default collision category assigned to colliders. */
#define ROHR_COLLISION_CATEGORY_DEFAULT UINT64_C(1)
/** Mask matching every collision category. */
#define ROHR_COLLISION_CATEGORY_ALL UINT64_MAX
/** Mask matching no collision categories. */
#define ROHR_COLLISION_CATEGORY_NONE UINT64_C(0)
/** Engine-reserved category assigned to soft-body nodes by default. */
#define ROHR_COLLISION_CATEGORY_SOFT_BODY_NODE (UINT64_C(1) << 63)

/** Collision filtering configuration owned by one collider entity. */
typedef struct CollisionFilterConfig {
    /** Categories represented by this collider. */
    RohrCollisionCategoryMask category;
    /** Categories this collider permits collision checks against. */
    RohrCollisionCategoryMask collides_with;
} CollisionFilterConfig;

/** Result type for functions returning collision filter configuration. */
ERROR_DECLARE_RESULT_TYPE(CollisionFilterConfigResult, CollisionFilterConfig);

/** Surface friction coefficient. */
typedef Vec1D Friction;

/** Collision restitution coefficient. */
typedef Vec1D Restitution;

/** Entity orientation angle in radians. */
typedef Vec1D Orientation;
/** Entity world position. */
typedef Vec2D Position;
/** Entity linear velocity. */
typedef Vec2D Velocity;
#define PHYSICS_CONTACT_POINT_MAX 2
/** Solver information for one world-space point in a contact manifold. */
typedef struct ContactPointInfo {
    /** World-space contact position. */
    Position position;
    /** Pre-resolution velocity of the second entity relative to the first. */
    Velocity relative_velocity;
    /** Normal impulse applied to the second entity at this point. */
    Vec2D normal_impulse;
    /** Friction impulse applied to the second entity at this point. */
    Vec2D friction_impulse;
} ContactPointInfo;

/** Physical response information for one entity contact. */
typedef struct ContactInfo {
    /** true when physical collision response was entered. */
    bool detected;
    /** Normal pointing from the first entity toward the second. */
    Axis normal;
    /** Penetration depth along the contact normal. */
    Vec1D depth;
    /** World-space points and solver results in the collision manifold. */
    ContactPointInfo points[PHYSICS_CONTACT_POINT_MAX];
    /** Number of valid manifold points. */
    uint8_t point_count;
} ContactInfo;

/** One current physical contact involving a queried entity. */
typedef struct EntityContact {
    /** The other entity in the contact. */
    Entity target;
    /** Contact data oriented from the queried entity toward target. */
    ContactInfo contact;
} EntityContact;
/** Entity angular velocity in radians per second. */
typedef Orientation AngularVelocity;
/** Entity angular acceleration. */
typedef Orientation AngularAcceleration;
/** Entity linear acceleration. */
typedef Vec2D Acceleration;
/** Set the acceleration applied to entities carrying ROHR_GRAVITY. */
EngineResult physics_gravity_set(Acceleration gravity);
/** Return the current global gravity acceleration. */
Acceleration physics_gravity_get(void);
/** Add the ROHR_GRAVITY component to an entity. */
EngineResult physics_gravity_enable(Entity entity);
/** Remove the ROHR_GRAVITY component from an entity. */
EngineResult physics_gravity_disable(Entity entity);
/** Return whether an entity carries ROHR_GRAVITY. */
bool physics_gravity_check(Entity entity);
/** Linear force vector. */
typedef Vec2D Force;
/** Entity mass value. */
typedef float Mass;
/** Torque value. */
typedef Orientation Torque;

ERROR_DECLARE_RESULT_TYPE(PositionResult, Position);
ERROR_DECLARE_RESULT_TYPE(AngularVelocityResult, AngularVelocity);

/** Constraint that locks movement onto an axis through a point. */
typedef struct AxisLock {
    /** Axis to constrain movement along. */
    Axis axis;
    /** Point that the locked axis passes through. */
    Position point_on_axis;
} AxisLock;

/** Constraint that clamps orientation to a min/max angle. */
typedef struct AngleLock {
    /** Minimum allowed orientation. */
    Orientation min;
    /** Maximum allowed orientation. */
    Orientation max;
} AngleLock;

/** Constraint that drives one entity from another entity's transform. */
typedef struct TransformLock {
    /** Entity that drives the transform. */
    Entity driver;

    /** Local position offset from the driver. */
    Vec2D local_offset;
    /** Local orientation offset from the driver. */
    Orientation local_angle;

    /** Whether position should follow the driver. */
    bool lock_position;
    /** Whether orientation should follow the driver. */
    bool lock_orientation;
    /** Whether velocity should be inherited from the driver. */
    bool inherit_velocity;
} TransformLock;

/** Joint behavior type. */
typedef enum JointType {
    /** Apply a damped spring between anchor points. */
    JOINT_SPRING,
    /** Preserve relative position and orientation. */
    JOINT_WELD,
    /** Keep anchor points together while allowing relative rotation. */
    JOINT_PIN
} JointType;

/** Stable handle for an entity-owned joint anchor. */
typedef uint64_t JointAnchorId;
#define JOINT_ANCHOR_INVALID UINT64_C(0)
#define MAX_JOINT_ANCHORS 10000
#define MAX_JOINT_ANCHORS_PER_ENTITY 32

/** Entity-owned point stored relative to the entity's centroid. */
typedef struct JointAnchor {
    Entity entity;
    Vec2D centroid_offset;
} JointAnchor;

typedef struct JointAnchorList {
    JointAnchorId values[MAX_JOINT_ANCHORS_PER_ENTITY];
    uint32_t count;
} JointAnchorList;

ERROR_DECLARE_RESULT_TYPE(JointAnchorIdResult, JointAnchorId);
ERROR_DECLARE_RESULT_TYPE(JointAnchorResult, JointAnchor);
ERROR_DECLARE_RESULT_TYPE(JointAnchorListResult, JointAnchorList);
ERROR_DECLARE_RESULT_TYPE(JointAnchorPositionResult, Position);

/** Joint component data stored on a joint entity. */
typedef struct Joint {
    /** Joint behavior type. */
    JointType type;

    /** First explicit anchor handle, when configured through the anchor API. */
    JointAnchorId anchor_a;
    /** Second explicit anchor handle, when configured through the anchor API. */
    JointAnchorId anchor_b;

    /** First constrained entity. */
    Entity a;
    /** Second constrained entity. */
    Entity b;

    /** Anchor point local to entity a. */
    Vec2D local_anchor_a;
    /** Anchor point local to entity b. */
    Vec2D local_anchor_b;

    /** Resting distance for spring joints. */
    float rest_length;

    /** Linear spring stiffness. */
    float stiffness;
    /** Linear damping. */
    float damping;

    /** Whether the joint locks relative angle. */
    bool lock_angle;
    /** Resting relative angle. */
    Orientation rest_angle;
    /** Angular spring stiffness. */
    float angular_stiffness;
    /** Angular damping. */
    float angular_damping;
} Joint;

#define SOFT_BODY_MAX_NODES 64
#define SOFT_BODY_MAX_BEAMS 256
#define SOFT_BODY_MAX_TRIANGLES 128

/** Entity-owned collection of soft-body topology entities. */
typedef struct SoftBody {
    Entity nodes[SOFT_BODY_MAX_NODES];
    Entity beams[SOFT_BODY_MAX_BEAMS];
    Entity triangles[SOFT_BODY_MAX_TRIANGLES];
    uint32_t node_count;
    uint32_t beam_count;
    uint32_t triangle_count;
} SoftBody;

/** Lightweight point mass participating in soft-body collision. */
typedef struct SoftBodyNode {
    Entity soft_body;
    float radius;
    /** Mirrored for source compatibility; standard collision filters are authoritative. */
    RohrCollisionCategoryMask category;
    /** Mirrored for source compatibility; standard collision filters are authoritative. */
    RohrCollisionCategoryMask collides_with;
} SoftBodyNode;

/** Elastic connection between two soft-body nodes. */
typedef struct SoftBodyBeam {
    Entity soft_body;
    Entity node_a;
    Entity node_b;
    float rest_length;
    float stiffness;
    float damping;
} SoftBodyBeam;

/** Deforming triangular surface referencing three soft-body nodes. */
typedef struct SoftBodyTriangle {
    Entity soft_body;
    Entity node_a;
    Entity node_b;
    Entity node_c;
} SoftBodyTriangle;

/** Explicit handles created when pinning a soft-body node to an anchor. */
typedef struct SoftBodyNodeAnchorPin {
    Entity joint;
    JointAnchorId node_anchor;
} SoftBodyNodeAnchorPin;

ERROR_DECLARE_RESULT_TYPE(SoftBodyResult, SoftBody);
ERROR_DECLARE_RESULT_TYPE(SoftBodyNodeResult, SoftBodyNode);
ERROR_DECLARE_RESULT_TYPE(SoftBodyBeamResult, SoftBodyBeam);
ERROR_DECLARE_RESULT_TYPE(SoftBodyTriangleResult, SoftBodyTriangle);
ERROR_DECLARE_RESULT_TYPE(SoftBodyNodeAnchorPinResult, SoftBodyNodeAnchorPin);

/** Pool storing positions by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(PositionPool, Position);
/** Pool storing velocities by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(VelocityPool, Velocity);
/** Pool storing accelerations by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(AccelerationPool, Acceleration);
/** Pool storing masses by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(MassPool, float);
/** Pool storing forces by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(ForcePool, Force);
/** Pool storing shapes by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(ShapePool, Shape);
/** Pool storing collision filters by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(CollisionFilterConfigPool, CollisionFilterConfig);
/** Pool storing orientations by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(OrientationPool, Orientation);
/** Pool storing angular velocities by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(AngularVelocityPool, AngularVelocity);
/** Pool storing angular accelerations by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(AngularAccelerationPool, AngularAcceleration);
/** Pool storing torques by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(TorquePool, Torque);
/** Pool storing friction values by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(FrictionPool, Friction);
/** Pool storing restitution values by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(RestitutionPool, Restitution);
/** Pool storing angle locks by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(AngleLockPool, AngleLock);
/** Pool storing axis locks by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(AxisLockPool, AxisLock);
/** Pool storing transform locks by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(TransformLockPool, TransformLock);
/** Pool storing joints by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(JointPool, Joint);
MEMORY_DECLARE_OBJECT_POOL(SoftBodyPool, SoftBody);
MEMORY_DECLARE_OBJECT_POOL(SoftBodyNodePool, SoftBodyNode);
MEMORY_DECLARE_OBJECT_POOL(SoftBodyBeamPool, SoftBodyBeam);
MEMORY_DECLARE_OBJECT_POOL(SoftBodyTrianglePool, SoftBodyTriangle);

extern PositionPool positions_pool;
extern VelocityPool velocities_pool;
extern AccelerationPool accelerations_pool;
extern MassPool mass_pool;
extern ForcePool forces_pool;
extern AccelerationPool force_accelerations_pool;
extern ShapePool hit_boxes_pool;
extern ShapePool world_hit_boxes_pool;
extern CollisionFilterConfigPool collision_filters_pool;
extern OrientationPool orientations_pool;
extern AngularVelocityPool angular_velocities_pool;
extern AngularVelocityPool angular_velocity_maximums_pool;
extern AngularAccelerationPool angular_accelerations_pool;
extern TorquePool torques_pool;
extern AngularVelocityPool torque_angular_accelerations_pool;
extern FrictionPool frictions_pool;
extern RestitutionPool restitutions_pool;
extern AngleLockPool angle_locks_pool;
extern AxisLockPool axis_locks_pool;
extern TransformLockPool transform_locks_pool;
extern JointPool joints_pool;
extern SoftBodyPool soft_bodies_pool;
extern SoftBodyNodePool soft_body_nodes_pool;
extern SoftBodyBeamPool soft_body_beams_pool;
extern SoftBodyTrianglePool soft_body_triangles_pool;
#define positions positions_pool.objects
#define velocities velocities_pool.objects
#define accelerations accelerations_pool.objects
#define mass mass_pool.objects
#define forces forces_pool.objects
#define force_accelerations force_accelerations_pool.objects
#define hit_boxes hit_boxes_pool.objects
#define world_hit_boxes world_hit_boxes_pool.objects
#define collision_filters collision_filters_pool.objects
#define orientations orientations_pool.objects
#define angular_velocities angular_velocities_pool.objects
#define angular_velocity_maximums angular_velocity_maximums_pool.objects
#define angular_accelerations angular_accelerations_pool.objects
#define torques torques_pool.objects
#define torque_angular_accelerations torque_angular_accelerations_pool.objects
#define frictions frictions_pool.objects
#define restitutions restitutions_pool.objects
#define angle_locks angle_locks_pool.objects
#define axis_locks axis_locks_pool.objects
#define transform_locks transform_locks_pool.objects
#define joints joints_pool.objects
#define soft_bodies soft_bodies_pool.objects
#define soft_body_nodes soft_body_nodes_pool.objects
#define soft_body_beams soft_body_beams_pool.objects
#define soft_body_triangles soft_body_triangles_pool.objects

/**
 * Translate a local shape into world coordinates.
 *
 * @param shape Local-space shape.
 * @param position World position.
 * @param angle World orientation in radians.
 * @return World-space shape.
 */
Shape physics_shape_world_translate(Shape shape, Position position, Orientation angle);

/**
 * Approximate polygon moment of inertia.
 *
 * @param shape Shape whose vertices define the body.
 * @param mass_value Body mass.
 * @return Moment of inertia.
 */
float physics_polygon_moment_of_inertia(Shape shape, Mass mass_value);

/**
 * Run SAT collision detection between two shapes.
 *
 * @param shape_1 First world-space shape.
 * @param shape_2 Second world-space shape.
 * @return Geometric overlap information.
 */
OverlapInfo physics_sat_overlap_get(Shape shape_1, Shape shape_2);

/**
 * Compute circle moment of inertia from a circle-like shape.
 *
 * @param circle Shape representing a circle.
 * @param mass_value Body mass.
 * @return Moment of inertia.
 */
Vec1D physics_circle_moment_of_inertia(Shape circle, Mass mass_value);

/** Check whether an entity index has ROHR_HOLD. */
bool physics_entity_held_get(EntityIndex index);
/** Check whether an entity index can be moved by physics update stages. */
bool physics_entity_movable_get(EntityIndex index);

/** Set an entity's base linear acceleration. */
EngineResult physics_acceleration_set(Entity entity, Acceleration a);
/** Set an entity's angular acceleration and mark it dynamic. */
EngineResult physics_angular_acceleration_set(
        Entity entity,
        AngularAcceleration acceleration
);
/** Set acceleration toward a world position using a scalar magnitude. */
EngineResult physics_acceleration_toward_position_set(Entity entity, float acceleration_magnitude, Position position);
/** Set acceleration toward another entity's current world position. */
EngineResult physics_acceleration_toward_entity_set(Entity entity, float acceleration_magnitude, Entity target);
/** Set acceleration away from a world position using a scalar magnitude. */
EngineResult physics_acceleration_away_from_position_set(Entity entity, float acceleration_magnitude, Position position);
/** Set acceleration away from another entity's current world position. */
EngineResult physics_acceleration_away_from_entity_set(Entity entity, float acceleration_magnitude, Entity target);
/** Set acceleration toward an entity for every live entity in a group. */
EngineResult physics_group_acceleration_toward_entity_set(GroupId group, float acceleration_magnitude, Entity target);
/** Set acceleration away from an entity for every live entity in a group. */
EngineResult physics_group_acceleration_away_from_entity_set(GroupId group, float acceleration_magnitude, Entity target);
/** Set an entity's linear velocity. */
EngineResult physics_velocity_set(Entity entity, Velocity v);
/** Set velocity toward a world position using a scalar speed. */
EngineResult physics_velocity_toward_position_set(Entity entity, float speed, Position position);
/** Set velocity toward another entity's current world position. */
EngineResult physics_velocity_toward_entity_set(Entity entity, float speed, Entity target);
/** Set velocity away from a world position using a scalar speed. */
EngineResult physics_velocity_away_from_position_set(Entity entity, float speed, Position position);
/** Set velocity away from another entity's current world position. */
EngineResult physics_velocity_away_from_entity_set(Entity entity, float speed, Entity target);
/** Set velocity toward an entity for every live entity in a group. */
EngineResult physics_group_velocity_toward_entity_set(GroupId group, float speed, Entity target);
/** Set velocity away from an entity for every live entity in a group. */
EngineResult physics_group_velocity_away_from_entity_set(GroupId group, float speed, Entity target);
/** Set an entity's velocity to zero. */
EngineResult physics_entity_stop(Entity entity);
/** Set velocity to zero for every live entity in a group. */
EngineResult physics_group_entities_stop(GroupId group);
/** Apply an immediate linear impulse to an entity's velocity. */
EngineResult physics_impulse_apply(Entity entity, Vec2D impulse);
/** Set an entity's world position. */
EngineResult physics_position_set(Entity entity, Position p);
PositionResult physics_position_get(Entity entity);
/** Set an entity's mass and add the ROHR_MASS component. */
/** Set finite, non-negative mass. Zero represents an explicitly massless entity. */
EngineResult physics_mass_set(Entity entity, Mass m);
/** Create a force entity targeting the given entity. */
EntityResult physics_force_create(Entity entity, Force f);
/** Set force component data directly on an existing entity. */
EngineResult physics_force_component_set(Entity entity, Force force);
/** Create a force entity that applies for one physics tick. */
EngineResult physics_force_for_one_tick_apply(Entity entity, Force f);
/** Create a torque entity targeting the given entity. */
EntityResult physics_torque_create(Entity entity, Torque t);
/** Set torque component data directly on an existing entity. */
EngineResult physics_torque_component_set(Entity entity, Torque torque);
/** Create a torque entity that applies for one physics tick. */
EngineResult physics_torque_for_one_tick_apply(Entity entity, Torque t);
/** Set an entity's hitbox. This does not enable physical collision response. */
EngineResult physics_hitbox_set(Entity entity, Shape hitbox);
/** Return the default collision filter: default category against all categories. */
CollisionFilterConfig physics_collision_filter_config_default_get(void);
/** Replace an entity's complete collision filter. */
EngineResult physics_collision_filter_set(Entity entity, CollisionFilterConfig config);
/** Return an entity's collision filter. Colliders without an override use defaults. */
CollisionFilterConfigResult physics_collision_filter_get(Entity entity);
/** Set the categories represented by an entity. */
EngineResult physics_collision_category_set(Entity entity, RohrCollisionCategoryMask category);
/** Set the category whitelist an entity can collide with. */
EngineResult physics_collision_with_set(Entity entity, RohrCollisionCategoryMask categories);
/** Allow an entity to collide with every category. */
EngineResult physics_collision_with_all_set(Entity entity);
/** Prevent an entity from colliding with every category. */
EngineResult physics_collision_with_none_set(Entity entity);
/** Return whether two entities mutually permit collision checks. */
bool physics_collision_between_check(Entity entity_1, Entity entity_2);
/** Set an entity's orientation in radians. */
EngineResult physics_orientation_set(Entity entity, Orientation angle);
/** Set an entity's angular velocity. */
EngineResult physics_angular_velocity_set(Entity entity, AngularVelocity v);
/** Return an entity's angular velocity. */
AngularVelocityResult physics_angular_velocity_get(Entity entity);
EngineResult physics_angular_velocity_maximum_set(Entity entity, AngularVelocity maximum);
AngularVelocityResult physics_angular_velocity_maximum_get(Entity entity);
/** Get an entity's current world-space hitbox. */
ShapeResult physics_global_hit_box_get(Entity entity);
/** Set an entity's collision restitution. */
EngineResult physics_restitution_set(Entity entity, Restitution restitution);
/** Mark an entity dynamic and remove ROHR_STATIC. */
EngineResult physics_dynamic_set(Entity entity);
/** Mark an entity static and remove ROHR_DYNAMIC. */
EngineResult physics_static_set(Entity entity);
/** Add ROHR_HOLD so physics update stages preserve current values. */
EngineResult physics_entity_hold(Entity entity);
/** Remove ROHR_HOLD without changing ROHR_STATIC or ROHR_DYNAMIC state. */
EngineResult physics_entity_unhold(Entity entity);
/** Add ROHR_HOLD to every live entity in a group. */
EngineResult physics_group_entities_hold(GroupId group);
/** Remove ROHR_HOLD from every live entity in a group. */
EngineResult physics_group_entities_unhold(GroupId group);
/** Add or update an angle lock constraint. */
EngineResult physics_angle_lock_set(Entity entity, Orientation min, Orientation max);
/** Add or update an axis lock constraint. */
EngineResult physics_axis_lock_set(Entity entity, Axis axis, Position axis_point);
/** Set an entity's friction value. */
EngineResult physics_friction_set(Entity entity, float friction);

/**
 * Add or update a transform lock.
 *
 * @param driven Entity whose transform is controlled.
 * @param driver Entity that drives the transform.
 * @param local_offset Offset from the driver in local space.
 * @param local_angle Orientation offset from the driver.
 * @param lock_position Whether to lock position.
 * @param lock_orientation Whether to lock orientation.
 * @param inherit_velocity Whether to inherit velocity.
 * @return EngineResult describing success or failure.
 */
EngineResult physics_transform_lock_set(
        Entity driven,
        Entity driver,
        Vec2D local_offset,
        Orientation local_angle,
        bool lock_position,
        bool lock_orientation,
        bool inherit_velocity
);

/** Remove a transform lock from an entity. */
EngineResult physics_transform_lock_remove(Entity entity);

/**
 * Add a transform lock using the current relative transform as the offset.
 */
EngineResult physics_transform_lock_current_transform_set(
        Entity driven,
        Entity driver,
        bool lock_position,
        bool lock_orientation,
        bool inherit_velocity
);

/** Set the target component used by force and torque source entities. */
EngineResult physics_target_set(Entity entity, Entity target);

/** Add or replace complete joint component data on an existing entity. */
EngineResult physics_joint_component_set(Entity entity, Joint joint);

JointAnchorIdResult physics_joint_anchor_create(Entity entity, Vec2D centroid_offset);
JointAnchorListResult physics_joint_anchors_get(Entity entity);
JointAnchorPositionResult physics_joint_anchor_position_get(JointAnchorId anchor);
JointAnchorPositionResult physics_joint_anchor_world_position_get(JointAnchorId anchor);
EngineResult physics_joint_anchor_position_set(JointAnchorId anchor, Vec2D centroid_offset);
EngineResult physics_joint_anchor_remove(JointAnchorId anchor);

EngineResult physics_joint_pin_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b);
EngineResult physics_joint_weld_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b);
EngineResult physics_joint_spring_set(
    Entity joint,
    JointAnchorId anchor_a,
    JointAnchorId anchor_b,
    float rest_length,
    float stiffness,
    float damping
);

EntityResult physics_soft_body_create(void);
SoftBodyResult physics_soft_body_get(Entity soft_body);
EntityResult physics_soft_body_node_create(
    Entity soft_body,
    Position position,
    Mass mass_value,
    float radius
);
SoftBodyNodeResult physics_soft_body_node_get(Entity node);
EngineResult physics_soft_body_node_collision_filter_set(
    Entity node,
    RohrCollisionCategoryMask category,
    RohrCollisionCategoryMask collides_with
);
EngineResult physics_soft_body_node_force_for_one_tick_apply(Entity node, Force force);
EngineResult physics_soft_body_node_impulse_apply(Entity node, Vec2D impulse);
EngineResult physics_soft_body_force_for_one_tick_apply(Entity soft_body, Force force);
EngineResult physics_soft_body_torque_for_one_tick_apply(Entity soft_body, Torque torque);
SoftBodyNodeAnchorPinResult physics_soft_body_node_to_anchor_pin_create(
    Entity node,
    JointAnchorId anchor
);
EntityResult physics_soft_body_beam_create(
    Entity soft_body,
    Entity node_a,
    Entity node_b,
    float stiffness,
    float damping
);
SoftBodyBeamResult physics_soft_body_beam_get(Entity beam);
EntityResult physics_soft_body_triangle_create(
    Entity soft_body,
    Entity node_a,
    Entity node_b,
    Entity node_c
);
SoftBodyTriangleResult physics_soft_body_triangle_get(Entity triangle);

/**
 * Create a joint entity connecting two live entities.
 *
 * @return EntityResult containing the new joint entity, or an error.
 */
EntityResult physics_joint_create(
    Entity a,
    Entity b,
    JointType type,
    Vec2D local_anchor_a,
    Vec2D local_anchor_b,
    float stiffness,
    float damping
);

/**
 * Run particle collision detection between two circle-like shapes.
 */
OverlapInfo physics_particle_overlap_get(Shape shape_1, Shape shape_2);

/** Return whether two entities overlap during the current physics step. */
bool physics_overlap_check(Entity entity, Entity target);
/** Return current overlap geometry in the requested entity order. */
OverlapInfo physics_overlap_get(Entity entity, Entity target);
/** Return whether an overlap began during the current physics step. */
bool physics_overlap_entered_check(Entity entity, Entity target);
/** Return whether an overlap continued from the previous physics step. */
bool physics_overlap_stayed_check(Entity entity, Entity target);
/** Return whether an overlap ended during the current physics step. */
bool physics_overlap_exited_check(Entity entity, Entity target);
/** Return the number of current overlaps involving an entity. */
size_t physics_overlap_count_get(Entity entity);
/** Write up to capacity current overlaps and return the number written. */
size_t physics_overlaps_get(
    Entity entity,
    EntityInteraction *results,
    size_t capacity
);

/** Return whether two entities physically contacted during the current physics step. */
bool physics_contact_check(Entity entity, Entity target);
/** Return current contact geometry in the requested entity order. */
ContactInfo physics_contact_get(Entity entity, Entity target);
/** Return the sum of a contact's normal and friction impulses. */
Vec2D physics_contact_total_impulse_get(ContactInfo contact);
/** Return whether a physical contact began during the current physics step. */
bool physics_contact_entered_check(Entity entity, Entity target);
/** Return whether a physical contact continued from the previous physics step. */
bool physics_contact_stayed_check(Entity entity, Entity target);
/** Return whether a physical contact ended during the current physics step. */
bool physics_contact_exited_check(Entity entity, Entity target);
/** Return the number of current physical contacts involving an entity. */
size_t physics_contact_count_get(Entity entity);
/** Write up to capacity current contacts and return the number written. */
size_t physics_contacts_get(
    Entity entity,
    EntityContact *results,
    size_t capacity
);

/** Set an explicit simulation delta for each engine tick. */
EngineResult physics_dt_per_tick_set(Time dt);
/** Return the explicit delta or the engine time per tick when using defaults. */
Time physics_dt_per_tick_get(void);
/** Return physics timing to the engine time-per-tick default. */
void physics_engine_time_per_tick_use(void);
/** Advance physics for a number of elapsed engine ticks. */
void physics_update(Tick ticks);
/** Advance physics once using an explicit exceptional delta. */
void physics_dt_update(Time dt);
#endif
