#ifndef ROHR_H
#define ROHR_H

#include "console.h"
#include "controller.h"
#include "engine.h"
#include "entity_components.h"
#include "error.h"
#include "graphics.h"
#include "game_state.h"
#include "grid.h"
#include "level_editor.h"
#include "math2d.h"
#include "physics.h"
#include "systems.h"
#include "tools.h"
#include "ui.h"

/**
 * @file rohr.h
 * @brief Public Rohr Engine API facade.
 *
 * Include this header from application code to use the engine through the
 * stable `rohr_`-prefixed API. Entity ids are stable handles, not component
 * table indexes. Use the entity API to validate ids and resolve indexes.
 */

/**
 * @brief Initializes core engine state.
 * @return EngineResult containing true on success, or an engine error.
 */
EngineResult rohr_engine_init(void);

/**
 * @brief Releases core engine state.
 */
void rohr_engine_shutdown(void);

/**
 * @brief Updates accumulated engine time from the platform clock.
 */
void rohr_engine_update_time(void);

/**
 * @brief Returns the current engine time in seconds.
 * @return Current engine time.
 */
Time rohr_engine_get_time(void);

/**
 * @brief Returns the current engine tick counter.
 * @return Current tick.
 */
Tick rohr_engine_get_tick(void);

/**
 * @brief Pauses engine time-dependent updates.
 */
void rohr_engine_pause(void);

/**
 * @brief Resumes engine time-dependent updates.
 */
void rohr_engine_resume(void);

/**
 * @brief Advances the engine tick counter.
 */
void rohr_engine_update_tick(void);

/**
 * @brief Polls one SDL event.
 * @return SDL event value returned by the engine event poller.
 */
SDL_Event rohr_engine_poll_event(void);

/**
 * @brief Checks whether the engine is paused.
 * @return true when paused, false when running.
 */
bool rohr_engine_is_paused(void);

/**
 * @brief Returns the current fixed or calculated delta time.
 * @return Delta time in seconds.
 */
Time rohr_engine_get_dt(void);

/**
 * @brief Calculates delta time from elapsed engine time.
 */
void rohr_engine_calculate_dt(void);

/**
 * @brief Sets the engine delta time manually.
 * @param dt Delta time in seconds.
 */
void rohr_engine_set_dt(Time dt);

/**
 * @brief Resets the engine clock baseline.
 */
void rohr_engine_reset_clock(void);

/**
 * @brief Creates a successful boolean engine result.
 * @param value Boolean value to store in the result.
 * @return EngineResult containing value.
 */
EngineResult rohr_error_result_value(bool value);

/**
 * @brief Creates a failed engine result.
 * @param error Error code to store in the result.
 * @return EngineResult containing error.
 */
EngineResult rohr_error_result_error(EngineError error);

/**
 * @brief Checks whether a result contains an error.
 * @param ResultValue Result value to inspect.
 * @return true when ResultValue contains an error, false otherwise.
 */
#define rohr_error_check(ResultValue) error_check(ResultValue)

/**
 * @brief Returns a user-facing default message for an engine error.
 * @param error Error code to describe.
 * @return Static string describing error.
 */
const char *rohr_error_default_message(EngineError error);

/**
 * @brief Returns the symbolic name for an engine error.
 * @param error Error code to name.
 * @return Static string containing the error name.
 */
const char *rohr_error_string(EngineError error);

/**
 * @brief Prints buffered console log messages.
 */
void rohr_console_print_logs(void);

/**
 * @brief Initializes the engine console.
 */
void rohr_console_init(void);

/**
 * @brief Shuts down the engine console.
 */
void rohr_console_shutdown(void);

/**
 * @brief Reads one console log string.
 * @param input Destination for the log string.
 * @return true when a log string was read, false otherwise.
 */
bool rohr_console_read(ConsoleLogString *input);

/**
 * @brief Writes a formatted message to the engine console.
 * @param source Source category for the log entry.
 * @param fmt printf-style format string.
 */
void rohr_console_write(LogSourceType source, const char *fmt, ...);

/**
 * @brief Checks whether the console is active.
 * @return true when active, false otherwise.
 */
bool rohr_console_is_active(void);

/**
 * @brief Writes a formatted debug message when debug logging is enabled.
 * @param source Source category for the log entry.
 * @param fmt printf-style format string.
 */
void rohr_console_debug_write(LogSourceType source, const char *fmt, ...);

/**
 * @brief Enables or disables debug console output.
 * @param state true to enable debug logging, false to disable it.
 */
void rohr_console_set_debug(bool state);

/**
 * @brief Checks whether an entity id currently refers to a live entity.
 * @param entity Stable entity id to inspect.
 * @return true when the entity is alive, false otherwise.
 */
bool rohr_entity_is_alive(Entity entity);

/**
 * @brief Checks whether an entity table index currently contains a live entity.
 * @param index Component table index to inspect.
 * @return true when the index contains a live entity, false otherwise.
 */
bool rohr_entity_index_is_alive(EntityIndex index);

/**
 * @brief Returns the number of currently alive entities.
 * @return Number of alive entities.
 */
uint32_t rohr_entity_alive_count(void);

/**
 * @brief Returns the entity id stored at a dense alive-list position.
 *
 * The position is not a component table index and can change when entities are
 * deleted.
 *
 * @param position Dense alive-list position.
 * @return EntityResult containing the entity id, or an error.
 */
EntityResult rohr_entity_alive_at(uint32_t position);

