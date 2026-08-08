# Engine API Reference

This page is the GitHub-readable reference for the public Rohr Engine core C API.
The source of truth is [`include/rohr.h`](../include/rohr.h), which uses Doxygen comments for the generated HTML documentation.

Application code should include the public facade:

```c
#include "rohr.h"
```

Entity values are stable ids, not component table indexes. Use the public entity functions to validate ids and resolve indexes.

## Contents

- <a href="#engine">Engine</a>
- <a href="#errors-and-results">Errors and Results</a>
- <a href="#console">Console</a>
- <a href="#entities">Entities</a>
- <a href="#physics">Physics</a>
- <a href="#graphics">Graphics</a>
- <a href="#math">Math</a>
- <a href="#systems">Systems</a>
- <a href="#controller-input">Controller Input</a>
- <a href="#tools">Tools</a>
- <a href="#other">Other</a>

## Engine

### `rohr_engine_init`

```c
EngineResult rohr_engine_init(void);
```

Initializes core engine state.

**Returns:** EngineResult containing true on success, or an engine error.

### `rohr_engine_shutdown`

```c
void rohr_engine_shutdown(void);
```

Releases core engine state.

### `rohr_engine_time_update`

```c
void rohr_engine_time_update(void);
```

Updates accumulated engine time from the platform clock.

### `rohr_engine_time_get`

```c
Time rohr_engine_time_get(void);
```

Returns the current engine time in seconds.

**Returns:** Current engine time.

### `rohr_engine_tick_get`

```c
Tick rohr_engine_tick_get(void);
```

Returns the current engine tick counter.

**Returns:** Current tick.

### `rohr_engine_pause`

```c
void rohr_engine_pause(void);
```

Pauses engine time-dependent updates.

### `rohr_engine_resume`

```c
void rohr_engine_resume(void);
```

Resumes engine time-dependent updates.

### `rohr_engine_time_per_tick_set`

```c
EngineResult rohr_engine_time_per_tick_set(Time time_per_tick);
```

 Sets the real-time duration required for one engine tick.

### `rohr_engine_time_per_tick_get`

```c
Time rohr_engine_time_per_tick_get(void);
```

 Returns the real-time duration required for one engine tick.

### `rohr_engine_event_poll`

```c
SDL_Event rohr_engine_event_poll(void);
```

Polls one SDL event.

**Returns:** SDL event value returned by the engine event poller.

### `rohr_engine_paused_get`

```c
bool rohr_engine_paused_get(void);
```

Checks whether the engine is paused.

**Returns:** true when paused, false when running.

### `rohr_engine_clock_reset`

```c
void rohr_engine_clock_reset(void);
```

Resets the engine clock baseline.

## Errors and Results

### `rohr_error_result_value`

```c
EngineResult rohr_error_result_value(bool value);
```

Creates a successful boolean engine result.

| Parameter | Description |
| --- | --- |
| `value` | Boolean value to store in the result. |

**Returns:** EngineResult containing value.

### `rohr_error_result_error`

```c
EngineResult rohr_error_result_error(EngineError error);
```

Creates a failed engine result.

| Parameter | Description |
| --- | --- |
| `error` | Error code to store in the result. |

**Returns:** EngineResult containing error.

### `rohr_error_check`

```c
#define rohr_error_check(ResultValue) error_check(ResultValue)
```

Checks whether a result contains an error.

| Parameter | Description |
| --- | --- |
| `ResultValue` | Result value to inspect. |

**Returns:** true when ResultValue contains an error, false otherwise.

### `rohr_error_default_message_get`

```c
const char *rohr_error_default_message_get(EngineError error);
```

Returns a user-facing default message for an engine error.

| Parameter | Description |
| --- | --- |
| `error` | Error code to describe. |

**Returns:** Static string describing error.

### `rohr_error_string`

```c
const char *rohr_error_string(EngineError error);
```

Returns the symbolic name for an engine error.

| Parameter | Description |
| --- | --- |
| `error` | Error code to name. |

**Returns:** Static string containing the error name.

### `rohr_error_stderr_print`

```c
void rohr_error_stderr_print(EngineError error);
```

Prints an engine error message to stderr.

| Parameter | Description |
| --- | --- |
| `error` | Error code to print. |

## Console

### `rohr_console_logs_print`

```c
void rohr_console_logs_print(void);
```

Prints buffered console log messages.

### `rohr_console_init`

```c
void rohr_console_init(void);
```

Initializes the engine console.

### `rohr_console_shutdown`

```c
void rohr_console_shutdown(void);
```

Shuts down the engine console.

### `rohr_console_read`

```c
bool rohr_console_read(ConsoleLogString *input);
```

Reads one console log string.

| Parameter | Description |
| --- | --- |
| `input` | Destination for the log string. |

**Returns:** true when a log string was read, false otherwise.

### `rohr_console_write`

```c
void rohr_console_write(LogSourceType source, const char *fmt, ...);
```

Writes a formatted message to the engine console.

| Parameter | Description |
| --- | --- |
| `source` | Source category for the log entry. |
| `fmt` | printf-style format string. |

### `rohr_console_active_get`

```c
bool rohr_console_active_get(void);
```

Checks whether the console is active.

**Returns:** true when active, false otherwise.

### `rohr_console_debug_write`

```c
void rohr_console_debug_write(LogSourceType source, const char *fmt, ...);
```

Writes a formatted debug message when debug logging is enabled.

| Parameter | Description |
| --- | --- |
| `source` | Source category for the log entry. |
| `fmt` | printf-style format string. |

### `rohr_console_debug_set`

```c
void rohr_console_debug_set(bool state);
```

Enables or disables debug console output.

| Parameter | Description |
| --- | --- |
| `state` | true to enable debug logging, false to disable it. |

## Entities

### `rohr_entity_alive_check`

```c
bool rohr_entity_alive_check(Entity entity);
```

Checks whether an entity id currently refers to a live entity.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to inspect. |

**Returns:** true when the entity is alive, false otherwise.

### `rohr_entity_index_alive_check`

```c
bool rohr_entity_index_alive_check(EntityIndex index);
```

Checks whether an entity table index currently contains a live entity.

| Parameter | Description |
| --- | --- |
| `index` | Component table index to inspect. |

**Returns:** true when the index contains a live entity, false otherwise.

### `rohr_entity_alive_count_get`

```c
uint32_t rohr_entity_alive_count_get(void);
```

Returns the number of currently alive entities.

**Returns:** Number of alive entities.

### `rohr_entity_alive_at_get`

```c
EntityResult rohr_entity_alive_at_get(uint32_t position);
```

Returns the entity id stored at a dense alive-list position.

The position is not a component table index and can change when entities are

deleted.

| Parameter | Description |
| --- | --- |
| `position` | Dense alive-list position. |

**Returns:** EntityResult containing the entity id, or an error.

### `rohr_entity_index_get`

```c
EntityIndexResult rohr_entity_index_get(Entity entity);
```

Resolves a stable entity id to its current component table index.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to resolve. |

**Returns:** EntityIndexResult containing the index or an invalid-entity error.

### `rohr_entity_from_index_get`

```c
EntityResult rohr_entity_from_index_get(EntityIndex index);
```

Returns the stable entity id stored at a component table index.

| Parameter | Description |
| --- | --- |
| `index` | Component table index to inspect. |

**Returns:** EntityResult containing the entity id, or an error.

### `rohr_entity_add`

```c
EntityResult rohr_entity_add(void);
```

Creates a new entity.

Entity ids are stable handles and may not match component table indexes.

**Returns:** EntityResult containing the new entity id, or an error if the entity limit is reached.

### `rohr_entity_name_set`

```c
EngineResult rohr_entity_name_set(Entity entity, const char *name);
```

 Assigns a unique fixed-size name to an entity.

### `rohr_entity_by_name_get`

```c
EntityResult rohr_entity_by_name_get(const char *name);
```

 Finds a live entity by its state-file name.

### `rohr_entity_name_get`

```c
EntityNameResult rohr_entity_name_get(Entity entity);
```

 Returns a copy of an entity's fixed-size name component.

### `rohr_entity_delete`

```c
EngineResult rohr_entity_delete(Entity entity);
```

Deletes an entity and releases its slot for reuse.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to delete. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_components_add`

```c
EngineResult rohr_entity_components_add(Entity entity, RohrComponentMask mask);
```

Adds components to an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to modify. |
| `mask` | Component mask to add. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_components_check`

```c
bool rohr_entity_components_check(Entity entity, RohrComponentMask components);
```

Checks whether an entity has all requested components.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to inspect. |
| `components` | Component mask to test. |

**Returns:** true when entity has every requested component, false otherwise.

### `rohr_entity_index_components_check`

```c
bool rohr_entity_index_components_check(EntityIndex index, RohrComponentMask components);
```

Checks whether an entity table index has all requested components.

| Parameter | Description |
| --- | --- |
| `index` | Component table index to inspect. |
| `components` | Component mask to test. |

**Returns:** true when index has every requested component, false otherwise.

### `rohr_entity_group_create`

```c
GroupIdResult rohr_entity_group_create(void);
```

Creates a reusable entity group.

**Returns:** GroupIdResult containing a group id, or an error.

### `rohr_entity_group_name_set`

```c
EngineResult rohr_entity_group_name_set(GroupId group, const char *name);
```

 Assigns a unique fixed-size name to a generic group.

### `rohr_entity_group_by_name_get`

```c
GroupIdResult rohr_entity_group_by_name_get(const char *name);
```

 Finds a live generic group by name.

### `rohr_entity_group_name_get`

```c
GroupNameResult rohr_entity_group_name_get(GroupId group);
```

 Returns a copy of a generic group's fixed-size name.

### `rohr_entity_group_destroy`

