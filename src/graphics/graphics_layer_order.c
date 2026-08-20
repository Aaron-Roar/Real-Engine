/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "graphics_layer_order.h"

int graphics_layer_order_compare(const void *left_pointer,
        const void *right_pointer) {
    const GraphicsLayerOrder *left = left_pointer;
    const GraphicsLayerOrder *right = right_pointer;
    if(left->layer < right->layer) return -1;
    if(left->layer > right->layer) return 1;
    return 0;
}
