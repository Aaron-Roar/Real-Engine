# Next Steps

Rohr Engine is the runtime beneath an optional C authoring tool. Work is
ordered by dependency and engine workflow value: shared runtime models first,
physics semantics second, authoring coverage third, and platform expansion
last.

## Requirements for Every Step

- Keep direct C, CLI, JSON, generated C, project loading, and editor preview in
  behavioral parity wherever the feature applies.
- Add focused tests in the same commit as behavior.
- Keep generated output deterministic and compile it on Linux and Windows.
- Document physics behavior changes before implementing them.
- Preserve explicit ownership, lifetime, and allocation-failure handling.

## 1. Shared Render Attachment Model

This is first because sprites already expose body-specific APIs, and the same
model is required by animated sprites and cameras.

- Rename sprite APIs that use `sprite_body_*`; body attachment is optional and
  should not define the sprite API's identity.
- Introduce one explicit attachment value containing the target rigid body,
  position offset, orientation offset, and position/orientation lock settings.
- Share the model between sprites, animated sprites, and later cameras.
- Keep free-placed transforms valid when no attachment is present.
- Migrate engine API, CLI, JSON, generated C, editor GUI, loading, and undo/redo
  in separate reviewable commits.
- Test attachment, detachment, offsets, rotation locks, target destruction, and
  save/load/codegen parity.

## 2. Center of Mass

Center of mass must exist before force-at-point, torque authoring, and accurate
camera or debug visualization of physical bodies.

- Add an explicit rigid-body center of mass used by integration, torque,
  contacts, joints, and debug rendering.
- Author it relative to the stable local origin. When unspecified, calculate
  the shape centroid as the automatic default.
- Add an editor convenience action that moves the origin to the centroid.
- Define how vertex, origin, and mass edits affect automatic versus explicit
  centers of mass.
- Test centroid defaults, explicit offsets, transformed bodies, and shape edits.

## 3. Force-at-Point and Torque Response

- Add a force-at-point operation that derives torque from the force vector and
  its offset from the center of mass.
- A force through the center of mass must produce no torque.
- Accumulate derived torque for the current physics tick; do not create a
  persistent hidden torque component.
- Test aligned, perpendicular, rotated, zero-mass, static, and
  kinematic-driven bodies.

## 4. Authored Motion, Forces, and Torques

This editor work depends on the center-of-mass and force-at-point semantics
being stable.

- Expose initial velocity and acceleration for every movable ECS-backed item.
- Add object-owned force definitions with a target body, local position,
  direction, and magnitude. A body-locked force transforms its local position
  and direction each tick before applying force-at-point.
- Keep one-frame ECS force components separate from persistent authored force
  emitters.
- Add object-owned torque definitions with a target body, signed magnitude,
  visibility, and circular-arrow representation. Torque has no position offset.
- Give forces and torques hierarchy entries, editors, viewport handles,
  visibility, multi-select, reorder, delete, and transactional undo/redo.

## 5. Camera Authoring

Camera authoring follows the attachment model but does not depend on force
authoring, so it can begin as soon as section 1 is stable.

- Make cameras object-owned items with position, orientation, zoom, viewport
  dimensions, attachment, visibility, and all remaining camera properties.
- Draw a camera origin, rotation handle, and dotted view boundary. Allow
  resizing through boundary handles and numeric fields.
- Use the shared attachment model so dragging an attached camera or body keeps
  their relationship consistent with sprites and animations.
- Add hierarchy, selection, multi-edit, delete, undo/redo, CLI, JSON, generated
  C, project-load, editor-load, and runtime-load coverage.

## 6. Deterministic Joint Anchor Placement

- Audit pin, weld, and spring authoring so argument order has one meaning:
  `joint_create(anchor_1, anchor_2)` moves the object associated with
  `anchor_1` toward `anchor_2` when placement is requested.
- Preserve the rule for future bounded or maximum-length joints.
- Keep low-level runtime constraint construction free of implicit body movement;
  expose placement as an explicit authoring/helper operation.
