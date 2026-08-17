#ifndef ROHR_H
#define ROHR_H

#include "console.h"
#include "controller.h"
#include "engine.h"
#include "entity_components.h"
#include "error.h"
#include "graphics.h"
#include "game_state.h"
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
void rohr_engine_time_update(void);

/**
 * @brief Returns the current engine time in seconds.
 * @return Current engine time.
 */
Time rohr_engine_time_get(void);

/**
 * @brief Returns the current engine tick counter.
 * @return Current tick.
 */
Tick rohr_engine_tick_get(void);

/**
 * @brief Pauses engine time-dependent updates.
 */
void rohr_engine_pause(void);

/**
 * @brief Resumes engine time-dependent updates.
 */
void rohr_engine_resume(void);

/** Sets the real-time duration required for one engine tick. */
EngineResult rohr_engine_time_per_tick_set(Time time_per_tick);
/** Returns the real-time duration required for one engine tick. */
Time rohr_engine_time_per_tick_get(void);

/**
 * @brief Polls one SDL event.
 * @return SDL event value returned by the engine event poller.
 */
SDL_Event rohr_engine_event_poll(void);

/**
 * @brief Checks whether the engine is paused.
 * @return true when paused, false when running.
 */
bool rohr_engine_paused_get(void);

/**
 * @brief Resets the engine clock baseline.
 */
void rohr_engine_clock_reset(void);

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
 * @brief Returns the static message associated with an engine error code.
 * @param error Error code to describe.
 * @return Static string describing error.
 */
const char *rohr_error_code_message_get(EngineError error);

/**
 * @brief Returns a result's error message with captured low-level detail.
 *
 * ResultValue should be a result variable because this macro evaluates it
 * twice. Every generated Rohr result type supports this shared field layout.
 *
 * @param ResultValue Generated Rohr result value to describe.
 * @return Engine-owned string valid until the next error.
 */
#define rohr_error_message_get(ResultValue) \
    error_result_message_get((ResultValue).kind, (ResultValue).result.error)

/**
 * @brief Prints buffered console log messages.
 */
void rohr_console_logs_print(void);

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
bool rohr_console_active_get(void);

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
void rohr_console_debug_set(bool state);

/**
 * @brief Checks whether an entity id currently refers to a live entity.
 * @param entity Stable entity id to inspect.
 * @return true when the entity is alive, false otherwise.
 */
bool rohr_entity_alive_check(Entity entity);

/**
 * @brief Checks whether an entity table index currently contains a live entity.
 * @param index Component table index to inspect.
 * @return true when the index contains a live entity, false otherwise.
 */
bool rohr_entity_index_alive_check(EntityIndex index);

/**
 * @brief Returns the number of currently alive entities.
 * @return Number of alive entities.
 */
uint32_t rohr_entity_alive_count_get(void);

/**
 * @brief Returns the entity id stored at a dense alive-list position.
 *
 * The position is not a component table index and can change when entities are
 * deleted.
 *
 * @param position Dense alive-list position.
 * @return EntityResult containing the entity id, or an error.
 */
EntityResult rohr_entity_alive_at_get(uint32_t position);

/**
 * @brief Resolves a stable entity id to its current component table index.
 * @param entity Stable entity id to resolve.
 * @return EntityIndexResult containing the index or an invalid-entity error.
 */
EntityIndexResult rohr_entity_index_get(Entity entity);

/**
 * @brief Returns the stable entity id stored at a component table index.
 * @param index Component table index to inspect.
 * @return EntityResult containing the entity id, or an error.
 */
EntityResult rohr_entity_from_index_get(EntityIndex index);

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
EngineResult rohr_entity_name_set(Entity entity, const char *name);

/** Finds a live entity by its state-file name. */
EntityResult rohr_entity_by_name_get(const char *name);

/** Returns a copy of an entity's fixed-size name component. */
EntityNameResult rohr_entity_name_get(Entity entity);

/** Loads and merges one JSON game-state file. */
EngineResult rohr_game_state_file_load(const char *path);

/** Loads multiple JSON files with cross-file name resolution. */
EngineResult rohr_game_state_files_load(const char *const *paths, size_t path_count);

/** @brief Finds a UI button definition loaded from JSON. */
UIButtonDefinitionResult rohr_ui_button_by_name_get(const char *name);

/** @brief Finds a UI font definition loaded from JSON. */
UIFontDefinitionResult rohr_ui_font_by_name_get(const char *name);

/** @brief Finds a standalone UI label definition loaded from JSON. */
UILabelDefinitionResult rohr_ui_label_by_name_get(const char *name);

/** @brief Finds a UI slider definition loaded from JSON. */
UISliderDefinitionResult rohr_ui_slider_by_name_get(const char *name);

/** Saves all named live entities to a JSON game-state file. */
EngineResult rohr_game_state_file_save(const char *path);

/** Saves retained authored definitions without expanding prototypes. */
EngineResult rohr_game_state_template_file_save(const char *path);

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
EngineResult rohr_entity_components_add(Entity entity, RohrComponentMask mask);

/**
 * @brief Checks whether an entity has all requested components.
 * @param entity Stable entity id to inspect.
 * @param components Component mask to test.
 * @return true when entity has every requested component, false otherwise.
 */
bool rohr_entity_components_check(Entity entity, RohrComponentMask components);

/**
 * @brief Checks whether an entity table index has all requested components.
 * @param index Component table index to inspect.
 * @param components Component mask to test.
 * @return true when index has every requested component, false otherwise.
 */
bool rohr_entity_index_components_check(EntityIndex index, RohrComponentMask components);

/**
 * @brief Creates a reusable entity group.
 * @return GroupIdResult containing a group id, or an error.
 */
GroupIdResult rohr_entity_group_create(void);

/** Assigns a unique fixed-size name to a generic group. */
EngineResult rohr_entity_group_name_set(GroupId group, const char *name);

/** Finds a live generic group by name. */
GroupIdResult rohr_entity_group_by_name_get(const char *name);

/** Returns a copy of a generic group's fixed-size name. */
GroupNameResult rohr_entity_group_name_get(GroupId group);

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
bool rohr_entity_group_entity_check(GroupId group, Entity entity);

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
EntityGroupMembershipResult rohr_entity_groups_get(Entity entity);

/**
 * @brief Removes components from an entity.
 * @param entity Stable entity id to modify.
 * @param mask Component mask to remove.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_components_delete(Entity entity, RohrComponentMask mask);

/**
 * @brief Adds a child relationship from parent to child.
 * @param parent Parent entity id.
 * @param child Child entity id.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_child_set(Entity parent, Entity child);

/**
 * @brief Sets an entity parent relationship.
 * @param child Child entity id.
 * @param parent Parent entity id.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_parent_set(Entity child, Entity parent);

/**
 * @brief Removes the parent relationship from an entity.
 * @param child Child entity id.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_parent_remove(Entity child);

/**
 * @brief Removes a child relationship from a parent entity.
 * @param parent Parent entity id.
 * @param child Child entity id.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_child_remove(Entity parent, Entity child);

/**
 * @brief Returns the children group assigned to an entity.
 * @param entity Stable entity id to inspect.
 * @return ChildrenResult containing a group id, or an error.
 */
