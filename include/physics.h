#ifndef PHYSICS_H
#define PHYSICS_H
#include "math2d.h"
#include "entity_components.h"
/** Result type for functions that return a Shape. */
ERROR_DECLARE_RESULT_TYPE(ShapeResult, Shape);

/** Collision result from narrow-phase collision tests. */
typedef struct Collision {
    /** true when shapes overlap. */
    bool overlap;
    /** Collision normal pointing from the first shape toward the second. */
    Axis normal;
    /** Penetration depth along the collision normal. */
    Vec1D depth;
} Collision;

/** Per-target collision state table for one entity. */
typedef struct {
    /** Collision flags indexed by target EntityIndex. */
    bool collisions[MAX_ENTITIES];
} CollisionReport;

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
/** Entity angular velocity in radians per second. */
typedef Orientation AngularVelocity;
/** Entity angular acceleration. */
typedef Orientation AngularAcceleration;
/** Entity linear acceleration. */
typedef Vec2D Acceleration;
/** Linear force vector. */
typedef Vec2D Force;
/** Entity mass value. */
typedef float Mass;
/** Torque value. */
typedef Orientation Torque;

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
    /** Maintain distance between anchor points. */
    JOINT_DISTANCE,
    /** Reserved weld joint type. */
    JOINT_WELD,
    /** Pin one entity to another. */
    JOINT_PIN
} JointType;

