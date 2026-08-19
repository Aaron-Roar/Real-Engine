#include "shape_decomposition.h"

#include <math.h>
#include <stddef.h>

#define SHAPE_EPSILON 0.00001f

static float shape_orientation(Vec2D a, Vec2D b, Vec2D c) {
    return math_cross_2d(math_vector_subtract(b, a),
        math_vector_subtract(c, a));
}

static bool shape_point_on_segment(Vec2D point, Vec2D first, Vec2D second) {
    return fabsf(shape_orientation(first, second, point)) <= SHAPE_EPSILON &&
        point.x >= fminf(first.x, second.x) - SHAPE_EPSILON &&
        point.x <= fmaxf(first.x, second.x) + SHAPE_EPSILON &&
        point.y >= fminf(first.y, second.y) - SHAPE_EPSILON &&
        point.y <= fmaxf(first.y, second.y) + SHAPE_EPSILON;
}

static bool shape_segments_intersect(Vec2D a, Vec2D b, Vec2D c, Vec2D d) {
    float ab_c = shape_orientation(a, b, c);
    float ab_d = shape_orientation(a, b, d);
    float cd_a = shape_orientation(c, d, a);
    float cd_b = shape_orientation(c, d, b);

    if(((ab_c > SHAPE_EPSILON && ab_d < -SHAPE_EPSILON) ||
            (ab_c < -SHAPE_EPSILON && ab_d > SHAPE_EPSILON)) &&
            ((cd_a > SHAPE_EPSILON && cd_b < -SHAPE_EPSILON) ||
            (cd_a < -SHAPE_EPSILON && cd_b > SHAPE_EPSILON))) return true;
    return (fabsf(ab_c) <= SHAPE_EPSILON && shape_point_on_segment(c, a, b)) ||
        (fabsf(ab_d) <= SHAPE_EPSILON && shape_point_on_segment(d, a, b)) ||
        (fabsf(cd_a) <= SHAPE_EPSILON && shape_point_on_segment(a, c, d)) ||
        (fabsf(cd_b) <= SHAPE_EPSILON && shape_point_on_segment(b, c, d));
}

static float shape_signed_area_get(const Shape *shape) {
    float twice_area = 0.0f;
    for(uint16_t i = 0; i < shape->amount_of_vertices; i += 1) {
        Vec2D first = shape->vertices[i];
        Vec2D second = shape->vertices[(i + 1) % shape->amount_of_vertices];
        twice_area += first.x * second.y - second.x * first.y;
    }
    return twice_area * 0.5f;
}

static bool shape_simple_check(const Shape *shape) {
    uint16_t count = shape->amount_of_vertices;
    if(count < MIN_VERTICIES || count > MAX_VERTICIES) return false;
    for(uint16_t i = 0; i < count; i += 1) {
        Vec2D first = shape->vertices[i];
        Vec2D second = shape->vertices[(i + 1) % count];
        Vec2D edge = math_vector_subtract(second, first);
        if(math_dot_product(edge, edge) <= SHAPE_EPSILON * SHAPE_EPSILON)
            return false;
        for(uint16_t j = (uint16_t)(i + 1); j < count; j += 1) {
            uint16_t i_next = (uint16_t)((i + 1) % count);
            uint16_t j_next = (uint16_t)((j + 1) % count);
            if(i == j || i_next == j || j_next == i) continue;
            if(shape_segments_intersect(first, second,
                    shape->vertices[j], shape->vertices[j_next])) return false;
        }
    }
    return fabsf(shape_signed_area_get(shape)) > SHAPE_EPSILON;
}

static bool shape_convex_check(const Shape *shape, float winding) {
    for(uint16_t i = 0; i < shape->amount_of_vertices; i += 1) {
        Vec2D previous = shape->vertices[
            (i + shape->amount_of_vertices - 1) % shape->amount_of_vertices];
        Vec2D current = shape->vertices[i];
        Vec2D next = shape->vertices[(i + 1) % shape->amount_of_vertices];
        if(shape_orientation(previous, current, next) * winding < -SHAPE_EPSILON)
            return false;
    }
    return true;
}

static bool shape_point_in_triangle(Vec2D point, Vec2D a, Vec2D b, Vec2D c,
        float winding) {
    return shape_orientation(a, b, point) * winding >= -SHAPE_EPSILON &&
        shape_orientation(b, c, point) * winding >= -SHAPE_EPSILON &&
        shape_orientation(c, a, point) * winding >= -SHAPE_EPSILON;
}

static bool shape_concave_decompose(Shape *shape, float winding) {
    uint8_t remaining[MAX_VERTICIES];
    uint16_t count = shape->amount_of_vertices;

    for(uint16_t i = 0; i < count; i += 1) remaining[i] = (uint8_t)i;
    while(count > 3) {
        bool clipped = false;
        for(uint16_t i = 0; i < count; i += 1) {
            uint8_t previous = remaining[(i + count - 1) % count];
            uint8_t current = remaining[i];
            uint8_t next = remaining[(i + 1) % count];
            bool contains = false;
            if(shape_orientation(shape->vertices[previous],
                    shape->vertices[current], shape->vertices[next]) * winding <=
                    SHAPE_EPSILON) continue;
            for(uint16_t j = 0; j < count; j += 1) {
                uint8_t candidate = remaining[j];
                if(candidate == previous || candidate == current || candidate == next)
                    continue;
                if(shape_point_in_triangle(shape->vertices[candidate],
                        shape->vertices[previous], shape->vertices[current],
                        shape->vertices[next], winding)) {
                    contains = true;
                    break;
                }
            }
            if(contains) continue;
            shape->concave_pieces[shape->concave_piece_count++] =
                (ShapeConcavePiece){{previous, current, next}};
            for(uint16_t j = (uint16_t)(i + 1); j < count; j += 1)
                remaining[j - 1] = remaining[j];
            count -= 1;
            clipped = true;
            break;
        }
        if(!clipped) return false;
    }
    shape->concave_pieces[shape->concave_piece_count++] =
        (ShapeConcavePiece){{remaining[0], remaining[1], remaining[2]}};
    return true;
}

bool physics_shape_collision_prepare(Shape source, Shape *prepared) {
    float area;
    float winding;
    if(prepared == NULL || !shape_simple_check(&source)) return false;
    source.concave_piece_count = 0;
    source.collision_geometry_prepared = false;
    area = shape_signed_area_get(&source);
    winding = area > 0.0f ? 1.0f : -1.0f;
    if(!shape_convex_check(&source, winding) &&
            !shape_concave_decompose(&source, winding)) return false;
    source.collision_geometry_prepared = true;
    *prepared = source;
    return true;
}

uint8_t physics_shape_collision_piece_count_get(const Shape *shape) {
    if(shape == NULL) return 0;
    return shape->concave_piece_count == 0 ? 1 : shape->concave_piece_count;
}

Shape physics_shape_collision_piece_get(const Shape *shape, uint8_t index) {
    Shape piece = {0};
    if(shape == NULL) return piece;
    if(shape->concave_piece_count == 0) return index == 0 ? *shape : piece;
    if(index >= shape->concave_piece_count) return piece;
    piece.amount_of_vertices = 3;
    piece.collision_geometry_prepared = true;
    for(uint8_t i = 0; i < 3; i += 1)
        piece.vertices[i] = shape->vertices[
            shape->concave_pieces[index].vertex_indices[i]];
    return piece;
}
