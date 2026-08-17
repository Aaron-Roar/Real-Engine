#ifndef ROHR_GRAPHICS_LAYER_ORDER_H
#define ROHR_GRAPHICS_LAYER_ORDER_H

typedef struct GraphicsLayerOrder {
    int layer;
} GraphicsLayerOrder;

int graphics_layer_order_compare(const void *left, const void *right);

#endif
