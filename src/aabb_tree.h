#ifndef AABB_TREE_H
#define AABB_TREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "entity_components.h"
#include "math2d.h"

#define AABB_TREE_NODE_INVALID UINT32_MAX

typedef struct AABBTreeNode {
    AABB bounds;
    Entity entity;
    uint32_t parent;
    uint32_t left;
    uint32_t right;
    int32_t height;
} AABBTreeNode;

typedef struct AABBTree {
    AABBTreeNode *nodes;
    uint32_t *query_stack;
    size_t capacity;
    size_t count;
    uint32_t root;
} AABBTree;

typedef bool (*AABBTreeQueryCallback)(Entity entity, void *context);

EngineResult aabb_tree_init(AABBTree *tree, size_t capacity);
EngineResult aabb_tree_insert(AABBTree *tree, Entity entity, AABB bounds);
bool aabb_tree_query(
    AABBTree *tree,
    AABB bounds,
    AABBTreeQueryCallback callback,
    void *context
);
void aabb_tree_clear(AABBTree *tree);
void aabb_tree_destroy(AABBTree *tree);
bool aabb_tree_node_leaf_check(const AABBTreeNode *node);

#endif
