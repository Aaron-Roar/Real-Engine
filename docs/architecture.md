# Architecture {#architecture}

Rohr Engine keeps systems separate and explicit. Public engine APIs live in
`include/`, implementation files live in `src/`, and private engine contracts
that should not be application-facing live in `src/engine_internal.h`.

The engine initializes entity, physics, graphics, and grid tables during
`engine_init()`. Entity-indexed tables start with zero capacity and grow as
entities are added. `entity_add()` returns a stable entity id, while systems
resolve that id to the current component table index when table access is
needed.

The physics update pipeline is owned by `system_update_physics()` and currently
applies joints, forces, velocity/orientation integration, locks, global hitbox
updates, AABB/grid updates, and collision resolution.

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
