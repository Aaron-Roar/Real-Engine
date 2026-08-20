/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "aabb_tree.h"

#include <stdlib.h>

#define AABB_TREE_MIN_CAPACITY 16

static AABB aabb_tree_bounds_combine(AABB first, AABB second) {
    return (AABB){
        .min_x = first.min_x < second.min_x ? first.min_x : second.min_x,
        .max_x = first.max_x > second.max_x ? first.max_x : second.max_x,
        .min_y = first.min_y < second.min_y ? first.min_y : second.min_y,
        .max_y = first.max_y > second.max_y ? first.max_y : second.max_y
    };
}

static float aabb_tree_perimeter(AABB bounds) {
    return 2.0f * (
        bounds.max_x - bounds.min_x + bounds.max_y - bounds.min_y
    );
}

bool aabb_tree_node_leaf_check(const AABBTreeNode *node) {
    return node != NULL && node->left == AABB_TREE_NODE_INVALID;
}

static EngineResult aabb_tree_reserve(AABBTree *tree, size_t requested) {
    AABBTreeNode *nodes;
    uint32_t *query_stack;
    size_t capacity;

    if(tree == NULL) return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    if(requested <= tree->capacity) return error_result_value(true);
    capacity = tree->capacity == 0 ? AABB_TREE_MIN_CAPACITY : tree->capacity;
    while(capacity < requested) {
        if(capacity > SIZE_MAX / 2) {
            return error_result_error(ERROR_MEMORY_POOL_CAPACITY_OVERFLOW);
        }
        capacity *= 2;
    }
    if(capacity > SIZE_MAX / sizeof(*nodes) ||
            capacity > SIZE_MAX / sizeof(*query_stack)) {
        return error_result_error(ERROR_MEMORY_POOL_CAPACITY_OVERFLOW);
    }
    nodes = realloc(tree->nodes, capacity * sizeof(*nodes));
    if(nodes == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    tree->nodes = nodes;
    query_stack = realloc(tree->query_stack, capacity * sizeof(*query_stack));
    if(query_stack == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    tree->query_stack = query_stack;
    tree->capacity = capacity;
    return error_result_value(true);
}

EngineResult aabb_tree_init(AABBTree *tree, size_t capacity) {
    if(tree == NULL) return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    *tree = (AABBTree){.root = AABB_TREE_NODE_INVALID};
    if(capacity == 0) return error_result_value(true);
    return aabb_tree_reserve(tree, capacity);
}

static uint32_t aabb_tree_balance(AABBTree *tree, uint32_t a_index) {
    AABBTreeNode *a = &tree->nodes[a_index];
    uint32_t b_index;
    uint32_t c_index;
    AABBTreeNode *b;
    AABBTreeNode *c;
    int balance;

    if(aabb_tree_node_leaf_check(a) || a->height < 2) return a_index;
    b_index = a->left;
    c_index = a->right;
    b = &tree->nodes[b_index];
    c = &tree->nodes[c_index];
    balance = c->height - b->height;

    if(balance > 1) {
        uint32_t f_index = c->left;
        uint32_t g_index = c->right;
        AABBTreeNode *f = &tree->nodes[f_index];
        AABBTreeNode *g = &tree->nodes[g_index];

        c->left = a_index;
        c->parent = a->parent;
        a->parent = c_index;
        if(c->parent == AABB_TREE_NODE_INVALID) tree->root = c_index;
        else if(tree->nodes[c->parent].left == a_index) tree->nodes[c->parent].left = c_index;
        else tree->nodes[c->parent].right = c_index;
        if(f->height > g->height) {
            c->right = f_index;
            a->right = g_index;
            g->parent = a_index;
            a->bounds = aabb_tree_bounds_combine(b->bounds, g->bounds);
            c->bounds = aabb_tree_bounds_combine(a->bounds, f->bounds);
            a->height = 1 + (b->height > g->height ? b->height : g->height);
            c->height = 1 + (a->height > f->height ? a->height : f->height);
        } else {
            c->right = g_index;
            a->right = f_index;
            f->parent = a_index;
            a->bounds = aabb_tree_bounds_combine(b->bounds, f->bounds);
            c->bounds = aabb_tree_bounds_combine(a->bounds, g->bounds);
            a->height = 1 + (b->height > f->height ? b->height : f->height);
            c->height = 1 + (a->height > g->height ? a->height : g->height);
        }
        return c_index;
    }
    if(balance < -1) {
        uint32_t d_index = b->left;
        uint32_t e_index = b->right;
        AABBTreeNode *d = &tree->nodes[d_index];
        AABBTreeNode *e = &tree->nodes[e_index];

        b->left = a_index;
        b->parent = a->parent;
        a->parent = b_index;
        if(b->parent == AABB_TREE_NODE_INVALID) tree->root = b_index;
        else if(tree->nodes[b->parent].left == a_index) tree->nodes[b->parent].left = b_index;
        else tree->nodes[b->parent].right = b_index;
        if(d->height > e->height) {
            b->right = d_index;
            a->left = e_index;
            e->parent = a_index;
            a->bounds = aabb_tree_bounds_combine(c->bounds, e->bounds);
            b->bounds = aabb_tree_bounds_combine(a->bounds, d->bounds);
            a->height = 1 + (c->height > e->height ? c->height : e->height);
            b->height = 1 + (a->height > d->height ? a->height : d->height);
        } else {
            b->right = e_index;
            a->left = d_index;
            d->parent = a_index;
            a->bounds = aabb_tree_bounds_combine(c->bounds, d->bounds);
            b->bounds = aabb_tree_bounds_combine(a->bounds, e->bounds);
            a->height = 1 + (c->height > d->height ? c->height : d->height);
            b->height = 1 + (a->height > e->height ? a->height : e->height);
        }
        return b_index;
    }
    return a_index;
}

static void aabb_tree_ancestors_update(AABBTree *tree, uint32_t index) {
    while(index != AABB_TREE_NODE_INVALID) {
        AABBTreeNode *node;
        AABBTreeNode *left;
        AABBTreeNode *right;

        index = aabb_tree_balance(tree, index);
        node = &tree->nodes[index];
        left = &tree->nodes[node->left];
        right = &tree->nodes[node->right];
        node->height = 1 + (left->height > right->height ? left->height : right->height);
        node->bounds = aabb_tree_bounds_combine(left->bounds, right->bounds);
        index = node->parent;
    }
}

EngineResult aabb_tree_insert(AABBTree *tree, Entity entity, AABB bounds) {
    EngineResult result;
    uint32_t leaf_index;

    if(tree == NULL || entity == ENTITY_INVALID) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    result = aabb_tree_reserve(tree, tree->count + 2);
    if(error_check(result)) return result;
    leaf_index = (uint32_t)tree->count++;
    tree->nodes[leaf_index] = (AABBTreeNode){
        .bounds = bounds,
        .entity = entity,
        .parent = AABB_TREE_NODE_INVALID,
        .left = AABB_TREE_NODE_INVALID,
        .right = AABB_TREE_NODE_INVALID,
        .height = 0
    };
    if(tree->root == AABB_TREE_NODE_INVALID) {
        tree->root = leaf_index;
        return error_result_value(true);
    }
    {
        uint32_t sibling = tree->root;
        while(!aabb_tree_node_leaf_check(&tree->nodes[sibling])) {
            AABBTreeNode *node = &tree->nodes[sibling];
            float left_cost = aabb_tree_perimeter(aabb_tree_bounds_combine(
                bounds, tree->nodes[node->left].bounds));
            float right_cost = aabb_tree_perimeter(aabb_tree_bounds_combine(
                bounds, tree->nodes[node->right].bounds));
            sibling = left_cost < right_cost ? node->left : node->right;
        }
        {
            uint32_t old_parent = tree->nodes[sibling].parent;
            uint32_t parent_index = (uint32_t)tree->count++;
            tree->nodes[parent_index] = (AABBTreeNode){
                .bounds = aabb_tree_bounds_combine(bounds, tree->nodes[sibling].bounds),
                .entity = ENTITY_INVALID,
                .parent = old_parent,
                .left = sibling,
                .right = leaf_index,
                .height = tree->nodes[sibling].height + 1
            };
            tree->nodes[sibling].parent = parent_index;
            tree->nodes[leaf_index].parent = parent_index;
            if(old_parent == AABB_TREE_NODE_INVALID) tree->root = parent_index;
            else if(tree->nodes[old_parent].left == sibling) tree->nodes[old_parent].left = parent_index;
            else tree->nodes[old_parent].right = parent_index;
            aabb_tree_ancestors_update(tree, parent_index);
        }
    }
    return error_result_value(true);
}

bool aabb_tree_query(
    AABBTree *tree,
    AABB bounds,
    AABBTreeQueryCallback callback,
    void *context
) {
    size_t count = 0;

    if(tree == NULL || callback == NULL || tree->root == AABB_TREE_NODE_INVALID) {
        return false;
    }
    tree->query_stack[count++] = tree->root;
    while(count > 0) {
        AABBTreeNode *node = &tree->nodes[tree->query_stack[--count]];
        if(!math_aabb_overlap(bounds, node->bounds)) continue;
        if(aabb_tree_node_leaf_check(node)) {
            if(!callback(node->entity, context)) return false;
        } else {
            tree->query_stack[count++] = node->left;
            tree->query_stack[count++] = node->right;
        }
    }
    return true;
}

void aabb_tree_clear(AABBTree *tree) {
    if(tree == NULL) return;
    tree->count = 0;
    tree->root = AABB_TREE_NODE_INVALID;
}

void aabb_tree_destroy(AABBTree *tree) {
    if(tree == NULL) return;
    free(tree->nodes);
    free(tree->query_stack);
    *tree = (AABBTree){.root = AABB_TREE_NODE_INVALID};
}
