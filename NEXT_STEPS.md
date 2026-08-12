# Next Steps

The main direction is to make Rohr Engine the runtime beneath a C authoring
tool. Rohr Editor will let developers author entities, components, assets,
levels, and UI, then generate ordinary C configuration that is compiled with
the engine and handwritten game logic. The editor remains optional: every
generated feature must also be available through the direct public C API.

## 1. Stable Configuration and Build API

Define plain-C configuration structures and generic builders for entities,
components, assets, scenes, and UI. These structures are the shared contract
used by handwritten games, editor previews, generated C, JSON import/export,
and tests.

- Direct engine APIs are the primary implementation path.
- JSON only serializes features that the direct C API already supports.
- Keep configuration, runtime handles, owned assets, and per-frame results
  distinct and consistently named.
- Prefer static configuration arrays over generating thousands of arbitrary
  engine calls.
- Keep ownership, destruction, component dependencies, and failure cleanup
  explicit.
- Ensure generated and handwritten configurations produce identical runtime
  behavior.

## 2. Generated C Authoring Pipeline

Build a small deterministic command-line generator before rebuilding the
graphical editor. It should read an editor project model, validate references,
and emit clearly marked `.generated.c` and `.generated.h` files containing
static engine configuration.

- Generated files must never overwrite handwritten game code.
- Repeated generation without project changes must produce identical output.
- Escape C identifiers, strings, and file paths safely.
- Report generation errors in terms of the authored project data.
- Compile generated sources with the existing Makefile, Rohr Engine, and
  handwritten game behavior.
- Keep runtime JSON loading available where useful, but do not require a JSON
  parser in generated shipping games.
- First convert one existing example to generated static C configuration and
  compare it against the direct and JSON paths.

## 3. Rohr Editor Rework

Rework the existing level editor into Rohr Editor, a C program built with the
engine that owns a serializable project model and previews the same public
configuration consumed by generated games.

Begin with a small reliable workflow:

- Project create, save, load, and version migration.
- Entity list and component inspector.
- Asset and file-path configuration.
- Scene/level preview.
- Validation and C generation.

Add the UI canvas, asset browser, richer level tools, and build/run integration
after the project model and generator are stable. The editor authors game
structure; developers continue to write custom gameplay rules in C.

Add reusable editor and UI drawing tools for lines, arrows, handles, selection
outlines, relationship links, axes, and transform gizmos. Keep these tools
available through direct C APIs so they are useful outside Rohr Editor.

### Example Generation Coverage

Add the remaining editor and generator features needed to author the existing
examples while keeping runtime behavior in developer-owned C:

- Soft-body triangles and filled surfaces.
- Standalone particle definitions.
- Configurable soft-body node radius.
- Beam and triangle topology tooling.
- Textures, colors, and rendering configuration.
- UI layouts.
- Spawn templates and repeated entity arrays.
- Runtime behavior support for controls, torque, timed spawning, scoring, and
  recording.

## 4. Composable UI Entities

Replace the widget-specific direction with a small UI-specific entity and
component registry that remains independent from the world ECS. Do not begin
with special button, label, or slider runtime types. A control is initially
just a useful combination of explicit components built through a generic
`UIEntityConfig` and builder.

Initial components should cover:

- Screen-space transform, size, angle, and deterministic draw order.
- Rectangle and text rendering.
- Mutable visibility.
- Hoverable, pressable, and clickable behavior as separate capabilities.
- Interaction-driven colors.
- Later, draggable, numeric value, axis mapping, hierarchy, clipping, focus,
  layout, and animation components.

Reuse `math2d` geometry and point-containment operations for screen-space hit
testing, including rotated controls, without routing UI through the physics
collision or response pipeline. Validate required component combinations
rather than silently adding behavioral dependencies.

## 5. Immediate UI Interaction and Reactive Values

Keep UI entity components focused on definitions, configuration, and bindings.
Calculate hover, press, click, drag, and changed events once per frame, cache
them only for that frame, and expose explicit state queries. Retain only the
minimal cross-frame context required for pointer capture, focus, and input
transitions.

Use a clear frame pipeline:

1. Begin the UI frame with a logical screen-space input snapshot.
2. Resolve visibility, transforms, draw order, and hit shapes.
3. Calculate interaction and pointer capture once.
4. Apply bindings and actions.
5. Let game code inspect transient state.
6. Draw visible UI entities without changing interaction state.
7. End the frame and clear transient events as appropriate.

Add a small typed UI value store for UI-owned and application settings such as
volume, fullscreen, selected tabs, and menu visibility. UI entities may both
read and drive these values through bindings. Outside code reads and writes the
same authoritative store entries. Do not move core simulation state such as
health, position, or velocity into the UI store merely for display.

Support declarative built-in actions for common mutations such as setting,
toggling, incrementing, and decrementing typed values. Reserve explicit
function-pointer callbacks with documented borrowed context lifetimes for
custom game behavior. JSON and generated C must map to the same direct binding
and action APIs.

