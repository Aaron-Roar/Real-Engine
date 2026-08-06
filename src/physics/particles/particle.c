#include "physics.h"

#include <math.h>

OverlapInfo physics_particle_overlap_get(Shape first, Shape second) {
    Position first_center = math_polygon_centroid(first);
    Position second_center = math_polygon_centroid(second);
    float first_radius = math_circle_radius(first, first_center);
    float second_radius = math_circle_radius(second, second_center);
    Vec2D delta = math_vector_subtract(second_center, first_center);
    float distance_squared = math_dot_product(delta, delta);
    float radius_sum = first_radius + second_radius;
    float distance;
    Vec2D normal;

    if(distance_squared >= radius_sum * radius_sum) {
        return (OverlapInfo){.detected = false};
    }
    distance = sqrtf(distance_squared);
    if(distance > 0.00001f) {
        normal = (Vec2D){delta.x / distance, delta.y / distance};
    } else {
        normal = (Vec2D){1.0f, 0.0f};
        distance = 0.0f;
    }
    return (OverlapInfo){
        .detected = true,
        .normal = normal,
        .depth = radius_sum - distance
    };
}

Vec1D physics_circle_moment_of_inertia(Shape circle, Mass mass_value) {
    Vec1D radius = math_circle_radius(circle, math_polygon_centroid(circle));
    Vec1D area = PI_F * radius * radius;
    Vec1D density = mass_value / fabsf(area);
    Vec1D area_moment = 0.5f * area * radius * radius;

    return density * area_moment;
}
