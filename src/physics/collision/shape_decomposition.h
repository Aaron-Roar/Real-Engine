/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef SHAPE_DECOMPOSITION_H
#define SHAPE_DECOMPOSITION_H

#include "math2d.h"

bool physics_shape_collision_prepare(Shape source, Shape *prepared);
uint8_t physics_shape_collision_piece_count_get(const Shape *shape);
Shape physics_shape_collision_piece_get(const Shape *shape, uint8_t index);
bool physics_shape_collision_piece_edge_boundary_check(
    const Shape *shape,
    uint8_t piece_index,
    uint8_t edge_index
);

#endif