## 6. UI and Editor Testing Foundation

- Test UI entity generations, deletion, component masks, names, and pool
  limits.
- Test front-to-back hit selection and deterministic draw ordering.
- Test one-frame hover-enter, hover-exit, click, changed, drag-start, and
  drag-end events.
- Test pointer capture when press and release occur over different elements.
- Test direct config, JSON-loaded config, editor preview, and generated C for
  behavioral parity.
- Test generated output determinism and compile it in continuous integration.
- Add a headless example that constructs the same UI directly without JSON.

## 7. Non-Convex Polygon Collision Support

Improve SAT collision handling to support non-convex polygons while preserving
the existing collision pipeline and documenting any resulting simulation
behavior changes before implementation.

## 8. Expanded Joint Options

Add more joint types and configuration options while keeping joint ownership
and lifetimes explicit. Document the intended effect of each option on the
physics pipeline and existing simulation behavior before implementation.

Add bounded joint-result information and current/previous joint state tracking
similar to overlap and contact queries. Expose useful values such as applied
constraint impulse or force, current error, relative velocity, limit state,
and whether a joint entered, stayed in, or exited a limit or broken state.

## 9. Flexible Obstacle Prototype

Build flexible obstacles in a game first by composing particles with spring or
other joints. Use the prototype to identify missing physics primitives,
stability requirements, ownership rules, and reusable configuration before
considering an engine-level helper or abstraction.

## 10. Automated Tests and Validation

Add a headless test suite for entity lifetimes, object-pool growth,
relationships, serialization, collision detection, and physics constraints.
Add sanitizer and continuous-integration builds so memory errors, undefined
behavior, and regressions are caught early.

## 11. Fixed-Step Simulation Runner

Provide an optional, explicit loop helper that accumulates frame time and runs
physics at a fixed interval. Include a catch-up limit, separate rendering
timing, pause and single-step support, and optional render interpolation.

Add adaptive physics substeps that select a bounded substep count from
predicted entity movement and collision-feature size. Keep all interacting
entities on one global substep count, expose the selected count through debug
statistics, and retain explicit minimum and maximum limits so simulation cost
cannot grow without bound.

Add swept collision detection for fast-moving rigid bodies, particles, and
soft-body boundary edges where bounded substeps cannot reliably prevent
tunneling. Integrate swept contacts with the existing contact constraint and
interaction-event pipeline instead of creating a separate response path.

Any change to physics stepping must document how it affects existing
simulation behavior before it is implemented.

## 12. Asset Ownership and Lifetime Management

Introduce engine-owned texture, animation, and future audio asset handles.
Define clear load, sharing, unload, failure-cleanup, and shutdown behavior, and
avoid loading duplicate copies of the same asset.

## 13. Audio

Add a small audio system for loading sounds, one-shot playback, looping music,
and volume control. Keep ownership explicit and use existing SDL facilities
where practical rather than adding a large dependency.

## 14. Unified Input

Build an engine-owned per-frame input snapshot over the existing keyboard and
mouse primitives. Add gamepad support, analog axes, mouse wheel and text input,
device connection handling, and a simple action-mapping layer.

## 15. Collision Filtering, Triggers, and Queries

Add collision layers and masks, non-resolving sensors or triggers, and
enter/stay/exit collision events. Expose bounded contact data and basic
raycast or shape-query operations without replacing the existing physics
pipeline.

The intended effect on collision and simulation behavior must be documented
before this work begins.

Evaluate a sparse broad-phase interaction candidate list so narrow-phase
checks iterate likely pairs instead of all entity combinations. Keep the
current interaction table authoritative, avoid duplicate pair processing, and
adopt the extra index only when profiling demonstrates a useful scaling gain.

## 16. Configurable Platform and Renderer Setup

Replace compile-time-only window settings with a small initialization
descriptor. Allow applications to select the title, logical resolution,
window size, flags, VSync, initialized subsystems, and a headless mode for
tests or simulation tools.

## 17. Module and File Naming Cleanup

Audit and rename modules, source files, headers, and internal symbols so their
names match their current responsibilities and the public API conventions.
Perform this as focused mechanical changes, preserve subsystem boundaries, and
avoid combining naming-only changes with simulation or ownership changes.

## Later Features

These features are useful after the foundations above are reliable:

- Tile maps
- Particle emitters
- Prefabs
- Navigation and pathfinding
- Scripting
- Networking
- Asset hot reload

## Scaling, Testing, and Refactoring Direction

- Keep systems independently callable so applications can compose their own
  update loops.
- Prefer stable handles and explicit ownership for shared resources.
- Make subsystem state resettable so tests can run in isolation.
- Use fixed inputs and delta times for reproducible simulation tests.
- Add focused abstractions only when a demonstrated ownership, lifetime, or
  usability problem requires them.
- Preserve the engine's plain-C, data-oriented architecture while extending
  public API coverage incrementally.