- Test reversed arguments, world anchors, shared anchors, locked anchors, and
  connected groups.

## 7. Physics Robustness and Scaling

Complete these as separate physics commits after the immediate COM and force
semantics are stable.

- Add bounded adaptive substeps based on predicted movement and collision
  feature size, exposing the selected count through debug statistics.
- Add swept collision detection for fast rigid bodies, particles, and soft-body
  boundaries through the existing contact pipeline.
- Add non-convex polygon support through validated decomposition while keeping
  SAT and contact response explicit.
- Profile before adding a sparse broad-phase interaction candidate index.
- Keep active constraints compact and hot iteration free of full-capacity
  scans.

## 8. Asset Ownership, Audio, and Input Foundations

These runtime foundations should precede broad asset-heavy example authoring.

- Add engine-owned texture and animation handles with explicit sharing,
  unloading, failure cleanup, and shutdown behavior.
- Build a small SDL-backed audio API for sounds, looping, mixing, volume, and
  destruction without adding another framework.
- Add an engine-owned per-frame input snapshot, gamepads, analog axes, text
  input, device changes, and a small action-mapping layer.
- Implement texture ownership, audio, and input as separate commits.

## 9. Remaining Example Authoring Coverage

- Filled soft-body surfaces and topology tooling beyond simple triangles.
- Standalone particle definitions and particle emitters.
- UI layout authoring using primitive UI components.
- Spawn templates and repeated entity arrays.
- Runtime behavior hooks for controls, timed spawning, scoring, and recording;
  behavior remains developer-owned C rather than serialized arbitrary code.

## 10. Object Add Palette

This improves editor usability but does not unblock runtime or generated-C
features, so it follows the missing authoring foundations.

- Replace the object editor's long add list with one Add button that opens a
  grid of named item tiles, following the auto-shape palette style.
- Keep the palette open after adding an item.
- Close it on Escape, an empty click, or opening another menu.
- Add keyboard navigation, disabled-state explanations, and pointer-event
  tests.

## 11. Game-State Serialization Refactor

This is maintenance rather than a feature dependency, so it should happen
after higher-value runtime gaps unless the serializer blocks new work.

- Rename JSON construction helpers such as `state_vec2_write()` and
  `state_color_write()` to names such as `state_vec2_json_create()`.
- Reserve `_write` for functions that actually write into an output
  destination.
- Add save/load round-trip tests before moving code.
- Split the large serializer into focused asset, UI, entity, and component
  modules without changing the format.

## 12. Multiple Windows

This is a large public-API change and should wait until the single-window
authoring/runtime path is stable.

- Add engine-owned window handles and APIs for creating, configuring,
  presenting, and destroying multiple windows.
- Remove assumptions that graphics, UI, input, cameras, or presentation state
  belong to one global window.
- Define event routing, focus, ownership, and shutdown behavior first.
- Validate native Linux and Windows behavior.

## 13. Multiple Viewports per Window

- Allow each window to own multiple viewport handles with independent
  rectangles, cameras, logical sizing, clipping, and draw queues.
- Define how input coordinates and UI focus resolve through overlapping
  viewports.
- Test resize, clipping, camera assignment, destruction, and event routing.

## 14. Viewport Layout Editor

This depends on stable window and viewport handles.

- Add an object/project-level viewport layout model.
- Support adding, selecting, resizing, splitting, reordering, and assigning a
  camera to viewports.
- Generate the same layout through direct C, CLI, JSON, and generated C.
- Test overlapping layouts, invalid camera references, resize behavior, and
  save/load/codegen parity.

## Ongoing Maintenance

- Add CI enforcement for first-party and generated snake_case filenames while
  allowing required ecosystem names such as `CMakeLists.txt` and `README.md`.
- Add sanitizer builds and malformed-project fuzz/fixture coverage.
- Continue converting fixed-capacity editor collections to dynamic arrays.
- Split oversized modules when tests make the move mechanical and safe.

## Later Features

- Tile maps
- Prefabs
- Navigation and pathfinding
- Scripting
- Networking
- Asset hot reload
