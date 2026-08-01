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
- <a href="#spatial-grid">Spatial Grid</a>
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

### `rohr_engine_update_time`

```c
void rohr_engine_update_time(void);
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

### `rohr_engine_update_tick`

```c
Tick rohr_engine_update_tick(void);
```

Updates elapsed time and consumes every complete fixed tick.

**Returns:** Number of ticks consumed by this update.

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

### `rohr_engine_poll_event`

```c
SDL_Event rohr_engine_poll_event(void);
```

Polls one SDL event.

**Returns:** SDL event value returned by the engine event poller.

### `rohr_engine_is_paused`

```c
bool rohr_engine_is_paused(void);
```

Checks whether the engine is paused.

**Returns:** true when paused, false when running.

### `rohr_engine_reset_clock`

```c
void rohr_engine_reset_clock(void);
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

### `rohr_error_default_message`

```c
const char *rohr_error_default_message(EngineError error);
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

### `rohr_error_print_stderr`

```c
void rohr_error_print_stderr(EngineError error);
```

Prints an engine error message to stderr.

| Parameter | Description |
| --- | --- |
| `error` | Error code to print. |

## Console

### `rohr_console_print_logs`

```c
void rohr_console_print_logs(void);
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

### `rohr_console_is_active`

```c
bool rohr_console_is_active(void);
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

### `rohr_entity_is_alive`

```c
bool rohr_entity_is_alive(Entity entity);
```

Checks whether an entity id currently refers to a live entity.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to inspect. |

**Returns:** true when the entity is alive, false otherwise.

### `rohr_entity_index_is_alive`

```c
bool rohr_entity_index_is_alive(EntityIndex index);
```

Checks whether an entity table index currently contains a live entity.

| Parameter | Description |
| --- | --- |
| `index` | Component table index to inspect. |

**Returns:** true when the index contains a live entity, false otherwise.

### `rohr_entity_alive_count`

```c
uint32_t rohr_entity_alive_count(void);
```

Returns the number of currently alive entities.

**Returns:** Number of alive entities.

### `rohr_entity_alive_at`

```c
EntityResult rohr_entity_alive_at(uint32_t position);
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

### `rohr_entity_from_index`

```c
EntityResult rohr_entity_from_index(EntityIndex index);
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

### `rohr_entity_find_by_name`

```c
EntityResult rohr_entity_find_by_name(const char *name);
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

### `rohr_entity_add_components`

```c
EngineResult rohr_entity_add_components(Entity entity, RohrComponentMask mask);
```

Adds components to an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to modify. |
| `mask` | Component mask to add. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_has_components`

```c
bool rohr_entity_has_components(Entity entity, RohrComponentMask components);
```

Checks whether an entity has all requested components.

| Parameter | Description |
| --- | --- |
| `entity` | Stable entity id to inspect. |
| `components` | Component mask to test. |

**Returns:** true when entity has every requested component, false otherwise.

### `rohr_entity_index_has_components`

```c
bool rohr_entity_index_has_components(EntityIndex index, RohrComponentMask components);
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

### `rohr_entity_group_find_by_name`

```c
GroupIdResult rohr_entity_group_find_by_name(const char *name);
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

### `rohr_entity_group_contains`

```c
bool rohr_entity_group_contains(GroupId group, Entity entity);
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

### `rohr_entity_delete_components`

```c
EngineResult rohr_entity_delete_components(Entity entity, RohrComponentMask mask);
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

### `rohr_entity_remove_parent`

```c
EngineResult rohr_entity_remove_parent(Entity child);
```

Removes the parent relationship from an entity.

| Parameter | Description |
| --- | --- |
| `child` | Child entity id. |

**Returns:** EngineResult describing success or failure.

### `rohr_entity_remove_child`

```c
EngineResult rohr_entity_remove_child(Entity parent, Entity child);
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

### `rohr_entity_remove_life_time`

```c
EngineResult rohr_entity_remove_life_time(Entity entity);
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

### `rohr_physics_use_engine_time_per_tick`

```c
void rohr_physics_use_engine_time_per_tick(void);
```

 Restores the engine time-per-tick default.

### `rohr_physics_update`

```c
void rohr_physics_update(Tick ticks);
```

 Advances physics using the supplied number of elapsed engine ticks.

### `rohr_physics_update_dt`

```c
void rohr_physics_update_dt(Time dt);
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

