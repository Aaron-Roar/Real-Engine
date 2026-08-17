#ifndef ROHR_GRAPHICS_COMMAND_ORDER_H
#define ROHR_GRAPHICS_COMMAND_ORDER_H

#include <stdint.h>

typedef struct GraphicsCommandOrder {
    int layer;
    uint64_t sequence;
} GraphicsCommandOrder;

int graphics_command_order_compare(const void *left, const void *right);

#endif
