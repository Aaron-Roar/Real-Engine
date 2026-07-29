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

Counted prototypes overlap at their component `position` by default. An
optional `placement` object can generate initial positions from that position:

```json
"placement": {
  "type": "grid",
  "columns": 25,
  "spacing": {"x": 8, "y": 8},
  "centered": true
}
```

Supported placement types are:

- `point`: keep every copy at the prototype position; this is the default when
  `placement` is omitted
- `line`: requires a `{ "x", "y" }` `step`; `centered` is optional
- `grid`: requires positive `columns` and `{ "x", "y" }` `spacing`;
  `centered` is optional
- `circle`: requires a non-negative `radius`; optional `start_angle` is in
  radians

Placement requires the prototype to contain a `position`, except for `point`.
It only determines initial transforms and does not change physics behavior.

Generic groups are declared once at the document root and referenced by name
from entity components:

```json
{
  "groups": [{"name": "small_flies"}],
  "entities": [{
    "name": "small_fly",
    "count": 499,
    "placement": {
      "type": "grid",
      "columns": 25,
      "spacing": {"x": 8, "y": 8},
      "centered": true
    },
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
  `time_per_frame`, `ticks_per_frame`, and `start_frame`
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

Counted prototypes can vary sprite values deterministically by instance:

```json
"animated_sprite": {
  "animation": "elderfly_flying",
  "scale": {"x": 0.4, "y": 0.4},
  "time_per_frame": 0,
  "ticks_per_frame": 3,
  "start_frame": 0,
  "variation": {
    "scale": {
      "random": {
        "min": {"x": 0.3, "y": 0.3},
        "max": {"x": 0.5, "y": 0.5},
        "seed": 100
      }
    },
    "ticks_per_frame": {
      "cycle": [2, 3, 4, 5]
    },
    "start_frame": {
      "sequence": {"start": 0, "step": 1, "wrap": 12}
    }
  }
}
```

Variation supports `scale`, `time_per_frame`, `ticks_per_frame`, and
`start_frame`. Scalar fields support:

- `cycle`: choose array entries by instance index
- `sequence`: `start + step * instance`, with optional positive `wrap`
- `linear`: interpolate from `from` to `to` across the collection
- `random`: choose between `min` and `max` using an explicit unsigned `seed`

Scale supports the same generators with vector values, except vector
`sequence` does not use `wrap`. Random generation is derived independently from
the seed, field, vector axis, and instance index, so another varied field does
not perturb existing results. Generated scale values must be positive. Tick
rates and starting frames must resolve to non-negative integers, and starting
frames must exist in the referenced animation.

Saving includes named entities only. Collision reports, broad-phase grid state,
and generic groups are runtime-derived or not yet represented by schema version
1 and are not serialized. Parent/child structure is serialized from the
child-side `parent` reference, and the parent's children group is rebuilt while
loading.
