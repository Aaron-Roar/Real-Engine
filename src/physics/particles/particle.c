/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics.h"
#include "physics/physics_internal.h"

#include <math.h>

static ParticleGeometry physics_particle_geometry_effective_get(EntityIndex index) {
    ParticleGeometry geometry = {0};

    if(index < particle_geometries_pool.capacity &&
            particle_geometries_pool.used[index])
        geometry = particle_geometries_pool.objects[index];
    if(!geometry.radius_explicit && index < hit_boxes_pool.capacity &&
            hit_boxes_pool.used[index]) {
        Position center = math_polygon_centroid(hit_boxes[index]);
        geometry.radius = math_circle_radius(hit_boxes[index], center);
    }
    return geometry;
}

Position physics_particle_world_origin_by_index_get(EntityIndex index) {
    ParticleGeometry geometry = physics_particle_geometry_effective_get(index);
    float angle = index < orientations_pool.capacity && orientations_pool.used[index]
        ? orientations[index] : 0.0f;
    Position body = index < positions_pool.capacity && positions_pool.used[index]
        ? positions[index] : (Position){0};
    float cosine = cosf(angle);
    float sine = sinf(angle);

    return (Position){
        body.x + geometry.local_origin.x * cosine - geometry.local_origin.y * sine,
        body.y + geometry.local_origin.x * sine + geometry.local_origin.y * cosine
    };
}

float physics_particle_radius_by_index_get(EntityIndex index) {
    return physics_particle_geometry_effective_get(index).radius;
}

EngineResult physics_particle_origin_set(Entity entity, Position local_origin) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);
    ParticleGeometry geometry;

    if(error_check(result)) return result;
    if(!isfinite(local_origin.x) || !isfinite(local_origin.y))
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    geometry = physics_particle_geometry_effective_get(index);
    geometry.local_origin = local_origin;
    geometry.origin_explicit = true;
    (void)ParticleGeometryPool_store_at(
        &particle_geometries_pool, index, geometry);
    entity_mask[index] |= ROHR_PARTICLE;
    return error_result_value(true);
}

PositionResult physics_particle_origin_get(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(error_check(result))
        return ERROR_RESULT_MAKE_ERROR(PositionResult, result.result.error);
    if(!entity_index_components_check(index, ROHR_PARTICLE))
        return ERROR_RESULT_MAKE_ERROR(
            PositionResult, ERROR_ENGINE_COMPONENT_MISSING);
    return ERROR_RESULT_MAKE_VALUE(PositionResult,
        physics_particle_geometry_effective_get(index).local_origin);
}

EngineResult physics_particle_radius_set(Entity entity, float radius) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);
    ParticleGeometry geometry;

    if(error_check(result)) return result;
    if(!isfinite(radius) || radius <= 0.0f)
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    geometry = physics_particle_geometry_effective_get(index);
    geometry.radius = radius;
    geometry.radius_explicit = true;
    (void)ParticleGeometryPool_store_at(
        &particle_geometries_pool, index, geometry);
    entity_mask[index] |= ROHR_PARTICLE;
    return error_result_value(true);
}

ParticleRadiusResult physics_particle_radius_get(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(error_check(result))
        return ERROR_RESULT_MAKE_ERROR(
            ParticleRadiusResult, result.result.error);
    if(!entity_index_components_check(index, ROHR_PARTICLE))
        return ERROR_RESULT_MAKE_ERROR(
            ParticleRadiusResult, ERROR_ENGINE_COMPONENT_MISSING);
    return ERROR_RESULT_MAKE_VALUE(ParticleRadiusResult,
        physics_particle_radius_by_index_get(index));
}

static OverlapInfo physics_particle_values_overlap_get(
    Position first_center,
    float first_radius,
    Position second_center,
    float second_radius
) {
    Vec2D delta = math_vector_subtract(second_center, first_center);
    float distance_squared = math_dot_product(delta, delta);
    float radius_sum = first_radius + second_radius;
    float distance;
    Vec2D normal;

    if(distance_squared >= radius_sum * radius_sum)
        return (OverlapInfo){.detected = false};
    distance = sqrtf(distance_squared);
    normal = distance > 0.00001f
        ? (Vec2D){delta.x / distance, delta.y / distance}
        : (Vec2D){1.0f, 0.0f};
    return (OverlapInfo){
        .detected = true,
        .normal = normal,
        .depth = radius_sum - distance
    };
}

OverlapInfo physics_particle_entities_overlap_get(
    EntityIndex first,
    EntityIndex second
) {
    return physics_particle_values_overlap_get(
        physics_particle_world_origin_by_index_get(first),
        physics_particle_radius_by_index_get(first),
        physics_particle_world_origin_by_index_get(second),
        physics_particle_radius_by_index_get(second));
}

OverlapInfo physics_particle_overlap_get(Shape first, Shape second) {
    Position first_center = math_polygon_centroid(first);
    Position second_center = math_polygon_centroid(second);
    float first_radius = math_circle_radius(first, first_center);
    float second_radius = math_circle_radius(second, second_center);
    return physics_particle_values_overlap_get(
        first_center, first_radius, second_center, second_radius);
}

Vec1D physics_circle_moment_of_inertia(Shape circle, Mass mass_value) {
    Vec1D radius = math_circle_radius(circle, math_polygon_centroid(circle));
    Vec1D area = PI_F * radius * radius;
    Vec1D density = mass_value / fabsf(area);
    Vec1D area_moment = 0.5f * area * radius * radius;

    return density * area_moment;
}
