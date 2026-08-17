#include "graphics/graphics_layer_order.h"

#include <stdlib.h>

int main(void) {
    GraphicsLayerOrder layers[] = {
        {.layer = 20},
        {.layer = 1},
        {.layer = -4},
        {.layer = 500}
    };
    const int expected[] = {-4, 1, 20, 500};
    qsort(layers, sizeof(layers) / sizeof(layers[0]), sizeof(layers[0]),
        graphics_layer_order_compare);
    for(size_t i = 0; i < sizeof(layers) / sizeof(layers[0]); i += 1) {
        if(layers[i].layer != expected[i]) return 1;
    }
    return 0;
}