/** Joint component data stored on a joint entity. */
typedef struct Joint {
    /** Joint behavior type. */
    JointType type;

    /** First constrained entity. */
    Entity a;
    /** Second constrained entity. */
    Entity b;

    /** Anchor point local to entity a. */
    Vec2D local_anchor_a;
    /** Anchor point local to entity b. */
    Vec2D local_anchor_b;

    /** Resting distance for distance joints. */
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
/** Pool storing collision reports by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(CollisionReportPool, CollisionReport);
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

extern PositionPool positions_pool;
extern VelocityPool velocities_pool;
extern AccelerationPool accelerations_pool;
extern MassPool mass_pool;
extern ForcePool forces_pool;
extern AccelerationPool force_accelerations_pool;
extern ShapePool hit_boxes_pool;
extern ShapePool world_hit_boxes_pool;
extern CollisionReportPool collision_reports_pool;
extern OrientationPool orientations_pool;
extern AngularVelocityPool angular_velocities_pool;
extern AngularAccelerationPool angular_accelerations_pool;
extern TorquePool torques_pool;
extern AngularVelocityPool torque_angular_accelerations_pool;
extern FrictionPool frictions_pool;
extern RestitutionPool restitutions_pool;
extern AngleLockPool angle_locks_pool;
extern AxisLockPool axis_locks_pool;
extern TransformLockPool transform_locks_pool;
extern JointPool joints_pool;
#define positions positions_pool.objects
#define velocities velocities_pool.objects
#define accelerations accelerations_pool.objects
#define mass mass_pool.objects
#define forces forces_pool.objects
#define force_accelerations force_accelerations_pool.objects
#define hit_boxes hit_boxes_pool.objects
#define world_hit_boxes world_hit_boxes_pool.objects
#define collision_reports collision_reports_pool.objects
#define orientations orientations_pool.objects
#define angular_velocities angular_velocities_pool.objects
#define angular_accelerations angular_accelerations_pool.objects
#define torques torques_pool.objects
#define torque_angular_accelerations torque_angular_accelerations_pool.objects
#define frictions frictions_pool.objects
#define restitutions restitutions_pool.objects
#define angle_locks angle_locks_pool.objects
#define axis_locks axis_locks_pool.objects
#define transform_locks transform_locks_pool.objects
#define joints joints_pool.objects

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
 * @return Collision data.
 */
Collision physics_sat_collision(Shape shape_1, Shape shape_2);

/**
 * Compute circle moment of inertia from a circle-like shape.
 *
 * @param circle Shape representing a circle.
 * @param mass_value Body mass.
 * @return Moment of inertia.
 */
Vec1D physics_circle_moment_of_inertia(Shape circle, Mass mass_value);

/** Check whether an entity index has HOLD. */
bool physics_entity_is_held(EntityIndex index);
/** Check whether an entity index can be moved by physics update stages. */
bool physics_entity_can_move(EntityIndex index);

/** Set an entity's base linear acceleration. */
EngineResult physics_set_acceleration(Entity entity, Acceleration a);
/** Set an entity's angular acceleration and mark it dynamic. */
EngineResult physics_set_angular_acceleration(
        Entity entity,
        AngularAcceleration acceleration
);
/** Set acceleration toward a world position using a scalar magnitude. */
EngineResult physics_set_acceleration_toward_position(Entity entity, float acceleration_magnitude, Position position);
/** Set acceleration toward another entity's current world position. */
EngineResult physics_set_acceleration_toward_entity(Entity entity, float acceleration_magnitude, Entity target);
/** Set acceleration away from a world position using a scalar magnitude. */
EngineResult physics_set_acceleration_away_from_position(Entity entity, float acceleration_magnitude, Position position);
/** Set acceleration away from another entity's current world position. */
EngineResult physics_set_acceleration_away_from_entity(Entity entity, float acceleration_magnitude, Entity target);
/** Set acceleration toward an entity for every live entity in a group. */
EngineResult physics_group_set_acceleration_toward_entity(GroupId group, float acceleration_magnitude, Entity target);
/** Set acceleration away from an entity for every live entity in a group. */
EngineResult physics_group_set_acceleration_away_from_entity(GroupId group, float acceleration_magnitude, Entity target);
/** Set an entity's linear velocity. */
EngineResult physics_set_velocity(Entity entity, Velocity v);
/** Set velocity toward a world position using a scalar speed. */
EngineResult physics_set_velocity_toward_position(Entity entity, float speed, Position position);
/** Set velocity toward another entity's current world position. */
EngineResult physics_set_velocity_toward_entity(Entity entity, float speed, Entity target);
/** Set velocity away from a world position using a scalar speed. */
EngineResult physics_set_velocity_away_from_position(Entity entity, float speed, Position position);
/** Set velocity away from another entity's current world position. */
EngineResult physics_set_velocity_away_from_entity(Entity entity, float speed, Entity target);
/** Set velocity toward an entity for every live entity in a group. */
EngineResult physics_group_set_velocity_toward_entity(GroupId group, float speed, Entity target);
/** Set velocity away from an entity for every live entity in a group. */
EngineResult physics_group_set_velocity_away_from_entity(GroupId group, float speed, Entity target);
/** Set an entity's velocity to zero. */
EngineResult physics_stop_entity(Entity entity);
/** Set velocity to zero for every live entity in a group. */
EngineResult physics_group_stop_entities(GroupId group);
/** Apply an immediate linear impulse to an entity's velocity. */
EngineResult physics_apply_impulse(Entity entity, Vec2D impulse);
/** Set an entity's world position. */
EngineResult physics_set_position(Entity entity, Position p);
/** Set an entity's mass and add the MASS component. */
EngineResult physics_set_mass(Entity entity, Mass m);
/** Create a force entity targeting the given entity. */
EntityResult physics_set_force(Entity entity, Force f);
/** Set force component data directly on an existing entity. */
EngineResult physics_set_force_component(Entity entity, Force force);
/** Create a force entity that applies for one physics tick. */
EngineResult physics_apply_force_for_one_tick(Entity entity, Force f);
/** Create a torque entity targeting the given entity. */
EntityResult physics_set_torque(Entity entity, Torque t);
/** Set torque component data directly on an existing entity. */
EngineResult physics_set_torque_component(Entity entity, Torque torque);
/** Create a torque entity that applies for one physics tick. */
EngineResult physics_apply_torque_for_one_tick(Entity entity, Torque t);
/** Set an entity's hitbox and add collision/hitbox components. */
EngineResult physics_set_hitbox(Entity entity, Shape hitbox);
/** Set an entity's orientation in radians. */
EngineResult physics_set_orientation(Entity entity, Orientation angle);
/** Set an entity's angular velocity. */
EngineResult physics_set_angular_velocity(Entity entity, AngularVelocity v);
/** Get an entity's current world-space hitbox. */
ShapeResult physics_get_global_hit_box(Entity entity);
/** Set an entity's collision restitution. */
EngineResult physics_set_restitution(Entity entity, Restitution restitution);
/** Mark an entity dynamic and remove STATIC. */
EngineResult physics_set_dynamic(Entity entity);
/** Mark an entity static and remove DYNAMIC. */
EngineResult physics_set_static(Entity entity);
/** Add HOLD so physics update stages preserve current values. */
EngineResult physics_hold_entity(Entity entity);
/** Remove HOLD without changing STATIC or DYNAMIC state. */
EngineResult physics_unhold_entity(Entity entity);
/** Add HOLD to every live entity in a group. */
EngineResult physics_group_hold_entities(GroupId group);
/** Remove HOLD from every live entity in a group. */
EngineResult physics_group_unhold_entities(GroupId group);
/** Add or update an angle lock constraint. */
EngineResult physics_set_angle_lock(Entity entity, Orientation min, Orientation max);
/** Add or update an axis lock constraint. */
EngineResult physics_set_axis_lock(Entity entity, Axis axis, Position axis_point);
/** Set an entity's friction value. */
EngineResult physics_set_friction(Entity entity, float friction);

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
EngineResult physics_set_transform_lock(
        Entity driven,
        Entity driver,
        Vec2D local_offset,
        Orientation local_angle,
        bool lock_position,
        bool lock_orientation,
        bool inherit_velocity
);

/** Remove a transform lock from an entity. */
EngineResult physics_remove_transform_lock(Entity entity);

/**
 * Add a transform lock using the current relative transform as the offset.
 */
EngineResult physics_set_transform_lock_current_transform(
        Entity driven,
        Entity driver,
        bool lock_position,
        bool lock_orientation,
        bool inherit_velocity
);

/** Set the target component used by force and torque source entities. */
EngineResult physics_set_target(Entity entity, Entity target);

/** Add or replace complete joint component data on an existing entity. */
EngineResult physics_set_joint_component(Entity entity, Joint joint);

/**
 * Create a joint entity connecting two live entities.
 *
 * @return EntityResult containing the new joint entity, or an error.
 */
EntityResult physics_set_joint(
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
Collision physics_particle_collision(Shape shape_1, Shape shape_2);

/**
 * Set collision state between an entity and target entity.
 */
EngineResult physics_set_collision_report(Entity entity, Entity target, bool state);

/**
 * Get collision state between an entity and target entity.
 */
bool physics_get_collision_report(Entity entity, Entity target);

/** Set an explicit simulation delta for each engine tick. */
EngineResult physics_set_dt_per_tick(Time dt);
/** Return the explicit delta or the engine time per tick when using defaults. */
Time physics_get_dt_per_tick(void);
/** Return physics timing to the engine time-per-tick default. */
void physics_use_engine_time_per_tick(void);
/** Advance physics for a number of elapsed engine ticks. */
void physics_update(Tick ticks);
/** Advance physics once using an explicit exceptional delta. */
void physics_update_dt(Time dt);
#endif
