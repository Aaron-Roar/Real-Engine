/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef SOFT_BODY_INTERNAL_H
#define SOFT_BODY_INTERNAL_H

#include "physics.h"

Shape soft_body_boundary_shape_create(
    Position start,
    Position end,
    float radius
);

#endif