### `rohr_physics_sat_collision`

```c
Collision rohr_physics_sat_collision(Shape shape_1, Shape shape_2);
```

Tests two shapes with separating axis theorem collision detection.

| Parameter | Description |
| --- | --- |
| `shape_1` | First shape. |
| `shape_2` | Second shape. |

**Returns:** Collision information.

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

### `rohr_physics_entity_is_held`

```c
bool rohr_physics_entity_is_held(EntityIndex index);
```

Checks whether an entity index has HOLD.

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

### `rohr_physics_stop_entity`

```c
EngineResult rohr_physics_stop_entity(Entity entity);
```

Sets an entity velocity to zero.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_group_stop_entities`

```c
EngineResult rohr_physics_group_stop_entities(GroupId group);
```

Sets velocity to zero for every live entity in a group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to update. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_apply_impulse`

```c
EngineResult rohr_physics_apply_impulse(Entity entity, Vec2D impulse);
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

### `rohr_physics_force_set`

```c
EntityResult rohr_physics_force_set(Entity entity, Force f);
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

### `rohr_physics_apply_force_for_one_tick`

```c
EngineResult rohr_physics_apply_force_for_one_tick(Entity entity, Force f);
```

Applies force to an entity for one physics tick.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to target. |
| `f` | Force vector. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_torque_set`

```c
EntityResult rohr_physics_torque_set(Entity entity, Torque t);
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

### `rohr_physics_apply_torque_for_one_tick`

```c
EngineResult rohr_physics_apply_torque_for_one_tick(Entity entity, Torque t);
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

Sets an entity hitbox component value.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `hitbox` | Local-space hitbox shape. |

**Returns:** EngineResult describing success or failure.

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

### `rohr_physics_hold_entity`

```c
EngineResult rohr_physics_hold_entity(Entity entity);
```

Adds HOLD so physics update stages preserve current values.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_unhold_entity`

```c
EngineResult rohr_physics_unhold_entity(Entity entity);
```

Removes HOLD without changing STATIC or DYNAMIC state.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_group_hold_entities`

```c
EngineResult rohr_physics_group_hold_entities(GroupId group);
```

Adds HOLD to every live entity in a group.

| Parameter | Description |
| --- | --- |
| `group` | Group id to update. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_group_unhold_entities`

```c
EngineResult rohr_physics_group_unhold_entities(GroupId group);
```

Removes HOLD from every live entity in a group.

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

### `rohr_physics_remove_transform_lock`

```c
EngineResult rohr_physics_remove_transform_lock(Entity entity);
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

### `rohr_physics_joint_set`

```c
EntityResult rohr_physics_joint_set( Entity a, Entity b, JointType type, Vec2D local_anchor_a, Vec2D local_anchor_b, float stiffness, float damping );
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

### `rohr_physics_particle_collision`

```c
Collision rohr_physics_particle_collision(Shape shape_1, Shape shape_2);
```

Tests two particle shapes for collision.

| Parameter | Description |
| --- | --- |
| `shape_1` | First shape. |
| `shape_2` | Second shape. |

**Returns:** Collision information.

### `rohr_physics_collision_report_set`

```c
EngineResult rohr_physics_collision_report_set(Entity entity, Entity target, bool state);
```

Enables or disables collision reporting between two entities.

| Parameter | Description |
| --- | --- |
| `entity` | Reporting entity. |
| `target` | Target entity. |
| `state` | true to enable reporting, false to disable it. |

**Returns:** EngineResult describing success or failure.

### `rohr_physics_collision_report_get`

```c
bool rohr_physics_collision_report_get(Entity entity, Entity target);
```

Checks whether a collision report exists between two entities.

| Parameter | Description |
| --- | --- |
| `entity` | Reporting entity. |
| `target` | Target entity. |

**Returns:** true when reporting is enabled, false otherwise.

## Graphics

### `rohr_graphics_create_color_hex`