/**
 * @brief Resolves a stable entity id to its current component table index.
 * @param entity Stable entity id to resolve.
 * @param index Destination for the resolved table index.
 * @return true when entity was resolved, false otherwise.
 */
bool rohr_entity_get_index(Entity entity, EntityIndex *index);

/**
 * @brief Returns the stable entity id stored at a component table index.
 * @param index Component table index to inspect.
 * @return EntityResult containing the entity id, or an error.
 */
EntityResult rohr_entity_from_index(EntityIndex index);

/**
 * @brief Creates a new entity.
 *
 * Entity ids are stable handles and may not match component table indexes.
 *
 * @return EntityResult containing the new entity id, or an error if the entity
 * limit is reached.
 */
EntityResult rohr_entity_add(void);

/** Assigns a unique fixed-size name to an entity. */
EngineResult rohr_entity_set_name(Entity entity, const char *name);

/** Finds a live entity by its state-file name. */
EntityResult rohr_entity_find_by_name(const char *name);

/** Returns a copy of an entity's fixed-size name component. */
EntityNameResult rohr_entity_get_name(Entity entity);

/** Loads and merges one JSON game-state file. */
EngineResult rohr_game_state_load_file(const char *path);

/** Loads multiple JSON files with cross-file name resolution. */
EngineResult rohr_game_state_load_files(const char *const *paths, size_t path_count);

/** Saves all named live entities to a JSON game-state file. */
EngineResult rohr_game_state_save_file(const char *path);

/** Saves retained authored definitions without expanding prototypes. */
EngineResult rohr_game_state_save_template_file(const char *path);

/**
 * @brief Deletes an entity and releases its slot for reuse.
 * @param entity Stable entity id to delete.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_delete(Entity entity);

/**
 * @brief Adds components to an entity.
 * @param entity Stable entity id to modify.
 * @param mask Component mask to add.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_add_components(Entity entity, CMask mask);

/**
 * @brief Checks whether an entity has all requested components.
 * @param entity Stable entity id to inspect.
 * @param components Component mask to test.
 * @return true when entity has every requested component, false otherwise.
 */
bool rohr_entity_has_components(Entity entity, CMask components);

/**
 * @brief Checks whether an entity table index has all requested components.
 * @param index Component table index to inspect.
 * @param components Component mask to test.
 * @return true when index has every requested component, false otherwise.
 */
bool rohr_entity_index_has_components(EntityIndex index, CMask components);

/**
 * @brief Creates a reusable entity group.
 * @return GroupIdResult containing a group id, or an error.
 */
GroupIdResult rohr_entity_group_create(void);

/** Assigns a unique fixed-size name to a generic group. */
EngineResult rohr_entity_group_set_name(GroupId group, const char *name);

/** Finds a live generic group by name. */
GroupIdResult rohr_entity_group_find_by_name(const char *name);

/** Returns a copy of a generic group's fixed-size name. */
GroupNameResult rohr_entity_group_get_name(GroupId group);

/**
 * @brief Destroys a generic entity group and clears member group references.
 * @param group Group id to destroy.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_group_destroy(GroupId group);

/**
 * @brief Adds an entity to a generic group.
 * @param group Group id to update.
 * @param entity Entity id to add.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_group_add(GroupId group, Entity entity);

/**
 * @brief Removes an entity from a generic group.
 * @param group Group id to update.
 * @param entity Entity id to remove.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_group_remove(GroupId group, Entity entity);

/**
 * @brief Checks whether an entity belongs to a group.
 * @param group Group id to inspect.
 * @param entity Entity id to search for.
 * @return true when entity belongs to the group.
 */
bool rohr_entity_group_contains(GroupId group, Entity entity);

/**
 * @brief Returns an entity group.
 * @param group Group id to inspect.
 * @return EntityGroupResult containing group data, or an error.
 */
EntityGroupResult rohr_entity_group_get(GroupId group);

/**
 * @brief Returns the groups assigned to an entity.
 * @param entity Entity id to inspect.
 * @return EntityGroupMembershipResult containing group ids, or an error.
 */
EntityGroupMembershipResult rohr_entity_get_groups(Entity entity);

/**
 * @brief Removes components from an entity.
 * @param entity Stable entity id to modify.
 * @param mask Component mask to remove.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_delete_components(Entity entity, CMask mask);

/**
 * @brief Adds a child relationship from parent to child.
 * @param parent Parent entity id.
 * @param child Child entity id.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_set_child(Entity parent, Entity child);

/**
 * @brief Sets an entity parent relationship.
 * @param child Child entity id.
 * @param parent Parent entity id.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_set_parent(Entity child, Entity parent);

/**
 * @brief Removes the parent relationship from an entity.
 * @param child Child entity id.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_remove_parent(Entity child);

/**
 * @brief Removes a child relationship from a parent entity.
 * @param parent Parent entity id.
 * @param child Child entity id.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_remove_child(Entity parent, Entity child);

/**
 * @brief Returns the children group assigned to an entity.
 * @param entity Stable entity id to inspect.
 * @return ChildrenResult containing a group id, or an error.
 */
ChildrenResult rohr_entity_get_children(Entity entity);

/**
 * @brief Returns the parent assigned to an entity.
 * @param entity Stable entity id to inspect.
 * @return ParentResult containing parent id, or an error.
 */
ParentResult rohr_entity_get_parent(Entity entity);

