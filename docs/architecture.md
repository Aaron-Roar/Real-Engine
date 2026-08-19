# Architecture {#architecture}

Rohr Engine is a C and SDL3 engine built around explicit systems and visible
data ownership. The public facade is `include/rohr.h`; applications should not
depend on headers under `src/`.

## Runtime organization

```text
include/                 public engine and tools APIs
src/
├── core/                lifecycle, facade, errors, systems, timing
├── entity/              stable entity handles and component metadata
├── graphics/            renderer, deferred draw layers, UI, presentation
├── input/               keyboard, mouse, and controller input
├── math/                vectors, polygons, projections, and AABBs
├── physics/
│   ├── body/            body state, forces, and motion properties
│   ├── broadphase/      dynamic AABB tree
│   ├── collision/       filtering, decomposition, overlap, manifolds
│   ├── constraints/     iterative constraint orchestration
│   ├── joints/          anchors and joint constraints
│   ├── particles/       particle-circle geometry and response helpers
│   ├── pipeline/        standard and individually callable stages
│   ├── rigid_body/      integration and rigid contact response
│   └── soft_body/       nodes, beams, areas, and boundary contacts
└── state/               JSON runtime state and authored definitions
tools/
├── editor_core/         editor document commands and CLI grammar
├── cli/                 rohr-cli frontend
├── editor/              rohr-gui frontend
└── terminal/            reusable terminal emulator
```

Domain modules own their state and cleanup. Cross-domain private contracts are
declared in narrowly scoped internal headers such as
`src/core/engine_internal.h` and `src/physics/physics_internal.h`.

## Entities and components

`Entity` is a stable, generation-checked handle. `EntityIndex` is an internal
index into component pools. Systems resolve handles before direct table access;
applications should retain `Entity`, not `EntityIndex`.

Engine component pools grow with entity capacity. A pool owns a value array and
occupancy information, so optional data does not need a valid sentinel value.
Entity deletion clears owned component state before its index can be reused.

Engine component masks use `ROHR_` prefixes. Generated game components use a
separate `GameComponentMask` and typed sparse-set pools. Generated component
addresses are invalidated when their pool grows or swap-removes an item.

## Error flow

Fallible APIs return small generated result types. Errors are propagated to the
application boundary; the application decides whether to log, notify, recover,
or terminate. `rohr_error_message_get(result)` combines the stable Rohr error
message with a captured lower-level cause such as `SDL_GetError()`.

See [Error handling](errors.md).

## Physics

The standard physics update is assembled from public stages under
`src/physics/pipeline`. The default pipeline remains the behavioral reference,
while advanced applications may call stages directly.

Rigid contact constraints and active joints are gathered once per substep and
iterated together. Polygon collision uses an AABB-tree broadphase, SAT over
cached convex pieces, contact manifolds, accumulated impulses, positional
correction, friction, and restitution. Concave decomposition is internal.

See [Physics](physics.md) for ordering, collision modes, and limitations.

## Graphics

Drawing is deferred until presentation. Each active signed layer owns an
insertion-ordered command buffer. Only layers used during the frame exist in
the sparse active-layer list; layers are ordered before execution, while calls
within one layer retain submission order. Buffers retain capacity between
frames.

The UI is composed from primitive interactions, surfaces, clipping, text,
fields, sliders, dropdowns, and scroll regions. Higher-level tools use the same
public primitives available to applications.

## Editor and generated projects

The editor owns an authoring model separate from runtime ECS state. Editor ids
are stable references inside `EditorProject`, not live engine entities. JSON
stores editable objects and metadata; generated C creates runtime entities and
returns their handles through typed generated structs.

Saving JSON and generating C are separate operations. Generation replaces only
files under `src/generated/`; developer-owned `src/main.c` is created with a
new project and is not overwritten afterward.

See [Editor guide](editor.md) and [Editor architecture](editor_architecture.md).

## Build and distribution

CMake is authoritative. `dev.sh` and `dev.bat` are convenience frontends. SDK
installs export a `Rohr` CMake package, headers, static libraries, `rohr-cli`,
`rohr-gui`, configuration defaults, project templates, and licenses.

See [Building and SDK usage](building.md).
