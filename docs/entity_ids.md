# Entity Ids {#entity_ids}

`Entity` values are stable ids, not component table indexes.

Systems that need direct table access resolve an entity id through
`entity_index_get()`. This keeps ids decoupled from storage location and avoids
using raw array indexes as public handles.

`EntityIndex` values still correspond to component table indexes. That property
is important for compute-oriented loops and direct table access inside engine
systems.

When an entity is deleted, its id slot and table index can be reused. The
generation bits in the entity id prevent stale ids from resolving as live
entities after deletion and reuse.
