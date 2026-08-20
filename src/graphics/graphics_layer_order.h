/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_GRAPHICS_LAYER_ORDER_H
#define ROHR_GRAPHICS_LAYER_ORDER_H

typedef struct GraphicsLayerOrder {
    int layer;
} GraphicsLayerOrder;

int graphics_layer_order_compare(const void *left, const void *right);

#endif
