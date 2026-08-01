#ifndef ENGINE_INTERNAL_H
#define ENGINE_INTERNAL_H

#include <stddef.h>
#include "error.h"
#include "entity_components.h"

/**
 * Ensure all entity-indexed subsystem tables can address capacity slots.
 */
EngineResult engine_tables_ensure_capacity(size_t capacity);

/** Initialize entity tables. */
EngineResult entity_tables_init(void);
/** Ensure entity tables can address capacity slots. */
EngineResult entity_tables_ensure_capacity(size_t capacity);
/** Destroy entity tables. */
void entity_tables_destroy(void);

/** Initialize physics tables. */
EngineResult physics_tables_init(void);
/** Ensure physics tables can address capacity slots. */
EngineResult physics_tables_ensure_capacity(size_t capacity);
/** Destroy physics tables. */
void physics_tables_destroy(void);
/** Clear joints and anchors owned by or connected to a deleted entity. */
void physics_entity_clear(Entity entity, EntityIndex index);

/** Initialize graphics tables. */
EngineResult graphics_tables_init(void);
/** Ensure graphics tables can address capacity slots. */
EngineResult graphics_tables_ensure_capacity(size_t capacity);
/** Destroy graphics tables. */
void graphics_tables_destroy(void);

/** Initialize grid tables. */
EngineResult grid_tables_init(void);
/** Ensure grid tables can address capacity slots. */
EngineResult grid_tables_ensure_capacity(size_t capacity);
/** Destroy grid tables. */
void grid_tables_destroy(void);

/** Reset engine-owned serializable asset and sprite-reference state. */
void game_state_runtime_reset(void);
/** Clear serializable state associated with a deleted entity index. */
void game_state_entity_clear(EntityIndex index);

#endif