```c
EngineResult rohr_entity_group_destroy(GroupId group);
```

Destroys a generic entity group and clears member group references.

| Parameter | Description |
| --- | --- |
| `group` | Group id to destroy. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_group_add`

```c
EngineResult rohr_entity_group_add(GroupId group, Entity entity);
```

Adds an entity to a generic group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to update. |
| `entity` | Entity id to add. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_group_remove`

```c
EngineResult rohr_entity_group_remove(GroupId group, Entity entity);
```

Removes an entity from a generic group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to update. |
| `entity` | Entity id to remove. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_group_entity_check`

```c
bool rohr_entity_group_entity_check(GroupId group, Entity entity);
```

Checks whether an entity belongs to a group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to inspect. |
| `entity` | Entity id to search for. |

**Returns:** true when entity belongs to the group.

### `rohr_entity_group_get`

```c
EntityGroupResult rohr_entity_group_get(GroupId group);
```

Returns an entity group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to inspect. |

**Returns:** EntityGroupResult containing group data, or an error.

### `rohr_entity_groups_get`

```c
EntityGroupMembershipResult rohr_entity_groups_get(Entity entity);
```

Returns the groups assigned to an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Entity id to inspect. |

**Returns:** EntityGroupMembershipResult containing group ids, or an error.

### `rohr_entity_components_delete`

```c
EngineResult rohr_entity_components_delete(Entity entity, RohrComponentMask mask);
```

Removes components from an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to modify. |
| `mask` | Component mask to remove. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_child_set`

```c
EngineResult rohr_entity_child_set(Entity parent, Entity child);
```

Adds a child relationship from parent to child.

| Parameter | Description |
| --- | --- |
| `parent` | Parent entity id. |
| `child` | Child entity id. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_parent_set`

```c
EngineResult rohr_entity_parent_set(Entity child, Entity parent);
```

Sets an entity parent relationship.

| Parameter | Description |
| --- | --- |
| `child` | Child entity id. |
| `parent` | Parent entity id. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_parent_remove`

```c
EngineResult rohr_entity_parent_remove(Entity child);
```

Removes the parent relationship from an entity.

| Parameter | Description |
| --- | --- |
| `child` | Child entity id. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_child_remove`

```c
EngineResult rohr_entity_child_remove(Entity parent, Entity child);
```

Removes a child relationship from a parent entity.

| Parameter | Description |
| --- | --- |
| `parent` | Parent entity id. |
| `child` | Child entity id. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_children_get`

```c
ChildrenResult rohr_entity_children_get(Entity entity);
```

Returns the children group assigned to an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to inspect. |

**Returns:** ChildrenResult containing a group id, or an error.

### `rohr_entity_parent_get`

```c
ParentResult rohr_entity_parent_get(Entity entity);
```

Returns the parent assigned to an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to inspect. |

**Returns:** ParentResult containing parent id, or an error.

### `rohr_entity_life_time_set`

```c
EngineResult rohr_entity_life_time_set(Entity entity, Time expirey_time, Tick expirey_tick);
```

Adds or updates an entity lifetime.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to modify. |
| `expirey_time` | Engine time when the entity expires. |
| `expirey_tick` | Engine tick when the entity expires. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_life_time_remove`

```c
EngineResult rohr_entity_life_time_remove(Entity entity);
```

Removes lifetime data from an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to modify. |

**Returns:** EngineResult describing success or failure.

## Physics

### `rohr_physics_dt_per_tick_set`

```c
EngineResult rohr_physics_dt_per_tick_set(Time dt);
```

 Sets an explicit simulation delta per engine tick.

### `rohr_physics_dt_per_tick_get`

```c
Time rohr_physics_dt_per_tick_get(void);
```

 Returns the current physics delta per tick.

### `rohr_physics_engine_time_per_tick_use`

```c
void rohr_physics_engine_time_per_tick_use(void);
```

 Restores the engine time-per-tick default.

### Engine gravity

Gravity is an optional engine component. Existing and custom physics behavior
is unchanged until an entity receives `ROHR_GRAVITY`.

```c
rohr_physics_gravity_set((Acceleration){0.0f, 980.0f});
rohr_physics_gravity_enable(entity);
```

The component can also be managed directly:

```c
rohr_entity_components_add(entity, ROHR_GRAVITY);
rohr_entity_components_delete(entity, ROHR_GRAVITY);
```

```c
EngineResult rohr_physics_gravity_set(Acceleration gravity);
Acceleration rohr_physics_gravity_get(void);
EngineResult rohr_physics_gravity_enable(Entity entity);
EngineResult rohr_physics_gravity_disable(Entity entity);
bool rohr_physics_gravity_check(Entity entity);
```

The default acceleration is `ROHR_PHYSICS_GRAVITY_DEFAULT`, currently
`{0.0f, 980.0f}`. Gravity is acceleration and therefore does not scale with
mass. The gravity stage requires `ROHR_MASS` with a value greater than zero;
entities without mass or with zero/negative mass are skipped. Static and held
entities retain the component but do not move. Soft-body gravity is enabled on
individual node entities.

`rohr_physics_mass_set` accepts finite, non-negative values. Zero explicitly
represents a massless entity; negative, infinite, and NaN values return
`ERROR_ENGINE_STATE_INVALID` without changing its mass component.

### Modular physics pipeline

Most games should call `rohr_physics_update` or
`rohr_physics_pipeline_update`. The latter runs the following public stages for
each configured substep without changing the standard simulation order:

```c
rohr_physics_pipeline_step_begin();

rohr_physics_pipeline_substep_begin();
rohr_physics_pipeline_accelerations_clear();
rohr_physics_pipeline_gravity_apply();
rohr_physics_pipeline_forces_apply();
rohr_physics_pipeline_integrate(dt);
rohr_physics_pipeline_contacts_gather();
rohr_physics_pipeline_joints_gather();
rohr_physics_pipeline_constraints_solve(
    rohr_physics_solver_iterations_get());
```

The stages operate on the engine's component pools and reusable internal
constraint storage. They do not allocate a separate physics world.

- Call `rohr_physics_pipeline_step_begin` once per complete logical physics
  step. It advances entered/stayed/exited interaction state and resets debug
  counters.
- Call `rohr_physics_pipeline_substep_begin` before gathering constraints. It
  clears transient contact and joint constraint lists.
- Force stages must precede integration when using spring joints or soft-body
  beams.
- The optional gravity stage must run after acceleration clearing and before
  integration. Omit it when implementing custom gravity.
- Gather before solving. Contacts and rigid joints are normally gathered
  together so the iterative solver can alternate between them.
- `rohr_physics_pipeline_constraints_solve` does nothing when passed zero
  iterations.

Tools may intentionally use only part of the pipeline. For example, an editor
can enforce pin and weld joints without gravity, velocity integration, contact
detection, or interaction events:

```c
rohr_physics_pipeline_substep_begin();
rohr_physics_pipeline_joints_gather();
rohr_physics_pipeline_constraints_solve(
    rohr_physics_solver_iterations_get());
```

This partial sequence does not call `step_begin`, because it is not producing a
new collision-interaction frame.

### `rohr_physics_pipeline_update`

```c
void rohr_physics_pipeline_update(double dt);
```

Runs the standard plug-and-play pipeline using the configured substep and
solver-iteration counts.

### Individual pipeline stages

```c
void rohr_physics_pipeline_step_begin(void);
void rohr_physics_pipeline_substep_begin(void);
void rohr_physics_pipeline_accelerations_clear(void);
void rohr_physics_pipeline_gravity_apply(void);
void rohr_physics_pipeline_forces_apply(void);
void rohr_physics_pipeline_integrate(double dt);
void rohr_physics_pipeline_contacts_gather(void);
void rohr_physics_pipeline_joints_gather(void);
void rohr_physics_pipeline_constraints_solve(uint32_t iterations);
void rohr_physics_pipeline_substep(double dt);
```

These functions expose the standard pipeline's individual operations for
custom schedules. `rohr_physics_pipeline_substep` runs every substep stage in
the standard order, but does not advance interaction state by itself.

### Kinematic-driven bodies

`ROHR_KINEMATIC_DRIVEN` identifies a dynamic body whose transform follows
authored velocity and acceleration but does not respond to gravity, forces,
collision impulses, or joints. It still participates in contacts and can push
simulated bodies through its authored motion.

Physics setters automatically add the effective tag to a dynamic body without
positive mass and remove an automatically derived tag when positive mass is
present. State is correct immediately after each setter returns, and setup
order remains irrelevant:

```c
rohr_physics_dynamic_set(entity);
rohr_physics_mass_set(entity, 10.0f);
/* Fully simulated immediately after mass_set returns. */
```

Developers request persistent kinematic behavior through functions rather than
managing the effective mask directly:

```c
EngineResult rohr_physics_kinematic_driven_set(Entity entity);
EngineResult rohr_physics_kinematic_driven_remove(Entity entity);
bool rohr_physics_kinematic_driven_check(Entity entity);
```

An explicit request remains effective even with positive mass. Removing it
from a dynamic body without positive mass returns
`ERROR_ENGINE_STATE_INVALID`, because the setter invariant requires the
kinematic state. Explicit-request bookkeeping is private engine state.

Mass can be inspected or removed explicitly:

```c
bool rohr_physics_mass_check(Entity entity);
EngineResult rohr_physics_mass_remove(Entity entity);
```

### `rohr_physics_update`

```c
void rohr_physics_update(Tick ticks);
```

 Advances physics using the supplied number of elapsed engine ticks.

### `rohr_physics_dt_update`

```c
void rohr_physics_dt_update(Time dt);
```

 Advances physics once with an explicit exceptional delta.

### `rohr_physics_shape_world_translate`

```c
Shape rohr_physics_shape_world_translate(Shape shape, Position position, Orientation angle);
```

Translates a local shape into world space.

| Parameter | Description |
| --- | --- |
| `shape` | Local shape to transform. |
| `position` | World position. |
| `angle` | World orientation in radians. |

**Returns:** World-space shape.

### `rohr_physics_polygon_moment_of_inertia`

```c
float rohr_physics_polygon_moment_of_inertia(Shape shape, Mass mass_value);
```

Calculates polygon moment of inertia.

| Parameter | Description |
| --- | --- |
| `shape` | Polygon shape. |
| `mass_value` | Shape mass. |

**Returns:** Moment of inertia value.

### `rohr_physics_sat_overlap_get`

```c
OverlapInfo rohr_physics_sat_overlap_get(Shape shape_1, Shape shape_2);
```

Gets overlap information using the separating axis theorem.

| Parameter | Description |
| --- | --- |
| `shape_1` | First shape. |
| `shape_2` | Second shape. |

**Returns:** Geometric overlap information.

### `rohr_physics_circle_moment_of_inertia`

```c
Vec1D rohr_physics_circle_moment_of_inertia(Shape circle, Mass mass_value);
```

Calculates circle moment of inertia.

| Parameter | Description |
| --- | --- |
| `circle` | Circle shape. |
| `mass_value` | Circle mass. |

**Returns:** Moment of inertia value.

### `rohr_physics_entity_held_get`

```c
bool rohr_physics_entity_held_get(EntityIndex index);
```

Checks whether an entity index has `ROHR_HOLD`.

| Parameter | Description |
| --- | --- |
| `index` | Entity table index to inspect. |

**Returns:** true when index is live and held, false otherwise.

### `rohr_physics_acceleration_set`

```c
EngineResult rohr_physics_acceleration_set(Entity entity, Acceleration a);
```

Sets an entity acceleration component value.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `a` | Acceleration value. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_angular_acceleration_set`

