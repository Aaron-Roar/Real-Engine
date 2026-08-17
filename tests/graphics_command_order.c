#include "graphics/graphics_command_order.h"

#include <stdlib.h>

int main(void) {
    GraphicsCommandOrder commands[] = {
        {.layer = 20, .sequence = 0},
        {.layer = 1, .sequence = 1},
        {.layer = 20, .sequence = 2},
        {.layer = -4, .sequence = 3},
        {.layer = 1, .sequence = 4}
    };
    const GraphicsCommandOrder expected[] = {
        {.layer = -4, .sequence = 3},
        {.layer = 1, .sequence = 1},
        {.layer = 1, .sequence = 4},
        {.layer = 20, .sequence = 0},
        {.layer = 20, .sequence = 2}
    };
    qsort(commands, sizeof(commands) / sizeof(commands[0]), sizeof(commands[0]),
        graphics_command_order_compare);
    for(size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i += 1) {
        if(commands[i].layer != expected[i].layer ||
                commands[i].sequence != expected[i].sequence) return 1;
    }
    return 0;
}
