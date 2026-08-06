#include "contact_manifold.h"

#include <math.h>

int main(void) {
    Shape first = {
        .amount_of_vertices = 4,
        .vertices = {{-1.0f, -1.0f}, {1.0f, -1.0f},
            {1.0f, 1.0f}, {-1.0f, 1.0f}}
    };
    Shape second = {
        .amount_of_vertices = 4,
        .vertices = {{0.5f, -1.0f}, {2.5f, -1.0f},
            {2.5f, 1.0f}, {0.5f, 1.0f}}
    };
    ContactManifold manifold = contact_manifold_polygon_get(
        first, second, (Axis){1.0f, 0.0f});

    if(manifold.count != 2) return 1;
    for(uint8_t i = 0; i < manifold.count; i += 1) {
        if(fabsf(manifold.points[i].x - 0.75f) > 0.0001f ||
                fabsf(fabsf(manifold.points[i].y) - 1.0f) > 0.0001f) return 1;
    }
    return 0;
}