```c
EngineResult rohr_physics_angular_acceleration_set( Entity entity, AngularAcceleration acceleration );
```

Sets an entity angular acceleration component value.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `acceleration` | Angular acceleration value. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_acceleration_toward_position_set`

```c
EngineResult rohr_physics_acceleration_toward_position_set(Entity entity, float acceleration_magnitude, Position position);
```

Sets entity acceleration toward a world position.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `acceleration_magnitude` | Acceleration magnitude to apply along the direction to position. |
| `position` | Target world position. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_acceleration_toward_entity_set`

```c
EngineResult rohr_physics_acceleration_toward_entity_set(Entity entity, float acceleration_magnitude, Entity target);
```

Sets entity acceleration toward another entity's current world position.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `acceleration_magnitude` | Acceleration magnitude to apply along the direction to target. |
| `target` | Target entity. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_acceleration_away_from_position_set`

```c
EngineResult rohr_physics_acceleration_away_from_position_set(Entity entity, float acceleration_magnitude, Position position);
```

Sets entity acceleration away from a world position.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `acceleration_magnitude` | Acceleration magnitude to apply away from position. |
| `position` | Source world position. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_acceleration_away_from_entity_set`

```c
EngineResult rohr_physics_acceleration_away_from_entity_set(Entity entity, float acceleration_magnitude, Entity target);
```

Sets entity acceleration away from another entity's current world position.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `acceleration_magnitude` | Acceleration magnitude to apply away from target. |
| `target` | Source entity. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_group_acceleration_toward_entity_set`

```c
EngineResult rohr_physics_group_acceleration_toward_entity_set(GroupId group, float acceleration_magnitude, Entity target);
```

Sets acceleration toward an entity for every live entity in a group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to update. |
| `acceleration_magnitude` | Acceleration magnitude to apply along the direction to target. |
| `target` | Target entity. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_group_acceleration_away_from_entity_set`

```c
EngineResult rohr_physics_group_acceleration_away_from_entity_set(GroupId group, float acceleration_magnitude, Entity target);
```

Sets acceleration away from an entity for every live entity in a group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to update. |
| `acceleration_magnitude` | Acceleration magnitude to apply away from target. |
| `target` | Source entity. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_velocity_set`

```c
EngineResult rohr_physics_velocity_set(Entity entity, Velocity v);
```

Sets an entity velocity component value.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `v` | Velocity value. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_velocity_toward_position_set`

```c
EngineResult rohr_physics_velocity_toward_position_set(Entity entity, float speed, Position position);
```

Sets entity velocity toward a world position.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `speed` | Speed to apply along the direction to position. |
| `position` | Target world position. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_velocity_toward_entity_set`

```c
EngineResult rohr_physics_velocity_toward_entity_set(Entity entity, float speed, Entity target);
```

Sets entity velocity toward another entity's current world position.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `speed` | Speed to apply along the direction to target. |
| `target` | Target entity. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_velocity_away_from_position_set`

```c
EngineResult rohr_physics_velocity_away_from_position_set(Entity entity, float speed, Position position);
```

Sets entity velocity away from a world position.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `speed` | Speed to apply away from position. |
| `position` | Source world position. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_velocity_away_from_entity_set`

```c
EngineResult rohr_physics_velocity_away_from_entity_set(Entity entity, float speed, Entity target);
```

Sets entity velocity away from another entity's current world position.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `speed` | Speed to apply away from target. |
| `target` | Source entity. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_group_velocity_toward_entity_set`

```c
EngineResult rohr_physics_group_velocity_toward_entity_set(GroupId group, float speed, Entity target);
```

Sets velocity toward an entity for every live entity in a group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to update. |
| `speed` | Speed to apply along the direction to target. |
| `target` | Target entity. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_group_velocity_away_from_entity_set`

```c
EngineResult rohr_physics_group_velocity_away_from_entity_set(GroupId group, float speed, Entity target);
```

Sets velocity away from an entity for every live entity in a group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to update. |
| `speed` | Speed to apply away from target. |
| `target` | Source entity. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_entity_stop`

```c
EngineResult rohr_physics_entity_stop(Entity entity);
```

Sets an entity velocity to zero.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_group_entities_stop`

```c
EngineResult rohr_physics_group_entities_stop(GroupId group);
```

Sets velocity to zero for every live entity in a group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to update. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_impulse_apply`

```c
EngineResult rohr_physics_impulse_apply(Entity entity, Vec2D impulse);
```

Applies an immediate linear impulse to an entity velocity.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `impulse` | Impulse vector. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_position_set`

```c
EngineResult rohr_physics_position_set(Entity entity, Position p);
```

Sets an entity position component value.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `p` | Position value. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_position_get`

```c
PositionResult rohr_physics_position_get(Entity entity);
```

 Returns an entity's world position.

### `rohr_physics_mass_set`

```c
EngineResult rohr_physics_mass_set(Entity entity, Mass m);
```

Sets an entity mass component value.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `m` | Mass value. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_force_create`

```c
EntityResult rohr_physics_force_create(Entity entity, Force f);
```

Sets an entity force component value.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `f` | Force value. |

**Returns:** EntityResult containing entity on success, or an error.

### `rohr_physics_force_component_set`

```c
EngineResult rohr_physics_force_component_set(Entity entity, Force force);
```

Sets force component data directly on an existing entity.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `force` | Force component value. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_force_for_one_tick_apply`

```c
EngineResult rohr_physics_force_for_one_tick_apply(Entity entity, Force f);
```

Applies force to an entity for one physics tick.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to target. |
| `f` | Force vector. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_torque_create`

```c
EntityResult rohr_physics_torque_create(Entity entity, Torque t);
```

Sets an entity torque component value.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `t` | Torque value. |

**Returns:** EntityResult containing entity on success, or an error.

### `rohr_physics_torque_component_set`

```c
EngineResult rohr_physics_torque_component_set(Entity entity, Torque torque);
```

Sets torque component data directly on an existing entity.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `torque` | Torque component value. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_torque_for_one_tick_apply`

```c
EngineResult rohr_physics_torque_for_one_tick_apply(Entity entity, Torque t);
```

Applies torque to an entity for one physics tick.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to target. |
| `t` | Torque value. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_hitbox_set`

```c
EngineResult rohr_physics_hitbox_set(Entity entity, Shape hitbox);
```

Sets an entity hitbox component value without enabling physical collision response.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `hitbox` | Local-space hitbox shape. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_collision_filter_config_default_get`

```c
CollisionFilterConfig rohr_physics_collision_filter_config_default_get(void);
```

 Return the default collision filtering configuration.

### `rohr_physics_collision_filter_set`

```c
EngineResult rohr_physics_collision_filter_set(Entity entity, CollisionFilterConfig config);
```

 Replace an entity's collision category and whitelist.

### `rohr_physics_collision_filter_get`

```c
CollisionFilterConfigResult rohr_physics_collision_filter_get(Entity entity);
```

 Return an entity's collision filtering configuration.

### `rohr_physics_collision_category_set`

```c
EngineResult rohr_physics_collision_category_set(Entity entity, RohrCollisionCategoryMask category);
```

 Set the collision categories represented by an entity.

### `rohr_physics_collision_with_set`

```c
EngineResult rohr_physics_collision_with_set(Entity entity, RohrCollisionCategoryMask categories);
```

 Set the collision category whitelist for an entity.

### `rohr_physics_collision_with_all_set`

```c
EngineResult rohr_physics_collision_with_all_set(Entity entity);
```

 Allow an entity to collide with every category.

### `rohr_physics_collision_with_none_set`

```c
EngineResult rohr_physics_collision_with_none_set(Entity entity);
```

 Prevent an entity from colliding with every category.

### `rohr_physics_collision_between_check`

```c
bool rohr_physics_collision_between_check(Entity entity_1, Entity entity_2);
```

 Return whether two entities mutually permit collision checks.

### `rohr_physics_orientation_set`