ChildrenResult rohr_entity_children_get(Entity entity);

/**
 * @brief Returns the parent assigned to an entity.
 * @param entity Stable entity id to inspect.
 * @return ParentResult containing parent id, or an error.
 */
ParentResult rohr_entity_parent_get(Entity entity);

/**
 * @brief Adds or updates an entity lifetime.
 * @param entity Stable entity id to modify.
 * @param expirey_time Engine time when the entity expires.
 * @param expirey_tick Engine tick when the entity expires.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_life_time_set(Entity entity, Time expirey_time, Tick expirey_tick);

/**
 * @brief Removes lifetime data from an entity.
 * @param entity Stable entity id to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_entity_life_time_remove(Entity entity);

/** Sets an explicit simulation delta per engine tick. */
EngineResult rohr_physics_dt_per_tick_set(Time dt);
/** Returns the current physics delta per tick. */
Time rohr_physics_dt_per_tick_get(void);
/** Restores the engine time-per-tick default. */
void rohr_physics_engine_time_per_tick_use(void);
/** Sets constraint-solver iterations. Must be greater than zero. */
EngineResult rohr_physics_solver_iterations_set(uint32_t iterations);
/** Returns constraint-solver iterations. Defaults to 8. */
uint32_t rohr_physics_solver_iterations_get(void);
/** Sets integration and collision-detection substeps. Must be greater than zero. */
EngineResult rohr_physics_substeps_set(uint32_t substeps);
/** Returns physics substeps. Defaults to 1. */
uint32_t rohr_physics_substeps_get(void);
/** Sets the global acceleration applied to ROHR_GRAVITY entities. */
EngineResult rohr_physics_gravity_set(Acceleration gravity);
/** Returns the current global gravity acceleration. */
Acceleration rohr_physics_gravity_get(void);
/** Enables engine gravity for an entity. */
EngineResult rohr_physics_gravity_enable(Entity entity);
/** Disables engine gravity for an entity. */
EngineResult rohr_physics_gravity_disable(Entity entity);
/** Returns whether engine gravity is enabled for an entity. */
bool rohr_physics_gravity_check(Entity entity);
/** Begins a complete custom physics step and advances interaction state. */
void rohr_physics_pipeline_step_begin(void);
/** Clears transient constraints before one custom substep. */
void rohr_physics_pipeline_substep_begin(void);
/** Clears force-derived acceleration from the previous substep. */
void rohr_physics_pipeline_accelerations_clear(void);
/** Applies global gravity to opted-in movable entities. */
void rohr_physics_pipeline_gravity_apply(void);
/** Applies spring-joint and soft-body-beam forces. */
void rohr_physics_pipeline_forces_apply(void);
/** Integrates rigid-body state by dt seconds. */
void rohr_physics_pipeline_integrate(double dt);
/** Detects contacts and gathers contact constraints. */
void rohr_physics_pipeline_contacts_gather(void);
/** Gathers active pin and weld joint constraints. */
void rohr_physics_pipeline_joints_gather(void);
/** Solves currently gathered contacts and joints. */
void rohr_physics_pipeline_constraints_solve(uint32_t iterations);
/** Runs one standard physics substep. */
void rohr_physics_pipeline_substep(double dt);
/** Runs the standard plug-and-play physics pipeline. */
void rohr_physics_pipeline_update(double dt);
/** Advances physics using the supplied number of elapsed engine ticks. */
void rohr_physics_update(Tick ticks);
/** Advances physics once with an explicit exceptional delta. */
void rohr_physics_dt_update(Time dt);

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
 * @brief Gets overlap information using the separating axis theorem.
 * @param shape_1 First shape.
 * @param shape_2 Second shape.
 * @return Geometric overlap information.
 */
OverlapInfo rohr_physics_sat_overlap_get(Shape shape_1, Shape shape_2);

/**
 * @brief Calculates circle moment of inertia.
 * @param circle Circle shape.
 * @param mass_value Circle mass.
 * @return Moment of inertia value.
 */
Vec1D rohr_physics_circle_moment_of_inertia(Shape circle, Mass mass_value);

/**
 * @brief Checks whether an entity index has ROHR_HOLD.
 * @param index Entity table index to inspect.
 * @return true when index is live and held, false otherwise.
 */
bool rohr_physics_entity_held_get(EntityIndex index);

/**
 * @brief Sets an entity acceleration component value.
 * @param entity Entity to modify.
 * @param a Acceleration value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_acceleration_set(Entity entity, Acceleration a);

/**
 * @brief Sets an entity angular acceleration component value.
 * @param entity Entity to modify.
 * @param acceleration Angular acceleration value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_angular_acceleration_set(
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
EngineResult rohr_physics_acceleration_toward_position_set(Entity entity, float acceleration_magnitude, Position position);

/**
 * @brief Sets entity acceleration toward another entity's current world position.
 * @param entity Entity to modify.
 * @param acceleration_magnitude Acceleration magnitude to apply along the direction to target.
 * @param target Target entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_acceleration_toward_entity_set(Entity entity, float acceleration_magnitude, Entity target);

/**
 * @brief Sets entity acceleration away from a world position.
 * @param entity Entity to modify.
 * @param acceleration_magnitude Acceleration magnitude to apply away from position.
 * @param position Source world position.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_acceleration_away_from_position_set(Entity entity, float acceleration_magnitude, Position position);

/**
 * @brief Sets entity acceleration away from another entity's current world position.
 * @param entity Entity to modify.
 * @param acceleration_magnitude Acceleration magnitude to apply away from target.
 * @param target Source entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_acceleration_away_from_entity_set(Entity entity, float acceleration_magnitude, Entity target);

/**
 * @brief Sets acceleration toward an entity for every live entity in a group.
 * @param group Group id to update.
 * @param acceleration_magnitude Acceleration magnitude to apply along the direction to target.
 * @param target Target entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_acceleration_toward_entity_set(GroupId group, float acceleration_magnitude, Entity target);

/**
 * @brief Sets acceleration away from an entity for every live entity in a group.
 * @param group Group id to update.
 * @param acceleration_magnitude Acceleration magnitude to apply away from target.
 * @param target Source entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_acceleration_away_from_entity_set(GroupId group, float acceleration_magnitude, Entity target);

/**
 * @brief Sets an entity velocity component value.
 * @param entity Entity to modify.
 * @param v Velocity value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_velocity_set(Entity entity, Velocity v);

/**
 * @brief Sets entity velocity toward a world position.
 * @param entity Entity to modify.
 * @param speed Speed to apply along the direction to position.
 * @param position Target world position.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_velocity_toward_position_set(Entity entity, float speed, Position position);

/**
 * @brief Sets entity velocity toward another entity's current world position.
 * @param entity Entity to modify.
 * @param speed Speed to apply along the direction to target.
 * @param target Target entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_velocity_toward_entity_set(Entity entity, float speed, Entity target);

/**
 * @brief Sets entity velocity away from a world position.
 * @param entity Entity to modify.
 * @param speed Speed to apply away from position.
 * @param position Source world position.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_velocity_away_from_position_set(Entity entity, float speed, Position position);

/**
 * @brief Sets entity velocity away from another entity's current world position.
 * @param entity Entity to modify.
 * @param speed Speed to apply away from target.
 * @param target Source entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_velocity_away_from_entity_set(Entity entity, float speed, Entity target);

/**
 * @brief Sets velocity toward an entity for every live entity in a group.
 * @param group Group id to update.
 * @param speed Speed to apply along the direction to target.
 * @param target Target entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_velocity_toward_entity_set(GroupId group, float speed, Entity target);

/**
 * @brief Sets velocity away from an entity for every live entity in a group.
 * @param group Group id to update.
 * @param speed Speed to apply away from target.
 * @param target Source entity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_velocity_away_from_entity_set(GroupId group, float speed, Entity target);

/**
 * @brief Sets an entity velocity to zero.
 * @param entity Entity to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_entity_stop(Entity entity);

/**
 * @brief Sets velocity to zero for every live entity in a group.
 * @param group Group id to update.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_entities_stop(GroupId group);

/**
 * @brief Applies an immediate linear impulse to an entity velocity.
 * @param entity Entity to modify.
 * @param impulse Impulse vector.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_impulse_apply(Entity entity, Vec2D impulse);

/**
 * @brief Sets an entity position component value.
 * @param entity Entity to modify.
 * @param p Position value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_position_set(Entity entity, Position p);
/** Returns an entity's world position. */
PositionResult rohr_physics_position_get(Entity entity);