/**
 * @brief Adds or updates an entity lifetime.
 * @param entity Stable entity id to modify.
 * @param expirey_time Engine time when the entity expires.
 * @param expirey_tick Engine tick when the entity expires.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_set_life_time(Entity entity, Time expirey_time, Tick expirey_tick);

/**
 * @brief Removes lifetime data from an entity.
 * @param entity Stable entity id to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_remove_life_time(Entity entity);

/**
 * @brief Translates a local shape into world space.
 * @param shape Local shape to transform.
 * @param position World position.
 * @param angle World orientation in radians.
 * @return World-space shape.
 */
Shape rohr_physics_shape_world_translate(Shape shape, Position position, Orientation angle);

/**
 * @brief Calculates polygon moment of inertia.
 * @param shape Polygon shape.
 * @param mass_value Shape mass.
 * @return Moment of inertia value.
 */
float rohr_physics_polygon_moment_of_inertia(Shape shape, Mass mass_value);

/**
 * @brief Tests two shapes with separating axis theorem collision detection.
 * @param shape_1 First shape.
 * @param shape_2 Second shape.
 * @return Collision information.
 */
Collision rohr_physics_sat_collision(Shape shape_1, Shape shape_2);

/**
 * @brief Calculates circle moment of inertia.
 * @param circle Circle shape.
 * @param mass_value Circle mass.
 * @return Moment of inertia value.
 */
Vec1D rohr_physics_circle_moment_of_inertia(Shape circle, Mass mass_value);

/**
 * @brief Checks whether an entity index has HOLD.
 * @param index Entity table index to inspect.
 * @return true when index is live and held, false otherwise.
 */
bool rohr_physics_entity_is_held(EntityIndex index);

/**
 * @brief Sets an entity acceleration component value.
 * @param entity Entity to modify.
 * @param a Acceleration value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_acceleration(Entity entity, Acceleration a);

/**
 * @brief Sets an entity angular acceleration component value.
 * @param entity Entity to modify.
 * @param acceleration Angular acceleration value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_angular_acceleration(
    Entity entity,
    AngularAcceleration acceleration
);

/**
 * @brief Sets entity acceleration toward a world position.
 * @param entity Entity to modify.
 * @param acceleration_magnitude Acceleration magnitude to apply along the direction to position.
 * @param position Target world position.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_acceleration_toward_position(Entity entity, float acceleration_magnitude, Position position);

/**
 * @brief Sets entity acceleration toward another entity's current world position.
 * @param entity Entity to modify.
 * @param acceleration_magnitude Acceleration magnitude to apply along the direction to target.
 * @param target Target entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_acceleration_toward_entity(Entity entity, float acceleration_magnitude, Entity target);

/**
 * @brief Sets entity acceleration away from a world position.
 * @param entity Entity to modify.
 * @param acceleration_magnitude Acceleration magnitude to apply away from position.
 * @param position Source world position.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_acceleration_away_from_position(Entity entity, float acceleration_magnitude, Position position);

/**
 * @brief Sets entity acceleration away from another entity's current world position.
 * @param entity Entity to modify.
 * @param acceleration_magnitude Acceleration magnitude to apply away from target.
 * @param target Source entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_acceleration_away_from_entity(Entity entity, float acceleration_magnitude, Entity target);

/**
 * @brief Sets acceleration toward an entity for every live entity in a group.
 * @param group Group id to update.
 * @param acceleration_magnitude Acceleration magnitude to apply along the direction to target.
 * @param target Target entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_set_acceleration_toward_entity(GroupId group, float acceleration_magnitude, Entity target);

/**
 * @brief Sets acceleration away from an entity for every live entity in a group.
 * @param group Group id to update.
 * @param acceleration_magnitude Acceleration magnitude to apply away from target.
 * @param target Source entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_set_acceleration_away_from_entity(GroupId group, float acceleration_magnitude, Entity target);

/**
 * @brief Sets an entity velocity component value.
 * @param entity Entity to modify.
 * @param v Velocity value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_velocity(Entity entity, Velocity v);

/**
 * @brief Sets entity velocity toward a world position.
 * @param entity Entity to modify.
 * @param speed Speed to apply along the direction to position.
 * @param position Target world position.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_velocity_toward_position(Entity entity, float speed, Position position);

/**
 * @brief Sets entity velocity toward another entity's current world position.
 * @param entity Entity to modify.
 * @param speed Speed to apply along the direction to target.
 * @param target Target entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_velocity_toward_entity(Entity entity, float speed, Entity target);

/**
 * @brief Sets entity velocity away from a world position.
 * @param entity Entity to modify.
 * @param speed Speed to apply away from position.
 * @param position Source world position.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_velocity_away_from_position(Entity entity, float speed, Position position);

/**
 * @brief Sets entity velocity away from another entity's current world position.
 * @param entity Entity to modify.
 * @param speed Speed to apply away from target.
 * @param target Source entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_velocity_away_from_entity(Entity entity, float speed, Entity target);

/**
 * @brief Sets velocity toward an entity for every live entity in a group.
 * @param group Group id to update.
 * @param speed Speed to apply along the direction to target.
 * @param target Target entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_set_velocity_toward_entity(GroupId group, float speed, Entity target);

/**
 * @brief Sets velocity away from an entity for every live entity in a group.
 * @param group Group id to update.
 * @param speed Speed to apply away from target.
 * @param target Source entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_set_velocity_away_from_entity(GroupId group, float speed, Entity target);

/**
 * @brief Sets an entity velocity to zero.
 * @param entity Entity to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_stop_entity(Entity entity);

/**
 * @brief Sets velocity to zero for every live entity in a group.
 * @param group Group id to update.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_stop_entities(GroupId group);

/**
 * @brief Applies an immediate linear impulse to an entity velocity.
 * @param entity Entity to modify.
 * @param impulse Impulse vector.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_apply_impulse(Entity entity, Vec2D impulse);

/**
 * @brief Sets an entity position component value.
 * @param entity Entity to modify.
 * @param p Position value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_position(Entity entity, Position p);

/**
 * @brief Sets an entity mass component value.
 * @param entity Entity to modify.
 * @param m Mass value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_mass(Entity entity, Mass m);

/**
 * @brief Sets an entity force component value.
 * @param entity Entity to modify.
 * @param f Force value.
 * @return EntityResult containing entity on success, or an error.
 */
