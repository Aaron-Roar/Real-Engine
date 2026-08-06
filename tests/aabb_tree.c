#include "aabb_tree.h"

#include <stddef.h>

typedef struct QueryResults {
    Entity entities[512];
    size_t count;
} QueryResults;

static bool result_add(Entity entity, void *context) {
    QueryResults *results = context;

    if(results == NULL || results->count >= 512) return false;
    results->entities[results->count++] = entity;
    return true;
}

static bool result_has(const QueryResults *results, Entity entity) {
    for(size_t i = 0; i < results->count; i += 1) {
        if(results->entities[i] == entity) return true;
    }
    return false;
}

int main(void) {
    AABBTree tree;
    QueryResults results = {0};
    AABB bounds[256];

    if(error_check(aabb_tree_init(&tree, 2)) ||
            error_check(aabb_tree_insert(
                &tree, 1, (AABB){0.0f, 2.0f, 0.0f, 2.0f})) ||
            error_check(aabb_tree_insert(
                &tree, 2, (AABB){1.0f, 3.0f, 1.0f, 3.0f})) ||
            error_check(aabb_tree_insert(
                &tree, 3, (AABB){100.0f, 102.0f, 100.0f, 102.0f}))) {
        aabb_tree_destroy(&tree);
        return 1;
    }
    if(!aabb_tree_query(
                &tree,
                (AABB){0.5f, 1.5f, 0.5f, 1.5f},
                result_add,
                &results
            ) || results.count != 2 || !result_has(&results, 1) ||
            !result_has(&results, 2) || result_has(&results, 3) ||
            tree.root == AABB_TREE_NODE_INVALID || tree.count != 5) {
        aabb_tree_destroy(&tree);
        return 1;
    }
    aabb_tree_clear(&tree);
    if(tree.count != 0 || tree.root != AABB_TREE_NODE_INVALID) {
        aabb_tree_destroy(&tree);
        return 1;
    }
    for(size_t i = 0; i < 256; i += 1) {
        float x = (float)((i * 37) % 97);
        float y = (float)((i * 53) % 89);
        float width = 0.5f + (float)(i % 11);
        float height = 0.5f + (float)(i % 7);

        bounds[i] = (AABB){x, x + width, y, y + height};
        if(error_check(aabb_tree_insert(&tree, (Entity)(i + 1), bounds[i]))) {
            aabb_tree_destroy(&tree);
            return 1;
        }
    }
    for(size_t query_index = 0; query_index < 256; query_index += 1) {
        results = (QueryResults){0};
        if(!aabb_tree_query(
                    &tree, bounds[query_index], result_add, &results)) {
            aabb_tree_destroy(&tree);
            return 1;
        }
        for(size_t candidate = 0; candidate < 256; candidate += 1) {
            if(math_aabb_overlap(bounds[query_index], bounds[candidate]) !=
                    result_has(&results, (Entity)(candidate + 1))) {
                aabb_tree_destroy(&tree);
                return 1;
            }
        }
    }
    aabb_tree_destroy(&tree);
    return 0;
}
