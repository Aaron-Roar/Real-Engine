#ifndef GRID_H
#define GRID_H
#include "math2d.h"
#include "entity_components.h"
/** Width and height of one grid cell in world units. */
#define CELL_SIZE 20
/** Number of grid rows. */
#define GRID_ROWS 35
/** Number of grid columns. */
#define GRID_COLS 425
/** Maximum entities stored in one broad-phase cell. */
#define GRID_CELL_ENTITY_CAPACITY 512

/** Grid cell containing entities that overlap the cell. */
typedef struct {
    /** Entity ids stored densely in this cell. */
    Entity entities[GRID_CELL_ENTITY_CAPACITY];
    /** Number of occupied entity slots. */
    uint16_t entity_count;
} Cell;

/** Fixed broad-phase collision grid. */
typedef struct {
    /** Grid cells indexed by row and column. */
    Cell cells[GRID_ROWS][GRID_COLS];
} Grid;


/** Pair table used to prevent duplicate collision checks. */
typedef struct {
    /** Checked pair flags indexed by EntityIndex. */
    bool pairs[MAX_ENTITIES][MAX_ENTITIES];
} BooleanPairs;

/** Add an entity to every grid cell touched by its AABB. */
void grid_entity_add(Entity entity);
/** Check whether an entity pair has already been processed. */
bool grid_pair_checked_get(Entity entity_1, Entity entity_2);
/** Mark an entity pair as processed. */
void grid_pair_add(Entity entity_1, Entity entity_2);
/** Clear all grid cells and checked-pair state. */
void grid_clear(void);
/** Recompute and store an entity AABB from its world hitbox. */
void grid_aabb_update(Entity entity);
/** Global collision grid. */
extern Grid grid;
/** Global checked-pair table. */
extern BooleanPairs pair_checked;
#endif