EntityResult rohr_physics_set_force(Entity entity, Force f);

/**
 * @brief Sets force component data directly on an existing entity.
 * @param entity Entity to modify.
 * @param force Force component value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_force_component(Entity entity, Force force);

/**
 * @brief Applies force to an entity for one physics tick.
 * @param entity Entity to target.
 * @param f Force vector.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_apply_force_for_one_tick(Entity entity, Force f);

/**
 * @brief Sets an entity torque component value.
 * @param entity Entity to modify.
 * @param t Torque value.
 * @return EntityResult containing entity on success, or an error.
 */
EntityResult rohr_physics_set_torque(Entity entity, Torque t);

/**
 * @brief Sets torque component data directly on an existing entity.
 * @param entity Entity to modify.
 * @param torque Torque component value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_torque_component(Entity entity, Torque torque);

/**
 * @brief Applies torque to an entity for one physics tick.
 * @param entity Entity to target.
 * @param t Torque value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_apply_torque_for_one_tick(Entity entity, Torque t);

/**
 * @brief Sets an entity hitbox component value.
 * @param entity Entity to modify.
 * @param hitbox Local-space hitbox shape.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_hitbox(Entity entity, Shape hitbox);

/**
 * @brief Sets an entity orientation component value.
 * @param entity Entity to modify.
 * @param angle Orientation in radians.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_orientation(Entity entity, Orientation angle);

/**
 * @brief Sets an entity angular velocity component value.
 * @param entity Entity to modify.
 * @param v Angular velocity value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_angular_velocity(Entity entity, AngularVelocity v);

/**
 * @brief Returns an entity hitbox transformed into world space.
 * @param entity Entity to inspect.
 * @return ShapeResult containing the world-space hitbox, or an error.
 */
ShapeResult rohr_physics_get_global_hit_box(Entity entity);

/**
 * @brief Sets an entity restitution value.
 * @param entity Entity to modify.
 * @param restitution Restitution value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_restitution(Entity entity, Restitution restitution);

/**
 * @brief Marks an entity as dynamic for physics simulation.
 * @param entity Entity to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_dynamic(Entity entity);

/**
 * @brief Marks an entity as static for physics simulation.
 * @param entity Entity to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_static(Entity entity);

/**
 * @brief Adds HOLD so physics update stages preserve current values.
 * @param entity Entity to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_hold_entity(Entity entity);

/**
 * @brief Removes HOLD without changing STATIC or DYNAMIC state.
 * @param entity Entity to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_unhold_entity(Entity entity);

/**
 * @brief Adds HOLD to every live entity in a group.
 * @param group Group id to update.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_hold_entities(GroupId group);

/**
 * @brief Removes HOLD from every live entity in a group.
 * @param group Group id to update.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_unhold_entities(GroupId group);

/**
 * @brief Locks an entity orientation between minimum and maximum angles.
 * @param entity Entity to modify.
 * @param min Minimum orientation in radians.
 * @param max Maximum orientation in radians.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_angle_lock(Entity entity, Orientation min, Orientation max);

/**
 * @brief Locks an entity position along an axis.
 * @param entity Entity to modify.
 * @param axis Axis to lock against.
 * @param axis_point Point on the locked axis.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_axis_lock(Entity entity, Axis axis, Position axis_point);

/**
 * @brief Sets an entity friction value.
 * @param entity Entity to modify.
 * @param friction Friction value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_friction(Entity entity, float friction);

/**
 * @brief Locks one entity transform to another entity.
 * @param driven Entity whose transform is driven.
 * @param driver Entity used as the transform source.
 * @param local_offset Offset from driver to driven in driver-local space.
 * @param local_angle Orientation offset from driver to driven.
 * @param lock_position true to lock position.
 * @param lock_orientation true to lock orientation.
 * @param inherit_velocity true to inherit driver velocity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_transform_lock(
    Entity driven,
    Entity driver,
    Vec2D local_offset,
    Orientation local_angle,
    bool lock_position,
    bool lock_orientation,
    bool inherit_velocity
);

/**
 * @brief Removes an entity transform lock.
 * @param entity Entity to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_remove_transform_lock(Entity entity);

/**
 * @brief Locks one entity to another using their current transform offset.
 * @param driven Entity whose transform is driven.
 * @param driver Entity used as the transform source.
 * @param lock_position true to lock position.
 * @param lock_orientation true to lock orientation.
 * @param inherit_velocity true to inherit driver velocity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_transform_lock_current_transform(
    Entity driven,
    Entity driver,
    bool lock_position,
    bool lock_orientation,
    bool inherit_velocity
);

/**
 * @brief Sets the target used by a force or torque source entity.
 * @param entity Force or torque source entity to modify.
 * @param target Live entity that receives the force or torque.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_target(Entity entity, Entity target);

/**
 * @brief Adds or replaces complete joint data on an existing entity.
 * @param entity Entity that owns the joint component.
 * @param joint Complete joint component value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_joint_component(Entity entity, Joint joint);

/**
 * @brief Creates a joint between two entities.
 * @param a First entity.
 * @param b Second entity.
 * @param type Joint behavior type.
 * @param local_anchor_a Anchor on the first entity in local space.
 * @param local_anchor_b Anchor on the second entity in local space.
 * @param stiffness Spring stiffness.
 * @param damping Spring damping.
 * @return EntityResult containing the joint entity, or an error.
 */