```c
Color rohr_graphics_create_color_hex(uint32_t hex_color_code);
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

### `rohr_graphics_poll_events`

```c
bool rohr_graphics_poll_events(SDL_Event *event);
```

Polls graphics/window events.

| Parameter | Description |
| --- | --- |
| `event` | Destination for the SDL event. |

**Returns:** true when an event was read, false otherwise.

### `rohr_graphics_draw_background`

```c
void rohr_graphics_draw_background(Color color);
```

Draws the frame background.

| Parameter | Description |
| --- | --- |
| `color` | Background color. |

### `rohr_graphics_draw_screen_rect`

```c
bool rohr_graphics_draw_screen_rect(float x, float y, float width, float height, Color color);
```

Draws a filled rectangle in logical screen coordinates.

**Returns:** true when SDL accepted the draw command.

### `rohr_graphics_draw_screen_quad`

```c
bool rohr_graphics_draw_screen_quad(Position center, float width, float height, float angle, Color color);
```

 @brief Draws a centered rotated rectangle in logical screen space.

### `rohr_graphics_show`

```c
void rohr_graphics_show(void);
```

Presents the current graphics frame.

### `rohr_graphics_draw_hit_box`

```c
void rohr_graphics_draw_hit_box(Entity entity, Fill fill_type);
```

Draws one entity hitbox.

| Parameter | Description |
| --- | --- |
| `entity` | Entity whose hitbox should be drawn. |
| `fill_type` | Fill mode for drawing. |

### `rohr_graphics_draw_hit_box_colored`

```c
void rohr_graphics_draw_hit_box_colored(Entity entity, Fill fill_type, Color color);
```

Draws one entity hitbox with a caller supplied color.

| Parameter | Description |
| --- | --- |
| `entity` | Entity whose hitbox should be drawn. |
| `fill_type` | Fill mode for drawing. |
| `color` | Color to draw with. |

### `rohr_graphics_draw_hit_boxes`

```c
void rohr_graphics_draw_hit_boxes(void);
```

Draws hitboxes for all renderable hitbox entities.

### `rohr_graphics_load_texture`

```c
TextureAssetResult rohr_graphics_load_texture(TextureDescriptor text_desc);
```

Loads a texture asset.

| Parameter | Description |
| --- | --- |
| `text_desc` | Texture descriptor containing load settings. |

**Returns:** TextureAssetResult containing the asset, or an error.

### `rohr_graphics_load_font`

```c
FontAssetResult rohr_graphics_load_font(FontDescriptor descriptor);
```

 @brief Loads a caller-owned font asset.

### `rohr_graphics_destroy_font`

```c
void rohr_graphics_destroy_font(FontAsset *font);
```

 @brief Destroys a font after its text assets have been destroyed.

### `rohr_graphics_create_text`

```c
TextAssetResult rohr_graphics_create_text(const FontAsset *font, const char *value, Color color);
```

 @brief Creates reusable caller-owned text.

### `rohr_graphics_destroy_text`

```c
void rohr_graphics_destroy_text(TextAsset *text);
```

 @brief Destroys reusable text.

### `rohr_graphics_draw_text`

```c
bool rohr_graphics_draw_text(const TextAsset *text, Position position);
```

 @brief Draws text in logical screen coordinates.

### `rohr_graphics_load_animation`

```c
AnimationAssetResult rohr_graphics_load_animation(AnimationDescriptor anim_desc);
```

Loads an animation asset.

| Parameter | Description |
| --- | --- |
| `anim_desc` | Animation descriptor containing load settings. |

**Returns:** AnimationAssetResult containing the asset, or an error.

### `rohr_graphics_create_animated_sprite`

```c
AnimatedSprite rohr_graphics_create_animated_sprite(AnimationAsset asset_ptr, Scale scale);
```

Creates an animated sprite from an animation asset.

| Parameter | Description |
| --- | --- |
| `asset_ptr` | Animation asset to use. |
| `scale` | Sprite scale. |

**Returns:** Animated sprite value.

### `rohr_graphics_add_animated_sprite`

```c
EngineResult rohr_graphics_add_animated_sprite(Entity entity, AnimatedSprite sprite);
```

Adds an animated sprite to an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `sprite` | Animated sprite component value. |

**Returns:** EngineResult describing success or failure.

### `rohr_graphics_draw_animated_sprites`

```c
void rohr_graphics_draw_animated_sprites(void);
```

Draws all animated sprite components.

### `rohr_graphics_update_sprite_frames`

```c
void rohr_graphics_update_sprite_frames(Tick current_tick, Time current_time);
```

Updates animated sprite frames.

| Parameter | Description |
| --- | --- |
| `current_tick` | Current engine tick. |
| `current_time` | Current engine time. |

### `rohr_graphics_scale_textures`

```c
void rohr_graphics_scale_textures(Entity entity, Scale scale);
```

Scales textures attached to an entity.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to modify. |
| `scale` | Scale value. |

### `rohr_graphics_active_camera_set`

```c
void rohr_graphics_active_camera_set(Camera camera);
```

Replaces the active camera transform.

| Parameter | Description |
| --- | --- |
| `camera` | World-space camera position and orientation. |

### `rohr_graphics_active_camera_get`

```c
Camera rohr_graphics_active_camera_get(void);
```

Returns the active camera transform.

**Returns:** Current world-space camera transform.

### `rohr_graphics_move_camera`

```c
void rohr_graphics_move_camera(Vec2D translation);
```

Translates the active camera in world space.

| Parameter | Description |
| --- | --- |
| `translation` | World-space translation to add. |

### `rohr_graphics_rotate_camera`

```c
void rohr_graphics_rotate_camera(Orientation radians);
```

Rotates the active camera counterclockwise.

| Parameter | Description |
| --- | --- |
| `radians` | Rotation in radians to add. |

### `rohr_graphics_attach_camera`

```c
EngineResult rohr_graphics_attach_camera( Entity entity, Vec2D position_offset, Orientation orientation_offset );
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