/**
 * @brief Sets an entity mass component value.
 * @param entity Entity to modify.
 * @param m Mass value.
 * @return EngineResult describing success or failure.
 */
/** Sets finite, non-negative mass. Zero represents an explicitly massless entity. */
EngineResult rohr_physics_mass_set(Entity entity, Mass m);
EngineResult rohr_physics_mass_remove(Entity entity);
bool rohr_physics_mass_check(Entity entity);
EngineResult rohr_physics_kinematic_driven_set(Entity entity);
EngineResult rohr_physics_kinematic_driven_remove(Entity entity);
bool rohr_physics_kinematic_driven_check(Entity entity);

/**
 * @brief Sets an entity force component value.
 * @param entity Entity to modify.
 * @param f Force value.
 * @return EntityResult containing entity on success, or an error.
 */
EntityResult rohr_physics_force_create(Entity entity, Force f);

/**
 * @brief Sets force component data directly on an existing entity.
 * @param entity Entity to modify.
 * @param force Force component value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_force_component_set(Entity entity, Force force);

/**
 * @brief Applies force to an entity for one physics tick.
 * @param entity Entity to target.
 * @param f Force vector.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_force_for_one_tick_apply(Entity entity, Force f);

/**
 * @brief Sets an entity torque component value.
 * @param entity Entity to modify.
 * @param t Torque value.
 * @return EntityResult containing entity on success, or an error.
 */
EntityResult rohr_physics_torque_create(Entity entity, Torque t);

/**
 * @brief Sets torque component data directly on an existing entity.
 * @param entity Entity to modify.
 * @param torque Torque component value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_torque_component_set(Entity entity, Torque torque);

/**
 * @brief Applies torque to an entity for one physics tick.
 * @param entity Entity to target.
 * @param t Torque value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_torque_for_one_tick_apply(Entity entity, Torque t);

/**
 * @brief Sets an entity hitbox component value without enabling physical collision response.
 * @param entity Entity to modify.
 * @param hitbox Local-space hitbox shape.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_hitbox_set(Entity entity, Shape hitbox);

/** Return the default collision filtering configuration. */
CollisionFilterConfig rohr_physics_collision_filter_config_default_get(void);
/** Replace an entity's collision category and whitelist. */
EngineResult rohr_physics_collision_filter_set(Entity entity, CollisionFilterConfig config);
/** Return an entity's collision filtering configuration. */
CollisionFilterConfigResult rohr_physics_collision_filter_get(Entity entity);
/** Set the collision categories represented by an entity. */
EngineResult rohr_physics_collision_category_set(Entity entity, RohrCollisionCategoryMask category);
/** Set the collision category whitelist for an entity. */
EngineResult rohr_physics_collision_with_set(Entity entity, RohrCollisionCategoryMask categories);
/** Allow an entity to collide with every category. */
EngineResult rohr_physics_collision_with_all_set(Entity entity);
/** Prevent an entity from colliding with every category. */
EngineResult rohr_physics_collision_with_none_set(Entity entity);
/** Return whether two entities mutually permit collision checks. */
bool rohr_physics_collision_between_check(Entity entity_1, Entity entity_2);

/**
 * @brief Sets an entity orientation component value.
 * @param entity Entity to modify.
 * @param angle Orientation in radians.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_orientation_set(Entity entity, Orientation angle);

/**
 * @brief Sets an entity angular velocity component value.
 * @param entity Entity to modify.
 * @param v Angular velocity value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_angular_velocity_set(Entity entity, AngularVelocity v);

/**
 * @brief Returns an entity angular velocity.
 * @param entity Entity to inspect.
 * @return AngularVelocityResult containing radians per second, or an error.
 */
AngularVelocityResult rohr_physics_angular_velocity_get(Entity entity);

/** Sets the absolute angular-velocity limit applied before orientation integration. */
EngineResult rohr_physics_angular_velocity_maximum_set(
    Entity entity,
    AngularVelocity maximum
);
/** Returns an entity's configured absolute angular-velocity limit. */
AngularVelocityResult rohr_physics_angular_velocity_maximum_get(Entity entity);

/**
 * @brief Returns an entity hitbox transformed into world space.
 * @param entity Entity to inspect.
 * @return ShapeResult containing the world-space hitbox, or an error.
 */
ShapeResult rohr_physics_global_hit_box_get(Entity entity);