EntityResult rohr_physics_set_joint(
    Entity a,
    Entity b,
    JointType type,
    Vec2D local_anchor_a,
    Vec2D local_anchor_b,
    float stiffness,
    float damping
);

/**
 * @brief Tests two particle shapes for collision.
 * @param shape_1 First shape.
 * @param shape_2 Second shape.
 * @return Collision information.
 */
Collision rohr_physics_particle_collision(Shape shape_1, Shape shape_2);

/**
 * @brief Enables or disables collision reporting between two entities.
 * @param entity Reporting entity.
 * @param target Target entity.
 * @param state true to enable reporting, false to disable it.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_set_collision_report(Entity entity, Entity target, bool state);

/**
 * @brief Checks whether a collision report exists between two entities.
 * @param entity Reporting entity.
 * @param target Target entity.
 * @return true when reporting is enabled, false otherwise.
 */
bool rohr_physics_get_collision_report(Entity entity, Entity target);

/**
 * @brief Creates an engine color from a hexadecimal RGB or RGBA value.
 * @param hex_color_code Hex color value.
 * @return Color created from hex_color_code.
 */
Color rohr_graphics_create_color_hex(uint32_t hex_color_code);

/**
 * @brief Starts the graphics system.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_graphics_start(void);

/**
 * @brief Shuts down the graphics system.
 */
void rohr_graphics_end(void);

/**
 * @brief Polls graphics/window events.
 * @param event Destination for the SDL event.
 * @return true when an event was read, false otherwise.
 */
bool rohr_graphics_poll_events(SDL_Event *event);

/**
 * @brief Draws the frame background.
 * @param color Background color.
 */
void rohr_graphics_draw_background(Color color);

/**
 * @brief Draws a filled rectangle in logical screen coordinates.
 * @return true when SDL accepted the draw command.
 */
bool rohr_graphics_draw_screen_rect(float x, float y, float width, float height, Color color);

/**
 * @brief Presents the current graphics frame.
 */
void rohr_graphics_show(void);

/**
 * @brief Draws one entity hitbox.
 * @param entity Entity whose hitbox should be drawn.
 * @param fill_type Fill mode for drawing.
 */
void rohr_graphics_draw_hit_box(Entity entity, Fill fill_type);

/**
 * @brief Draws one entity hitbox with a caller supplied color.
 * @param entity Entity whose hitbox should be drawn.
 * @param fill_type Fill mode for drawing.
 * @param color Color to draw with.
 */
void rohr_graphics_draw_hit_box_colored(Entity entity, Fill fill_type, Color color);

/**
 * @brief Draws hitboxes for all renderable hitbox entities.
 */
void rohr_graphics_draw_hit_boxes(void);

/**
 * @brief Loads a texture asset.
 * @param text_desc Texture descriptor containing load settings.
 * @return TextureAssetResult containing the asset, or an error.
 */
TextureAssetResult rohr_graphics_load_texture(TextureDescriptor text_desc);

/**
 * @brief Loads an animation asset.
 * @param anim_desc Animation descriptor containing load settings.
 * @return AnimationAssetResult containing the asset, or an error.
 */
AnimationAssetResult rohr_graphics_load_animation(AnimationDescriptor anim_desc);

/**
 * @brief Creates an animated sprite from an animation asset.
 * @param asset_ptr Animation asset to use.
 * @param scale Sprite scale.
 * @return Animated sprite value.
 */
AnimatedSprite rohr_graphics_create_animated_sprite(AnimationAsset asset_ptr, Scale scale);

/**
 * @brief Adds an animated sprite to an entity.
 * @param entity Entity to modify.
 * @param sprite Animated sprite component value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_graphics_add_animated_sprite(Entity entity, AnimatedSprite sprite);

/**
 * @brief Draws all animated sprite components.
 */
void rohr_graphics_draw_animated_sprites(void);

/**
 * @brief Updates animated sprite frames.
 * @param current_tick Current engine tick.
 * @param current_time Current engine time.
 */
void rohr_graphics_update_sprite_frames(Tick current_tick, Time current_time);

/**
 * @brief Scales textures attached to an entity.
 * @param entity Entity to modify.
 * @param scale Scale value.
 */
void rohr_graphics_scale_textures(Entity entity, Scale scale);

/**
 * @brief Replaces the active camera transform.
 * @param camera World-space camera position and orientation.
 */
void rohr_graphics_set_camera(Camera camera);

/**
 * @brief Returns the active camera transform.
 * @return Current world-space camera transform.
 */
Camera rohr_graphics_get_camera(void);

/**
 * @brief Translates the active camera in world space.
 * @param translation World-space translation to add.
 */