### `rohr_graphics_attach_camera_with_options`

```c
EngineResult rohr_graphics_attach_camera_with_options( Entity entity, Vec2D position_offset, Orientation orientation_offset, bool follow_position, bool follow_orientation );
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

### `rohr_graphics_detach_camera`

```c
void rohr_graphics_detach_camera(void);
```

Detaches the camera and preserves its current world transform.

### `rohr_graphics_camera_is_attached`

```c
bool rohr_graphics_camera_is_attached(void);
```

Reports whether the camera is attached to a live entity transform.

**Returns:** true when attached to a valid entity transform.

### `rohr_graphics_camera_attachment_get`

```c
CameraAttachmentResult rohr_graphics_camera_attachment_get(void);
```

Returns the active camera attachment description.

**Returns:** CameraAttachmentResult containing the attachment or a missing-component error.

### `rohr_graphics_world_to_screen`

```c
Position rohr_graphics_world_to_screen(Position pos);
```

Converts a world position to screen coordinates.

| Parameter | Description |
| --- | --- |
| `pos` | World position. |

**Returns:** Screen-space position.

### `rohr_graphics_screen_to_world`

```c
Position rohr_graphics_screen_to_world(Position screen);
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

### `rohr_graphics_draw_grid`

```c
void rohr_graphics_draw_grid(void);
```

Draws the spatial grid overlay.

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

### `rohr_graphics_draw_particles`

```c
void rohr_graphics_draw_particles(void);
```

Draws active particle components.

### `rohr_graphics_draw_local_origins`

```c
void rohr_graphics_draw_local_origins(void);
```

Draws local origin markers for entities.

## Math

### `rohr_math_create_normals`

```c
Vec2DList rohr_math_create_normals(Shape shape);
```

Creates normalized edge normals for a shape.

| Parameter | Description |
| --- | --- |
| `shape` | Shape to inspect. |

**Returns:** List of normal vectors.

### `rohr_math_normalize_vector`

```c
Vec2D rohr_math_normalize_vector(Vec2D vector);
```

Normalizes a vector.

| Parameter | Description |
| --- | --- |
| `vector` | Vector to normalize. |

**Returns:** Normalized vector.

### `rohr_math_normalize_vectors`

