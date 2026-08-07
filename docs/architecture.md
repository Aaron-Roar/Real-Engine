# Architecture {#architecture}

Rohr Engine keeps systems separate and explicit. Public engine APIs live in
`include/`, implementation files are grouped by domain under `src/`, and
private engine contracts that should not be application-facing live in
`src/core/engine_internal.h`.

The engine initializes entity, physics, graphics, and grid tables during
`engine_init()`. Entity-indexed tables start with zero capacity and grow as
entities are added. `entity_add()` returns a stable entity id, while systems
resolve that id to the current component table index when table access is
needed.

The standard physics update is composed from public pipeline stages, while
`system_physics_update()` remains the systems-layer entry point.

## Generated game components

The component-generation tools keep game tags and data components separate from
engine-owned components. Engine APIs use `RohrComponentMask`; generated games own a distinct
`GameComponentMask` and never pass it to engine component functions.

Tags consume one generated mask bit and have no value storage. Data components
generate a typed sparse-set pool containing a dense entity list, dense values,
and a small `EntityIndex`-to-dense-index lookup table. This keeps lookup and
swap-removal constant-time while allocating large values only for entities that
actually have the component.

Generated games explicitly initialize and shut down their component storage.
They must call `game_components_clear(entity)` before deleting an engine entity.
Optional generated destruction hooks handle values that own resources. Addresses
returned by generated `get_addr` functions become invalid when that component's
pool grows or removes an entry.
## Physics pipeline

Physics implementation is divided by domain under `src/physics`. Pipeline
orchestration lives in `src/physics/pipeline`:

- `physics_stages.c` exposes individual operations over the engine component
  pools and reusable constraint buffers.
- `physics_pipeline.c` composes those operations into the standard substep and
  complete update used by most games.
- Rigid-body, joint, soft-body, collision, constraint, and broadphase modules
  implement the work invoked by those stages.

The standard pipeline remains the behavioral reference. Custom pipelines may
omit stages, but their caller owns the resulting behavior and must preserve
required ordering such as clearing transient constraints before gathering.