```c
EngineResult rohr_physics_orientation_set(Entity entity, Orientation angle);
```

Sets an entity orientation component value.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `angle` | Orientation in radians. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_angular_velocity_set`

```c
EngineResult rohr_physics_angular_velocity_set(Entity entity, AngularVelocity v);
```

Sets an entity angular velocity component value.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `v` | Angular velocity value. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_angular_velocity_get`

```c
AngularVelocityResult rohr_physics_angular_velocity_get(Entity entity);
```

Returns an entity angular velocity.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to inspect. |

**Returns:** AngularVelocityResult containing radians per second, or an error.

### `rohr_physics_angular_velocity_maximum_set`

```c
EngineResult rohr_physics_angular_velocity_maximum_set( Entity entity, AngularVelocity maximum );
```

 Sets the absolute angular-velocity limit applied before orientation integration.

### `rohr_physics_angular_velocity_maximum_get`

```c
AngularVelocityResult rohr_physics_angular_velocity_maximum_get(Entity entity);
```

 Returns an entity's configured absolute angular-velocity limit.

### `rohr_physics_global_hit_box_get`

```c
ShapeResult rohr_physics_global_hit_box_get(Entity entity);
```

Returns an entity hitbox transformed into world space.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to inspect. |

**Returns:** ShapeResult containing the world-space hitbox, or an error.

### `rohr_physics_restitution_set`

```c
EngineResult rohr_physics_restitution_set(Entity entity, Restitution restitution);
```

Sets an entity restitution value.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `restitution` | Restitution value. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_dynamic_set`

```c
EngineResult rohr_physics_dynamic_set(Entity entity);
```

Marks an entity as dynamic for physics simulation.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_static_set`

```c
EngineResult rohr_physics_static_set(Entity entity);
```

Marks an entity as static for physics simulation.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_entity_hold`

```c
EngineResult rohr_physics_entity_hold(Entity entity);
```

Adds `ROHR_HOLD` so physics update stages preserve current values.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_entity_unhold`

```c
EngineResult rohr_physics_entity_unhold(Entity entity);
```

Removes `ROHR_HOLD` without changing `ROHR_STATIC` or `ROHR_DYNAMIC` state.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_group_entities_hold`

```c
EngineResult rohr_physics_group_entities_hold(GroupId group);
```

Adds `ROHR_HOLD` to every live entity in a group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to update. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_group_entities_unhold`

```c
EngineResult rohr_physics_group_entities_unhold(GroupId group);
```

Removes `ROHR_HOLD` from every live entity in a group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to update. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_angle_lock_set`

```c
EngineResult rohr_physics_angle_lock_set(Entity entity, Orientation min, Orientation max);
```

Locks an entity orientation between minimum and maximum angles.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `min` | Minimum orientation in radians. |
| `max` | Maximum orientation in radians. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_axis_lock_set`

```c
EngineResult rohr_physics_axis_lock_set(Entity entity, Axis axis, Position axis_point);
```

Locks an entity position along an axis.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `axis` | Axis to lock against. |
| `axis_point` | Point on the locked axis. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_friction_set`

```c
EngineResult rohr_physics_friction_set(Entity entity, float friction);
```

Sets an entity friction value.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `friction` | Friction value. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_transform_lock_set`

```c
EngineResult rohr_physics_transform_lock_set( Entity driven, Entity driver, Vec2D local_offset, Orientation local_angle, bool lock_position, bool lock_orientation, bool inherit_velocity );
```

Locks one entity transform to another entity.

| Parameter | Description |
| --- | --- |
| `driven` | Entity whose transform is driven. |
| `driver` | Entity used as the transform source. |
| `local_offset` | Offset from driver to driven in driver-local space. |
| `local_angle` | Orientation offset from driver to driven. |
| `lock_position` | true to lock position. |
| `lock_orientation` | true to lock orientation. |
| `inherit_velocity` | true to inherit driver velocity. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_transform_lock_remove`

```c
EngineResult rohr_physics_transform_lock_remove(Entity entity);
```

Removes an entity transform lock.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_transform_lock_current_transform_set`

```c
EngineResult rohr_physics_transform_lock_current_transform_set( Entity driven, Entity driver, bool lock_position, bool lock_orientation, bool inherit_velocity );
```

Locks one entity to another using their current transform offset.

| Parameter | Description |
| --- | --- |
| `driven` | Entity whose transform is driven. |
| `driver` | Entity used as the transform source. |
| `lock_position` | true to lock position. |
| `lock_orientation` | true to lock orientation. |
| `inherit_velocity` | true to inherit driver velocity. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_target_set`

```c
EngineResult rohr_physics_target_set(Entity entity, Entity target);
```

Sets the target used by a force or torque source entity.

| Parameter | Description |
| --- | --- |
| `entity` | Force or torque source entity to modify. |
| `target` | Live entity that receives the force or torque. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_joint_component_set`

```c
EngineResult rohr_physics_joint_component_set(Entity entity, Joint joint);
```

Adds or replaces complete joint data on an existing entity.

| Parameter | Description |
| --- | --- |
| `entity` | Entity that owns the joint component. |
| `joint` | Complete joint component value. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_joint_anchor_create`

```c
JointAnchorIdResult rohr_physics_joint_anchor_create(Entity entity, Vec2D centroid_offset);
```

Creates an anchor owned by an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Entity that owns and moves the anchor. |
| `centroid_offset` | Anchor offset from the hitbox centroid, or entity position without a hitbox. |

**Returns:** JointAnchorIdResult containing the stable anchor handle, or an error.

### `rohr_physics_joint_anchors_get`

```c
JointAnchorListResult rohr_physics_joint_anchors_get(Entity entity);
```

Returns the anchors owned by an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Entity whose anchors should be listed. |

**Returns:** JointAnchorListResult containing the anchor handles, or an error.

### `rohr_physics_joint_anchor_position_get`

```c
JointAnchorPositionResult rohr_physics_joint_anchor_position_get(JointAnchorId anchor);
```

Returns an anchor offset relative to its owner's centroid.

| Parameter | Description |
| --- | --- |
| `anchor` | Anchor to inspect. |

**Returns:** JointAnchorPositionResult containing the centroid-relative offset, or an error.

### `rohr_physics_joint_anchor_world_position_get`

```c
JointAnchorPositionResult rohr_physics_joint_anchor_world_position_get(JointAnchorId anchor);
```

Returns the current world position of an anchor.

| Parameter | Description |
| --- | --- |
| `anchor` | Anchor to resolve. |

**Returns:** JointAnchorPositionResult containing its world position, or an error.

### `rohr_physics_joint_anchor_position_set`

```c
EngineResult rohr_physics_joint_anchor_position_set(JointAnchorId anchor, Vec2D centroid_offset);
```

Sets an anchor offset relative to its owner's centroid.

| Parameter | Description |
| --- | --- |
| `anchor` | Anchor to modify. |
| `centroid_offset` | New centroid-relative offset. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_joint_anchor_remove`

```c
EngineResult rohr_physics_joint_anchor_remove(JointAnchorId anchor);
```

Removes an anchor and its connected joint entities.

| Parameter | Description |
| --- | --- |
| `anchor` | Anchor to remove. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_joint_pin_set`

```c
EngineResult rohr_physics_joint_pin_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b);
```

Configures a joint entity as a rigid pin between two anchors.

| Parameter | Description |
| --- | --- |
| `joint` | Entity that owns the joint component. |
| `anchor_a` | First anchor. |
| `anchor_b` | Second anchor. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_joint_weld_set`

```c
EngineResult rohr_physics_joint_weld_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b);
```

Configures a joint entity as a rigid weld between two anchors.

| Parameter | Description |
| --- | --- |
| `joint` | Entity that owns the joint component. |
| `anchor_a` | First anchor. |
| `anchor_b` | Second anchor. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_joint_spring_set`

```c
EngineResult rohr_physics_joint_spring_set( Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b, float rest_length, float stiffness, float damping );
```

Configures a joint entity as a damped spring between two anchors.

| Parameter | Description |
| --- | --- |
| `joint` | Entity that owns the joint component. |
| `anchor_a` | First anchor. |
| `anchor_b` | Second anchor. |
| `rest_length` | Unstretched anchor distance. |
| `stiffness` | Spring stiffness. |
| `damping` | Relative-motion damping. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_soft_body_create`

```c
EntityResult rohr_physics_soft_body_create(void);
```

 @brief Creates an empty soft-body owner entity. @return EntityResult containing the owner.

### `rohr_physics_soft_body_get`

```c
SoftBodyResult rohr_physics_soft_body_get(Entity soft_body);
```

 @brief Returns soft-body topology. @param soft_body Soft-body owner. @return SoftBodyResult.

### `rohr_physics_soft_body_node_create`

```c
EntityResult rohr_physics_soft_body_node_create(Entity soft_body, Position position, Mass mass_value, float radius);
```

Creates a lightweight point-mass node.

| Parameter | Description |
| --- | --- |
| `soft_body` | Owning soft body. |
| `position` | Initial world position. |
| `mass` | Node mass. |
| `radius` | Collision radius. |

**Returns:** EntityResult containing the node entity.

### `rohr_physics_soft_body_node_get`

```c
SoftBodyNodeResult rohr_physics_soft_body_node_get(Entity node);
```

 @brief Returns soft-body node data. @param node Node entity. @return SoftBodyNodeResult.

### `rohr_physics_soft_body_node_collision_filter_set`

```c
EngineResult rohr_physics_soft_body_node_collision_filter_set( Entity node, RohrCollisionCategoryMask category, RohrCollisionCategoryMask collides_with );
```

Sets node-versus-rigid collision filtering.