void rohr_graphics_move_camera(Vec2D translation);

/**
 * @brief Rotates the active camera counterclockwise.
 * @param radians Rotation in radians to add.
 */
void rohr_graphics_rotate_camera(Orientation radians);

/**
 * @brief Attaches the camera to an entity's position and orientation.
 *
 * The position offset is in the entity's local space and rotates with the
 * entity. The orientation offset is added to the entity's orientation.
 *
 * @param entity Entity transform to follow.
 * @param position_offset Local-space position offset.
 * @param orientation_offset Orientation offset in radians.
 * @return EngineResult describing success or a missing transform.
 */
EngineResult rohr_graphics_attach_camera(
    Entity entity,
    Vec2D position_offset,
    Orientation orientation_offset
);

/**
 * @brief Attaches a camera with independent transform inheritance.
 *
 * When orientation following is disabled, position_offset is world-space.
 * When position following is disabled, position_offset is the fixed camera
 * position. When orientation following is disabled, orientation_offset is the
 * fixed camera orientation.
 *
 * @param entity Entity to associate with the camera.
 * @param position_offset Relative offset or fixed world position.
 * @param orientation_offset Relative or fixed orientation in radians.
 * @param follow_position Whether to inherit entity position.
 * @param follow_orientation Whether to inherit entity orientation.
 * @return EngineResult describing success or a missing required transform.
 */
EngineResult rohr_graphics_attach_camera_with_options(
    Entity entity,
    Vec2D position_offset,
    Orientation orientation_offset,
    bool follow_position,
    bool follow_orientation
);

/**
 * @brief Detaches the camera and preserves its current world transform.
 */
void rohr_graphics_detach_camera(void);

/**
 * @brief Reports whether the camera is attached to a live entity transform.
 * @return true when attached to a valid entity transform.
 */
bool rohr_graphics_camera_is_attached(void);

/**
 * @brief Copies the active camera attachment description.
 * @param attachment Destination owned by the caller.
 * @return true when the camera has a valid entity attachment.
 */
bool rohr_graphics_get_camera_attachment(CameraAttachment *attachment);

/**
 * @brief Converts a world position to screen coordinates.
 * @param pos World position.
 * @return Screen-space position.
 */
Position rohr_graphics_world_to_screen(Position pos);

/**
 * @brief Converts a screen position to world coordinates.
 * @param screen Screen-space position.
 * @return World-space position.
 */
Position rohr_graphics_screen_to_world(Position screen);

/**
 * @brief Returns the current mouse position in screen coordinates.
 * @return Mouse screen position.
 */
Position rohr_graphics_get_mouse_screen_position(void);

/**
 * @brief Draws the spatial grid overlay.
 */
void rohr_graphics_draw_grid(void);

/**
 * @brief Starts recording rendered frames to a video file.
 * @param output_path Path where the recording should be written.
 * @param fps Recording frame rate.
 * @return true when recording starts successfully, false otherwise.
 */
bool rohr_graphics_recording_start(const char *output_path, int fps);

/**
 * @brief Draws active particle components.
 */
void rohr_graphics_draw_particles(void);

/**
 * @brief Draws local origin markers for entities.
 */
void rohr_graphics_draw_local_origins(void);

/**
 * @brief Creates normalized edge normals for a shape.
 * @param shape Shape to inspect.
 * @return List of normal vectors.
 */
Vec2DList rohr_math_create_normals(Shape shape);

/**
 * @brief Normalizes a vector.
 * @param vector Vector to normalize.
 * @return Normalized vector.
 */
Vec2D rohr_math_normalize_vector(Vec2D vector);

/**
 * @brief Normalizes all vectors in a list.
 * @param vectors Vector list to normalize.
 * @return Normalized vector list.
 */
Vec2DList rohr_math_normalize_vectors(Vec2DList vectors);

/**
 * @brief Calculates the dot product of two vectors.
 * @param vector_1 First vector.
 * @param vector_2 Second vector.
 * @return Dot product.
 */
float rohr_math_dot_product(Vec2D vector_1, Vec2D vector_2);

/**
 * @brief Creates a rectangular polygon shape.
 * @param width Rectangle width.
 * @param height Rectangle height.
 * @return Shape containing rectangle vertices.
 */
Shape rohr_math_create_square(float width, float height);

/**
 * @brief Creates a circle approximation shape.
 * @param radius Circle radius.
 * @param verticies Number of vertices used to approximate the circle.
 * @return Shape containing circle vertices.
 */
Shape rohr_math_create_circle(float radius, uint8_t verticies);

/**
 * @brief Projects a shape onto an axis.
 * @param shape Shape to project.
 * @param axis Axis to project onto.
 * @return Projection interval.
 */
Projection rohr_math_project_shape_on_axis(Shape shape, Axis axis);

/**
 * @brief Calculates the scalar 2D cross product.
 * @param a First vector.
 * @param b Second vector.
 * @return Cross product value.
 */
float rohr_math_cross_2d(Vec2D a, Vec2D b);

/**
 * @brief Calculates angular velocity crossed with a vector.
 * @param omega Angular velocity.
 * @param r Radius or offset vector.
 * @return Tangential velocity vector.
 */
Vec2D rohr_math_angular_velocity_cross_vec(float omega, Vec2D r);

/**
 * @brief Projects a vector onto an axis.
 * @param v Vector to project.
 * @param axis Axis to project onto.
 * @return Projected vector.
 */
