/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef OVERLAP_GEOMETRY_H
#define OVERLAP_GEOMETRY_H

#include "physics.h"

OverlapInfo physics_sat_collision_piece_overlap_get(
    const Shape *first_source,
    uint8_t first_index,
    const Shape *second_source,
    uint8_t second_index
);

#endif
