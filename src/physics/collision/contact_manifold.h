#ifndef CONTACT_MANIFOLD_H
#define CONTACT_MANIFOLD_H

#include "math2d.h"

#define CONTACT_MANIFOLD_POINT_MAX 2

typedef struct ContactManifold {
    Vec2D points[CONTACT_MANIFOLD_POINT_MAX];
    uint32_t feature_ids[CONTACT_MANIFOLD_POINT_MAX];
    uint8_t count;
} ContactManifold;

ContactManifold contact_manifold_polygon_get(
    Shape first,
    Shape second,
    Axis normal
);

#endif