Vec2D rohr_math_project_onto_axis(Vec2D v, Axis axis);

/**
 * @brief Calculates axis magnitude.
 * @param axis Axis vector.
 * @return Magnitude.
 */
float rohr_math_axis_magnitude(Axis axis);

/**
 * @brief Calculates vector magnitude.
 * @param vector Vector to inspect.
 * @return Magnitude.
 */
float rohr_math_vector_magnitude(Vec2D vector);

/**
 * @brief Rotates a vector by an angle.
 * @param vector Vector to rotate.
 * @param angle Angle in radians.
 * @return Rotated vector.
 */
Vec2D rohr_math_rotate_vector(Vec2D vector, float angle);

/**
 * @brief Calculates a circle radius from its shape and centroid.
 * @param circle Circle shape.
 * @param centroid Circle centroid.
 * @return Circle radius.
 */
Vec1D rohr_math_circle_radius(Shape circle, Vec2D centroid);

/**
 * @brief Subtracts one vector from another.
 * @param vector_a Vector to subtract from.
 * @param vector_b Vector to subtract.
 * @return vector_a minus vector_b.
 */
Vec2D rohr_math_vector_subtract(Vec2D vector_a, Vec2D vector_b);

/**
 * @brief Calculates overlap depth between two circles.
 * @param centroid_1 First circle centroid.
 * @param radius_1 First circle radius.
 * @param centroid_2 Second circle centroid.
 * @param radius_2 Second circle radius.
 * @return Circle overlap depth.
 */
Vec1D rohr_math_circle_overlap_depth(Vec2D centroid_1, Vec1D radius_1, Vec2D centroid_2, Vec1D radius_2);

/**
 * @brief Calculates overlap between two projection intervals.
 * @param projection_1 First projection.
 * @param projection_2 Second projection.
 * @return Overlap depth.
 */
float rohr_math_projection_overlap(Projection projection_1, Projection projection_2);

/**
 * @brief Scales a shape uniformly.
 * @param shape Shape to scale.
 * @param scale Uniform scale value.
 * @return Scaled shape.
 */
Shape rohr_math_scale_shape(Shape shape, float scale);

/**
 * @brief Scales a shape along the y axis.
 * @param shape Shape to scale.
 * @param scale Y-axis scale value.
 * @return Scaled shape.
 */
Shape rohr_math_scale_shape_y(Shape shape, float scale);

/**
 * @brief Scales a shape along the x axis.
 * @param shape Shape to scale.
 * @param scale X-axis scale value.
 * @return Scaled shape.
 */
Shape rohr_math_scale_shape_x(Shape shape, float scale);

/**
 * @brief Calculates a polygon centroid.
 * @param shape Polygon shape.
 * @return Centroid position.
 */
Vec2D rohr_math_polygon_centroid(Shape shape);

/**
 * @brief Adds a vertex slot to a shape.
 * @param shape Shape to modify.
 * @return Shape with an additional vertex slot.
 */
Shape rohr_math_add_vertex(Shape shape);

/**
 * @brief Deletes the last vertex slot from a shape.
 * @param shape Shape to modify.
 * @return Shape with one fewer vertex slot.
 */
Shape rohr_math_delete_vertex(Shape shape);

/**
 * @brief Creates an axis-aligned bounding box for a world-space shape.
 * @param world_shape World-space shape.
 * @return Axis-aligned bounding box.
 */
AABB rohr_math_create_aabb(Shape world_shape);

/**
 * @brief Runs one physics-system update.
 * @param dt Simulation delta time in seconds.
 */
void rohr_system_update_physics(double dt);

/**
 * @brief Deletes entities whose lifetime has expired.
 */
void rohr_system_clean_entities_past_lifetime(void);

/**
 * @brief Initializes the level editor.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_level_editor_init(void);

/**
 * @brief Updates the level editor.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_level_editor_update(void);

/**
 * @brief Updates keyboard key states for the frame.
 * @param keyboard Keyboard state table to update.
 */
void rohr_controller_update_key_states(KeyboardState *keyboard);

/**
 * @brief Adds a keyboard event to a keyboard state table.
 * @param keyboard Keyboard state table to modify.
 * @param key_event Keyboard event to add.
 */
void rohr_controller_add_key_event(KeyboardState *keyboard, KeyboardEvent key_event);

/**
 * @brief Converts an SDL event into a Rohr keyboard event.
 * @param sdl_event SDL event to inspect.
 * @return KeyboardEvent derived from sdl_event.
 */
KeyboardEvent rohr_controller_capture_keyboard_event(const SDL_Event *sdl_event);

/**
 * @brief Checks whether an SDL keycode is currently held or was pressed this frame.
 * @param keyboard Keyboard state table to inspect.
 * @param keycode SDL keycode to check.
 * @return true when the key is down or pressed.
 */
bool rohr_controller_key_down(const KeyboardState *keyboard, SDL_Keycode keycode);

/**
 * @brief Checks whether an SDL keycode was pressed this frame.
 * @param keyboard Keyboard state table to inspect.
 * @param keycode SDL keycode to check.
 * @return true when the key was pressed this frame.
 */
bool rohr_controller_key_pressed(const KeyboardState *keyboard, SDL_Keycode keycode);

/**
 * @brief Checks whether an SDL keycode was released this frame.
 * @param keyboard Keyboard state table to inspect.
 * @param keycode SDL keycode to check.
 * @return true when the key was released this frame.
 */