| Parameter | Description |
| --- | --- |
| `node` | Node entity. |
| `category` | Categories represented by the node. |
| `collides_with` | Rigid collider categories accepted by the node. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_soft_body_node_force_for_one_tick_apply`

```c
EngineResult rohr_physics_soft_body_node_force_for_one_tick_apply(Entity node, Force force);
```

Applies a force to one soft-body node for the next physics tick.

| Parameter | Description |
| --- | --- |
| `node` | Soft-body node entity. |
| `force` | Force to apply. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_soft_body_node_impulse_apply`

```c
EngineResult rohr_physics_soft_body_node_impulse_apply(Entity node, Vec2D impulse);
```

Applies an immediate impulse to one soft-body node.

| Parameter | Description |
| --- | --- |
| `node` | Soft-body node entity. |
| `impulse` | Impulse to apply. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_soft_body_force_for_one_tick_apply`

```c
EngineResult rohr_physics_soft_body_force_for_one_tick_apply(Entity soft_body, Force force);
```

Distributes a total force across a soft body for the next physics tick.

| Parameter | Description |
| --- | --- |
| `soft_body` | Soft-body owner entity. |
| `force` | Total force to distribute by node mass. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_soft_body_torque_for_one_tick_apply`

```c
EngineResult rohr_physics_soft_body_torque_for_one_tick_apply(Entity soft_body, Torque torque);
```

Applies body-level torque as balanced node forces for the next physics tick.

