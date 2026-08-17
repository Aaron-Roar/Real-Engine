#include "graphics_command_order.h"

int graphics_command_order_compare(const void *left_pointer,
        const void *right_pointer) {
    const GraphicsCommandOrder *left = left_pointer;
    const GraphicsCommandOrder *right = right_pointer;
    if(left->layer < right->layer) return -1;
    if(left->layer > right->layer) return 1;
    if(left->sequence < right->sequence) return -1;
    if(left->sequence > right->sequence) return 1;
    return 0;
}