bool rohr_controller_key_released(const KeyboardState *keyboard, SDL_Keycode keycode);

/**
 * @brief Returns normalized movement input from supplied up/left/down/right SDL keycodes.
 *
 * Opposing directions cancel before normalization. For example, left+right
 * produces zero X, and up+down produces zero Y.
 *
 * @param keyboard Keyboard state table to inspect.
 * @param up SDL keycode for positive Y.
 * @param left SDL keycode for negative X.
 * @param down SDL keycode for negative Y.
 * @param right SDL keycode for positive X.
 * @return Direction vector from the supplied directional keys.
 */
Vec2D rohr_controller_axis_from_keycodes(
        const KeyboardState *keyboard,
        SDL_Keycode up,
        SDL_Keycode left,
        SDL_Keycode down,
        SDL_Keycode right
);

/**
 * @brief Returns normalized movement input from W/A/S/D.
 * @param keyboard Keyboard state table to inspect.
 * @return Direction vector where W is positive Y and D is positive X.
 */
Vec2D rohr_controller_wasd_axis(const KeyboardState *keyboard);

/**
 * @brief Returns normalized movement input from arrow keys.
 * @param keyboard Keyboard state table to inspect.
 * @return Direction vector where up is positive Y and right is positive X.
 */
Vec2D rohr_controller_arrow_axis(const KeyboardState *keyboard);

/**
 * @brief Prints a mouse event for debugging.
 * @param event Mouse event to print.
 */
void rohr_controller_print_mouse_event(MouseEvent event);

/**
 * @brief Updates mouse button states for the frame.
 * @param mouse Mouse state table to update.
 */
void rohr_controller_update_mouse_states(MouseState *mouse);

/**
 * @brief Adds a mouse event to a mouse state table.
 * @param mouse Mouse state table to modify.
 * @param mouse_event Mouse event to add.
 */
void rohr_controller_add_mouse_event(MouseState *mouse, MouseEvent mouse_event);

/**
 * @brief Converts an SDL event into a Rohr mouse event.
 * @param sdl_event SDL event to inspect.
 * @return MouseEvent derived from sdl_event.
 */
MouseEvent rohr_controller_capture_mouse_event(const SDL_Event *sdl_event);

/**
 * @brief Converts the current logical screen-space mouse position to world space.
 * @param mouse Mouse state to convert.
 * @return World position under the mouse, or zero when mouse is NULL.
 */
Position rohr_controller_mouse_world_position(const MouseState *mouse);

/**
 * @brief Adds an entity to the spatial grid tables.
 * @param entity Entity to add.
 */
void rohr_grid_add_entity_to_grids(Entity entity);

/**
 * @brief Checks whether a pair of entities has already been processed.
 * @param entity_1 First entity.
 * @param entity_2 Second entity.
 * @return true when the pair was already checked, false otherwise.
 */
bool rohr_grid_checked_pair(Entity entity_1, Entity entity_2);

/**
 * @brief Stores a processed entity pair.
 * @param entity_1 First entity.
 * @param entity_2 Second entity.
 */
void rohr_grid_add_pair(Entity entity_1, Entity entity_2);

/**
 * @brief Clears spatial grid state.
 */
void rohr_grid_clear(void);

/**
 * @brief Updates an entity axis-aligned bounding box in the grid.
 * @param entity Entity to update.
 */
void rohr_grid_update_aabb(Entity entity);

/**
 * @brief Delays execution for a number of seconds.
 * @param seconds Number of seconds to delay.
 */
void rohr_tools_delay(int seconds);

/**
 * @brief Writes a binary string representation of a value.
 * @param value Value to convert.
 * @param buffer Destination buffer.
 * @param size Size of buffer in bytes.
 */
void rohr_tools_binary_to_string(uint32_t value, char *buffer, size_t size);

/**
 * @brief Appends one string to another using explicit buffer sizes.
 * @param src Source string.
 * @param dst Destination string.
 * @param src_size Source buffer size.
 * @param dst_size Destination buffer size.
 */
void rohr_tools_append_string(char *src, char *dst, size_t src_size, size_t dst_size);

/**
 * @brief Counts characters in a string until a delimiter.
 * @param str String to inspect.
 * @param delimiter Delimiter that stops counting.
 * @return Number of characters before delimiter.
 */
uint32_t rohr_tools_sizeof_string(char *str, char delimiter);

/**
 * @brief Returns a random integer in a range.
 * @param min Minimum value.
 * @param max Maximum value.
 * @return Random integer between min and max.
 */
int rohr_tools_random_range(int min, int max);

/**
 * @brief Returns a random float in a range.
 * @param min Minimum value.
 * @param max Maximum value.
 * @return Random float between min and max.
 */
float rohr_tools_random_range_float(float min, float max);

/** @brief Starts a UI frame with logical screen-space pointer input. */
void rohr_ui_begin_frame(UIInput input);

/** @brief Draws and updates one button identified by a stable string. */
UIButtonResult rohr_ui_button(const char *id, UIRect bounds, const UIButtonStyle *style);

/** @brief Draws a disabled button that cannot capture input. */
void rohr_ui_button_disabled(UIRect bounds, const UIButtonStyle *style);

/** @brief Returns whether UI consumed pointer input during this frame. */
bool rohr_ui_pointer_consumed(void);

/** @brief Finishes the current UI frame. */
void rohr_ui_end_frame(void);

/** @brief Returns the default button colors. */
UIButtonStyle rohr_ui_default_button_style(void);

#endif
