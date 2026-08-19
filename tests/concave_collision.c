#include "physics.h"
#include "physics/collision/contact_manifold.h"
#include "physics/collision/shape_decomposition.h"

#include <math.h>

static Shape l_shape_get(void) {
    return (Shape){
        .amount_of_vertices = 6,
        .vertices = {
            {0.0f, 0.0f}, {4.0f, 0.0f}, {4.0f, 1.0f},
            {1.0f, 1.0f}, {1.0f, 4.0f}, {0.0f, 4.0f}
        }
    };
}

static Shape square_get(float min_x, float min_y, float max_x, float max_y) {
    return (Shape){
        .amount_of_vertices = 4,
        .vertices = {
            {min_x, min_y}, {max_x, min_y},
            {max_x, max_y}, {min_x, max_y}
        }
    };
}

static Shape notched_shape_get(void) {
    return (Shape){
        .amount_of_vertices = 5,
        .vertices = {
            {25.0f, 24.0f}, {25.0f, -25.0f}, {-1.0f, 3.0f},
            {-25.0f, -25.0f}, {-25.0f, 25.0f}
        }
    };
}

int main(void) {
    OverlapInfo overlap;
    Shape concave;
    Shape reversed = l_shape_get();
    Shape convex;
    Shape invalid = {
        .amount_of_vertices = 4,
        .vertices = {{0.0f, 0.0f}, {2.0f, 2.0f},
            {0.0f, 2.0f}, {2.0f, 0.0f}}
    };

    if(!physics_shape_collision_prepare(l_shape_get(), &concave)) return 1;
    if(concave.concave_piece_count != 4) return 2;
    if(!physics_shape_collision_prepare(square_get(0.0f, 0.0f, 1.0f, 1.0f),
            &convex)) return 3;
    if(convex.concave_piece_count != 0) return 4;

    for(uint16_t i = 0; i < reversed.amount_of_vertices / 2; i += 1) {
        Vec2D swap = reversed.vertices[i];
        uint16_t opposite = reversed.amount_of_vertices - i - 1;
        reversed.vertices[i] = reversed.vertices[opposite];
        reversed.vertices[opposite] = swap;
    }
    if(!physics_shape_collision_prepare(reversed, &reversed)) return 5;
    if(reversed.concave_piece_count != 4) return 6;
    if(physics_shape_collision_prepare(invalid, &invalid)) return 7;

    if(physics_sat_overlap_get(l_shape_get(),
            square_get(2.0f, 2.0f, 3.0f, 3.0f)).detected) return 8;
    overlap = physics_sat_overlap_get(l_shape_get(),
        square_get(0.25f, 2.0f, 0.75f, 3.0f));
    if(!overlap.detected) return 9;
    if(contact_manifold_polygon_get(l_shape_get(),
            square_get(0.25f, 2.0f, 0.75f, 3.0f), overlap.normal).count == 0)
        return 10;
    if(!physics_sat_overlap_get(l_shape_get(),
            square_get(0.5f, 0.5f, 1.5f, 1.5f)).detected) return 11;
    overlap = physics_sat_overlap_get(notched_shape_get(),
        square_get(24.0f, 8.0f, 27.0f, 12.0f));
    if(!overlap.detected || overlap.normal.x < 0.9f ||
            fabsf(overlap.normal.y) > 0.1f) return 12;

    return 0;
}
