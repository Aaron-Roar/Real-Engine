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
editor-cli --project . build
./build/MyGame
```

The installed CLI locates the Rohr SDK beside itself. When configuring manually,
provide the unpacked SDK to CMake:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/rohr
cmake --build build
```

## Lua configuration

The SDK provides `share/rohr/editor.lua`. New projects receive a portable
`editor.lua` in the project root. The CLI resolves build settings in this order:

1. Project `cli` override.
2. Project shared `project` setting.
3. SDK `cli` override.
4. SDK shared `project` setting.
5. Built-in CMake command.

Commands are argument arrays rather than shell strings. This preserves paths
containing spaces and does not invoke a shell:

```lua
return {
    project = {
        configure = { "cmake", "-S", "{project}", "-B", "{build}" },
        compile = { "cmake", "--build", "{build}" },
    },

    cli = {
        configure = nil,
        compile = nil,
    },

    gui = {
        configure = nil,
        compile = nil,
    },
}
```

`{project}` and `{build}` expand to absolute paths. `{sdk}` expands to the SDK
root when the CLI runs from an installed SDK. Missing fields inherit the next
configuration layer. Malformed fields stop the build with the Lua filename and
an error message.

Configuration files execute as real Lua, but the editor does not expose Lua's
OS, I/O, package, or debug libraries while loading them. The CLI reads these
files and never rewrites them. The GUI consumes its own layered overrides.

The GUI's **Settings > Build** menu shows the effective configure and compile
commands as Lua arrays. Applying an edited field validates that it is a
sequential array of strings using only `{project}`, `{build}`, and `{sdk}`
placeholders. GUI-owned overrides are written atomically to
`.rohr/gui-overrides.lua`; `editor.lua` remains untouched. Each field can be
reset independently to its inherited value. The command fields show three
lines at once, wrap long Lua expressions, and scroll vertically when focused.
Validation runs while editing and reports success or the specific field error.
Apply saves only when both Lua command arrays are valid and their first
arguments resolve to executable files through explicit paths or `PATH`. Windows
lookup also checks the standard executable extensions without running the file.
An Apply failure creates a bottom-left notification reading
`Build configuration (GUI) - FAIL`. Clicking it opens a detailed report with
the editor parser error followed by Lua's raw error; invalid configuration is
never written. A successful Apply saves the overrides and immediately starts
the configured configure-and-compile sequence, so a separate test-command
action is unnecessary.

The bottom-left **Notification Log** button is always available. Up to three
new notifications appear above it, with a fourth replacing the oldest visible
notification. The log retains the latest 100 notifications independently of
that visible stack. Its entries are shown newest first in a scrollable menu;
clicking either a visible notification or a log entry opens the same detailed
report. A visible notification expires after 10 seconds but remains available
in the log. Its top-right **x** removes it from the visible stack immediately
without deleting its log entry. The log closes through its Close button, Escape,
or by clicking **Notification Log** again. Opening a report from the log keeps
the log underneath it, so closing the report returns to the same log view.
Detailed reports scroll when their wrapped text is taller than the
report area. Asynchronous configure or compile failures also create a build
failure notification; visible builds retain their complete output in the
terminal. Successful C generation reports the generated files, object count,
and whether the object tree was written to the terminal. Successful compilation
is reported only after configure and compile both finish, with the project path
and terminal-output state in its detailed report.

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
- `Ctrl` + click toggles items of the same type in a multi-selection. Selecting
  another type replaces the current selection.
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

When multiple items are selected, the column shows their shared property menu.
Its fields start empty: leaving a field empty preserves each item's current
value, while submitting a value applies it to every selected item. Bulk edits
and bulk deletion are atomic, so one `Ctrl+Z` or `Ctrl+Y` restores the complete
operation. Boolean bulk fields accept `true`, `false`, `1`, or `0`; color fields
accept six- or eight-digit hexadecimal colors.

## Viewport controls

- Left drag moves the selected draggable item.
- Right drag pans the camera.
- `Ctrl` + left drag beginning on empty viewport space also pans, for trackpads
  and one-button pointing devices. `Ctrl` + click on an item toggles selection.
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

**Auto Shape** opens a three-column triangle, square, and circle picker. The
square option requires at least four vertices; triangle and circle require at
least three. Choosing a shape opens its parameter editor. Polygon corners are
always retained and additional vertices are distributed deterministically over
the perimeter. Circles distribute every vertex at an equal angle. Existing
vertex IDs and names are preserved.

Choosing a shape applies it immediately using the displayed defaults. Editing a
parameter reapplies the shape when that field is submitted with Enter, exited
with Escape, or loses focus.

While its shape editor remains open, the geometry stays constrained. Dragging
a circle point changes its radius. Dragging a square corner changes its width
and height. Triangle corners change the dimensions allowed by the selected
triangle type; a scalene apex also changes its apex offset. Additional points
distributed along polygon edges are derived points and cannot be dragged
independently.

Triangle modes are equilateral, isosceles, and scalene. Equilateral height is
derived from width, isosceles centers its apex using editable width and height,
and scalene adds an editable horizontal apex offset. Width and length
independently control the square tool, so unequal values produce a rectangle.

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

Closed node-and-beam loops generate colorable areas automatically. A hexagonal
loop becomes one area; cross-section beams divide an enclosure into multiple
independently selectable areas. Beam crossings without nodes are not topology
and do not divide an area. Concave areas are triangulated internally for drawing
and generated runtime physics, while the editor preserves one boundary and one
color setting for the complete area.

Areas may override the parent area color and their boundary beams may override
the parent beam color. When **Inherit** is selected, the local color control is
disabled and follows the soft-body color.

Dragging any node, beam, or filled area while in the soft-body editor translates
the whole soft body. Double-click nodes, beams, or areas to open their individual
editors.

Soft bodies provide the same **Auto Shape** tool. It repositions existing nodes
without replacing node IDs, names, beams, or area relationships.

## Current boundary

The editor authors structural data and generates object creation/destruction and
drawing support. Controls, gameplay rules, torque, timed spawning, scoring,
recording, and other runtime behavior belong in developer-owned C source.
