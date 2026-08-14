# Editor architecture

The editor is a separate executable under `tools/editor`. It links Rohr Engine
and uses the engine's graphics, input, and primitive UI systems, but its authoring
model is intentionally separate from runtime ECS entities.

```text
EditorWorkspace
├── workspace manifest and filesystem layout
└── EditorProject
    └── EditorObject[]
        ├── EditorRigidBody[]
        │   └── EditorHitbox[] -> EditorVertex[]
        ├── EditorAnchor[]
        ├── EditorJoint[]
        └── EditorSoftBody[]
            ├── EditorSoftNode[]
            ├── EditorSoftBeam[]
            └── EditorSoftArea[]
```

Editor ids are stable references inside editor data. They are not engine
`Entity` handles and are never serialized as live ECS state. Generated C creates
fresh runtime entities and returns them through a generated typed object whose
fields hold the resulting handles.

## Modules

`tools/editor/src/main.c`
: Owns application lifetime, top-level menus, UI composition, keyboard routing,
  color picking, scroll layout, dirty-state checks, and editor panels.

`editor_project.h` and `editor_project.c`
: Define and mutate the in-memory authoring model. This layer owns stable editor
  ids, defaults, naming rules, topology changes, derived particle fitting, and
  editor-time joint constraints. It does not allocate engine entities.

`editor_project_json.c`
: Serializes and validates the editor project. Missing fields with defined
  defaults support loading older project files. `EDITOR_PROJECT_FORMAT_VERSION`
  identifies incompatible schema changes.

`editor_workspace.h` and `editor_workspace.c`
: Own project-directory creation, manifest loading/saving, starter content,
  CMake scaffolding, and C generation. Generation writes only configured
  generated paths; developer-owned `main.c` is written only during initial
  project creation.

`editor_viewport.h` and `editor_viewport.c`
: Own camera transforms, drawing, hit testing, selection, dragging, editor-mode
  transitions, and keyboard nudging. World-to-screen conversion also performs
  the engine/editor Y-axis inversion.

`editor_file_browser.h` and `editor_file_browser.c`
: Implement the modal directory browser and its two-pane directory preview.

`editor_layout.h`
: Shares the current logical viewport/tool-column dimensions.

## State separation

Three states are deliberately distinct:

- `EditorProject` is persistent authored content.
- `EditorViewportState` is transient navigation, selection, camera, and drag state.
- `EditorWorkspace` is persistent project configuration plus the currently open path.

Viewport selection ids refer into `EditorProject`, but selection and camera data
are not the source of authored geometry. UI labels and field assets are render
resources owned by `main.c` and are destroyed when the editor exits.

## Input and hit priority

The UI frame consumes pointer input before the viewport. Unconsumed viewport
input is transformed from screen space to authored world space and resolved in
mode-specific priority order. Handles such as origins, rotation controls,
vertices, and lines take priority over parent-body interiors.

Editor modes are explicit (`EDITOR_VIEWPORT_RIGID_BODY`,
`EDITOR_VIEWPORT_HITBOX`, `EDITOR_VIEWPORT_PARTICLE`, and others). A click may
change selection without changing mode; a double-click or `Enter` normally opens
the selected item's mode. `Escape` moves to the parent mode.

This mode-aware priority is important. For example, the owning particle ring is
ignored in hitbox mode so it cannot steal vertex editing, while another body's
particle ring remains selectable.

## UI composition

Editor widgets are composed from Rohr UI primitives: interactions, surfaces,
borders, labels, fields, sliders, dropdowns, and scroll regions. Bounds are
resolved by the active scroll region so drawing, clipping, navigation, and hit
testing use the same coordinates.

Custom editor visuals should also be built from scroll-aware primitives. Direct
screen drawing inside a scrolling panel can visually separate an icon from its
interaction bounds.

The tools column is one scroll region. Its content height determines the final
delete-row position. Modal dropdowns and the file browser temporarily capture
navigation so arrow keys cannot move unrelated column controls.

## Persistence and generation

Saving follows this path:

```text
EditorProject -> objects/project.rohr.json
EditorWorkspaceConfig -> project.rohr.json
```

Generating follows a separate path:

```text
EditorProject -> src/generated/project_objects.h
              -> src/generated/project_objects.c
```

Generated creation is transactional: if a component or entity operation fails,
already-created runtime entities are destroyed. Generated destruction tolerates
partially created objects and clears handles afterward.

Particle auto-fit is derived from the first rigid-body hitbox using its polygon
centroid and farthest vertex. The derived value is refreshed in editor state,
written to JSON, and recomputed during generation to prevent stale output.

## Ownership rules

- `EditorProject` owns all fixed-capacity editor arrays and names.
- Parent removal owns removal of its children.
- Anchors are independent object children and are referenced by joints.
- Joints do not own anchors and therefore do not implicitly delete them.
- Soft areas are bounded faces derived from the planar node/beam graph and are
  synchronized after topology changes. Ordered boundaries preserve separate
  cross-section faces; ear-clipping produces runtime triangles without changing
  the editor-level area.
- Generated source owns no editor data; it materializes runtime entities only.

## Extending the editor

When adding an authored property:

1. Add it to the appropriate editor model with a safe default.
2. Persist it in JSON while preserving a default for older files.
3. Add UI without mixing transient selection into persistent data.
4. Update viewport preview and hit testing if the property is visual.
5. Update generated C if it affects runtime behavior.
6. Extend `editor_project_test` with round-trip and generated-output checks.

Keep editor-only concepts out of the public engine API unless a game can use the
same concept independently of the editor.
