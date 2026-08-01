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

## User interface definitions

Fonts, standalone labels, and immediate-mode buttons can be authored under
`ui`:

```json
"ui": {
  "fonts": [{
    "name": "menu_font",
    "file": "assets/fonts/menu.ttf",
    "point_size": 24
  }],
  "labels": [{
    "name": "menu_title",
    "text": "Main Menu",
    "font": "menu_font",
    "color": {"red": 255, "green": 255, "blue": 255, "alpha": 255},
    "bounds": {"x": 0, "y": 30, "width": 640, "height": 50}
  }],
  "buttons": [{
    "name": "settings_button",
    "label": "Settings",
    "font": "menu_font",
    "text_color": {"red": 255, "green": 255, "blue": 255, "alpha": 255},
    "bounds": {
      "x": 220,
      "y": 205,
      "width": 200,
      "height": 55
    },
    "style": {
      "hovered": {"red": 95, "green": 80, "blue": 145, "alpha": 255}
    }
  }]
}
```

Font definitions provide a unique name, font-file path, and positive point
size. They are retrieved with `ui_font_find_by_name()`; the game explicitly
loads and owns the resulting `FontAsset`.

Standalone labels have a unique name, text, named font, color, and bounds. They
are retrieved with `ui_label_find_by_name()`. This data is independent from
buttons and can be drawn with `ui_label()` after the game creates its text
asset.

Button `name` identifies the authored definition for
`ui_button_find_by_name()` and is its default stable runtime interaction ID.
Pass that name to `ui_button()` for ordinary use, or supply a different runtime
ID when instantiating one definition more than once. `label` is optional and defaults to an empty string. A
non-empty button label requires a valid named `font`; `text_color` is optional
and defaults to opaque white. Bounds use logical screen coordinates and require
positive width and height.

The `style` object and each of its `idle`, `hovered`, `pressed`, and `disabled`
colors are optional. Omitted colors use the default button style. Each supplied
color requires unsigned `red`, `green`, `blue`, and `alpha` values from 0 to
255.

Loading a definition does not draw it or handle its click. The game retrieves
the definition, creates any desired text asset, calls `ui_button()` while the
menu is active, and handles the returned interaction result. Runtime and
template saves preserve loaded font, label, and button definitions.

Sliders are authored independently from their runtime values:

```json
"sliders": [{
  "name": "volume_slider",
  "center": {"x": 320, "y": 280},
  "length": 220,
  "angle": 0.25,
  "range": {"min": -100, "max": 100},
  "step": 5,
  "initial_value": 0,
  "label": "Volume",
  "font": "menu_font",
  "value_format": "%.0f%%",
  "text_color": {"red": 240, "green": 244, "blue": 250, "alpha": 255},
  "style": {
    "track": {"red": 50, "green": 58, "blue": 72, "alpha": 255},
    "fill": {"red": 100, "green": 140, "blue": 230, "alpha": 255},
    "track_thickness": 10,
    "handle_width": 18,
    "handle_height": 32
  }
}]
```

Slider `name` is both the key used by `ui_slider_find_by_name()` and the
ordinary runtime ID passed to `ui_slider()` or `ui_slider_with_text()`. A game
may still pass a different runtime ID when reusing one definition for multiple
live sliders.

`center` and positive `length` are required. `angle` is optional and defaults
to zero; it is measured counterclockwise in logical screen-space radians. The
range defaults to `0..1` when omitted. Explicit endpoints may be positive,
negative, or descending, but cannot be equal. `initial_value` defaults to the
first range endpoint and must fall between the two endpoints.

`step` is optional and defaults to zero, which gives continuous movement. A
positive step snaps values relative to the first range endpoint. Step controls
are implied by `style.step_button_size`: omission or zero disables them, while
a positive size enables them when `step` is positive. The minus control is
placed at the numerically lower end and the plus control at the higher end.
Their screen sides therefore reverse automatically for a descending range.

`label`, `font`, `value_format`, and `text_color` describe optional slider
text. A non-empty label or value format requires a valid named font. Text
assets remain owned by the game: pass prepared label/value and optional `-`/`+`
assets through `ui_slider_with_text()`. Recreate the formatted value asset only
when `UISliderResult.changed` is true. `NULL` assets omit the corresponding
automatic text.

Slider colors and sizes are optional and inherit the engine defaults. Supported
colors are `track`, `fill`, `handle`, `handle_hovered`, and `handle_pressed`.
Supported positive sizes are `track_thickness`, `handle_width`,
`handle_height`, and `step_button_size`; `step_button_size` defaults to zero,
and `step_button_gap` may always be zero. The game
retrieves definitions with
`ui_slider_find_by_name()`, retains the current value, and passes that value
back to `ui_slider()` each frame.

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

Saving includes named entities and named generic groups. Collision reports and
broad-phase grid state are runtime-derived and are not serialized. Parent/child
structure is serialized from the child-side `parent` reference, and the
parent's children group is rebuilt while loading.

## Runtime saves and authored templates

`game_state_save_file()` writes an exact runtime-oriented save. Counted
prototypes are expanded so independently changed positions, velocities,
animation values, and relationships are not lost.

`game_state_save_template_file()` writes the immutable authored definitions
retained from every successfully loaded state document. It merges named groups,
animation assets, and entity descriptions while preserving `count`,
`placement`, and `variation`. It deliberately does not capture runtime
mutations or entities created only through C APIs.

Retained template documents are owned by the engine state module and released
by `engine_shutdown()`. Template saving returns an error when no state document
has been loaded. Loading more than 64 retained documents in one engine session
returns `ERROR_ENGINE_STATE_TEMPLATE_DOCUMENT_LIMIT_EXCEEDED`. The public
`GAME_STATE_MAX_TEMPLATE_DOCUMENTS` constant exposes this capacity.

Duplicate names in the asset catalog return
`ERROR_ENGINE_STATE_DUPLICATE_ASSET_DEFINITION`. An entity component referring
to an animation or other named asset that was not defined returns
`ERROR_ENGINE_STATE_ASSET_REFERENCE_NOT_FOUND`.

## Cameras

A state may attach the active camera to one named entity:

```json
"camera": {
  "attachment": {
    "entity": "player",
    "position_offset": {"x": 0, "y": 20},
    "orientation_offset": 0,
    "follow_position": true,
    "follow_orientation": false
  }
}
```

The entity may be declared in any file passed to the same
`game_state_load_files()` call. It must have both a position and orientation.
`follow_position` and `follow_orientation` are optional and default to `true`.
They independently control which parts of the entity transform are inherited.
When orientation is followed, `position_offset` is in entity-local space and
rotates with the entity. Otherwise it is world-space. When position is not
followed, `position_offset` is the fixed camera world position. Likewise,
`orientation_offset` is added to the entity orientation when it is followed,
and becomes the fixed world orientation when it is not.

A standalone camera uses an explicit world-space transform:

```json
"camera": {
  "transform": {
    "position": {"x": 320, "y": 180},
    "orientation": 0.5
  }
}
```

`attachment` and `transform` are mutually exclusive. Loading a standalone
transform detaches any currently followed entity.

Only one retained input document per engine session may declare a camera;
additional declarations are rejected as ambiguous.

Runtime saves emit either the active named attachment or the current standalone
transform. Compact template saves preserve the authored camera declaration.
