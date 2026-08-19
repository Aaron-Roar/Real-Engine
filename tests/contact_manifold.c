#include "physics/collision/contact_manifold.h"

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
    Shape concave = {
        .amount_of_vertices = 8,
        .vertices = {
            {-20.0f, -20.0f}, {-5.0f, -20.0f}, {-5.0f, -5.0f},
            {5.0f, -5.0f}, {5.0f, -20.0f}, {20.0f, -20.0f},
            {20.0f, 20.0f}, {-20.0f, 20.0f}
        }
    };
    Shape corner = {
        .amount_of_vertices = 4,
        .vertices = {
            {-7.0f, -7.0f}, {-3.0f, -7.0f},
            {-3.0f, -3.0f}, {-7.0f, -3.0f}
        }
    };
    ContactManifoldSet set;

    if(manifold.count != 2) return 1;
    for(uint8_t i = 0; i < manifold.count; i += 1) {
        if(fabsf(manifold.points[i].x - 0.75f) > 0.0001f ||
                fabsf(fabsf(manifold.points[i].y) - 1.0f) > 0.0001f) return 1;
    }
    set = contact_manifold_set_polygon_get(concave, corner);
    if(set.count < 2) return 2;
    if(fabsf(math_dot_product(set.values[0].normal,
            set.values[1].normal)) > 0.1f) return 3;
    return 0;
}
