#include "graphics_layer_order.h"

int graphics_layer_order_compare(const void *left_pointer,
        const void *right_pointer) {
    const GraphicsLayerOrder *left = left_pointer;
    const GraphicsLayerOrder *right = right_pointer;
    if(left->layer < right->layer) return -1;
    if(left->layer > right->layer) return 1;
    return 0;
}