/**
 * @brief Sets an entity restitution value.
 * @param entity Entity to modify.
 * @param restitution Restitution value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_restitution_set(Entity entity, Restitution restitution);

/**
 * @brief Marks an entity as dynamic for physics simulation.
 * @param entity Entity to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_dynamic_set(Entity entity);

/**
 * @brief Marks an entity as static for physics simulation.
 * @param entity Entity to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_static_set(Entity entity);

/**
 * @brief Adds ROHR_HOLD so physics update stages preserve current values.
 * @param entity Entity to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_entity_hold(Entity entity);

/**
 * @brief Removes ROHR_HOLD without changing ROHR_STATIC or ROHR_DYNAMIC state.
 * @param entity Entity to modify.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_entity_unhold(Entity entity);

/**
 * @brief Adds ROHR_HOLD to every live entity in a group.
 * @param group Group id to update.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_entities_hold(GroupId group);

/**
 * @brief Removes ROHR_HOLD from every live entity in a group.
 * @param group Group id to update.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_group_entities_unhold(GroupId group);

/**
 * @brief Locks an entity orientation between minimum and maximum angles.
 * @param entity Entity to modify.
 * @param min Minimum orientation in radians.
 * @param max Maximum orientation in radians.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_angle_lock_set(Entity entity, Orientation min, Orientation max);

/**
 * @brief Locks an entity position along an axis.
 * @param entity Entity to modify.
 * @param axis Axis to lock against.
 * @param axis_point Point on the locked axis.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_axis_lock_set(Entity entity, Axis axis, Position axis_point);

/**
 * @brief Sets an entity friction value.
 * @param entity Entity to modify.
 * @param friction Friction value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_friction_set(Entity entity, float friction);

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
EngineResult rohr_physics_transform_lock_set(
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
EngineResult rohr_physics_transform_lock_remove(Entity entity);

/**
 * @brief Locks one entity to another using their current transform offset.
 * @param driven Entity whose transform is driven.
 * @param driver Entity used as the transform source.
 * @param lock_position true to lock position.
 * @param lock_orientation true to lock orientation.
 * @param inherit_velocity true to inherit driver velocity.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_transform_lock_current_transform_set(
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
EngineResult rohr_physics_target_set(Entity entity, Entity target);

/**
 * @brief Adds or replaces complete joint data on an existing entity.
 * @param entity Entity that owns the joint component.
 * @param joint Complete joint component value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_joint_component_set(Entity entity, Joint joint);

/**
 * @brief Creates an anchor owned by an entity.
 * @param entity Entity that owns and moves the anchor.
 * @param local_offset Anchor offset from the entity's stable local origin.
 * @return JointAnchorIdResult containing the stable anchor handle, or an error.
 */
JointAnchorIdResult rohr_physics_joint_anchor_create(Entity entity, Vec2D local_offset);
/**
 * @brief Returns the anchors owned by an entity.
 * @param entity Entity whose anchors should be listed.
 * @return JointAnchorListResult containing the anchor handles, or an error.
 */
JointAnchorListResult rohr_physics_joint_anchors_get(Entity entity);
/**
 * @brief Returns an anchor offset relative to its owner's stable local origin.
 * @param anchor Anchor to inspect.
 * @return JointAnchorPositionResult containing the origin-relative local offset, or an error.
 */
JointAnchorPositionResult rohr_physics_joint_anchor_local_position_get(JointAnchorId anchor);
/**
 * @brief Returns the current world position of an anchor.
 * @param anchor Anchor to resolve.
 * @return JointAnchorPositionResult containing its world position, or an error.
 */
JointAnchorPositionResult rohr_physics_joint_anchor_world_position_get(JointAnchorId anchor);
/**
 * @brief Sets an anchor offset relative to its owner's stable local origin.
 * @param anchor Anchor to modify.
 * @param local_offset New origin-relative local offset.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_joint_anchor_local_position_set(JointAnchorId anchor, Vec2D local_offset);
/**
 * @brief Removes an anchor and its connected joint entities.
 * @param anchor Anchor to remove.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_joint_anchor_remove(JointAnchorId anchor);
/**
 * @brief Configures a joint entity as a rigid pin between two anchors.
 * @param joint Entity that owns the joint component.
 * @param anchor_a First anchor.
 * @param anchor_b Second anchor.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_joint_pin_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b);
/**
 * @brief Configures a joint entity as a rigid weld between two anchors.
 * @param joint Entity that owns the joint component.
 * @param anchor_a First anchor.
 * @param anchor_b Second anchor.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_joint_weld_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b);
/**
 * @brief Configures a joint entity as a damped spring between two anchors.
 * @param joint Entity that owns the joint component.
 * @param anchor_a First anchor.
 * @param anchor_b Second anchor.
 * @param rest_length Unstretched anchor distance.
 * @param stiffness Spring stiffness.
 * @param damping Relative-motion damping.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_joint_spring_set(
    Entity joint,
    JointAnchorId anchor_a,
    JointAnchorId anchor_b,
    float rest_length,
    float stiffness,
    float damping
);

/** @brief Creates an empty soft-body owner entity. @return EntityResult containing the owner. */
EntityResult rohr_physics_soft_body_create(void);
/** @brief Returns soft-body topology. @param soft_body Soft-body owner. @return SoftBodyResult. */
SoftBodyResult rohr_physics_soft_body_get(Entity soft_body);
/**
 * @brief Creates a lightweight point-mass node.
 * @param soft_body Owning soft body.
 * @param position Initial world position.
 * @param mass Node mass.
 * @param radius Collision radius.
 * @return EntityResult containing the node entity.
 */
EntityResult rohr_physics_soft_body_node_create(Entity soft_body, Position position, Mass mass_value, float radius);
/** @brief Returns soft-body node data. @param node Node entity. @return SoftBodyNodeResult. */
SoftBodyNodeResult rohr_physics_soft_body_node_get(Entity node);
/**
 * @brief Sets node-versus-rigid collision filtering.
 * @param node Node entity.
 * @param category Categories represented by the node.
 * @param collides_with Rigid collider categories accepted by the node.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_soft_body_node_collision_filter_set(
    Entity node,
    RohrCollisionCategoryMask category,
    RohrCollisionCategoryMask collides_with
);
/**
 * @brief Applies a force to one soft-body node for the next physics tick.
 * @param node Soft-body node entity.
 * @param force Force to apply.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_soft_body_node_force_for_one_tick_apply(Entity node, Force force);
/**
 * @brief Applies an immediate impulse to one soft-body node.
 * @param node Soft-body node entity.
 * @param impulse Impulse to apply.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_soft_body_node_impulse_apply(Entity node, Vec2D impulse);
/**
 * @brief Distributes a total force across a soft body for the next physics tick.
 * @param soft_body Soft-body owner entity.
 * @param force Total force to distribute by node mass.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_soft_body_force_for_one_tick_apply(Entity soft_body, Force force);
/**
 * @brief Applies body-level torque as balanced node forces for the next physics tick.
 * @param soft_body Soft-body owner entity.
 * @param torque Total torque to distribute around the center of mass.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_physics_soft_body_torque_for_one_tick_apply(Entity soft_body, Torque torque);
/**
 * @brief Pins a soft-body node to an existing anchor.
 * @param node Soft-body node entity.
 * @param anchor Existing anchor to connect to the node.
 * @return SoftBodyNodeAnchorPinResult containing the joint and node-owned anchor.
 * Remove node_anchor to remove both the attachment joint and the created anchor.
 */
SoftBodyNodeAnchorPinResult rohr_physics_soft_body_node_to_anchor_pin_create(
    Entity node,
    JointAnchorId anchor
);
/**
 * @brief Creates an elastic beam between two nodes.
 * @param soft_body Owning soft body.
 * @param node_a First node.
 * @param node_b Second node.
 * @param stiffness Spring stiffness.
 * @param damping Relative velocity damping.
 * @return EntityResult containing the beam entity.
 */