```c
Vec2DList rohr_math_normalize_vectors(Vec2DList vectors);
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

### `rohr_math_create_square`

```c
Shape rohr_math_create_square(float width, float height);
```

Creates a rectangular polygon shape.

| Parameter | Description |
| --- | --- |
| `width` | Rectangle width. |
| `height` | Rectangle height. |

**Returns:** Shape containing rectangle vertices.

### `rohr_math_create_circle`

```c
Shape rohr_math_create_circle(float radius, uint8_t verticies);
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

### `rohr_math_rotate_vector`

```c
Vec2D rohr_math_rotate_vector(Vec2D vector, float angle);
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

### `rohr_math_scale_shape`

```c
Shape rohr_math_scale_shape(Shape shape, float scale);
```

Scales a shape uniformly.

| Parameter | Description |
| --- | --- |
| `shape` | Shape to scale. |
| `scale` | Uniform scale value. |

**Returns:** Scaled shape.

### `rohr_math_scale_shape_y`

```c
Shape rohr_math_scale_shape_y(Shape shape, float scale);
```

Scales a shape along the y axis.

| Parameter | Description |
| --- | --- |
| `shape` | Shape to scale. |
| `scale` | Y-axis scale value. |

**Returns:** Scaled shape.

### `rohr_math_scale_shape_x`

```c
Shape rohr_math_scale_shape_x(Shape shape, float scale);
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

### `rohr_math_add_vertex`

```c
Shape rohr_math_add_vertex(Shape shape);
```

Adds a vertex slot to a shape.

| Parameter | Description |
| --- | --- |
| `shape` | Shape to modify. |

**Returns:** Shape with an additional vertex slot.

### `rohr_math_delete_vertex`

```c
Shape rohr_math_delete_vertex(Shape shape);
```

Deletes the last vertex slot from a shape.

| Parameter | Description |
| --- | --- |
| `shape` | Shape to modify. |

**Returns:** Shape with one fewer vertex slot.

### `rohr_math_create_aabb`

```c
AABB rohr_math_create_aabb(Shape world_shape);
```

Creates an axis-aligned bounding box for a world-space shape.

| Parameter | Description |
| --- | --- |
| `world_shape` | World-space shape. |

**Returns:** Axis-aligned bounding box.

## Systems

### `rohr_system_update_physics`

```c
void rohr_system_update_physics(double dt);
```

Runs one physics-system update.

| Parameter | Description |
| --- | --- |
| `dt` | Simulation delta time in seconds. |

### `rohr_system_clean_entities_past_lifetime`

```c
void rohr_system_clean_entities_past_lifetime(void);
```

Deletes entities whose lifetime has expired.

## Controller Input

### `rohr_controller_update_key_states`

```c
void rohr_controller_update_key_states(KeyboardState *keyboard);
```

Updates keyboard key states for the frame.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to update. |

### `rohr_controller_add_key_event`

```c
void rohr_controller_add_key_event(KeyboardState *keyboard, KeyboardEvent key_event);
```

Adds a keyboard event to a keyboard state table.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to modify. |
| `key_event` | Keyboard event to add. |

### `rohr_controller_capture_keyboard_event`

```c
KeyboardEvent rohr_controller_capture_keyboard_event(const SDL_Event *sdl_event);
```

Converts an SDL event into a Rohr keyboard event.

| Parameter | Description |
| --- | --- |
| `sdl_event` | SDL event to inspect. |

**Returns:** KeyboardEvent derived from sdl_event.

### `rohr_controller_key_down`

```c
bool rohr_controller_key_down(const KeyboardState *keyboard, SDL_Keycode keycode);
```

Checks whether an SDL keycode is currently held or was pressed this frame.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to inspect. |
| `keycode` | SDL keycode to check. |

**Returns:** true when the key is down or pressed.

### `rohr_controller_key_pressed`

```c
bool rohr_controller_key_pressed(const KeyboardState *keyboard, SDL_Keycode keycode);
```

Checks whether an SDL keycode was pressed this frame.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to inspect. |
| `keycode` | SDL keycode to check. |

**Returns:** true when the key was pressed this frame.

### `rohr_controller_key_released`

