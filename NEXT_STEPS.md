# Next Steps

This roadmap focuses on the foundational features needed to make Rohr Engine
more reliable and practical for building complete 2D games. The items are
ordered roughly by implementation priority.

## 1. Level Editor Rework

Rework the level editor to provide a more reliable and practical workflow for
creating, inspecting, and modifying complete game levels.

## 2. Non-Convex Polygon Collision Support

Improve SAT collision handling to support non-convex polygons while preserving
the existing collision pipeline and documenting any resulting simulation
behavior changes before implementation.

## 3. Expanded Joint Options

Add more joint types and configuration options while keeping joint ownership
and lifetimes explicit. Document the intended effect of each option on the
physics pipeline and existing simulation behavior before implementation.

## 4. Flexible Obstacle Prototype

Build flexible obstacles in a game first by composing particles with spring or
other joints. Use the prototype to identify missing physics primitives,
stability requirements, ownership rules, and reusable configuration before
considering an engine-level helper or abstraction.

## 5. Automated Tests and Validation

Add a headless test suite for entity lifetimes, object-pool growth,
relationships, serialization, collision detection, and physics constraints.
Add sanitizer and continuous-integration builds so memory errors, undefined
behavior, and regressions are caught early.

## 6. Fixed-Step Simulation Runner

Provide an optional, explicit loop helper that accumulates frame time and runs
physics at a fixed interval. Include a catch-up limit, separate rendering
timing, pause and single-step support, and optional render interpolation.

Any change to physics stepping must document how it affects existing
simulation behavior before it is implemented.

## 7. Asset Ownership and Lifetime Management

Introduce engine-owned texture, animation, and future audio asset handles.
Define clear load, sharing, unload, failure-cleanup, and shutdown behavior, and
avoid loading duplicate copies of the same asset.

## 8. Audio

Add a small audio system for loading sounds, one-shot playback, looping music,
and volume control. Keep ownership explicit and use existing SDL facilities
where practical rather than adding a large dependency.

## 9. Unified Input

Build an engine-owned per-frame input snapshot over the existing keyboard and
mouse primitives. Add gamepad support, analog axes, mouse wheel and text input,
device connection handling, and a simple action-mapping layer.

## 10. Collision Filtering, Triggers, and Queries

Add collision layers and masks, non-resolving sensors or triggers, and
enter/stay/exit collision events. Expose bounded contact data and basic
raycast or shape-query operations without replacing the existing physics
pipeline.

The intended effect on collision and simulation behavior must be documented
before this work begins.

## 11. Configurable Platform and Renderer Setup

Replace compile-time-only window settings with a small initialization
descriptor. Allow applications to select the title, logical resolution,
window size, flags, VSync, initialized subsystems, and a headless mode for
tests or simulation tools.

## Later Features

These features are useful after the foundations above are reliable:

- Text rendering
- A lightweight debug UI
- Tile maps
- Particle emitters
- Prefabs
- Navigation and pathfinding
- Scripting
- Networking
- Expanded level-editing tools
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