EntityResult rohr_physics_soft_body_beam_create(
    Entity soft_body, Entity node_a, Entity node_b, float stiffness, float damping
);
/** @brief Returns soft-body beam data. @param beam Beam entity. @return SoftBodyBeamResult. */
SoftBodyBeamResult rohr_physics_soft_body_beam_get(Entity beam);
/**
 * @brief Creates a deforming triangular surface from three nodes.
 * @param soft_body Owning soft body.
 * @param node_a First node.
 * @param node_b Second node.
 * @param node_c Third node.
 * @return EntityResult containing the triangle entity.
 */
EntityResult rohr_physics_soft_body_triangle_create(
    Entity soft_body, Entity node_a, Entity node_b, Entity node_c
);
/** @brief Returns soft-body triangle data. @param triangle Triangle entity. @return SoftBodyTriangleResult. */
SoftBodyTriangleResult rohr_physics_soft_body_triangle_get(Entity triangle);

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
EntityResult rohr_physics_joint_create(
    Entity a,
    Entity b,
    JointType type,
    Vec2D local_anchor_a,
    Vec2D local_anchor_b,
    float stiffness,
    float damping
);

/**
 * @brief Gets overlap information for two particle shapes.
 * @param shape_1 First shape.
 * @param shape_2 Second shape.
 * @return Geometric overlap information.
 */
OverlapInfo rohr_physics_particle_overlap_get(Shape shape_1, Shape shape_2);

/** Return whether two entities overlap during the current physics step. */
bool rohr_physics_overlap_check(Entity entity, Entity target);
/** Return current overlap geometry in the requested entity order. */
OverlapInfo rohr_physics_overlap_get(Entity entity, Entity target);
/** Return whether an overlap began during the current physics step. */
bool rohr_physics_overlap_entered_check(Entity entity, Entity target);
/** Return whether an overlap continued from the previous physics step. */
bool rohr_physics_overlap_stayed_check(Entity entity, Entity target);
/** Return whether an overlap ended during the current physics step. */
bool rohr_physics_overlap_exited_check(Entity entity, Entity target);
/** Return the number of current overlaps involving an entity. */
size_t rohr_physics_overlap_count_get(Entity entity);
/** Write up to capacity current overlaps and return the number written. */
size_t rohr_physics_overlaps_get(
    Entity entity,
    EntityInteraction *results,
    size_t capacity
);

/** Return whether two entities physically contacted during the current physics step. */
bool rohr_physics_contact_check(Entity entity, Entity target);
/** Return current contact geometry in the requested entity order. */
ContactInfo rohr_physics_contact_get(Entity entity, Entity target);
/** Return the sum of a contact's normal and friction impulses. */
Vec2D rohr_physics_contact_total_impulse_get(ContactInfo contact);
/** Return whether a physical contact began during the current physics step. */
bool rohr_physics_contact_entered_check(Entity entity, Entity target);
/** Return whether a physical contact continued from the previous physics step. */
bool rohr_physics_contact_stayed_check(Entity entity, Entity target);
/** Return whether a physical contact ended during the current physics step. */
bool rohr_physics_contact_exited_check(Entity entity, Entity target);
/** Return the number of current physical contacts involving an entity. */
size_t rohr_physics_contact_count_get(Entity entity);
/** Write up to capacity current contacts and return the number written. */
size_t rohr_physics_contacts_get(
    Entity entity,
    EntityContact *results,
    size_t capacity
);

/**
 * @brief Creates an engine color from a hexadecimal RRGGBBAA value.
 * @param hex_color_code RRGGBBAA hex color value.
 * @return Color created from hex_color_code.
 */
Color rohr_graphics_color_hex_create(uint32_t hex_color_code);

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
bool rohr_graphics_events_poll(SDL_Event *event);

/**
 * @brief Draws the frame background.
 * @param color Background color.
 */
void rohr_graphics_background_draw(Color color);

/** Sets the ordering layer captured by subsequent draw commands. */
void rohr_graphics_layer_set(int layer);

/** Returns the ordering layer used by subsequent draw commands. */
int rohr_graphics_layer_get(void);

/**
 * @brief Draws a filled rectangle in logical screen coordinates.
 * @return true when the draw command was queued.
 */
bool rohr_graphics_screen_rect_draw(float x, float y, float width, float height, Color color);
/** Returns the renderer output size in physical pixels. */
Scale rohr_graphics_render_output_size_get(void);
/** Changes the logical screen size while preserving aspect-correct presentation. */
bool rohr_graphics_logical_size_set(int width, int height);
/** Changes the logical aspect ratio while preserving the logical height. */
bool rohr_graphics_aspect_ratio_set(int width, int height);
/** Matches the logical aspect ratio to the render output when enabled. */
bool rohr_graphics_aspect_ratio_auto_set(bool enabled);
GraphicsWindowPresentationConfig rohr_graphics_window_presentation_default_get(void);
/** Returns the active window mode and actual physical/logical dimensions. */
GraphicsWindowPresentationConfig rohr_graphics_window_presentation_get(void);
EngineResult rohr_graphics_window_presentation_set(
    GraphicsWindowPresentationConfig config);
/** Clips subsequent screen-space drawing until the clip is cleared. */
bool rohr_graphics_screen_clip_set(float x, float y, float width, float height);
/** Clears the active screen-space drawing clip. */
void rohr_graphics_screen_clip_clear(void);

/** @brief Draws a centered rotated rectangle in logical screen space. */
bool rohr_graphics_screen_quad_draw(Position center, float width, float height, float angle, Color color);

/**
 * @brief Presents the current graphics frame.
 */
void rohr_graphics_show(void);
/** Enables or disables synchronization with the display refresh rate. */
EngineResult rohr_graphics_vsync_set(bool enabled);
/** Sets the non-VSync frame limit; zero uses the 120 FPS fallback. */
EngineResult rohr_graphics_frame_limit_set(int frames_per_second);

/**
 * @brief Draws one entity hitbox.
 * @param entity Entity whose hitbox should be drawn.
 * @param fill_type Fill mode for drawing.
 */
void rohr_graphics_hit_box_draw(Entity entity, Fill fill_type);

/**
 * @brief Draws one entity hitbox with a caller supplied color.
 * @param entity Entity whose hitbox should be drawn.
 * @param fill_type Fill mode for drawing.
 * @param color Color to draw with.
 */
void rohr_graphics_hit_box_colored_draw(Entity entity, Fill fill_type, Color color);

/**
 * @brief Draws hitboxes for all renderable hitbox entities.
 */
void rohr_graphics_hit_boxes_draw(void);

/**
 * @brief Draws one joint using an engineering-style debug symbol.
 * @param joint Joint entity to draw.
 * @param color Symbol color.
 * @return true when the symbol's draw commands were queued.
 */
bool rohr_graphics_joint_draw(Entity joint, Color color);

/**
 * @brief Draws all live joints using engineering-style debug symbols.
 * @param color Symbol color.
 */
