# JSON game state

Game state uses yyjson and schema version `1`. A state may be split across
multiple files. `game_state_load_files()` registers every entity name before
loading component values, so a relationship may refer to an entity in any file
in the same call.

```json
{
  "version": 1,
  "entities": [
    {
      "name": "player",
      "components": {
        "flags": ["dynamic", "collision"],
        "position": {"x": 40, "y": 60},
        "mass": 4,
        "target": "goal"
      }
    }
  ]
}
```

Names are unique among live entities and may contain at most 63 bytes. The
loader accepts JSON comments and trailing commas for handcrafted files.

`components.mask` is an optional numeric `CMask`. `flags` is the more readable
form for `static`, `dynamic`, `collision`, `targetable`, `particle`, and `hold`.
When saving, both the numeric mask and readable flags are emitted.

Supported value keys are:

- `position`, `velocity`, `acceleration`, and `force`: `{ "x", "y" }`
- `mass`, `orientation`, `angular_velocity`, `angular_acceleration`, `torque`,
  `friction`, and `restitution`: numbers
- `hit_box`: an array of 3 to 50 `{ "x", "y" }` vertices
- `target` and `parent`: entity-name strings
- `lifetime`: `{ "time", "tick" }`
- `angle_lock`: `{ "min", "max" }`
- `axis_lock`: `{ "axis": {"x", "y"}, "point": {"x", "y"} }`
- `transform_lock`: `driver`, `local_offset`, `local_angle`, `lock_position`,
  `lock_orientation`, and `inherit_velocity`
- `joint`: named `a` and `b` entities, `type` (`distance`, `weld`, or `pin`),
  anchors, rest values, stiffness, damping, and angular settings

Saving includes named entities only. Collision reports, broad-phase grid state,
generic groups, and loaded SDL graphics assets are runtime-derived,
process-local, or not yet represented by schema version 1 and are not
serialized. Parent/child structure is serialized from the child-side `parent`
reference, and the parent's children group is rebuilt while loading.
