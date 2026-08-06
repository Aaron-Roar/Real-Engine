#include "contact_manifold.h"

#include <float.h>
#include <math.h>

typedef struct ContactFace {
    Vec2D first;
    Vec2D second;
    Axis normal;
    float alignment;
} ContactFace;

static ContactFace contact_face_best_get(Shape shape, Axis direction) {
    ContactFace result = {.alignment = -FLT_MAX};
    Vec2D center = math_polygon_centroid(shape);

    for(uint16_t i = 0; i < shape.amount_of_vertices; i += 1) {
        Vec2D first = shape.vertices[i];
        Vec2D second = shape.vertices[(i + 1) % shape.amount_of_vertices];
        Vec2D edge = math_vector_subtract(second, first);
        float length = math_vector_magnitude(edge);
        Vec2D midpoint;
        Axis normal;
        float alignment;

        if(length <= 0.00001f) continue;
        normal = (Axis){edge.y / length, -edge.x / length};
        midpoint = (Vec2D){
            (first.x + second.x) * 0.5f,
            (first.y + second.y) * 0.5f
        };
        if(math_dot_product(normal, math_vector_subtract(midpoint, center)) < 0.0f) {
            normal.x = -normal.x;
            normal.y = -normal.y;
        }
        alignment = math_dot_product(normal, direction);
        if(alignment > result.alignment) {
            result = (ContactFace){first, second, normal, alignment};
        }
    }
    return result;
}

static ContactFace contact_face_incident_get(Shape shape, Axis reference_normal) {
    ContactFace result = {.alignment = FLT_MAX};
    Vec2D center = math_polygon_centroid(shape);

    for(uint16_t i = 0; i < shape.amount_of_vertices; i += 1) {
        Vec2D first = shape.vertices[i];
        Vec2D second = shape.vertices[(i + 1) % shape.amount_of_vertices];
        Vec2D edge = math_vector_subtract(second, first);
        float length = math_vector_magnitude(edge);
        Vec2D midpoint;
        Axis normal;
        float alignment;

        if(length <= 0.00001f) continue;
        normal = (Axis){edge.y / length, -edge.x / length};
        midpoint = (Vec2D){
            (first.x + second.x) * 0.5f,
            (first.y + second.y) * 0.5f
        };
        if(math_dot_product(normal, math_vector_subtract(midpoint, center)) < 0.0f) {
            normal.x = -normal.x;
            normal.y = -normal.y;
        }
        alignment = math_dot_product(normal, reference_normal);
        if(alignment < result.alignment) {
            result = (ContactFace){first, second, normal, alignment};
        }
    }
    return result;
}

static uint8_t contact_segment_clip(
    Vec2D input[2],
    Vec2D output[2],
    Axis normal,
    float offset
) {
    float first_distance = math_dot_product(normal, input[0]) - offset;
    float second_distance = math_dot_product(normal, input[1]) - offset;
    uint8_t count = 0;

    if(first_distance <= 0.0f) output[count++] = input[0];
    if(second_distance <= 0.0f) output[count++] = input[1];
    if(first_distance * second_distance < 0.0f && count < 2) {
        float amount = first_distance / (first_distance - second_distance);
        output[count++] = (Vec2D){
            input[0].x + (input[1].x - input[0].x) * amount,
            input[0].y + (input[1].y - input[0].y) * amount
        };
    }
    return count;
}

ContactManifold contact_manifold_polygon_get(
    Shape first,
    Shape second,
    Axis normal
) {
    ContactManifold manifold = {0};
    ContactFace first_face;
    ContactFace second_face;
    ContactFace reference;
    ContactFace incident;
    Vec2D tangent;
    float tangent_length;
    Vec2D clipped_first[2];
    Vec2D clipped_second[2];
    uint8_t count;

    if(first.amount_of_vertices < 2 || second.amount_of_vertices < 2) return manifold;
    first_face = contact_face_best_get(first, normal);
    second_face = contact_face_best_get(second, (Axis){-normal.x, -normal.y});
    if(first_face.alignment >= second_face.alignment) {
        reference = first_face;
        incident = contact_face_incident_get(second, reference.normal);
    } else {
        reference = second_face;
        incident = contact_face_incident_get(first, reference.normal);
    }
    tangent = math_vector_subtract(reference.second, reference.first);
    tangent_length = math_vector_magnitude(tangent);
    if(tangent_length <= 0.00001f) return manifold;
    tangent.x /= tangent_length;
    tangent.y /= tangent_length;
    clipped_first[0] = incident.first;
    clipped_first[1] = incident.second;
    count = contact_segment_clip(
        clipped_first,
        clipped_second,
        (Axis){-tangent.x, -tangent.y},
        -math_dot_product(tangent, reference.first));
    if(count < 2) return manifold;
    count = contact_segment_clip(
        clipped_second,
        clipped_first,
        tangent,
        math_dot_product(tangent, reference.second));
    for(uint8_t i = 0; i < count && manifold.count < CONTACT_MANIFOLD_POINT_MAX; i += 1) {
        float separation = math_dot_product(
            reference.normal,
            math_vector_subtract(clipped_first[i], reference.first));

        if(separation > 0.001f) continue;
        manifold.points[manifold.count++] = (Vec2D){
            clipped_first[i].x - reference.normal.x * separation * 0.5f,
            clipped_first[i].y - reference.normal.y * separation * 0.5f
        };
    }
    return manifold;
}