void rohr_graphics_joints_draw(Color color);

/**
 * @brief Draws a soft body's current surfaces, beams, and collision nodes.
 * @param soft_body Soft-body owner entity.
 * @param surface Triangle surface color.
 * @param beam Beam color.
 * @param node Collision-node color.
 * @return true when the soft body's draw commands were queued.
 */
bool rohr_graphics_soft_body_draw(Entity soft_body, Color surface, Color beam, Color node);

/** Sets a drawing-color override for one node belonging to a soft body. */
EngineResult rohr_graphics_soft_body_node_color_set(
    Entity soft_body, Entity node, Color color);
/** Sets a drawing-color override for the beam connecting two soft-body nodes. */
EngineResult rohr_graphics_soft_body_beam_color_set(
    Entity soft_body, Entity node_a, Entity node_b, Color color);
/** Sets a drawing-color override for the area formed by three soft-body nodes. */
EngineResult rohr_graphics_soft_body_area_color_set(
    Entity soft_body, Entity node_a, Entity node_b, Entity node_c, Color color);

/**
 * @brief Loads a texture asset.
 * @param text_desc Texture descriptor containing load settings.
 * @return TextureAssetResult containing the asset, or an error.
 */
TextureAssetResult rohr_graphics_texture_load(TextureDescriptor text_desc);

/** @brief Loads a caller-owned font asset. */
FontAssetResult rohr_graphics_font_load(FontDescriptor descriptor);

/** @brief Destroys a font after its text assets have been destroyed. */
void rohr_graphics_font_destroy(FontAsset *font);

/** @brief Creates reusable caller-owned text. */
TextAssetResult rohr_graphics_text_create(const FontAsset *font, const char *value, Color color);
bool rohr_graphics_text_value_set(TextAsset *text, const char *value);

/** @brief Destroys reusable text. */
void rohr_graphics_text_destroy(TextAsset *text);

/** @brief Draws text in logical screen coordinates. */
bool rohr_graphics_text_draw(const TextAsset *text, Position position);

/**
 * @brief Loads an animation asset.
 * @param anim_desc Animation descriptor containing load settings.
 * @return AnimationAssetResult containing the asset, or an error.
 */
AnimationAssetResult rohr_graphics_animation_load(AnimationDescriptor anim_desc);

/**
 * @brief Creates an animated sprite from an animation asset.
 * @param asset_ptr Animation asset to use.
 * @param scale Sprite scale.
 * @return Animated sprite value.
 */
AnimatedSprite rohr_graphics_animated_sprite_create(AnimationAsset asset_ptr, Scale scale);

/**
 * @brief Adds an animated sprite to an entity.
 * @param entity Entity to modify.
 * @param sprite Animated sprite component value.
 * @return EngineResult describing success or failure.
 */
EngineResult rohr_graphics_animated_sprite_add(Entity entity, AnimatedSprite sprite);

/**
 * @brief Draws all animated sprite components.
 */
void rohr_graphics_animated_sprites_draw(void);

/**
 * @brief Updates animated sprite frames.
 * @param current_tick Current engine tick.
 * @param current_time Current engine time.
 */
void rohr_graphics_sprite_frames_update(Tick current_tick, Time current_time);

/**
 * @brief Scales textures attached to an entity.
 * @param entity Entity to modify.
 * @param scale Scale value.
 */
void rohr_graphics_textures_scale(Entity entity, Scale scale);

/**
 * @brief Translates the active camera in world space.
 * @param translation World-space translation to add.
 */
void rohr_graphics_camera_move(Vec2D translation);

/**
 * @brief Rotates the active camera counterclockwise.
 * @param radians Rotation in radians to add.
 */
