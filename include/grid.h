#ifndef GRID_H
#define GRID_H
#include "math2d.h"
#include "entity_components.h"
/** Width and height of one grid cell in world units. */
#define CELL_SIZE 20
/** Number of grid rows. */
#define GRID_ROWS 25
/** Number of grid columns. */
#define GRID_COLS 25

/** Grid cell containing entities that overlap the cell. */
typedef struct {
    /** Entity ids stored in this cell. */
    Entity entities[MAX_ENTITIES];
    /** Occupancy flags for entity slots. */
    bool entity_present[MAX_ENTITIES];
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
void add_entity_to_grids(Entity entity);
/** Check whether an entity pair has already been processed. */
bool grid_pair_checked_is(Entity entity_1, Entity entity_2);
/** Mark an entity pair as processed. */
void add_pair(Entity entity_1, Entity entity_2);
/** Clear all grid cells and checked-pair state. */
void clear_grid(void);
/** Recompute and store an entity AABB from its world hitbox. */
void grid_update_aabb(Entity entity);
/** Global collision grid. */
extern Grid grid;
/** Global checked-pair table. */
extern BooleanPairs pair_checked;
#endif
