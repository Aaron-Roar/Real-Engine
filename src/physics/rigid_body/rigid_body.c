/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics.h"

#include <math.h>

Shape physics_shape_world_translate(
    Shape shape,
    Position position,
    Orientation angle
) {
    Shape world_shape = shape;
    Position center = math_polygon_centroid(shape);
    float cosine = cosf(angle);
    float sine = sinf(angle);

    for(uint16_t i = 0; i < shape.amount_of_vertices; i += 1) {
        float x = shape.vertices[i].x - center.x;
        float y = shape.vertices[i].y - center.y;

        world_shape.vertices[i] = (Position){
            position.x + x * cosine - y * sine,
            position.y + x * sine + y * cosine
        };
    }
    return world_shape;
}

float physics_polygon_moment_of_inertia(Shape shape, Mass mass_value) {
    Position center = math_polygon_centroid(shape);
    float area_sum = 0.0f;
    float inertia_sum = 0.0f;

    for(uint16_t i = 0; i < shape.amount_of_vertices; i += 1) {
        uint16_t next = (uint16_t)((i + 1) % shape.amount_of_vertices);
        float first_x = shape.vertices[i].x - center.x;
        float first_y = shape.vertices[i].y - center.y;
        float second_x = shape.vertices[next].x - center.x;
        float second_y = shape.vertices[next].y - center.y;
        float cross = first_x * second_y - second_x * first_y;
        float squared_sum =
            first_x * first_x + first_x * second_x + second_x * second_x +
            first_y * first_y + first_y * second_y + second_y * second_y;

        area_sum += cross;
        inertia_sum += cross * squared_sum;
    }
    {
        float area = 0.5f * area_sum;
        float area_moment = inertia_sum / 12.0f;

        if(fabsf(area) < 1e-8f) return 0.0f;
        return mass_value / fabsf(area) * fabsf(area_moment);
    }
}