void rohr_graphics_camera_rotate(Orientation radians);

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
EngineResult rohr_graphics_camera_attach(
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
EngineResult rohr_graphics_camera_with_options_attach(
    Entity entity,
    Vec2D position_offset,
    Orientation orientation_offset,
    bool follow_position,
    bool follow_orientation
);

/**
 * @brief Detaches the camera and preserves its current world transform.
 */
void rohr_graphics_camera_detach(void);

/**
 * @brief Reports whether the camera is attached to a live entity transform.
 * @return true when attached to a valid entity transform.
 */
bool rohr_graphics_camera_attached_get(void);

/**
 * @brief Returns the active camera attachment description.
 * @return CameraAttachmentResult containing the attachment or a missing-component error.
 */
CameraAttachmentResult rohr_graphics_camera_attachment_get(void);

/** Returns defaults for a full-screen camera with unit zoom. */
CameraConfig rohr_camera_config_default_get(void);
/** Creates an engine-owned camera. */
CameraIdResult rohr_camera_create(CameraConfig config);
/** Destroys a non-active camera. */
EngineResult rohr_camera_destroy(CameraId camera);
/** Selects the camera used by transforms and subsequent drawing. */
EngineResult rohr_camera_active_set(CameraId camera);
/** Returns the active camera handle. */
CameraId rohr_camera_active_get(void);
/** Returns an engine-owned camera value. */
CameraResult rohr_camera_get(CameraId camera);
/** Replaces a camera value and detaches it. */
EngineResult rohr_camera_set(CameraId camera, Camera value);
/** Attaches a camera to an entity transform. */
EngineResult rohr_camera_attach(
    CameraId camera,
    Entity entity,
    Vec2D position_offset,
    Orientation orientation_offset,
    bool follow_position,
    bool follow_orientation
);
/** Detaches a camera while preserving its resolved transform. */
EngineResult rohr_camera_detach(CameraId camera);

EngineResult rohr_camera_render_callback_set(
    CameraId camera,
    CameraRenderCallback callback,
    void *context
);
EngineResult rohr_camera_enable_set(CameraId camera);
EngineResult rohr_camera_disable_set(CameraId camera);
EngineResult rohr_camera_pause_with_engine_set(CameraId camera);
EngineResult rohr_camera_render_when_paused_set(CameraId camera);
EngineResult rohr_camera_position_move(CameraId camera, Vec2D translation, Time duration);
EngineResult rohr_camera_position_set(CameraId camera, Position position, Time duration);
EngineResult rohr_camera_position_from_entity_set(CameraId camera, Entity entity, Time duration);
EngineResult rohr_camera_entity_attachment_set(CameraId camera, Entity entity);
EngineResult rohr_camera_moving_get(CameraId camera);
EngineResult rohr_camera_zoom_set(CameraId camera, float zoom, Time duration);
CameraZoomResult rohr_camera_zoom_get(CameraId camera);

ViewportConfig rohr_viewport_config_default_get(void);
ViewportIdResult rohr_viewport_create(ViewportConfig config);
EngineResult rohr_viewport_destroy(ViewportId viewport);
EngineResult rohr_viewport_camera_set(ViewportId viewport, CameraId camera);
EngineResult rohr_viewport_camera_clear(ViewportId viewport);
EngineResult rohr_viewport_enable_set(ViewportId viewport);
EngineResult rohr_viewport_disable_set(ViewportId viewport);

/**
 * @brief Converts a world position to screen coordinates.
 * @param pos World position.
 * @return Screen-space position.
 */
Position rohr_graphics_world_to_screen_get(Position pos);

/**
 * @brief Converts a screen position to world coordinates.
 * @param screen Screen-space position.
 * @return World-space position.
 */
Position rohr_graphics_screen_to_world_get(Position screen);

/**
 * @brief Returns the current mouse position in screen coordinates.
 * @return Mouse screen position.
 */
Position rohr_graphics_mouse_screen_position_get(void);

/** Enable or disable physics AABB-tree debug drawing. */
void rohr_graphics_aabb_tree_debug_set(bool enabled);
/** Return whether physics AABB-tree debug drawing is enabled. */
bool rohr_graphics_aabb_tree_debug_check(void);
/** Draw the current physics AABB-tree bounds when debug drawing is enabled. */
void rohr_graphics_aabb_tree_draw(void);

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
void rohr_graphics_particles_draw(void);

/**
 * @brief Draws local origin markers for entities.
 */
void rohr_graphics_local_origins_draw(void);

/**
 * @brief Creates normalized edge normals for a shape.
 * @param shape Shape to inspect.
 * @return List of normal vectors.
 */
Vec2DList rohr_math_normals_create(Shape shape);

/**
 * @brief Normalizes a vector.
 * @param vector Vector to normalize.
 * @return Normalized vector.
 */
Vec2D rohr_math_vector_normalize(Vec2D vector);

/**
 * @brief Normalizes all vectors in a list.
 * @param vectors Vector list to normalize.
 * @return Normalized vector list.
 */
Vec2DList rohr_math_vectors_normalize(Vec2DList vectors);

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
Shape rohr_math_square_create(float width, float height);

/**
 * @brief Creates a circle approximation shape.
 * @param radius Circle radius.
 * @param verticies Number of vertices used to approximate the circle.
 * @return Shape containing circle vertices.
 */
Shape rohr_math_circle_create(float radius, uint8_t verticies);

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
Vec2D rohr_math_vector_rotate(Vec2D vector, float angle);

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
Shape rohr_math_shape_scale(Shape shape, float scale);

/**
 * @brief Scales a shape along the y axis.
 * @param shape Shape to scale.
 * @param scale Y-axis scale value.
 * @return Scaled shape.
 */
Shape rohr_math_shape_y_scale(Shape shape, float scale);

/**
 * @brief Scales a shape along the x axis.
 * @param shape Shape to scale.
 * @param scale X-axis scale value.
 * @return Scaled shape.
 */
Shape rohr_math_shape_x_scale(Shape shape, float scale);

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
Shape rohr_math_vertex_add(Shape shape);

/**
 * @brief Deletes the last vertex slot from a shape.
 * @param shape Shape to modify.
 * @return Shape with one fewer vertex slot.
 */
Shape rohr_math_vertex_delete(Shape shape);

/**
 * @brief Creates an axis-aligned bounding box for a world-space shape.
 * @param world_shape World-space shape.
 * @return Axis-aligned bounding box.
 */
AABB rohr_math_aabb_create(Shape world_shape);

/**
 * @brief Runs one physics-system update.
 * @param dt Simulation delta time in seconds.
 */
void rohr_system_physics_update(double dt);

/**
 * @brief Advances engine time and clears expired entities.
 * @return Number of complete fixed ticks consumed by this update.
 */
Tick rohr_system_tick_update(void);

/**
 * @brief Deletes entities whose lifetime has expired.
 */
void rohr_system_entities_past_lifetime_clean(void);

/**
 * @brief Updates keyboard key states for the frame.
 * @param keyboard Keyboard state table to update.
 */
void rohr_controller_key_states_update(KeyboardState *keyboard);

/**
 * @brief Adds a keyboard event to a keyboard state table.
 * @param keyboard Keyboard state table to modify.
 * @param key_event Keyboard event to add.
 */
void rohr_controller_key_event_add(KeyboardState *keyboard, KeyboardEvent key_event);

/**
 * @brief Converts an SDL event into a Rohr keyboard event.
 * @param sdl_event SDL event to inspect.
 * @return KeyboardEvent derived from sdl_event.
 */
KeyboardEvent rohr_controller_keyboard_event_capture(const SDL_Event *sdl_event);

/**
 * @brief Checks whether an SDL keycode is currently held or was pressed this frame.
 * @param keyboard Keyboard state table to inspect.
 * @param keycode SDL keycode to check.
 * @return true when the key is down or pressed.
 */
bool rohr_controller_key_down_get(const KeyboardState *keyboard, SDL_Keycode keycode);

/**
 * @brief Checks whether an SDL keycode was pressed this frame.
 * @param keyboard Keyboard state table to inspect.
 * @param keycode SDL keycode to check.
 * @return true when the key was pressed this frame.
 */
bool rohr_controller_key_pressed_get(const KeyboardState *keyboard, SDL_Keycode keycode);

/**
 * @brief Checks whether an SDL keycode was released this frame.
 * @param keyboard Keyboard state table to inspect.
 * @param keycode SDL keycode to check.
 * @return true when the key was released this frame.
 */
bool rohr_controller_key_released_get(const KeyboardState *keyboard, SDL_Keycode keycode);

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
Vec2D rohr_controller_axis_from_keycodes_get(
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
Vec2D rohr_controller_wasd_axis_get(const KeyboardState *keyboard);

/**
 * @brief Returns normalized movement input from arrow keys.
 * @param keyboard Keyboard state table to inspect.
 * @return Direction vector where up is positive Y and right is positive X.
 */
Vec2D rohr_controller_arrow_axis_get(const KeyboardState *keyboard);

/**
 * @brief Returns an enabled, empty, game-owned controller.
 * @return Controller ready for named axes and buttons.
 */
Controller rohr_controller_default_get(void);

/**
 * @brief Returns a game-owned controller with W/A/S/D axis bindings.
 * @return Default enabled W/A/S/D controller.
 */
Controller rohr_controller_wasd_default_get(void);

/**
 * @brief Returns a game-owned controller with arrow-key axis bindings.
 * @return Default enabled arrow-key controller.
 */
Controller rohr_controller_arrows_default_get(void);

/**
 * @brief Replaces the axis mapping on a caller-owned controller.
 * @param controller Controller to modify. NULL is ignored.
 * @param binding New positive/negative X/Y key mapping.
 */
void rohr_controller_axis_binding_set(
    Controller *controller,
    ControllerAxisBinding binding
);

/**
 * @brief Reads a game-owned controller from shared keyboard state.
 * @param keyboard Shared keyboard state captured for the frame.
 * @param controller Game-owned mapping to read.
 * @return Normalized axis, or zero for NULL or disabled controllers.
 */
Vec2D rohr_controller_default_axis_get(
    const KeyboardState *keyboard,
    const Controller *controller
);

/** @brief Adds or replaces a named axis without allocating memory. */
bool rohr_controller_axis_add(
    Controller *controller,
    const char *name,
    ControllerAxisBinding binding
);

/** @brief Adds or replaces a named button without allocating memory. */
bool rohr_controller_button_add(
    Controller *controller,
    const char *name,
    SDL_Keycode keycode
);

/** @brief Reads a named axis, returning zero when unavailable or disabled. */
Vec2D rohr_controller_axis_get(
    const KeyboardState *keyboard,
    const Controller *controller,
    const char *name
);

/** @brief Checks whether a named button is held or newly pressed. */
bool rohr_controller_button_down_get(
    const KeyboardState *keyboard,
    const Controller *controller,
    const char *name
);

/** @brief Checks whether a named button was pressed this frame. */
bool rohr_controller_button_pressed_get(
    const KeyboardState *keyboard,
    const Controller *controller,
    const char *name
);

/** @brief Checks whether a named button was released this frame. */
bool rohr_controller_button_released_get(
    const KeyboardState *keyboard,
    const Controller *controller,
    const char *name
);

/**
 * @brief Prints a mouse event for debugging.
 * @param event Mouse event to print.
 */
void rohr_controller_mouse_event_print(MouseEvent event);

/**
 * @brief Updates mouse button states for the frame.
 * @param mouse Mouse state table to update.
 */
void rohr_controller_mouse_states_update(MouseState *mouse);

/**
 * @brief Adds a mouse event to a mouse state table.
 * @param mouse Mouse state table to modify.
 * @param mouse_event Mouse event to add.
 */
void rohr_controller_mouse_event_add(MouseState *mouse, MouseEvent mouse_event);

/**
 * @brief Converts an SDL event into a Rohr mouse event.
 * @param sdl_event SDL event to inspect.
 * @return MouseEvent derived from sdl_event.
 */
MouseEvent rohr_controller_mouse_event_capture(const SDL_Event *sdl_event);

/**
 * @brief Converts the current logical screen-space mouse position to world space.
 * @param mouse Mouse state to convert.
 * @return World position under the mouse, or zero when mouse is NULL.
 */
Position rohr_controller_mouse_world_position_get(const MouseState *mouse);

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
void rohr_ui_frame_begin(UIInput input);
void rohr_ui_modal_set(UIRect bounds);
void rohr_ui_modal_controls_begin(void);
void rohr_ui_modal_controls_end(void);
/** @brief Applies reusable sizing components to UI bounds. */
UIRect rohr_ui_component_bounds_get(UIRect bounds, const TextAsset *const *texts,
    size_t text_count, UIComponentConfig config);
/** Queues keyboard and pointer events used by UI controls. */
void rohr_ui_event_add(const SDL_Event *event);
/** Queues a keyboard event for focused UI fields. */
void rohr_ui_field_event_add(const SDL_Event *event);
/** Clears the currently focused UI text or number field. */
void rohr_ui_field_focus_clear(void);
/** Draws and edits a caller-owned string or float field. */
UIFieldResult rohr_ui_field(const char *id, UIFieldBinding binding,
    TextAsset *display, UIRect bounds, const UIButtonStyle *style);
UIFieldResult rohr_ui_multiline_field(const char *id, UIFieldBinding binding,
    TextAsset *display, UIRect bounds, const UIButtonStyle *style);

/**
 * @brief Draws and updates one button identified by a stable string.
 * @param label Optional centered label; NULL or empty draws no label.
 */
UIButtonResult rohr_ui_button(
    const char *id,
    const TextAsset *label,
    UIRect bounds,
    const UIButtonStyle *style
);

/** @brief Draws a dropdown and returns selection and hover-preview state. */
UIDropdownResult rohr_ui_dropdown(const char *id, const TextAsset *const *options,
    size_t option_count, size_t selected_index, UIRect bounds,
    const UIButtonStyle *style);
/** @brief Draws a menu button whose label is not repeated in its action list. */
UIDropdownResult rohr_ui_menu(const char *id, const TextAsset *label,
    const TextAsset *const *options, size_t option_count, UIRect bounds,
    const UIButtonStyle *style);
/** @brief Begins a clipped vertical scroll region for subsequent UI controls. */
UIScrollRegionResult rohr_ui_scroll_region_begin(const char *id, UIRect bounds,
    float content_height, float offset, float wheel_step);
/** @brief Ends the current UI scroll region. */
void rohr_ui_scroll_region_end(void);
/** @brief Updates pointer interaction without prescribing visuals. */
UIButtonResult rohr_ui_interaction(const char *id, UIRect bounds);
/** @brief Draws a filled rectangular UI primitive. */
void rohr_ui_surface(UIRect bounds, Color color);
/** @brief Draws a rectangular UI border primitive. */
void rohr_ui_border(UIRect bounds, float thickness, Color color);
/** @brief Draws centered clipped text content. */
void rohr_ui_content(const TextAsset *text, UIRect bounds);
/** @brief Draws an oriented UI rectangle primitive. */
void rohr_ui_quad(Position center, float width, float height, float angle, Color color);
/** @brief Begins and ends a clipped UI component region. */
bool rohr_ui_clip_begin(UIRect bounds);
void rohr_ui_clip_end(void);
/** @brief Moves UI keyboard focus in a screen-space direction. */
bool rohr_ui_navigation_move(UINavigationDirection direction);
/** @brief Activates the currently focused UI control. */
bool rohr_ui_navigation_activate(void);
/** @brief Returns the focused control's previous-frame screen bounds. */
bool rohr_ui_navigation_focus_bounds_get(UIRect *bounds);

/** @brief Draws reusable text centered inside bounds. */
void rohr_ui_label(const TextAsset *text, UIRect bounds);
EngineResult rohr_ui_physics_debug_panel_init(UIPhysicsDebugPanel *panel, FontDescriptor font);
void rohr_ui_physics_debug_panel_draw(UIPhysicsDebugPanel *panel);
void rohr_ui_physics_debug_panel_destroy(UIPhysicsDebugPanel *panel);
PhysicsDebugStats rohr_physics_debug_stats_get(void);
void rohr_physics_debug_stats_enabled_set(bool enabled);

/** @brief Draws a disabled button that cannot capture input. */
void rohr_ui_button_disabled(UIRect bounds, const UIButtonStyle *style);

/** @brief Returns whether UI consumed pointer input during this frame. */
bool rohr_ui_pointer_consumed_get(void);

/** @brief Finishes the current UI frame. */
void rohr_ui_frame_end(void);

/** @brief Returns the default button colors. */
UIButtonStyle rohr_ui_button_style_default_get(void);

/** @brief Returns the default slider configuration and 0..1 range. */
UISliderConfig rohr_ui_slider_config_default_get(void);

/** @brief Draws and updates a caller-owned slider value. */
UISliderResult rohr_ui_slider(const char *id, float value, const UISliderConfig *config);

/** @brief Draws a slider with optional caller-owned label and value text. */
UISliderResult rohr_ui_slider_with_text(const char *id, float value,
    const UISliderConfig *config, const UISliderText *text);

#endif
