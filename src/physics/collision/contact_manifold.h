#ifndef CONTACT_MANIFOLD_H
#define CONTACT_MANIFOLD_H

#include "math2d.h"

#define CONTACT_MANIFOLD_POINT_MAX 2
#define CONTACT_MANIFOLD_MAX 8

typedef struct ContactManifold {
    Axis normal;
    float depth;
    Vec2D points[CONTACT_MANIFOLD_POINT_MAX];
    uint32_t feature_ids[CONTACT_MANIFOLD_POINT_MAX];
    uint8_t count;
} ContactManifold;

typedef struct ContactManifoldSet {
    ContactManifold values[CONTACT_MANIFOLD_MAX];
    uint8_t count;
} ContactManifoldSet;

ContactManifoldSet contact_manifold_set_polygon_get(Shape first, Shape second);

ContactManifold contact_manifold_polygon_get(
    Shape first,
    Shape second,
    Axis normal
);

#endif
