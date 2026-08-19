#include "physics.h"

#include <float.h>

OverlapInfo physics_sat_overlap_on_axes_get(Shape shape_1, Shape shape_2, Vec2DList axes, OverlapInfo overlap_info) {
    for (int i = 0; i < axes.amount_of_vectors; i += 1) {
        Axis axis = axes.vectors[i];

        Projection p1 = math_project_shape_on_axis(shape_1, axis);
        Projection p2 = math_project_shape_on_axis(shape_2, axis);

        float overlap = math_projection_overlap(p1, p2);

        if (overlap <= 0.0f) {
            return (OverlapInfo){ .detected = false };
        }

        if (overlap < overlap_info.depth) {
            overlap_info.depth = overlap;
            overlap_info.normal = axis;
        }
    }

    return overlap_info;
}
OverlapInfo physics_sat_overlap_get(Shape shape_1, Shape shape_2)
{
    OverlapInfo collision = {
        .detected = true,
        .normal = {0},
        .depth = FLT_MAX
    };

    Vec2DList shape1_axes = math_vectors_normalize(math_normals_create(shape_1));
    Vec2DList shape2_axes = math_vectors_normalize(math_normals_create(shape_2));

    collision = physics_sat_overlap_on_axes_get(shape_1, shape_2, shape1_axes, collision);

    if (!collision.detected) {
        return collision;
    }

    collision = physics_sat_overlap_on_axes_get(shape_1, shape_2, shape2_axes, collision);

    if (!collision.detected) {
        return collision;
    }

    Position c1 = math_polygon_centroid(shape_1);
    Position c2 = math_polygon_centroid(shape_2);

    Vec2D center_delta = {
        .x = c2.x - c1.x,
        .y = c2.y - c1.y
    };

    if (math_dot_product(center_delta, collision.normal) < 0.0f) {
        collision.normal.x *= -1.0f;
        collision.normal.y *= -1.0f;
    }

    return collision;
}
Position physics_approximate_contact_point(Position p1, Position p2)
{
    return (Position){
        .x = (p1.x + p2.x) * 0.5f,
        .y = (p1.y + p2.y) * 0.5f
    };
}
