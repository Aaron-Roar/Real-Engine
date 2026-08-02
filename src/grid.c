#include "grid.h"
#include "engine_internal.h"
#include "entity_components.h"
#include "physics.h"
#include "math2d.h"
#include "console.h"
#include "math.h"

MEMORY_DECLARE_OBJECT_POOL(AABBPool, AABB);
MEMORY_DEFINE_OBJECT_POOL(AABBPool, AABB)

static AABBPool aabbs_pool = {0};
#define aabbs aabbs_pool.objects

Grid grid = {0};
BooleanPairs pair_checked = {0};

EngineResult grid_tables_init(void) {
    if(AABBPool_init(&aabbs_pool, 0).kind == ERROR_RESULT_ERROR) {
        grid_tables_destroy();
        return error_result_error(ERROR_ENGINE_GRID_TABLES_INIT_FAILED);
    }
    return error_result_value(true);
}

EngineResult grid_tables_ensure_capacity(size_t capacity) {
    size_t new_capacity;

    if(capacity > MAX_ENTITIES) {
        return error_result_error(ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
    }
    if(capacity <= aabbs_pool.capacity) {
        return error_result_value(true);
    }
    new_capacity = aabbs_pool.capacity == 0 ? 16 : aabbs_pool.capacity;
    while(new_capacity < capacity) {
        new_capacity *= 2;
    }
    if(new_capacity > MAX_ENTITIES) {
        new_capacity = MAX_ENTITIES;
    }
    if(AABBPool_expand(
        &aabbs_pool,
        new_capacity - aabbs_pool.capacity
    ).kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED);
    }
    return error_result_value(true);
}

void grid_tables_destroy(void) {
    (void)AABBPool_destroy(&aabbs_pool);
}

bool grid_pair_checked_is(Entity entity_1, Entity entity_2) {
    if(entity_1 >= MAX_ENTITIES || entity_2 >= MAX_ENTITIES) {
        return true;
    }
    return pair_checked.pairs[entity_1][entity_2];
}
void add_pair(Entity entity_1, Entity entity_2) {
    if(entity_1 >= MAX_ENTITIES || entity_2 >= MAX_ENTITIES) {
        return;
    }
    pair_checked.pairs[entity_1][entity_2] = true;
    pair_checked.pairs[entity_2][entity_1] = true;
}

void grid_update_aabb(Entity entity) {
    if(entity >= MAX_ENTITIES) {
        return;
    }
    (void)AABBPool_store_at(&aabbs_pool, entity, math_create_aabb(world_hit_boxes[entity]));
}

void clear_grid(void) {
    memset(&grid, 0, sizeof(Grid));
    memset(&pair_checked, 0, sizeof(BooleanPairs));
}

uint32_t world_x_to_col(Vec1D x) {
    float col = ((GRID_COLS - 1)/2) + (x/(CELL_SIZE));
    if(col >= GRID_COLS) {
        return GRID_COLS - 1;
    }
    if(col < 0) {
        return 0;
    }
    return col;
}

uint32_t world_y_to_row(Vec1D y) {
    float row = ((GRID_ROWS - 1)/2) + (y/(CELL_SIZE));
    if(row >= GRID_ROWS) {
        return GRID_ROWS - 1;
    }
    if(row < 0) {
        return 0;
    }
    return row;
}

void add_entity_to_grid(Entity entity, uint32_t row, uint32_t col) {
    Cell *cell;

    if(entity >= MAX_ENTITIES || row >= GRID_ROWS || col >= GRID_COLS) {
        return;
    }
    cell = &grid.cells[row][col];
    if(cell->entity_count >= GRID_CELL_ENTITY_CAPACITY) return;
    cell->entities[cell->entity_count++] = entity;
}

void add_entity_to_grids(Entity entity) {
    if(entity >= MAX_ENTITIES) {
        return;
    }
    AABB aabb = aabbs[entity];
    uint32_t top_row = world_y_to_row(aabb.max_y);
    uint32_t bot_row = world_y_to_row(aabb.min_y);
    uint32_t left_col = world_x_to_col(aabb.min_x);
    uint32_t right_col = world_x_to_col(aabb.max_x);

    for(uint32_t row = bot_row; row <= top_row; row += 1) {
        for(uint32_t col = left_col; col <= right_col; col += 1) {
            add_entity_to_grid(entity, row, col);
        }
    }
}
