#ifndef SHAPE_DECOMPOSITION_H
#define SHAPE_DECOMPOSITION_H

#include "math2d.h"

bool physics_shape_collision_prepare(Shape source, Shape *prepared);
uint8_t physics_shape_collision_piece_count_get(const Shape *shape);
Shape physics_shape_collision_piece_get(const Shape *shape, uint8_t index);

#endif
