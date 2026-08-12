# Editor guide

The Rohr editor is a visual authoring tool for engine objects. It stores editable
project data as JSON and generates C source that is compiled with the game. Game
runtime behavior remains developer-owned C code.

## Build and run

From the engine repository:

```sh
nix develop
cmake --preset linux
cmake --build build --target rohr_editor
./build/editor/rohr_editor
```

On startup, choose **New Project** or **Load Project**. The directory browser
starts in the current working directory. Single-click a directory to select it
and preview its contents; double-click it to enter it. `..` behaves the same way.

For a new project, navigate to the parent directory, enter the new directory
name, and choose **Create Project**. The editor creates a working starter scene
containing a static floor and a gravity-enabled box.

## Generated project layout

The default workspace is:

```text
my-game/
├── CMakeLists.txt
├── project.rohr.json
├── assets/
├── objects/
│   └── project.rohr.json
└── src/
    ├── main.c
    └── generated/
        ├── project_objects.c
        └── project_objects.h
```

`project.rohr.json` at the root is the workspace manifest. The JSON under
`objects/` is editor-owned scene data. Files under `src/generated/` are
replaceable generated output. `src/main.c` is created with a new project but is
not overwritten by **Generate C**.

Saving and generating are separate operations:

- **File > Save** writes the workspace manifest and editor JSON.
- **Build > Generate C** replaces only the generated object source and header.
- **File > Close** closes the project and asks about unsaved changes.
- **File > Exit** safely exits and asks about unsaved changes.

Configure and build a generated game from its project directory:

```sh
cmake -S . -B build
cmake --build build
./build/MyGame
```

The executable name is the PascalCase project directory name (`my-game`
becomes `MyGame`).

## Editing model

An object is an authored collection, similar to a prefab definition. It may own:

- rigid bodies and their hitboxes;
- particles represented by particle-enabled rigid bodies;
- anchors and joints;
- soft bodies, nodes, beams, and generated triangular areas.

Object names are formatted as PascalCase because they become generated C type
names. Child property names are formatted as snake_case because they become
generated fields and identifiers.

The right column shows the hierarchy or the editor for the current item:

- Single-click selects an item.
- Double-click opens its editor.
- `Enter` opens the selected item.
- `Escape` returns to the parent editor. At the root it deselects the object.
- `Delete` removes a deletable selected item.
- Arrow keys navigate the column while the pointer is over it.
- The red button at the bottom deletes the item whose editor is open.
- Eye controls change editor visibility only; hidden items remain project data.

Selection is shared between the hierarchy and viewport. Selecting an authored
item highlights its viewport representation. Clicking an empty viewport or
column background clears the current selection without jumping to the root.

## Viewport controls

- Left drag moves the selected draggable item.
- Right drag pans the camera.
- `Ctrl` + left drag also pans, for trackpads and one-button pointing devices.
- Mouse wheel zooms around the pointer.
- Arrow keys nudge the selected viewport item when the pointer is in the viewport.
- **World/Local** changes only the camera reference; it does not modify project data.
- Drag the divider between the viewport and tools column to resize the column.

Rigid-body and soft-body editors show an origin and rotation handle. Dragging a
body interior translates it. Dragging the rotation handle rotates it. Opening an
origin editor allows moving the local origin without moving the authored shape.

## Hitboxes and rigid bodies

A new rigid body starts with a triangular hitbox. In the hitbox editor, vertices
and lines are selectable and editable. Adding a vertex splits the selected line
at its midpoint without moving neighboring vertices. Locked vertices cannot be
moved; line-length edits distribute movement only to unlocked endpoints.

While a hitbox editor is open, clicks on its owning body or particle do not leave
the editor or drag the body. Vertex and line handles retain priority. Clicking
outside deselects the hitbox while leaving its editor open.

Rigid bodies expose mass, friction, restitution, gravity, motion type, rotation
locking, colors, and collision filtering. Collision filtering has two sets:

- **Collision Category** describes what the body is.
- **Collide With** describes categories it accepts.

A pair responds only when both directional filters accept one another.

## Particles

Enable **Collision**, then enable **Particle** on a rigid body. The viewport
shows a dotted particle ring with its fill behind rigid bodies. Single-clicking
the ring selects it, double-clicking opens the particle editor, and dragging the
ring translates its rigid body.

The particle editor provides radius, ring color, fill color, and **Auto Fit**.
Auto-fit is enabled by default. It places the particle at the hitbox polygon
centroid and sets its radius to the largest centroid-to-vertex distance. Moving
a vertex updates the fitted particle. Disabling auto-fit preserves its last
calculated radius, after which the radius field can be edited manually.

The same centroid and radius are saved and used by generated C, so the preview
and generated physics agree.

## Anchors and joints

An anchor refers to at most one rigid body. Its position and orientation may
follow that body independently, or remain in object/world space. Anchors can be
reused by multiple joints and are not automatically deleted when a joint changes
or is removed.

Joints select two anchors. Springs expose rest length, stiffness, and damping.
Revolute joints support damping. Pin and weld constraints update connected
bodies during editing so authored relationships remain visible.

## Soft bodies

Soft bodies own nodes and beams. Nodes provide mass, radius, friction,
restitution, gravity, particle collision filters, and color overrides. Beams
provide stiffness, damping, endpoints, and color overrides.

Three mutually connected nodes generate a triangular area automatically. Areas
may override the parent area color and their boundary beams may override the
parent beam color. When **Inherit** is selected, the local color control is
disabled and follows the soft-body color.

Dragging any node, beam, or filled area while in the soft-body editor translates
the whole soft body. Double-click nodes, beams, or areas to open their individual
editors.

## Current boundary

The editor authors structural data and generates object creation/destruction and
drawing support. Controls, gameplay rules, torque, timed spawning, scoring,
recording, and other runtime behavior belong in developer-owned C source.