```c
bool rohr_controller_key_released(const KeyboardState *keyboard, SDL_Keycode keycode);
```

Checks whether an SDL keycode was released this frame.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to inspect. |
| `keycode` | SDL keycode to check. |

**Returns:** true when the key was released this frame.

### `rohr_controller_axis_from_keycodes`

```c
Vec2D rohr_controller_axis_from_keycodes( const KeyboardState *keyboard, SDL_Keycode up, SDL_Keycode left, SDL_Keycode down, SDL_Keycode right );
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

### `rohr_controller_wasd_axis`

```c
Vec2D rohr_controller_wasd_axis(const KeyboardState *keyboard);
```

Returns normalized movement input from W/A/S/D.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to inspect. |

**Returns:** Direction vector where W is positive Y and D is positive X.

### `rohr_controller_arrow_axis`

```c
Vec2D rohr_controller_arrow_axis(const KeyboardState *keyboard);
```

Returns normalized movement input from arrow keys.

| Parameter | Description |
| --- | --- |
| `keyboard` | Keyboard state table to inspect. |

**Returns:** Direction vector where up is positive Y and right is positive X.

### `rohr_controller_default`

```c
Controller rohr_controller_default(void);
```

Returns an enabled, empty, game-owned controller.

**Returns:** Controller ready for named axes and buttons.

### `rohr_controller_default_wasd`

```c
Controller rohr_controller_default_wasd(void);
```

Returns a game-owned controller with W/A/S/D axis bindings.

**Returns:** Default enabled W/A/S/D controller.

### `rohr_controller_default_arrows`

```c
Controller rohr_controller_default_arrows(void);
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

### `rohr_controller_axis`

```c
Vec2D rohr_controller_axis( const KeyboardState *keyboard, const Controller *controller );
```

Reads a game-owned controller from shared keyboard state.

| Parameter | Description |
| --- | --- |
| `keyboard` | Shared keyboard state captured for the frame. |
| `controller` | Game-owned mapping to read. |

**Returns:** Normalized axis, or zero for NULL or disabled controllers.

### `rohr_controller_add_axis`

```c
bool rohr_controller_add_axis( Controller *controller, const char *name, ControllerAxisBinding binding );
```

 @brief Adds or replaces a named axis without allocating memory.

### `rohr_controller_add_button`

```c
bool rohr_controller_add_button( Controller *controller, const char *name, SDL_Keycode keycode );
```

 @brief Adds or replaces a named button without allocating memory.

### `rohr_controller_axis_get`

```c
Vec2D rohr_controller_axis_get( const KeyboardState *keyboard, const Controller *controller, const char *name );
```

 @brief Reads a named axis, returning zero when unavailable or disabled.

### `rohr_controller_button_down`

```c
bool rohr_controller_button_down( const KeyboardState *keyboard, const Controller *controller, const char *name );
```

 @brief Checks whether a named button is held or newly pressed.

### `rohr_controller_button_pressed`

```c
bool rohr_controller_button_pressed( const KeyboardState *keyboard, const Controller *controller, const char *name );
```

 @brief Checks whether a named button was pressed this frame.

### `rohr_controller_button_released`

```c
bool rohr_controller_button_released( const KeyboardState *keyboard, const Controller *controller, const char *name );
```

 @brief Checks whether a named button was released this frame.

### `rohr_controller_print_mouse_event`

```c
void rohr_controller_print_mouse_event(MouseEvent event);
```

Prints a mouse event for debugging.

| Parameter | Description |
| --- | --- |
| `event` | Mouse event to print. |

### `rohr_controller_update_mouse_states`

```c
void rohr_controller_update_mouse_states(MouseState *mouse);
```

Updates mouse button states for the frame.

| Parameter | Description |
| --- | --- |
| `mouse` | Mouse state table to update. |

### `rohr_controller_add_mouse_event`

```c
void rohr_controller_add_mouse_event(MouseState *mouse, MouseEvent mouse_event);
```

Adds a mouse event to a mouse state table.

| Parameter | Description |
| --- | --- |
| `mouse` | Mouse state table to modify. |
| `mouse_event` | Mouse event to add. |

