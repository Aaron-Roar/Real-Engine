/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics/soft_body/soft_body.h"

Shape soft_body_boundary_shape_create(
    Position start,
    Position end,
    float radius
) {
    Vec2D delta = math_vector_subtract(end, start);
    float length = math_vector_magnitude(delta);
    Vec2D normal;

    if(length <= 0.0001f) return (Shape){0};
    normal = (Vec2D){-delta.y * radius / length, delta.x * radius / length};
    return (Shape){
        .amount_of_vertices = 4,
        .vertices = {
            {start.x + normal.x, start.y + normal.y},
            {end.x + normal.x, end.y + normal.y},
            {end.x - normal.x, end.y - normal.y},
            {start.x - normal.x, start.y - normal.y}
        }
    };
}