| Parameter | Description |
| --- | --- |
| `soft_body` | Soft-body owner entity. |
| `torque` | Total torque to distribute around the center of mass. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_soft_body_node_to_anchor_pin_create`

```c
SoftBodyNodeAnchorPinResult rohr_physics_soft_body_node_to_anchor_pin_create( Entity node, JointAnchorId anchor );
```

Pins a soft-body node to an existing anchor.

| Parameter | Description |
| --- | --- |
| `node` | Soft-body node entity. |
| `anchor` | Existing anchor to connect to the node. |

**Returns:** SoftBodyNodeAnchorPinResult containing the joint and node-owned anchor. Remove node_anchor to remove both the attachment joint and the created anchor.

### `rohr_physics_soft_body_beam_create`

```c
EntityResult rohr_physics_soft_body_beam_create( Entity soft_body, Entity node_a, Entity node_b, float stiffness, float damping );
```

Creates an elastic beam between two nodes.

| Parameter | Description |
| --- | --- |
| `soft_body` | Owning soft body. |
| `node_a` | First node. |
| `node_b` | Second node. |
| `stiffness` | Spring stiffness. |
| `damping` | Relative velocity damping. |

**Returns:** EntityResult containing the beam entity.

### `rohr_physics_soft_body_beam_get`

```c
SoftBodyBeamResult rohr_physics_soft_body_beam_get(Entity beam);
```

 @brief Returns soft-body beam data. @param beam Beam entity. @return SoftBodyBeamResult.

### `rohr_physics_soft_body_triangle_create`

```c
EntityResult rohr_physics_soft_body_triangle_create( Entity soft_body, Entity node_a, Entity node_b, Entity node_c );
```

Creates a deforming triangular surface from three nodes.

| Parameter | Description |
| --- | --- |
| `soft_body` | Owning soft body. |
| `node_a` | First node. |
| `node_b` | Second node. |
| `node_c` | Third node. |

**Returns:** EntityResult containing the triangle entity.

### `rohr_physics_soft_body_triangle_get`

```c
SoftBodyTriangleResult rohr_physics_soft_body_triangle_get(Entity triangle);
```

 @brief Returns soft-body triangle data. @param triangle Triangle entity. @return SoftBodyTriangleResult.

### `rohr_physics_joint_create`

```c
EntityResult rohr_physics_joint_create( Entity a, Entity b, JointType type, Vec2D local_anchor_a, Vec2D local_anchor_b, float stiffness, float damping );
```

Creates a joint between two entities.

| Parameter | Description |
| --- | --- |
| `a` | First entity. |
| `b` | Second entity. |
| `type` | Joint behavior type. |
| `local_anchor_a` | Anchor on the first entity in local space. |
| `local_anchor_b` | Anchor on the second entity in local space. |
| `stiffness` | Spring stiffness. |
| `damping` | Spring damping. |

**Returns:** EntityResult containing the joint entity, or an error.

### `rohr_physics_particle_overlap_get`

```c
OverlapInfo rohr_physics_particle_overlap_get(Shape shape_1, Shape shape_2);
```

Gets overlap information for two particle shapes.

| Parameter | Description |
| --- | --- |
| `shape_1` | First shape. |
| `shape_2` | Second shape. |

**Returns:** Geometric overlap information.

### Entity overlap queries

```c
bool rohr_physics_overlap_check(Entity entity, Entity target);
OverlapInfo rohr_physics_overlap_get(Entity entity, Entity target);
bool rohr_physics_overlap_entered_check(Entity entity, Entity target);
bool rohr_physics_overlap_stayed_check(Entity entity, Entity target);
bool rohr_physics_overlap_exited_check(Entity entity, Entity target);
size_t rohr_physics_overlap_count_get(Entity entity);
size_t rohr_physics_overlaps_get(
    Entity entity,
    EntityInteraction *results,
    size_t capacity
);
```

Overlap queries include sensors and physically resolved colliders. `overlap_get`
returns geometry oriented from `entity` toward `target`; its `detected` field is
false when no current overlap exists. Transition checks compare the current and
previous physics steps.

`overlap_count_get` returns the required caller-owned buffer size.
`overlaps_get` writes up to `capacity` current results and returns the number
written. Result ordering is unspecified.

### Entity contact queries

```c
bool rohr_physics_contact_check(Entity entity, Entity target);
ContactInfo rohr_physics_contact_get(Entity entity, Entity target);
Vec2D rohr_physics_contact_total_impulse_get(ContactInfo contact);
bool rohr_physics_contact_entered_check(Entity entity, Entity target);
bool rohr_physics_contact_stayed_check(Entity entity, Entity target);
bool rohr_physics_contact_exited_check(Entity entity, Entity target);
size_t rohr_physics_contact_count_get(Entity entity);
size_t rohr_physics_contacts_get(
    Entity entity,
    EntityContact *results,
    size_t capacity
);
```

Contact queries include only entity pairs that entered physical collision
response. `contact_get` returns its normal, depth, approximate world-space
point, pre-resolution relative velocity, and separate normal and friction
impulses applied to the second requested entity. `contact_total_impulse_get`
returns the vector sum of those impulses. `contact_get` sets `detected` to
false when no current contact exists. Sensors do not produce contacts.

`contact_count_get` returns the required caller-owned buffer size.
`contacts_get` writes up to `capacity` current results and returns the number
written. Result ordering is unspecified.

## Graphics

### `rohr_graphics_color_hex_create`

```c
Color rohr_graphics_color_hex_create(uint32_t hex_color_code);
```

Creates an engine color from a hexadecimal RGB or RGBA value.

| Parameter | Description |
| --- | --- |
| `hex_color_code` | Hex color value. |

**Returns:** Color created from hex_color_code.

### `rohr_graphics_start`

```c
EngineResult rohr_graphics_start(void);
```

Starts the graphics system.

**Returns:** EngineResult describing success or failure.

### `rohr_graphics_end`

```c
void rohr_graphics_end(void);
```

Shuts down the graphics system.

### `rohr_graphics_events_poll`

```c
bool rohr_graphics_events_poll(SDL_Event *event);
```

Polls graphics/window events.

| Parameter | Description |
| --- | --- |
| `event` | Destination for the SDL event. |

**Returns:** true when an event was read, false otherwise.

### `rohr_graphics_background_draw`

```c
void rohr_graphics_background_draw(Color color);
```

Draws the frame background.

| Parameter | Description |
| --- | --- |
| `color` | Background color. |

### `rohr_graphics_screen_rect_draw`

```c
bool rohr_graphics_screen_rect_draw(float x, float y, float width, float height, Color color);
```

Draws a filled rectangle in logical screen coordinates.

**Returns:** true when SDL accepted the draw command.

### `rohr_graphics_screen_quad_draw`

```c
bool rohr_graphics_screen_quad_draw(Position center, float width, float height, float angle, Color color);
```

 @brief Draws a centered rotated rectangle in logical screen space.

### `rohr_graphics_show`

```c
void rohr_graphics_show(void);
```

Presents the current graphics frame.

### `rohr_graphics_hit_box_draw`

```c
void rohr_graphics_hit_box_draw(Entity entity, Fill fill_type);
```

Draws one entity hitbox.

| Parameter | Description |
| --- | --- |
| `entity` | Entity whose hitbox should be drawn. |
| `fill_type` | Fill mode for drawing. |

### `rohr_graphics_hit_box_colored_draw`

```c
void rohr_graphics_hit_box_colored_draw(Entity entity, Fill fill_type, Color color);
```

Draws one entity hitbox with a caller supplied color.

| Parameter | Description |
| --- | --- |
| `entity` | Entity whose hitbox should be drawn. |
| `fill_type` | Fill mode for drawing. |
| `color` | Color to draw with. |

### `rohr_graphics_hit_boxes_draw`

```c
void rohr_graphics_hit_boxes_draw(void);
```

Draws hitboxes for all renderable hitbox entities.

### `rohr_graphics_joint_draw`

```c
bool rohr_graphics_joint_draw(Entity joint, Color color);
```

Draws one joint using an engineering-style debug symbol.

| Parameter | Description |
| --- | --- |
| `joint` | Joint entity to draw. |
| `color` | Symbol color. |

**Returns:** true when the symbol was drawn successfully.

### `rohr_graphics_joints_draw`

```c
void rohr_graphics_joints_draw(Color color);
```

Draws all live joints using engineering-style debug symbols.

| Parameter | Description |
| --- | --- |
| `color` | Symbol color. |

### `rohr_graphics_soft_body_draw`

```c
bool rohr_graphics_soft_body_draw(Entity soft_body, Color surface, Color beam, Color node);
```

Draws a soft body's current surfaces, beams, and collision nodes.

| Parameter | Description |
| --- | --- |
| `soft_body` | Soft-body owner entity. |
| `surface` | Triangle surface color. |
| `beam` | Beam color. |
| `node` | Collision-node color. |

**Returns:** true when the soft body was drawn successfully.

### `rohr_graphics_texture_load`

```c
TextureAssetResult rohr_graphics_texture_load(TextureDescriptor text_desc);
```

Loads a texture asset.

| Parameter | Description |
| --- | --- |
| `text_desc` | Texture descriptor containing load settings. |

**Returns:** TextureAssetResult containing the asset, or an error.

### `rohr_graphics_font_load`

```c
FontAssetResult rohr_graphics_font_load(FontDescriptor descriptor);
```

 @brief Loads a caller-owned font asset.

### `rohr_graphics_font_destroy`

```c
void rohr_graphics_font_destroy(FontAsset *font);
```

 @brief Destroys a font after its text assets have been destroyed.

### `rohr_graphics_text_create`

```c
TextAssetResult rohr_graphics_text_create(const FontAsset *font, const char *value, Color color);
```

 @brief Creates reusable caller-owned text.

### `rohr_graphics_text_destroy`

```c
void rohr_graphics_text_destroy(TextAsset *text);
```

 @brief Destroys reusable text.

### `rohr_graphics_text_draw`

```c
bool rohr_graphics_text_draw(const TextAsset *text, Position position);
```

 @brief Draws text in logical screen coordinates.

### `rohr_graphics_animation_load`

```c
AnimationAssetResult rohr_graphics_animation_load(AnimationDescriptor anim_desc);
```

Loads an animation asset.

| Parameter | Description |
| --- | --- |
| `anim_desc` | Animation descriptor containing load settings. |

**Returns:** AnimationAssetResult containing the asset, or an error.

### `rohr_graphics_animated_sprite_create`

```c
AnimatedSprite rohr_graphics_animated_sprite_create(AnimationAsset asset_ptr, Scale scale);
```

Creates an animated sprite from an animation asset.

| Parameter | Description |
| --- | --- |
| `asset_ptr` | Animation asset to use. |
| `scale` | Sprite scale. |

**Returns:** Animated sprite value.

### `rohr_graphics_animated_sprite_add`

```c
EngineResult rohr_graphics_animated_sprite_add(Entity entity, AnimatedSprite sprite);
```

Adds an animated sprite to an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `sprite` | Animated sprite component value. |

**Returns:** EngineResult describing success or failure.

### `rohr_graphics_animated_sprites_draw`

```c
void rohr_graphics_animated_sprites_draw(void);
```

Draws all animated sprite components.

### `rohr_graphics_sprite_frames_update`

```c
void rohr_graphics_sprite_frames_update(Tick current_tick, Time current_time);
```

Updates animated sprite frames.

| Parameter | Description |
| --- | --- |
| `current_tick` | Current engine tick. |
| `current_time` | Current engine time. |

### `rohr_graphics_textures_scale`

```c
void rohr_graphics_textures_scale(Entity entity, Scale scale);
```

Scales textures attached to an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `scale` | Scale value. |

### `rohr_graphics_camera_move`

```c
void rohr_graphics_camera_move(Vec2D translation);
```

Translates the active camera in world space.

| Parameter | Description |
| --- | --- |
| `translation` | World-space translation to add. |

### `rohr_graphics_camera_rotate`

```c
void rohr_graphics_camera_rotate(Orientation radians);
```

Rotates the active camera counterclockwise.

| Parameter | Description |
| --- | --- |
| `radians` | Rotation in radians to add. |

### `rohr_graphics_camera_attach`

```c
EngineResult rohr_graphics_camera_attach( Entity entity, Vec2D position_offset, Orientation orientation_offset );
```

Attaches the camera to an entity's position and orientation.

The position offset is in the entity's local space and rotates with the

entity. The orientation offset is added to the entity's orientation.

| Parameter | Description |
| --- | --- |
| `entity` | Entity transform to follow. |
| `position_offset` | Local-space position offset. |
| `orientation_offset` | Orientation offset in radians. |

**Returns:** EngineResult describing success or a missing transform.

### `rohr_graphics_camera_with_options_attach`

```c
EngineResult rohr_graphics_camera_with_options_attach( Entity entity, Vec2D position_offset, Orientation orientation_offset, bool follow_position, bool follow_orientation );
```

Attaches a camera with independent transform inheritance.

When orientation following is disabled, position_offset is world-space.

When position following is disabled, position_offset is the fixed camera

position. When orientation following is disabled, orientation_offset is the

fixed camera orientation.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to associate with the camera. |
| `position_offset` | Relative offset or fixed world position. |
| `orientation_offset` | Relative or fixed orientation in radians. |
| `follow_position` | Whether to inherit entity position. |
| `follow_orientation` | Whether to inherit entity orientation. |

**Returns:** EngineResult describing success or a missing required transform.

### `rohr_graphics_camera_detach`

```c
void rohr_graphics_camera_detach(void);
```

Detaches the camera and preserves its current world transform.

### `rohr_graphics_camera_attached_get`

```c
bool rohr_graphics_camera_attached_get(void);
```

Reports whether the camera is attached to a live entity transform.

**Returns:** true when attached to a valid entity transform.

### `rohr_graphics_camera_attachment_get`

```c
CameraAttachmentResult rohr_graphics_camera_attachment_get(void);
```

Returns the active camera attachment description.

**Returns:** CameraAttachmentResult containing the attachment or a missing-component error.

### `rohr_graphics_world_to_screen_get`

```c
Position rohr_graphics_world_to_screen_get(Position pos);
```

Converts a world position to screen coordinates.

| Parameter | Description |
| --- | --- |
| `pos` | World position. |

**Returns:** Screen-space position.

### `rohr_graphics_screen_to_world_get`

```c
Position rohr_graphics_screen_to_world_get(Position screen);
```

Converts a screen position to world coordinates.

| Parameter | Description |
| --- | --- |
| `screen` | Screen-space position. |

**Returns:** World-space position.

### `rohr_graphics_mouse_screen_position_get`

```c
Position rohr_graphics_mouse_screen_position_get(void);
```

Returns the current mouse position in screen coordinates.

**Returns:** Mouse screen position.

### AABB-tree debug drawing

```c
void rohr_graphics_aabb_tree_debug_set(bool enabled);
bool rohr_graphics_aabb_tree_debug_check(void);
void rohr_graphics_aabb_tree_draw(void);
```

Enables, inspects, and draws the current physics broad-phase hierarchy. Leaf
bounds are green and internal hierarchy bounds are orange.

### `rohr_graphics_recording_start`

```c
bool rohr_graphics_recording_start(const char *output_path, int fps);
```

Starts recording rendered frames to a video file.

| Parameter | Description |
| --- | --- |
| `output_path` | Path where the recording should be written. |
| `fps` | Recording frame rate. |

**Returns:** true when recording starts successfully, false otherwise.

### `rohr_graphics_particles_draw`

```c
void rohr_graphics_particles_draw(void);
```

Draws active particle components.

### `rohr_graphics_local_origins_draw`

```c
void rohr_graphics_local_origins_draw(void);
```

Draws local origin markers for entities.

## Math

### `rohr_math_normals_create`

```c
Vec2DList rohr_math_normals_create(Shape shape);
```

Creates normalized edge normals for a shape.

| Parameter | Description |
| --- | --- |
| `shape` | Shape to inspect. |

**Returns:** List of normal vectors.

### `rohr_math_vector_normalize`

```c
Vec2D rohr_math_vector_normalize(Vec2D vector);
```

Normalizes a vector.

| Parameter | Description |
| --- | --- |
| `vector` | Vector to normalize. |

**Returns:** Normalized vector.

### `rohr_math_vectors_normalize`

```c
Vec2DList rohr_math_vectors_normalize(Vec2DList vectors);
```

Normalizes all vectors in a list.

| Parameter | Description |
| --- | --- |
| `vectors` | Vector list to normalize. |

**Returns:** Normalized vector list.

### `rohr_math_dot_product`

```c
float rohr_math_dot_product(Vec2D vector_1, Vec2D vector_2);
```

Calculates the dot product of two vectors.

| Parameter | Description |
| --- | --- |
| `vector_1` | First vector. |
| `vector_2` | Second vector. |

**Returns:** Dot product.

### `rohr_math_square_create`

```c
Shape rohr_math_square_create(float width, float height);
```

Creates a rectangular polygon shape.

| Parameter | Description |
| --- | --- |
| `width` | Rectangle width. |
| `height` | Rectangle height. |

**Returns:** Shape containing rectangle vertices.

### `rohr_math_circle_create`

```c
Shape rohr_math_circle_create(float radius, uint8_t verticies);
```

Creates a circle approximation shape.

| Parameter | Description |
| --- | --- |
| `radius` | Circle radius. |
| `verticies` | Number of vertices used to approximate the circle. |

**Returns:** Shape containing circle vertices.

### `rohr_math_project_shape_on_axis`

```c
Projection rohr_math_project_shape_on_axis(Shape shape, Axis axis);
```

Projects a shape onto an axis.

| Parameter | Description |
| --- | --- |
| `shape` | Shape to project. |
| `axis` | Axis to project onto. |

**Returns:** Projection interval.

### `rohr_math_cross_2d`

```c
float rohr_math_cross_2d(Vec2D a, Vec2D b);
```

Calculates the scalar 2D cross product.

| Parameter | Description |
| --- | --- |
| `a` | First vector. |
| `b` | Second vector. |

**Returns:** Cross product value.

### `rohr_math_angular_velocity_cross_vec`

```c
Vec2D rohr_math_angular_velocity_cross_vec(float omega, Vec2D r);
```

Calculates angular velocity crossed with a vector.

| Parameter | Description |
| --- | --- |
| `omega` | Angular velocity. |
| `r` | Radius or offset vector. |

**Returns:** Tangential velocity vector.

### `rohr_math_project_onto_axis`

```c
Vec2D rohr_math_project_onto_axis(Vec2D v, Axis axis);
```

Projects a vector onto an axis.

| Parameter | Description |
| --- | --- |
| `v` | Vector to project. |
| `axis` | Axis to project onto. |

**Returns:** Projected vector.

### `rohr_math_axis_magnitude`

```c
float rohr_math_axis_magnitude(Axis axis);
```

Calculates axis magnitude.

| Parameter | Description |
| --- | --- |
| `axis` | Axis vector. |

**Returns:** Magnitude.

### `rohr_math_vector_magnitude`

```c
float rohr_math_vector_magnitude(Vec2D vector);
```

Calculates vector magnitude.

| Parameter | Description |
| --- | --- |
| `vector` | Vector to inspect. |

**Returns:** Magnitude.

### `rohr_math_vector_rotate`

```c
Vec2D rohr_math_vector_rotate(Vec2D vector, float angle);
```

Rotates a vector by an angle.

| Parameter | Description |
| --- | --- |
| `vector` | Vector to rotate. |
| `angle` | Angle in radians. |

**Returns:** Rotated vector.

### `rohr_math_circle_radius`

```c
Vec1D rohr_math_circle_radius(Shape circle, Vec2D centroid);
```

Calculates a circle radius from its shape and centroid.

| Parameter | Description |
| --- | --- |
| `circle` | Circle shape. |
| `centroid` | Circle centroid. |

**Returns:** Circle radius.

### `rohr_math_vector_subtract`

```c
Vec2D rohr_math_vector_subtract(Vec2D vector_a, Vec2D vector_b);
```

Subtracts one vector from another.

| Parameter | Description |
| --- | --- |
| `vector_a` | Vector to subtract from. |
| `vector_b` | Vector to subtract. |

**Returns:** vector_a minus vector_b.

### `rohr_math_circle_overlap_depth`

```c
Vec1D rohr_math_circle_overlap_depth(Vec2D centroid_1, Vec1D radius_1, Vec2D centroid_2, Vec1D radius_2);
```

Calculates overlap depth between two circles.

| Parameter | Description |
| --- | --- |
| `centroid_1` | First circle centroid. |
| `radius_1` | First circle radius. |
| `centroid_2` | Second circle centroid. |
| `radius_2` | Second circle radius. |

**Returns:** Circle overlap depth.

### `rohr_math_projection_overlap`

```c
float rohr_math_projection_overlap(Projection projection_1, Projection projection_2);
```

Calculates overlap between two projection intervals.

| Parameter | Description |
| --- | --- |
| `projection_1` | First projection. |
| `projection_2` | Second projection. |

**Returns:** Overlap depth.

### `rohr_math_shape_scale`

```c
Shape rohr_math_shape_scale(Shape shape, float scale);
```

Scales a shape uniformly.

| Parameter | Description |
| --- | --- |
| `shape` | Shape to scale. |
| `scale` | Uniform scale value. |

**Returns:** Scaled shape.

### `rohr_math_shape_y_scale`

```c
Shape rohr_math_shape_y_scale(Shape shape, float scale);
```

Scales a shape along the y axis.

| Parameter | Description |
| --- | --- |
| `shape` | Shape to scale. |
| `scale` | Y-axis scale value. |

**Returns:** Scaled shape.

### `rohr_math_shape_x_scale`

```c
Shape rohr_math_shape_x_scale(Shape shape, float scale);
```

Scales a shape along the x axis.

| Parameter | Description |
| --- | --- |
| `shape` | Shape to scale. |
| `scale` | X-axis scale value. |

**Returns:** Scaled shape.

### `rohr_math_polygon_centroid`

```c
Vec2D rohr_math_polygon_centroid(Shape shape);
```

Calculates a polygon centroid.

| Parameter | Description |
| --- | --- |
| `shape` | Polygon shape. |

**Returns:** Centroid position.

### `rohr_math_vertex_add`

```c
Shape rohr_math_vertex_add(Shape shape);
```

Adds a vertex slot to a shape.

| Parameter | Description |
| --- | --- |
| `shape` | Shape to modify. |

**Returns:** Shape with an additional vertex slot.

### `rohr_math_vertex_delete`

```c
Shape rohr_math_vertex_delete(Shape shape);
```

Deletes the last vertex slot from a shape.

| Parameter | Description |
| --- | --- |
| `shape` | Shape to modify. |

**Returns:** Shape with one fewer vertex slot.

### `rohr_math_aabb_create`

```c
AABB rohr_math_aabb_create(Shape world_shape);
```

Creates an axis-aligned bounding box for a world-space shape.

| Parameter | Description |
| --- | --- |
| `world_shape` | World-space shape. |

**Returns:** Axis-aligned bounding box.

## Systems

### `rohr_system_tick_update`

```c
Tick rohr_system_tick_update(void);
```

Advances engine time, consumes every complete fixed tick, and deletes entities
whose time-based or tick-based lifetimes have expired.

**Returns:** Number of complete ticks consumed by this update.

### `rohr_system_physics_update`

```c
void rohr_system_physics_update(double dt);
```

Runs one physics-system update.

| Parameter | Description |
| --- | --- |
| `dt` | Simulation delta time in seconds. |

### `rohr_system_entities_past_lifetime_clean`

```c
void rohr_system_entities_past_lifetime_clean(void);
```

Deletes entities whose lifetime has expired.

## Controller Input

### `rohr_controller_key_states_update`

```c
void rohr_controller_key_states_update(KeyboardState *keyboard);
```

Updates keyboard key states for the frame.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to update. |

### `rohr_controller_key_event_add`

```c
void rohr_controller_key_event_add(KeyboardState *keyboard, KeyboardEvent key_event);
```

Adds a keyboard event to a keyboard state table.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to modify. |
| `key_event` | Keyboard event to add. |

### `rohr_controller_keyboard_event_capture`

```c
KeyboardEvent rohr_controller_keyboard_event_capture(const SDL_Event *sdl_event);
```

Converts an SDL event into a Rohr keyboard event.

| Parameter | Description |
| --- | --- |
| `sdl_event` | SDL event to inspect. |

**Returns:** KeyboardEvent derived from sdl_event.

### `rohr_controller_key_down_get`

```c
bool rohr_controller_key_down_get(const KeyboardState *keyboard, SDL_Keycode keycode);
```

Checks whether an SDL keycode is currently held or was pressed this frame.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to inspect. |
| `keycode` | SDL keycode to check. |

**Returns:** true when the key is down or pressed.

### `rohr_controller_key_pressed_get`

```c
bool rohr_controller_key_pressed_get(const KeyboardState *keyboard, SDL_Keycode keycode);
```

Checks whether an SDL keycode was pressed this frame.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to inspect. |
| `keycode` | SDL keycode to check. |

**Returns:** true when the key was pressed this frame.

### `rohr_controller_key_released_get`

```c
bool rohr_controller_key_released_get(const KeyboardState *keyboard, SDL_Keycode keycode);
```

Checks whether an SDL keycode was released this frame.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to inspect. |
| `keycode` | SDL keycode to check. |

**Returns:** true when the key was released this frame.

### `rohr_controller_axis_from_keycodes_get`

```c
Vec2D rohr_controller_axis_from_keycodes_get( const KeyboardState *keyboard, SDL_Keycode up, SDL_Keycode left, SDL_Keycode down, SDL_Keycode right );
```

Returns normalized movement input from supplied up/left/down/right SDL keycodes.

Opposing directions cancel before normalization. For example, left+right

produces zero X, and up+down produces zero Y.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to inspect. |
| `up` | SDL keycode for positive Y. |
| `left` | SDL keycode for negative X. |
| `down` | SDL keycode for negative Y. |
| `right` | SDL keycode for positive X. |

**Returns:** Direction vector from the supplied directional keys.

### `rohr_controller_wasd_axis_get`

```c
Vec2D rohr_controller_wasd_axis_get(const KeyboardState *keyboard);
```

Returns normalized movement input from W/A/S/D.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to inspect. |

**Returns:** Direction vector where W is positive Y and D is positive X.

### `rohr_controller_arrow_axis_get`

```c
Vec2D rohr_controller_arrow_axis_get(const KeyboardState *keyboard);
```

Returns normalized movement input from arrow keys.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to inspect. |

**Returns:** Direction vector where up is positive Y and right is positive X.

### `rohr_controller_default_get`

```c
Controller rohr_controller_default_get(void);
```

Returns an enabled, empty, game-owned controller.

**Returns:** Controller ready for named axes and buttons.

### `rohr_controller_wasd_default_get`

```c
Controller rohr_controller_wasd_default_get(void);
```

Returns a game-owned controller with W/A/S/D axis bindings.

**Returns:** Default enabled W/A/S/D controller.

### `rohr_controller_arrows_default_get`

```c
Controller rohr_controller_arrows_default_get(void);
```

Returns a game-owned controller with arrow-key axis bindings.

**Returns:** Default enabled arrow-key controller.

### `rohr_controller_axis_binding_set`

```c
void rohr_controller_axis_binding_set( Controller *controller, ControllerAxisBinding binding );
```

Replaces the axis mapping on a caller-owned controller.

| Parameter | Description |
| --- | --- |
| `controller` | Controller to modify. NULL is ignored. |
| `binding` | New positive/negative X/Y key mapping. |

### `rohr_controller_default_axis_get`

```c
Vec2D rohr_controller_default_axis_get( const KeyboardState *keyboard, const Controller *controller );
```

Reads a game-owned controller from shared keyboard state.

| Parameter | Description |
| --- | --- |
| `keyboard` | Shared keyboard state captured for the frame. |
| `controller` | Game-owned mapping to read. |

**Returns:** Normalized axis, or zero for NULL or disabled controllers.

### `rohr_controller_axis_add`

```c
bool rohr_controller_axis_add( Controller *controller, const char *name, ControllerAxisBinding binding );
```

 @brief Adds or replaces a named axis without allocating memory.

### `rohr_controller_button_add`

```c
bool rohr_controller_button_add( Controller *controller, const char *name, SDL_Keycode keycode );
```

 @brief Adds or replaces a named button without allocating memory.

### `rohr_controller_axis_get`

```c
Vec2D rohr_controller_axis_get( const KeyboardState *keyboard, const Controller *controller, const char *name );
```

 @brief Reads a named axis, returning zero when unavailable or disabled.

### `rohr_controller_button_down_get`

```c
bool rohr_controller_button_down_get( const KeyboardState *keyboard, const Controller *controller, const char *name );
```

 @brief Checks whether a named button is held or newly pressed.

### `rohr_controller_button_pressed_get`

```c
bool rohr_controller_button_pressed_get( const KeyboardState *keyboard, const Controller *controller, const char *name );
```

 @brief Checks whether a named button was pressed this frame.

### `rohr_controller_button_released_get`

```c
bool rohr_controller_button_released_get( const KeyboardState *keyboard, const Controller *controller, const char *name );
```

 @brief Checks whether a named button was released this frame.

### `rohr_controller_mouse_event_print`

```c
void rohr_controller_mouse_event_print(MouseEvent event);
```

Prints a mouse event for debugging.

| Parameter | Description |
| --- | --- |
| `event` | Mouse event to print. |

### `rohr_controller_mouse_states_update`

```c
void rohr_controller_mouse_states_update(MouseState *mouse);
```

Updates mouse button states for the frame.

| Parameter | Description |
| --- | --- |
| `mouse` | Mouse state table to update. |

### `rohr_controller_mouse_event_add`

```c
void rohr_controller_mouse_event_add(MouseState *mouse, MouseEvent mouse_event);
```

Adds a mouse event to a mouse state table.

| Parameter | Description |
| --- | --- |
| `mouse` | Mouse state table to modify. |
| `mouse_event` | Mouse event to add. |

### `rohr_controller_mouse_event_capture`

```c
MouseEvent rohr_controller_mouse_event_capture(const SDL_Event *sdl_event);
```

Converts an SDL event into a Rohr mouse event.

| Parameter | Description |
| --- | --- |
| `sdl_event` | SDL event to inspect. |

**Returns:** MouseEvent derived from sdl_event.

### `rohr_controller_mouse_world_position_get`

```c
Position rohr_controller_mouse_world_position_get(const MouseState *mouse);
```

Converts the current logical screen-space mouse position to world space.

| Parameter | Description |
| --- | --- |
| `mouse` | Mouse state to convert. |

**Returns:** World position under the mouse, or zero when mouse is NULL.

## Tools

### `rohr_tools_delay`

```c
void rohr_tools_delay(int seconds);
```

Delays execution for a number of seconds.

| Parameter | Description |
| --- | --- |
| `seconds` | Number of seconds to delay. |

### `rohr_tools_binary_to_string`

```c
void rohr_tools_binary_to_string(uint32_t value, char *buffer, size_t size);
```

Writes a binary string representation of a value.

| Parameter | Description |
| --- | --- |
| `value` | Value to convert. |
| `buffer` | Destination buffer. |
| `size` | Size of buffer in bytes. |

### `rohr_tools_append_string`

```c
void rohr_tools_append_string(char *src, char *dst, size_t src_size, size_t dst_size);
```

Appends one string to another using explicit buffer sizes.

| Parameter | Description |
| --- | --- |
| `src` | Source string. |
| `dst` | Destination string. |
| `src_size` | Source buffer size. |
| `dst_size` | Destination buffer size. |

### `rohr_tools_sizeof_string`

```c
uint32_t rohr_tools_sizeof_string(char *str, char delimiter);
```

Counts characters in a string until a delimiter.

| Parameter | Description |
| --- | --- |
| `str` | String to inspect. |
| `delimiter` | Delimiter that stops counting. |

**Returns:** Number of characters before delimiter.

### `rohr_tools_random_range`

```c
int rohr_tools_random_range(int min, int max);
```

Returns a random integer in a range.

| Parameter | Description |
| --- | --- |
| `min` | Minimum value. |
| `max` | Maximum value. |

**Returns:** Random integer between min and max.

### `rohr_tools_random_range_float`

```c
float rohr_tools_random_range_float(float min, float max);
```

Returns a random float in a range.

| Parameter | Description |
| --- | --- |
| `min` | Minimum value. |
| `max` | Maximum value. |

**Returns:** Random float between min and max.

## Other

### `rohr_game_state_file_load`

```c
EngineResult rohr_game_state_file_load(const char *path);
```

 Loads and merges one JSON game-state file.

### `rohr_game_state_files_load`

```c
EngineResult rohr_game_state_files_load(const char *const *paths, size_t path_count);
```

 Loads multiple JSON files with cross-file name resolution.

### `rohr_ui_button_by_name_get`

```c
UIButtonDefinitionResult rohr_ui_button_by_name_get(const char *name);
```

 @brief Finds a UI button definition loaded from JSON.

### `rohr_ui_font_by_name_get`

```c
UIFontDefinitionResult rohr_ui_font_by_name_get(const char *name);
```

 @brief Finds a UI font definition loaded from JSON.

### `rohr_ui_label_by_name_get`

```c
UILabelDefinitionResult rohr_ui_label_by_name_get(const char *name);
```

 @brief Finds a standalone UI label definition loaded from JSON.

### `rohr_ui_slider_by_name_get`

```c
UISliderDefinitionResult rohr_ui_slider_by_name_get(const char *name);
```

 @brief Finds a UI slider definition loaded from JSON.

### `rohr_game_state_file_save`

```c
EngineResult rohr_game_state_file_save(const char *path);
```

 Saves all named live entities to a JSON game-state file.

### `rohr_game_state_template_file_save`

```c
EngineResult rohr_game_state_template_file_save(const char *path);
```

 Saves retained authored definitions without expanding prototypes.

### `rohr_camera_config_default_get`

```c
CameraConfig rohr_camera_config_default_get(void);
```

 Returns defaults for a full-screen camera with unit zoom.

### `rohr_camera_create`

```c
CameraIdResult rohr_camera_create(CameraConfig config);
```

 Creates an engine-owned camera.

### `rohr_camera_destroy`

```c
EngineResult rohr_camera_destroy(CameraId camera);
```

 Destroys a non-active camera.

### `rohr_camera_active_set`

```c
EngineResult rohr_camera_active_set(CameraId camera);
```

 Selects the camera used by transforms and subsequent drawing.

### `rohr_camera_active_get`

```c
CameraId rohr_camera_active_get(void);
```

 Returns the active camera handle.

### `rohr_camera_get`

```c
CameraResult rohr_camera_get(CameraId camera);
```

 Returns an engine-owned camera value.

### `rohr_camera_set`

```c
EngineResult rohr_camera_set(CameraId camera, Camera value);
```

 Replaces a camera value and detaches it.

### `rohr_camera_attach`

```c
EngineResult rohr_camera_attach( CameraId camera, Entity entity, Vec2D position_offset, Orientation orientation_offset, bool follow_position, bool follow_orientation );
```

 Attaches a camera to an entity transform.

### `rohr_camera_detach`

```c
EngineResult rohr_camera_detach(CameraId camera);
```

 Detaches a camera while preserving its resolved transform.

### `rohr_ui_frame_begin`

```c
void rohr_ui_frame_begin(UIInput input);
```

 @brief Starts a UI frame with logical screen-space pointer input.

### `rohr_ui_button`

```c
UIButtonResult rohr_ui_button( const char *id, const TextAsset *label, UIRect bounds, const UIButtonStyle *style );
```

Draws and updates one button identified by a stable string.

| Parameter | Description |
| --- | --- |
| `label` | Optional centered label; NULL or empty draws no label. |

### `rohr_ui_label`

```c
void rohr_ui_label(const TextAsset *text, UIRect bounds);
```

 @brief Draws reusable text centered inside bounds.

### `rohr_ui_button_disabled`

```c
void rohr_ui_button_disabled(UIRect bounds, const UIButtonStyle *style);
```

 @brief Draws a disabled button that cannot capture input.

### `rohr_ui_pointer_consumed_get`

```c
bool rohr_ui_pointer_consumed_get(void);
```

 @brief Returns whether UI consumed pointer input during this frame.

### `rohr_ui_frame_end`

```c
void rohr_ui_frame_end(void);
```

 @brief Finishes the current UI frame.

### `rohr_ui_button_style_default_get`

```c
UIButtonStyle rohr_ui_button_style_default_get(void);
```

 @brief Returns the default button colors.

### `rohr_ui_slider_config_default_get`

```c
UISliderConfig rohr_ui_slider_config_default_get(void);
```

 @brief Returns the default slider configuration and 0..1 range.

### `rohr_ui_slider`

```c
UISliderResult rohr_ui_slider(const char *id, float value, const UISliderConfig *config);
```

 @brief Draws and updates a caller-owned slider value.

### `rohr_ui_slider_with_text`

```c
UISliderResult rohr_ui_slider_with_text(const char *id, float value, const UISliderConfig *config, const UISliderText *text);
```

 @brief Draws a slider with optional caller-owned label and value text.