### `rohr_controller_capture_mouse_event`

```c
MouseEvent rohr_controller_capture_mouse_event(const SDL_Event *sdl_event);
```

Converts an SDL event into a Rohr mouse event.

| Parameter | Description |
| --- | --- |
| `sdl_event` | SDL event to inspect. |

**Returns:** MouseEvent derived from sdl_event.

### `rohr_controller_mouse_world_position`

```c
Position rohr_controller_mouse_world_position(const MouseState *mouse);
```

Converts the current logical screen-space mouse position to world space.

| Parameter | Description |
| --- | --- |
| `mouse` | Mouse state to convert. |

**Returns:** World position under the mouse, or zero when mouse is NULL.

## Spatial Grid

### `rohr_grid_add_entity_to_grids`

```c
void rohr_grid_add_entity_to_grids(Entity entity);
```

Adds an entity to the spatial grid tables.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to add. |

### `rohr_grid_checked_pair`

```c
bool rohr_grid_checked_pair(Entity entity_1, Entity entity_2);
```

Checks whether a pair of entities has already been processed.

| Parameter | Description |
| --- | --- |
| `entity_1` | First entity. |
| `entity_2` | Second entity. |

**Returns:** true when the pair was already checked, false otherwise.

### `rohr_grid_add_pair`

```c
void rohr_grid_add_pair(Entity entity_1, Entity entity_2);
```

Stores a processed entity pair.

| Parameter | Description |
| --- | --- |
| `entity_1` | First entity. |
| `entity_2` | Second entity. |

### `rohr_grid_clear`

```c
void rohr_grid_clear(void);
```

Clears spatial grid state.

### `rohr_grid_update_aabb`

```c
void rohr_grid_update_aabb(Entity entity);
```

Updates an entity axis-aligned bounding box in the grid.

| Parameter | Description |
| --- | --- |
| `entity` | Entity to update. |

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

### `rohr_game_state_load_file`

```c
EngineResult rohr_game_state_load_file(const char *path);
```

 Loads and merges one JSON game-state file.

### `rohr_game_state_load_files`

```c
EngineResult rohr_game_state_load_files(const char *const *paths, size_t path_count);
```

 Loads multiple JSON files with cross-file name resolution.

### `rohr_ui_button_find_by_name`

```c
UIButtonDefinitionResult rohr_ui_button_find_by_name(const char *name);
```

 @brief Finds a UI button definition loaded from JSON.

### `rohr_ui_font_find_by_name`

```c
UIFontDefinitionResult rohr_ui_font_find_by_name(const char *name);
```

 @brief Finds a UI font definition loaded from JSON.

### `rohr_ui_label_find_by_name`

```c
UILabelDefinitionResult rohr_ui_label_find_by_name(const char *name);
```

 @brief Finds a standalone UI label definition loaded from JSON.

### `rohr_ui_slider_find_by_name`

```c
UISliderDefinitionResult rohr_ui_slider_find_by_name(const char *name);
```

 @brief Finds a UI slider definition loaded from JSON.

### `rohr_game_state_save_file`

```c
EngineResult rohr_game_state_save_file(const char *path);
```

 Saves all named live entities to a JSON game-state file.

### `rohr_game_state_save_template_file`

```c
EngineResult rohr_game_state_save_template_file(const char *path);
```

 Saves retained authored definitions without expanding prototypes.

### `rohr_camera_default_config`

```c
CameraConfig rohr_camera_default_config(void);
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

### `rohr_ui_begin_frame`

```c
void rohr_ui_begin_frame(UIInput input);
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

### `rohr_ui_pointer_consumed`

```c
bool rohr_ui_pointer_consumed(void);
```

 @brief Returns whether UI consumed pointer input during this frame.

### `rohr_ui_end_frame`

```c
void rohr_ui_end_frame(void);
```

 @brief Finishes the current UI frame.

### `rohr_ui_default_button_style`

```c
UIButtonStyle rohr_ui_default_button_style(void);
```

 @brief Returns the default button colors.

### `rohr_ui_default_slider_config`

```c
UISliderConfig rohr_ui_default_slider_config(void);
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
