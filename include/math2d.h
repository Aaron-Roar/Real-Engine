/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef MATH_2D_H
#define MATH_2D_H
#include <stdint.h>
#include <stdbool.h>

#define MAX_VECTORS 100
#define MAX_VERTICIES 50
#define MIN_VERTICIES 3
#define MAX_CONCAVE_PIECES (MAX_VERTICIES - 2)

#define PI_F 3.14159265358979323846f

/** RGBA color value shared by rendering-facing data structures. */
typedef struct Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} Color;

/** Scalar one-dimensional math value. */
typedef float Vec1D;

/** Two-dimensional vector. */
typedef struct {
    /** Horizontal component. */
    float x;
    /** Vertical component. */
    float y;
} Vec2D;

/** Fixed list of scalar values. */
typedef struct Vec1DList {
    /** Number of valid values. */
    uint16_t amount_of_vectors;
    /** Stored scalar values. */
    Vec1D vectors[MAX_VECTORS];
} Vec1DList;

/** Fixed list of 2D vectors. */
typedef struct Vec2DList {
    /** Number of valid vectors. */
    uint16_t amount_of_vectors;
    /** Stored vectors. */
    Vec2D vectors[MAX_VECTORS];
} Vec2DList;

/** Triangle indexing the original vertices of an internally decomposed shape. */
typedef struct ShapeConcavePiece {
    uint8_t vertex_indices[3];
} ShapeConcavePiece;

/** Polygon or circle-like shape in local or world space. */
typedef struct Shape {
    /** Number of valid vertices. */
    uint16_t amount_of_vertices;
    /** Vertex coordinates. Local shapes must be translated before rendering/collision. */
    Vec2D vertices[MAX_VERTICIES]; //Local coordinate of vertices. Must be translated to world coordinate
    /** Internal convex decomposition; zero pieces keeps the convex fast path. */
    ShapeConcavePiece concave_pieces[MAX_CONCAVE_PIECES];
    /** Number of cached pieces generated for a concave polygon. */
    uint8_t concave_piece_count;
    /** Internal marker distinguishing prepared convex shapes from raw input. */
    bool collision_geometry_prepared;
} Shape;

/** Axis vector used for projection and constraints. */
typedef Vec2D Axis;

/** Projection interval on an axis. */
typedef struct Projection {
    /** Minimum projected coordinate. */
    float min;
    /** Maximum projected coordinate. */
    float max;
} Projection;

/** Axis-aligned bounding box. */
typedef struct AABB {
    /** Minimum x coordinate. */
    float min_x;
    /** Maximum x coordinate. */
    float max_x;
    /** Minimum y coordinate. */
    float min_y;
    /** Maximum y coordinate. */
    float max_y;
} AABB;

/** Create edge normals for a shape. */
Vec2DList math_normals_create(Shape shape);
/** Normalize a vector. */
Vec2D math_vector_normalize(Vec2D vector);
/** Normalize all vectors in a list. */
Vec2DList math_vectors_normalize(Vec2DList vectors);
/** Compute dot product of two vectors. */
float math_dot_product(Vec2D vector_1, Vec2D vector_2);
/** Create an axis-aligned rectangular shape centered on local origin. */
Shape math_square_create(float width, float height);
/** Create a polygon approximation of a circle. */
Shape math_circle_create(float radius, uint8_t verticies);
/** Project a shape onto an axis. */
Projection math_project_shape_on_axis(Shape shape, Axis axis);
/** Compute 2D scalar cross product. */
float math_cross_2d(Vec2D a, Vec2D b);
/** Compute angular velocity cross a radius vector. */
Vec2D math_angular_velocity_cross_vec(float omega, Vec2D r);
/** Project a vector onto an axis. */
Vec2D math_project_onto_axis(Vec2D v, Axis axis);
/** Compute axis magnitude. */
float math_axis_magnitude(Axis axis);
/** Compute vector magnitude. */
float math_vector_magnitude(Vec2D vector);
/** Rotate a vector by an angle in radians. */
Vec2D math_vector_rotate(Vec2D vector, float angle);
/** Approximate circle radius from centroid to first vertex. */
Vec1D math_circle_radius(Shape circle, Vec2D centroid);
/** Subtract vector_b from vector_a. */
Vec2D math_vector_subtract(Vec2D vector_a, Vec2D vector_b);
/** Compute overlap depth between two circle-like shapes. */
Vec1D math_circle_overlap_depth(Vec2D centroid_1, Vec1D radius_1, Vec2D centroid_2, Vec1D radius_2);
/** Compute overlap between two projection intervals. */
float math_projection_overlap(Projection projection_1, Projection projection_2);
/** Scale a shape uniformly around its centroid. */
Shape math_shape_scale(Shape shape, float scale);
/** Scale a shape vertically around its centroid. */
Shape math_shape_y_scale(Shape shape, float scale);
/** Scale a shape horizontally around its centroid. */
Shape math_shape_x_scale(Shape shape, float scale);
/** Compute polygon centroid. */
Vec2D math_polygon_centroid(Shape shape);
/** Add one vertex to a circle-like shape. */
Shape math_vertex_add(Shape shape);
/** Remove one vertex from a circle-like shape. */
Shape math_vertex_delete(Shape shape);
/** Create an axis-aligned bounding box around a world-space shape. */
AABB math_aabb_create(Shape world_shape);
/** Return whether two axis-aligned bounding boxes overlap. */
bool math_aabb_overlap(AABB a, AABB b);
#endif
