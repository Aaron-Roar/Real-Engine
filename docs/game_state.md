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
      "count": 1,
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

An entity description may use `count` to create multiple copies from one
component prototype. The first copy keeps the base name and later copies use
`_1`, `_2`, and so on. If a generated name already exists, the loader advances
the numeric suffix until it finds a unique name.

Generic groups are declared once at the document root and referenced by name
from entity components:

```json
{
  "groups": [{"name": "small_flies"}],
  "entities": [{
    "name": "small_fly",
    "count": 499,
    "components": {
      "groups": ["small_flies"]
    }
  }]
}
```

Group names are unique among live generic groups and may contain at most 63
bytes. Parent-owned child groups remain internal and cannot be named through
this schema.

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
- `animated_sprite`: a named `animation`, per-entity `scale`, and
  `time_per_frame`
- `groups`: an array of named generic groups

Animations are named once in the top-level asset catalog. State loading must
occur after `graphics_start()` when a file contains animation assets, because
the engine creates SDL textures while connecting the state.

```json
"assets": {
  "animations": [{
    "name": "elderfly_flying",
    "ticks_per_frame": 0,
    "time_per_frame": 0.05,
    "frames": [{
      "file": "assets/elderfly/flying/f1.png",
      "size": {"x": 50, "y": 50}
    }]
  }]
}
```

Saving includes named entities only. Collision reports, broad-phase grid state,
and generic groups are runtime-derived or not yet represented by schema version
1 and are not serialized. Parent/child structure is serialized from the
child-side `parent` reference, and the parent's children group is rebuilt while
loading.
