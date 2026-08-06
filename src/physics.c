#include "physics.h"
#include "physics_interaction_set.h"
#include "engine_internal.h"
#include "float.h"
#include <math.h>
#include "console.h"
#include "systems.h"

static Time physics_dt_per_tick = 0.0;
static bool physics_dt_overwritten = false;

MEMORY_DEFINE_OBJECT_POOL(PositionPool, Position)
MEMORY_DEFINE_OBJECT_POOL(VelocityPool, Velocity)
MEMORY_DEFINE_OBJECT_POOL(AccelerationPool, Acceleration)
MEMORY_DEFINE_OBJECT_POOL(MassPool, float)
MEMORY_DEFINE_OBJECT_POOL(ForcePool, Force)
MEMORY_DEFINE_OBJECT_POOL(ShapePool, Shape)
MEMORY_DEFINE_OBJECT_POOL(CollisionFilterConfigPool, CollisionFilterConfig)
MEMORY_DEFINE_OBJECT_POOL(OrientationPool, Orientation)
MEMORY_DEFINE_OBJECT_POOL(AngularVelocityPool, AngularVelocity)
MEMORY_DEFINE_OBJECT_POOL(AngularAccelerationPool, AngularAcceleration)
MEMORY_DEFINE_OBJECT_POOL(TorquePool, Torque)
MEMORY_DEFINE_OBJECT_POOL(FrictionPool, Friction)
MEMORY_DEFINE_OBJECT_POOL(RestitutionPool, Restitution)
MEMORY_DEFINE_OBJECT_POOL(AngleLockPool, AngleLock)
MEMORY_DEFINE_OBJECT_POOL(AxisLockPool, AxisLock)
MEMORY_DEFINE_OBJECT_POOL(TransformLockPool, TransformLock)
MEMORY_DEFINE_OBJECT_POOL(JointPool, Joint)
MEMORY_DEFINE_OBJECT_POOL(SoftBodyPool, SoftBody)
MEMORY_DEFINE_OBJECT_POOL(SoftBodyNodePool, SoftBodyNode)
MEMORY_DEFINE_OBJECT_POOL(SoftBodyBeamPool, SoftBodyBeam)
MEMORY_DEFINE_OBJECT_POOL(SoftBodyTrianglePool, SoftBodyTriangle)

PositionPool positions_pool = {0};
OrientationPool orientations_pool = {0};
VelocityPool velocities_pool = {0};
AccelerationPool accelerations_pool = {0};
AccelerationPool force_accelerations_pool = {0};
MassPool mass_pool = {0};
ForcePool forces_pool = {0};
ShapePool hit_boxes_pool = {0};
ShapePool world_hit_boxes_pool = {0};
static PhysicsInteractionSet current_interactions = {0};
static PhysicsInteractionSet previous_interactions = {0};
CollisionFilterConfigPool collision_filters_pool = {0};
AngularVelocityPool angular_velocities_pool = {0};
AngularVelocityPool angular_velocity_maximums_pool = {0};
AngularAccelerationPool angular_accelerations_pool = {0};
AngularVelocityPool torque_angular_accelerations_pool = {0};
TorquePool torques_pool = {0};
FrictionPool frictions_pool = {0};
RestitutionPool restitutions_pool = {0};
AngleLockPool angle_locks_pool = {0};
AxisLockPool axis_locks_pool = {0};
TransformLockPool transform_locks_pool = {0};
JointPool joints_pool = {0};
SoftBodyPool soft_bodies_pool = {0};
SoftBodyNodePool soft_body_nodes_pool = {0};
SoftBodyBeamPool soft_body_beams_pool = {0};
SoftBodyTrianglePool soft_body_triangles_pool = {0};

static JointAnchor joint_anchors[MAX_JOINT_ANCHORS];
static uint32_t joint_anchor_generations[MAX_JOINT_ANCHORS];
static bool joint_anchor_used[MAX_JOINT_ANCHORS];

static JointAnchorId physics_joint_anchor_id_make(uint32_t slot) {
    return ((uint64_t)joint_anchor_generations[slot] << 32) | ((uint64_t)slot + 1);
}

static bool physics_joint_anchor_slot_get(JointAnchorId anchor, uint32_t *slot) {
    uint32_t candidate;
    uint32_t generation;

    if(anchor == JOINT_ANCHOR_INVALID || slot == NULL) return false;
    candidate = (uint32_t)(anchor & UINT32_MAX);
    generation = (uint32_t)(anchor >> 32);
    if(candidate == 0) return false;
    candidate -= 1;
    if(candidate >= MAX_JOINT_ANCHORS || !joint_anchor_used[candidate] ||
            joint_anchor_generations[candidate] != generation) return false;
    *slot = candidate;
    return true;
}

EngineResult physics_tables_init(void) {
    physics_dt_per_tick = 0.0;
    physics_dt_overwritten = false;
    memset(joint_anchors, 0, sizeof(joint_anchors));
    memset(joint_anchor_used, 0, sizeof(joint_anchor_used));
    for(uint32_t i = 0; i < MAX_JOINT_ANCHORS; i += 1) joint_anchor_generations[i] = 1;
    if(error_check(physics_interaction_set_init(&current_interactions, 64))) { goto fail; }
    if(error_check(physics_interaction_set_init(&previous_interactions, 64))) { goto fail; }
    if(PositionPool_init(&positions_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(OrientationPool_init(&orientations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(VelocityPool_init(&velocities_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AccelerationPool_init(&accelerations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AccelerationPool_init(&force_accelerations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(MassPool_init(&mass_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(ForcePool_init(&forces_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(ShapePool_init(&hit_boxes_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(ShapePool_init(&world_hit_boxes_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(CollisionFilterConfigPool_init(&collision_filters_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngularVelocityPool_init(&angular_velocities_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngularVelocityPool_init(&angular_velocity_maximums_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngularAccelerationPool_init(&angular_accelerations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngularVelocityPool_init(&torque_angular_accelerations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(TorquePool_init(&torques_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(FrictionPool_init(&frictions_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(RestitutionPool_init(&restitutions_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngleLockPool_init(&angle_locks_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AxisLockPool_init(&axis_locks_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(TransformLockPool_init(&transform_locks_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(JointPool_init(&joints_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(SoftBodyPool_init(&soft_bodies_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(SoftBodyNodePool_init(&soft_body_nodes_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(SoftBodyBeamPool_init(&soft_body_beams_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(SoftBodyTrianglePool_init(&soft_body_triangles_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    return error_result_value(true);

fail:
    physics_tables_destroy();
    return error_result_error(ERROR_ENGINE_PHYSICS_TABLES_INIT_FAILED);
}

EngineResult physics_tables_ensure_capacity(size_t capacity) {
    size_t new_capacity;

    if(capacity > MAX_ENTITIES) {
        return error_result_error(ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
    }
    if(capacity <= positions_pool.capacity) {
        return error_result_value(true);
    }
    new_capacity = positions_pool.capacity == 0 ? 16 : positions_pool.capacity;
    while(new_capacity < capacity) {
        new_capacity *= 2;
    }
    if(new_capacity > MAX_ENTITIES) {
        new_capacity = MAX_ENTITIES;
    }
    if(new_capacity > positions_pool.capacity && PositionPool_expand(&positions_pool, new_capacity - positions_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > orientations_pool.capacity && OrientationPool_expand(&orientations_pool, new_capacity - orientations_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > velocities_pool.capacity && VelocityPool_expand(&velocities_pool, new_capacity - velocities_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > accelerations_pool.capacity && AccelerationPool_expand(&accelerations_pool, new_capacity - accelerations_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > force_accelerations_pool.capacity && AccelerationPool_expand(&force_accelerations_pool, new_capacity - force_accelerations_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > mass_pool.capacity && MassPool_expand(&mass_pool, new_capacity - mass_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > forces_pool.capacity && ForcePool_expand(&forces_pool, new_capacity - forces_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > hit_boxes_pool.capacity && ShapePool_expand(&hit_boxes_pool, new_capacity - hit_boxes_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > world_hit_boxes_pool.capacity && ShapePool_expand(&world_hit_boxes_pool, new_capacity - world_hit_boxes_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > collision_filters_pool.capacity && CollisionFilterConfigPool_expand(&collision_filters_pool, new_capacity - collision_filters_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > angular_velocities_pool.capacity && AngularVelocityPool_expand(&angular_velocities_pool, new_capacity - angular_velocities_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > angular_velocity_maximums_pool.capacity && AngularVelocityPool_expand(&angular_velocity_maximums_pool, new_capacity - angular_velocity_maximums_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > angular_accelerations_pool.capacity && AngularAccelerationPool_expand(&angular_accelerations_pool, new_capacity - angular_accelerations_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > torque_angular_accelerations_pool.capacity && AngularVelocityPool_expand(&torque_angular_accelerations_pool, new_capacity - torque_angular_accelerations_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > torques_pool.capacity && TorquePool_expand(&torques_pool, new_capacity - torques_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > frictions_pool.capacity && FrictionPool_expand(&frictions_pool, new_capacity - frictions_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > restitutions_pool.capacity && RestitutionPool_expand(&restitutions_pool, new_capacity - restitutions_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > angle_locks_pool.capacity && AngleLockPool_expand(&angle_locks_pool, new_capacity - angle_locks_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > axis_locks_pool.capacity && AxisLockPool_expand(&axis_locks_pool, new_capacity - axis_locks_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > transform_locks_pool.capacity && TransformLockPool_expand(&transform_locks_pool, new_capacity - transform_locks_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > joints_pool.capacity && JointPool_expand(&joints_pool, new_capacity - joints_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > soft_bodies_pool.capacity && SoftBodyPool_expand(&soft_bodies_pool, new_capacity - soft_bodies_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > soft_body_nodes_pool.capacity && SoftBodyNodePool_expand(&soft_body_nodes_pool, new_capacity - soft_body_nodes_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > soft_body_beams_pool.capacity && SoftBodyBeamPool_expand(&soft_body_beams_pool, new_capacity - soft_body_beams_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > soft_body_triangles_pool.capacity && SoftBodyTrianglePool_expand(&soft_body_triangles_pool, new_capacity - soft_body_triangles_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    return error_result_value(true);
}

void physics_tables_destroy(void) {
    physics_interaction_set_destroy(&current_interactions);
    physics_interaction_set_destroy(&previous_interactions);
    (void)PositionPool_destroy(&positions_pool);
    (void)OrientationPool_destroy(&orientations_pool);
    (void)VelocityPool_destroy(&velocities_pool);
    (void)AccelerationPool_destroy(&accelerations_pool);
    (void)AccelerationPool_destroy(&force_accelerations_pool);
    (void)MassPool_destroy(&mass_pool);
    (void)ForcePool_destroy(&forces_pool);
    (void)ShapePool_destroy(&hit_boxes_pool);
    (void)ShapePool_destroy(&world_hit_boxes_pool);
    (void)CollisionFilterConfigPool_destroy(&collision_filters_pool);
    (void)AngularVelocityPool_destroy(&angular_velocities_pool);
    (void)AngularVelocityPool_destroy(&angular_velocity_maximums_pool);
    (void)AngularAccelerationPool_destroy(&angular_accelerations_pool);
    (void)AngularVelocityPool_destroy(&torque_angular_accelerations_pool);
    (void)TorquePool_destroy(&torques_pool);
    (void)FrictionPool_destroy(&frictions_pool);
    (void)RestitutionPool_destroy(&restitutions_pool);
    (void)AngleLockPool_destroy(&angle_locks_pool);
    (void)AxisLockPool_destroy(&axis_locks_pool);
    (void)TransformLockPool_destroy(&transform_locks_pool);
    (void)JointPool_destroy(&joints_pool);
    (void)SoftBodyPool_destroy(&soft_bodies_pool);
    (void)SoftBodyNodePool_destroy(&soft_body_nodes_pool);
    (void)SoftBodyBeamPool_destroy(&soft_body_beams_pool);
    (void)SoftBodyTrianglePool_destroy(&soft_body_triangles_pool);
}

void physics_interactions_step_begin(void) {
    PhysicsInteractionSet interactions = previous_interactions;

    previous_interactions = current_interactions;
    current_interactions = interactions;
    physics_interaction_set_clear(&current_interactions);
}

EngineResult physics_interaction_record(
    Entity entity,
    Entity target,
    OverlapInfo overlap,
    PhysicsInteractionFlags flags
) {
    return physics_interaction_set_record(
        &current_interactions, entity, target, overlap, flags
    );
}

bool physics_interaction_current_check(
    Entity entity,
    Entity target,
    PhysicsInteractionFlags flags
) {
    return physics_interaction_set_check(
        &current_interactions, entity, target, flags
    );
}

bool physics_interaction_previous_check(
    Entity entity,
    Entity target,
    PhysicsInteractionFlags flags
) {
    return physics_interaction_set_check(
        &previous_interactions, entity, target, flags
    );
}

bool physics_interaction_current_get(
    Entity entity,
    Entity target,
    PhysicsInteraction *interaction
) {
    return physics_interaction_set_get(
        &current_interactions, entity, target, interaction
    );
}

bool physics_interaction_previous_get(
    Entity entity,
    Entity target,
    PhysicsInteraction *interaction
) {
    return physics_interaction_set_get(
        &previous_interactions, entity, target, interaction
    );
}

static void physics_soft_body_entity_list_remove(Entity *values, uint32_t *count, Entity entity) {
    if(values == NULL || count == NULL) return;
    for(uint32_t i = 0; i < *count; i += 1) {
        if(values[i] != entity) continue;
        values[i] = values[*count - 1];
        values[*count - 1] = ENTITY_INVALID;
        *count -= 1;
        return;
    }
}

void physics_entity_clear(Entity entity, EntityIndex index) {
    if(index < angular_velocity_maximums_pool.capacity &&
            angular_velocity_maximums_pool.used[index]) {
        (void)AngularVelocityPool_release_at(&angular_velocity_maximums_pool, index);
    }
    if(index < soft_bodies_pool.capacity && soft_bodies_pool.used[index]) {
        SoftBody body = soft_bodies[index];
        for(uint32_t i = 0; i < body.triangle_count; i += 1) {
            if(entity_alive_check(body.triangles[i])) (void)entity_delete(body.triangles[i]);
        }
        for(uint32_t i = 0; i < body.beam_count; i += 1) {
            if(entity_alive_check(body.beams[i])) (void)entity_delete(body.beams[i]);
        }
        for(uint32_t i = 0; i < body.node_count; i += 1) {
            if(entity_alive_check(body.nodes[i])) (void)entity_delete(body.nodes[i]);
        }
        (void)SoftBodyPool_release_at(&soft_bodies_pool, index);
    }
    if(index < soft_body_nodes_pool.capacity && soft_body_nodes_pool.used[index]) {
        EntityIndex body_index;
        Entity owner = soft_body_nodes[index].soft_body;
        for(EntityIndex connected = 0; connected < soft_body_beams_pool.capacity; connected += 1) {
            if(!soft_body_beams_pool.used[connected] ||
                    (soft_body_beams[connected].node_a != entity && soft_body_beams[connected].node_b != entity)) continue;
            EntityResult connected_entity = entity_from_index_get(connected);
            if(connected_entity.kind == ERROR_RESULT_VALUE) (void)entity_delete(connected_entity.result.value);
        }
        for(EntityIndex connected = 0; connected < soft_body_triangles_pool.capacity; connected += 1) {
            SoftBodyTriangle triangle;
            EntityResult connected_entity;
            if(!soft_body_triangles_pool.used[connected]) continue;
            triangle = soft_body_triangles[connected];
            if(triangle.node_a != entity && triangle.node_b != entity && triangle.node_c != entity) continue;
            connected_entity = entity_from_index_get(connected);
            if(connected_entity.kind == ERROR_RESULT_VALUE) (void)entity_delete(connected_entity.result.value);
        }
        if(entity_index_get(owner, &body_index) && body_index < soft_bodies_pool.capacity && soft_bodies_pool.used[body_index]) {
            physics_soft_body_entity_list_remove(soft_bodies[body_index].nodes,
                &soft_bodies[body_index].node_count, entity);
        }
        (void)SoftBodyNodePool_release_at(&soft_body_nodes_pool, index);
    }
    if(index < soft_body_beams_pool.capacity && soft_body_beams_pool.used[index]) {
        EntityIndex body_index;
        Entity owner = soft_body_beams[index].soft_body;
        if(entity_index_get(owner, &body_index) && body_index < soft_bodies_pool.capacity && soft_bodies_pool.used[body_index]) {
            physics_soft_body_entity_list_remove(soft_bodies[body_index].beams,
                &soft_bodies[body_index].beam_count, entity);
        }
        (void)SoftBodyBeamPool_release_at(&soft_body_beams_pool, index);
    }
    if(index < soft_body_triangles_pool.capacity && soft_body_triangles_pool.used[index]) {
        EntityIndex body_index;
        Entity owner = soft_body_triangles[index].soft_body;
        if(entity_index_get(owner, &body_index) && body_index < soft_bodies_pool.capacity && soft_bodies_pool.used[body_index]) {
            physics_soft_body_entity_list_remove(soft_bodies[body_index].triangles,
                &soft_bodies[body_index].triangle_count, entity);
        }
        (void)SoftBodyTrianglePool_release_at(&soft_body_triangles_pool, index);
    }
    for(uint32_t slot = 0; slot < MAX_JOINT_ANCHORS; slot += 1) {
        if(joint_anchor_used[slot] && joint_anchors[slot].entity == entity) {
            (void)physics_joint_anchor_remove(physics_joint_anchor_id_make(slot));
        }
    }
    for(EntityIndex joint_index = 0; joint_index < joints_pool.capacity; joint_index += 1) {
        if(!joints_pool.used[joint_index] ||
                (joints[joint_index].a != entity && joints[joint_index].b != entity)) continue;
        EntityResult joint_entity = entity_from_index_get(joint_index);
        if(joint_entity.kind == ERROR_RESULT_VALUE && joint_entity.result.value != entity) {
            (void)entity_delete(joint_entity.result.value);
        }
    }
    if(index < joints_pool.capacity && joints_pool.used[index]) {
        (void)JointPool_release_at(&joints_pool, index);
    }
}

Shape physics_shape_world_translate(Shape shape, Position position, Orientation angle) {
    Shape world_shape = {0};
    world_shape.amount_of_vertices = shape.amount_of_vertices;

    Position center = math_polygon_centroid(shape);

    float cos_a = cosf(angle);
    float sin_a = sinf(angle);

    for (int i = 0; i < shape.amount_of_vertices; i++) {
        float x = shape.vertices[i].x - center.x;
        float y = shape.vertices[i].y - center.y;

        float rotated_x = x*cos_a - y*sin_a;
        float rotated_y = x*sin_a + y*cos_a;

        world_shape.vertices[i].x = position.x + rotated_x;
        world_shape.vertices[i].y = position.y + rotated_y;
    }

    return world_shape;
}
float physics_polygon_moment_of_inertia(Shape shape, Mass mass_value) {
    Position c = math_polygon_centroid(shape);

    float area_sum = 0.0f;
    float inertia_sum = 0.0f;

    for (int i = 0; i < shape.amount_of_vertices; i++) {
        int j = (i + 1) % shape.amount_of_vertices;

        float xi = shape.vertices[i].x - c.x;
        float yi = shape.vertices[i].y - c.y;

        float xj = shape.vertices[j].x - c.x;
        float yj = shape.vertices[j].y - c.y;

        float cross = xi * yj - xj * yi;

        float q =
            xi*xi + xi*xj + xj*xj +
            yi*yi + yi*yj + yj*yj;

        area_sum += cross;
        inertia_sum += cross * q;
    }

    float area = 0.5f * area_sum;
    float area_moment = inertia_sum / 12.0f;

    if (fabsf(area) < 1e-8f) { //Very small area no inertia calc needed
        return 0;
    }

    float density = mass_value / fabsf(area);
    float inertia = density * fabsf(area_moment);

    return inertia;
}
OverlapInfo physics_sat_overlap_on_axes_get(Shape shape_1, Shape shape_2, Vec2DList axes, OverlapInfo overlap_info) {
    for (int i = 0; i < axes.amount_of_vectors; i += 1) {
        Axis axis = axes.vectors[i];

        Projection p1 = math_project_shape_on_axis(shape_1, axis);
        Projection p2 = math_project_shape_on_axis(shape_2, axis);

        float overlap = math_projection_overlap(p1, p2);

        if (overlap <= 0.0f) {
            return (OverlapInfo){ .detected = false };
        }

        if (overlap < overlap_info.depth) {
            overlap_info.depth = overlap;
            overlap_info.normal = axis;
        }
    }

    return overlap_info;
}
OverlapInfo physics_particle_overlap_get(Shape shape_1, Shape shape_2)
{
    Position center_1 = math_polygon_centroid(shape_1);
    Position center_2 = math_polygon_centroid(shape_2);

    float radius_1 = math_circle_radius(shape_1, center_1);
    float radius_2 = math_circle_radius(shape_2, center_2);

    Vec2D delta = {
        .x = center_2.x - center_1.x,
        .y = center_2.y - center_1.y
    };

    float distance_squared =
        delta.x * delta.x +
        delta.y * delta.y;

    float radius_sum = radius_1 + radius_2;
    float radius_sum_squared = radius_sum * radius_sum;

    if(distance_squared >= radius_sum_squared) {
        return (OverlapInfo){
            .detected = false
        };
    }

    float distance = sqrtf(distance_squared);

    Vec2D normal;

    if(distance > 0.00001f) {
        normal = (Vec2D){
            .x = delta.x / distance,
            .y = delta.y / distance
        };
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

OverlapInfo physics_sat_overlap_get(Shape shape_1, Shape shape_2)
{
    OverlapInfo collision = {
        .detected = true,
        .normal = {0},
        .depth = FLT_MAX
    };

    Vec2DList shape1_axes = math_vectors_normalize(math_normals_create(shape_1));
    Vec2DList shape2_axes = math_vectors_normalize(math_normals_create(shape_2));

    collision = physics_sat_overlap_on_axes_get(shape_1, shape_2, shape1_axes, collision);

    if (!collision.detected) {
        return collision;
    }

    collision = physics_sat_overlap_on_axes_get(shape_1, shape_2, shape2_axes, collision);

    if (!collision.detected) {
        return collision;
    }

    Position c1 = math_polygon_centroid(shape_1);
    Position c2 = math_polygon_centroid(shape_2);

    Vec2D center_delta = {
        .x = c2.x - c1.x,
        .y = c2.y - c1.y
    };

    if (math_dot_product(center_delta, collision.normal) < 0.0f) {
        collision.normal.x *= -1.0f;
        collision.normal.y *= -1.0f;
    }

    return collision;
}
Position physics_approximate_contact_point(Position p1, Position p2)
{
    return (Position){
        .x = (p1.x + p2.x) * 0.5f,
        .y = (p1.y + p2.y) * 0.5f
    };
}
Vec1D physics_circle_moment_of_inertia(Shape circle, Mass mass_value) {
  Vec1D radius = math_circle_radius(circle, math_polygon_centroid(circle));
  Vec1D area = PI_F*radius*radius;
  Vec1D density = mass_value/fabsf(area);
  Vec1D area_moment = 0.5f * area * radius * radius;
  return density * area_moment;
}

static EngineResult physics_live_index_get(Entity entity, EntityIndex *index) {
    if(index == NULL) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(!entity_index_get(entity, index)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(!entity_index_alive_check(*index)) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    return error_result_value(true);
}

bool physics_entity_held_get(EntityIndex index) {
    if(!entity_index_alive_check(index)) {
        return false;
    }
    return entity_index_components_check(index, HOLD);
}

bool physics_entity_movable_get(EntityIndex index) {
    if(!entity_index_alive_check(index)) {
        return false;
    }
    return entity_index_components_check(index, DYNAMIC)
        && !entity_index_components_check(index, STATIC)
        && !physics_entity_held_get(index);
}

static Vec2D physics_direction_between_positions(Position from, Position to) {
    Vec2D delta = {
        .x = to.x - from.x,
        .y = to.y - from.y
    };
    float distance = math_vector_magnitude(delta);

    if(distance <= 0.00001f) {
        return (Vec2D){0};
    }
    return (Vec2D){
        .x = delta.x / distance,
        .y = delta.y / distance
    };
}

typedef EngineResult (*PhysicsGroupEntityTargetFn)(Entity entity, float magnitude, Entity target);
typedef EngineResult (*PhysicsGroupEntityFn)(Entity entity);

static EngineResult physics_group_entity_target_apply(GroupId group, float magnitude, Entity target, PhysicsGroupEntityTargetFn fn) {
    EntityGroupResult group_result;
    EntityGroup group_storage;
    size_t i;

    if(fn == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    group_result = entity_group_get(group);
    if(group_result.kind == ERROR_RESULT_ERROR) {
        return error_result_error(group_result.result.error);
    }
    group_storage = group_result.result.value;
    if(group_storage.entities.objects == NULL || group_storage.entities.used == NULL) {
        return error_result_value(true);
    }
    for(i = 0; i < group_storage.entities.capacity; i += 1) {
        EngineResult result;
        Entity entity;

        if(group_storage.entities.used[i] == 0) {
            continue;
        }
        entity = group_storage.entities.objects[i];
        if(!entity_alive_check(entity)) {
            continue;
        }
        result = fn(entity, magnitude, target);
        if(result.kind == ERROR_RESULT_ERROR) {
            return result;
        }
    }
    return error_result_value(true);
}

static EngineResult physics_group_entity_apply(GroupId group, PhysicsGroupEntityFn fn) {
    EntityGroupResult group_result;
    EntityGroup group_storage;
    size_t i;

    if(fn == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    group_result = entity_group_get(group);
    if(group_result.kind == ERROR_RESULT_ERROR) {
        return error_result_error(group_result.result.error);
    }
    group_storage = group_result.result.value;
    if(group_storage.entities.objects == NULL || group_storage.entities.used == NULL) {
        return error_result_value(true);
    }
    for(i = 0; i < group_storage.entities.capacity; i += 1) {
        EngineResult result;
        Entity entity;

        if(group_storage.entities.used[i] == 0) {
            continue;
        }
        entity = group_storage.entities.objects[i];
        if(!entity_alive_check(entity)) {
            continue;
        }
        result = fn(entity);
        if(result.kind == ERROR_RESULT_ERROR) {
            return result;
        }
    }
    return error_result_value(true);
}

//Entity
EngineResult physics_velocity_set(Entity entity, Velocity v) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    (void)VelocityPool_store_at(&velocities_pool, index, v);
    console_debug_write(LOG_ENGINE, "Set Entity: %d Velocity: {x: %f, y: %f}\n", entity, v.x, v.y);
    return error_result_value(true);
}

EngineResult physics_velocity_toward_position_set(Entity entity, float speed, Position position) {
    EntityIndex index;
    Vec2D direction;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    direction = physics_direction_between_positions(positions[index], position);
    return physics_velocity_set(entity, (Velocity){
        .x = direction.x * speed,
        .y = direction.y * speed
    });
}

EngineResult physics_velocity_toward_entity_set(Entity entity, float speed, Entity target) {
    EntityIndex target_index;
    EngineResult result = physics_live_index_get(target, &target_index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    return physics_velocity_toward_position_set(entity, speed, positions[target_index]);
}

EngineResult physics_velocity_away_from_position_set(Entity entity, float speed, Position position) {
    return physics_velocity_toward_position_set(entity, -speed, position);
}

EngineResult physics_velocity_away_from_entity_set(Entity entity, float speed, Entity target) {
    return physics_velocity_toward_entity_set(entity, -speed, target);
}

EngineResult physics_group_velocity_toward_entity_set(GroupId group, float speed, Entity target) {
    return physics_group_entity_target_apply(group, speed, target, physics_velocity_toward_entity_set);
}

EngineResult physics_group_velocity_away_from_entity_set(GroupId group, float speed, Entity target) {
    return physics_group_entity_target_apply(group, speed, target, physics_velocity_away_from_entity_set);
}

EngineResult physics_entity_stop(Entity entity) {
    return physics_velocity_set(entity, (Velocity){0});
}

EngineResult physics_group_entities_stop(GroupId group) {
    return physics_group_entity_apply(group, physics_entity_stop);
}

EngineResult physics_impulse_apply(Entity entity, Vec2D impulse) {
    EntityIndex index;
    Velocity velocity;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(!entity_index_components_check(index, MASS)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    if(mass[index] == 0.0f) {
        return error_result_value(true);
    }
    velocity = (Velocity){
        .x = velocities[index].x + impulse.x / mass[index],
        .y = velocities[index].y + impulse.y / mass[index]
    };
    (void)VelocityPool_store_at(&velocities_pool, index, velocity);
    console_debug_write(
        LOG_ENGINE,
        "Apply Entity: %d Impulse: {x: %f, y: %f} Velocity: {x: %f, y: %f}\n",
        entity,
        impulse.x,
        impulse.y,
        velocity.x,
        velocity.y
    );
    return error_result_value(true);
}

EngineResult physics_position_set(Entity entity, Position p) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    (void)PositionPool_store_at(&positions_pool, index, p);
    console_debug_write(LOG_ENGINE, "Set Entity: %d Position: {x: %f, y: %f}\n", entity, p.x, p.y);
    return error_result_value(true);
}

PositionResult physics_position_get(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return ERROR_RESULT_MAKE_ERROR(PositionResult, result.result.error);
    }
    if(!positions_pool.used[index]) {
        return ERROR_RESULT_MAKE_ERROR(PositionResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(PositionResult, positions[index]);
}

EngineResult physics_mass_set(Entity entity, Mass m) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    entity_mask[index] |= MASS;
    (void)MassPool_store_at(&mass_pool, index, m);
    console_debug_write(LOG_ENGINE, "Set Entity: %d Mass: %f\n", entity, m);
    return error_result_value(true);
}
EntityResult physics_force_create(Entity entity, Force f) {
    EntityIndex index;
    EntityResult force_result;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, result.result.error);
    }
    force_result = entity_add();
    if(force_result.kind == ERROR_RESULT_ERROR) {
        return force_result;
    }
    Entity force_entity = force_result.result.value;
    EntityIndex force_index;
    if(!(entity_index_get(force_entity, &force_index) && entity_index_alive_check(force_index))) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    (void)ForcePool_store_at(&forces_pool, force_index, f);
    (void)TargetPool_store_at(&targets_pool, force_index, entity);
    entity_mask[force_index] |= TARGETABLE | FORCE;
    console_debug_write(LOG_ENGINE, "Set Entity: %d Force: {x: %f, y: %f}\n", entity, f.x, f.y);
    return ERROR_RESULT_MAKE_VALUE(EntityResult, force_entity);
}

EngineResult physics_force_component_set(Entity entity, Force force) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(ForcePool_store_at(&forces_pool, index, force).kind
            == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[index] |= FORCE;
    return error_result_value(true);
}

EngineResult physics_force_for_one_tick_apply(Entity entity, Force f) {
    EntityResult force_result = physics_force_create(entity, f);
    EngineResult result;

    if(force_result.kind == ERROR_RESULT_ERROR) {
        return error_result_error(force_result.result.error);
    }
    result = entity_life_time_set(force_result.result.value, 0.0, engine_tick_get() + 1);
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)entity_delete(force_result.result.value);
        return result;
    }
    return error_result_value(true);
}

EngineResult physics_acceleration_set(Entity entity, Acceleration a) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    (void)AccelerationPool_store_at(&accelerations_pool, index, a);
    console_debug_write(LOG_ENGINE, "Set Entity: %d Acceleration: {x: %f, y: %f}\n", entity, a.x, a.y);
    return error_result_value(true);
}

EngineResult physics_acceleration_toward_position_set(Entity entity, float acceleration_magnitude, Position position) {
    EntityIndex index;
    Vec2D direction;
    Acceleration acceleration;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }

    direction = physics_direction_between_positions(positions[index], position);
    acceleration = (Acceleration){
        .x = direction.x * acceleration_magnitude,
        .y = direction.y * acceleration_magnitude
    };

    (void)AccelerationPool_store_at(&accelerations_pool, index, acceleration);
    console_debug_write(
        LOG_ENGINE,
        "Set Entity: %d Acceleration Toward Position: {x: %f, y: %f} Magnitude: %f Acceleration: {x: %f, y: %f}\n",
        entity,
        position.x,
        position.y,
        acceleration_magnitude,
        acceleration.x,
        acceleration.y
    );
    return error_result_value(true);
}

EngineResult physics_acceleration_toward_entity_set(Entity entity, float acceleration_magnitude, Entity target) {
    EntityIndex target_index;
    EngineResult result = physics_live_index_get(target, &target_index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    return physics_acceleration_toward_position_set(entity, acceleration_magnitude, positions[target_index]);
}

EngineResult physics_acceleration_away_from_position_set(Entity entity, float acceleration_magnitude, Position position) {
    return physics_acceleration_toward_position_set(entity, -acceleration_magnitude, position);
}

EngineResult physics_acceleration_away_from_entity_set(Entity entity, float acceleration_magnitude, Entity target) {
    return physics_acceleration_toward_entity_set(entity, -acceleration_magnitude, target);
}

EngineResult physics_group_acceleration_toward_entity_set(GroupId group, float acceleration_magnitude, Entity target) {
    return physics_group_entity_target_apply(group, acceleration_magnitude, target, physics_acceleration_toward_entity_set);
}

EngineResult physics_group_acceleration_away_from_entity_set(GroupId group, float acceleration_magnitude, Entity target) {
    return physics_group_entity_target_apply(group, acceleration_magnitude, target, physics_acceleration_away_from_entity_set);
}

EntityResult physics_torque_create(Entity entity, Torque t) {
    EntityIndex index;
    EntityResult torque_result;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, result.result.error);
    }
    torque_result = entity_add();
    if(torque_result.kind == ERROR_RESULT_ERROR) {
        return torque_result;
    }
    Entity torque_entity = torque_result.result.value;
    EntityIndex torque_index;
    if(!(entity_index_get(torque_entity, &torque_index) && entity_index_alive_check(torque_index))) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    (void)TorquePool_store_at(&torques_pool, torque_index, t);
    (void)TargetPool_store_at(&targets_pool, torque_index, entity);
    entity_mask[torque_index] |= TARGETABLE | TORQUE;
    console_debug_write(LOG_ENGINE, "Set Entity: %d Torque: %f\n", entity, t);
    return ERROR_RESULT_MAKE_VALUE(EntityResult, torque_entity);
}

EngineResult physics_torque_component_set(Entity entity, Torque torque) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(TorquePool_store_at(&torques_pool, index, torque).kind
            == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[index] |= TORQUE;
    return error_result_value(true);
}

EngineResult physics_torque_for_one_tick_apply(Entity entity, Torque t) {
    EntityResult torque_result = physics_torque_create(entity, t);
    EngineResult result;

    if(torque_result.kind == ERROR_RESULT_ERROR) {
        return error_result_error(torque_result.result.error);
    }
    result = entity_life_time_set(torque_result.result.value, 0.0, engine_tick_get() + 1);
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)entity_delete(torque_result.result.value);
        return result;
    }
    return error_result_value(true);
}

EngineResult physics_hitbox_set(Entity entity, Shape hitbox) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    entity_mask[index] |= HIT_BOX;
    (void)ShapePool_store_at(&hit_boxes_pool, index, hitbox);
    console_debug_write(LOG_ENGINE, "Set Entity: %d to have a hit box\n", entity);
    return error_result_value(true);
}

CollisionFilterConfig physics_collision_filter_config_default_get(void) {
    return (CollisionFilterConfig) {
        .category = ROHR_COLLISION_CATEGORY_DEFAULT,
        .collides_with = ROHR_COLLISION_CATEGORY_ALL
    };
}

EngineResult physics_collision_filter_set(Entity entity, CollisionFilterConfig config) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!entity_index_components_check(index, HIT_BOX)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    if(CollisionFilterConfigPool_store_at(&collision_filters_pool, index, config).kind
            == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED);
    }
    entity_mask[index] |= COLLISION_FILTER;
    return error_result_value(true);
}

CollisionFilterConfigResult physics_collision_filter_get(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return ERROR_RESULT_MAKE_ERROR(CollisionFilterConfigResult, result.result.error);
    }
    if(!entity_index_components_check(index, HIT_BOX)) {
        return ERROR_RESULT_MAKE_ERROR(CollisionFilterConfigResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    if(!entity_index_components_check(index, COLLISION_FILTER)) {
        return ERROR_RESULT_MAKE_VALUE(CollisionFilterConfigResult, physics_collision_filter_config_default_get());
    }
    return ERROR_RESULT_MAKE_VALUE(CollisionFilterConfigResult, collision_filters[index]);
}

EngineResult physics_collision_category_set(Entity entity, RohrCollisionCategoryMask category) {
    CollisionFilterConfigResult result = physics_collision_filter_get(entity);

    if(result.kind == ERROR_RESULT_ERROR) return error_result_error(result.result.error);
    result.result.value.category = category;
    return physics_collision_filter_set(entity, result.result.value);
}

EngineResult physics_collision_with_set(Entity entity, RohrCollisionCategoryMask categories) {
    CollisionFilterConfigResult result = physics_collision_filter_get(entity);

    if(result.kind == ERROR_RESULT_ERROR) return error_result_error(result.result.error);
    result.result.value.collides_with = categories;
    return physics_collision_filter_set(entity, result.result.value);
}

EngineResult physics_collision_with_all_set(Entity entity) {
    return physics_collision_with_set(entity, ROHR_COLLISION_CATEGORY_ALL);
}

EngineResult physics_collision_with_none_set(Entity entity) {
    return physics_collision_with_set(entity, ROHR_COLLISION_CATEGORY_NONE);
}

bool physics_collision_between_check(Entity entity_1, Entity entity_2) {
    CollisionFilterConfigResult first = physics_collision_filter_get(entity_1);
    CollisionFilterConfigResult second = physics_collision_filter_get(entity_2);

    if(first.kind == ERROR_RESULT_ERROR || second.kind == ERROR_RESULT_ERROR) return false;
    return (first.result.value.collides_with & second.result.value.category) != 0
        && (second.result.value.collides_with & first.result.value.category) != 0;
}
EngineResult physics_orientation_set(Entity entity, Orientation angle) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    (void)OrientationPool_store_at(&orientations_pool, index, angle);
    console_debug_write(LOG_ENGINE, "Set Entity: %d Orientation: %f\n", entity, angle);
    return error_result_value(true);
}
EngineResult physics_angular_velocity_set(Entity entity, AngularVelocity v) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = physics_dynamic_set(entity);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    (void)AngularVelocityPool_store_at(&angular_velocities_pool, index, v);
    console_debug_write(LOG_ENGINE, "Set Entity: %d Angular Velocity: %f\n", entity, v);
    return error_result_value(true);
}

AngularVelocityResult physics_angular_velocity_get(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return ERROR_RESULT_MAKE_ERROR(AngularVelocityResult, result.result.error);
    }
    if(!angular_velocities_pool.used[index]) {
        return ERROR_RESULT_MAKE_ERROR(AngularVelocityResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(AngularVelocityResult, angular_velocities[index]);
}

EngineResult physics_angular_velocity_maximum_set(
        Entity entity, AngularVelocity maximum) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(maximum < 0.0f) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    if(AngularVelocityPool_store_at(
            &angular_velocity_maximums_pool, index, maximum).kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    return error_result_value(true);
}

AngularVelocityResult physics_angular_velocity_maximum_get(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return ERROR_RESULT_MAKE_ERROR(AngularVelocityResult, result.result.error);
    }
    if(!angular_velocity_maximums_pool.used[index]) {
        return ERROR_RESULT_MAKE_ERROR(AngularVelocityResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(
        AngularVelocityResult, angular_velocity_maximums[index]);
}

EngineResult physics_angular_acceleration_set(
        Entity entity,
        AngularAcceleration acceleration
) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = physics_dynamic_set(entity);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(AngularAccelerationPool_store_at(
            &angular_accelerations_pool,
            index,
            acceleration
        ).kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    return error_result_value(true);
}
ShapeResult physics_global_hit_box_get(Entity entity) {
    RohrComponentMask filter = HIT_BOX;
    EntityIndex index;

    if((entity_index_get(entity, &index) && entity_index_alive_check(index))) {
        if( entity_index_components_check(index, filter) ) {
            return ERROR_RESULT_MAKE_VALUE(ShapeResult, world_hit_boxes[index]);
        }
        return ERROR_RESULT_MAKE_ERROR(ShapeResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_ERROR(ShapeResult, ERROR_ENGINE_INVALID_ENTITY);
}
EngineResult physics_restitution_set(Entity entity, Restitution restitution) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
     if(restitution < 0) {
        (void)RestitutionPool_store_at(&restitutions_pool, index, 0);
        console_debug_write(LOG_ENGINE, "Set Entity: %d Restitution: %f\n", entity, 0);
     }
     else if(restitution > 1) {
        (void)RestitutionPool_store_at(&restitutions_pool, index, 1);
        console_debug_write(LOG_ENGINE, "Set Entity: %d Restitution: %f\n", entity, 1);
     }
     else {
        (void)RestitutionPool_store_at(&restitutions_pool, index, restitution);
        console_debug_write(LOG_ENGINE, "Set Entity: %d Restitution: %f\n", entity, restitution);
     }
     return entity_components_add(entity, COLLISION);
}
EngineResult physics_dynamic_set(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_components_add(entity, DYNAMIC);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_components_delete(entity, STATIC);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    console_debug_write(LOG_ENGINE, "Set Entity: %d to DYNAMIC\n", entity);
    return error_result_value(true);
}
EngineResult physics_static_set(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_components_add(entity, STATIC);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_components_delete(entity, DYNAMIC);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    console_debug_write(LOG_ENGINE, "Set Entity: %d to STATIC\n", entity);
    return error_result_value(true);
}

EngineResult physics_entity_hold(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_components_add(entity, HOLD);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    console_debug_write(LOG_ENGINE, "Hold Entity: %d\n", entity);
    return error_result_value(true);
}

EngineResult physics_entity_unhold(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_components_delete(entity, HOLD);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    console_debug_write(LOG_ENGINE, "Unhold Entity: %d\n", entity);
    return error_result_value(true);
}

EngineResult physics_group_entities_hold(GroupId group) {
    return physics_group_entity_apply(group, physics_entity_hold);
}

EngineResult physics_group_entities_unhold(GroupId group) {
    return physics_group_entity_apply(group, physics_entity_unhold);
}

EngineResult physics_angle_lock_set(Entity entity, Orientation min, Orientation max) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_components_add(entity, ANGLE_LOCK);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    (void)AngleLockPool_store_at(&angle_locks_pool, index, (AngleLock){
        .min = min,
        .max = max
    });
    return error_result_value(true);
}
EngineResult physics_axis_lock_set(Entity entity, Axis axis, Position axis_point) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_components_add(entity, AXIS_LOCK);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    Axis normalized_axis = math_vector_normalize(axis);
    (void)AxisLockPool_store_at(&axis_locks_pool, index, (AxisLock){
        .axis = (Axis){
            .x = normalized_axis.x,
            .y = normalized_axis.y
        },
        .point_on_axis = (Position){
            .x = axis_point.x,
            .y = axis_point.y
        }
    });
    return error_result_value(true);
}
EngineResult physics_friction_set(Entity entity, float friction) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(friction < 0) {
        (void)FrictionPool_store_at(&frictions_pool, index, 0);
    }
    else if(friction >= 0) {
        (void)FrictionPool_store_at(&frictions_pool, index, friction);
    }
    return error_result_value(true);
}
EngineResult physics_transform_lock_set(
        Entity driven,
        Entity driver, 
        Vec2D local_offset,
        Orientation local_angle,
        bool lock_position,
        bool lock_orientation,
        bool inherit_velocity
        ) {
    EntityIndex driven_index;
    EntityIndex driver_index;
    EngineResult result;

    result = physics_live_index_get(driven, &driven_index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = physics_live_index_get(driver, &driver_index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_components_add(driven, TRANSFORM_LOCK);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    (void)TransformLockPool_store_at(&transform_locks_pool, driven_index, (TransformLock) {
        .driver = driver,
        .local_offset = local_offset,
        .local_angle = local_angle,
        .lock_position = lock_position,
        .lock_orientation = lock_orientation,
        .inherit_velocity = inherit_velocity
    });
    return error_result_value(true);
}
EngineResult physics_transform_lock_remove(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_components_delete(entity, TRANSFORM_LOCK);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(index < transform_locks_pool.capacity && transform_locks_pool.used[index]) {
        (void)TransformLockPool_release_at(&transform_locks_pool, index);
    }
    return error_result_value(true);
}
EngineResult physics_transform_lock_current_transform_set(
        Entity driven,
        Entity driver,
        bool lock_position,
        bool lock_orientation,
        bool inherit_velocity
        ) {
    EntityIndex driven_index;
    EntityIndex driver_index;
    EngineResult result;

    result = physics_live_index_get(driven, &driven_index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = physics_live_index_get(driver, &driver_index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }

    Vec2D world_offset = {
        .x = positions[driven_index].x - positions[driver_index].x,
        .y = positions[driven_index].y - positions[driver_index].y
    };

    Vec2D local_offset = math_vector_rotate(
        world_offset,
        -orientations[driver_index]
    );

    Orientation local_angle =
        orientations[driven_index] - orientations[driver_index];

    return physics_transform_lock_set(
        driven,
        driver,
        local_offset,
        local_angle,
        lock_position,
        lock_orientation,
        inherit_velocity
    );
}
EngineResult physics_target_set(Entity entity, Entity target) {
    EntityIndex index;
    EntityIndex target_index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = physics_live_index_get(target, &target_index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(TargetPool_store_at(&targets_pool, index, target).kind
            == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[index] |= TARGETABLE;
    return error_result_value(true);
}

EngineResult physics_joint_component_set(Entity entity, Joint joint) {
    EntityIndex index;
    EntityIndex a_index;
    EntityIndex b_index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(joint.type < JOINT_SPRING || joint.type > JOINT_PIN) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    result = physics_live_index_get(joint.a, &a_index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = physics_live_index_get(joint.b, &b_index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(JointPool_store_at(&joints_pool, index, joint).kind
            == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[index] |= JOINT;
    return error_result_value(true);
}

JointAnchorIdResult physics_joint_anchor_create(Entity entity, Vec2D centroid_offset) {
    EntityIndex entity_index;
    uint32_t owned_count = 0;
    EngineResult result = physics_live_index_get(entity, &entity_index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return ERROR_RESULT_MAKE_ERROR(JointAnchorIdResult, result.result.error);
    }
    for(uint32_t slot = 0; slot < MAX_JOINT_ANCHORS; slot += 1) {
        if(joint_anchor_used[slot] && joint_anchors[slot].entity == entity) owned_count += 1;
    }
    if(owned_count >= MAX_JOINT_ANCHORS_PER_ENTITY) {
        return ERROR_RESULT_MAKE_ERROR(JointAnchorIdResult, ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
    }
    for(uint32_t slot = 0; slot < MAX_JOINT_ANCHORS; slot += 1) {
        if(joint_anchor_used[slot]) continue;
        joint_anchor_used[slot] = true;
        joint_anchors[slot] = (JointAnchor){
            .entity = entity,
            .centroid_offset = centroid_offset
        };
        return ERROR_RESULT_MAKE_VALUE(JointAnchorIdResult, physics_joint_anchor_id_make(slot));
    }
    return ERROR_RESULT_MAKE_ERROR(JointAnchorIdResult, ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
}

JointAnchorListResult physics_joint_anchors_get(Entity entity) {
    EntityIndex entity_index;
    JointAnchorList list = {0};
    EngineResult result = physics_live_index_get(entity, &entity_index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return ERROR_RESULT_MAKE_ERROR(JointAnchorListResult, result.result.error);
    }
    for(uint32_t slot = 0; slot < MAX_JOINT_ANCHORS; slot += 1) {
        if(!joint_anchor_used[slot] || joint_anchors[slot].entity != entity) continue;
        if(list.count >= MAX_JOINT_ANCHORS_PER_ENTITY) {
            return ERROR_RESULT_MAKE_ERROR(JointAnchorListResult, ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
        }
        list.values[list.count++] = physics_joint_anchor_id_make(slot);
    }
    return ERROR_RESULT_MAKE_VALUE(JointAnchorListResult, list);
}

JointAnchorPositionResult physics_joint_anchor_position_get(JointAnchorId anchor) {
    uint32_t slot;

    if(!physics_joint_anchor_slot_get(anchor, &slot)) {
        return ERROR_RESULT_MAKE_ERROR(JointAnchorPositionResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    return ERROR_RESULT_MAKE_VALUE(JointAnchorPositionResult, joint_anchors[slot].centroid_offset);
}

JointAnchorPositionResult physics_joint_anchor_world_position_get(JointAnchorId anchor) {
    uint32_t slot;
    EntityIndex entity_index;
    Vec2D local_position;
    Vec2D rotated;

    if(!physics_joint_anchor_slot_get(anchor, &slot) ||
            !entity_index_get(joint_anchors[slot].entity, &entity_index) ||
            !entity_index_alive_check(entity_index)) {
        return ERROR_RESULT_MAKE_ERROR(JointAnchorPositionResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    local_position = joint_anchors[slot].centroid_offset;
    if(entity_index_components_check(entity_index, HIT_BOX)) {
        Vec2D centroid = math_polygon_centroid(hit_boxes[entity_index]);
        local_position.x += centroid.x;
        local_position.y += centroid.y;
    }
    rotated = math_vector_rotate(local_position, orientations[entity_index]);
    return ERROR_RESULT_MAKE_VALUE(JointAnchorPositionResult, ((Position){
        .x = positions[entity_index].x + rotated.x,
        .y = positions[entity_index].y + rotated.y
    }));
}

EngineResult physics_joint_anchor_position_set(JointAnchorId anchor, Vec2D centroid_offset) {
    uint32_t slot;

    if(!physics_joint_anchor_slot_get(anchor, &slot)) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    joint_anchors[slot].centroid_offset = centroid_offset;
    return error_result_value(true);
}

EngineResult physics_joint_anchor_remove(JointAnchorId anchor) {
    uint32_t slot;

    if(!physics_joint_anchor_slot_get(anchor, &slot)) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    for(EntityIndex index = 0; index < joints_pool.capacity; index += 1) {
        if(!joints_pool.used[index] ||
                (joints[index].anchor_a != anchor && joints[index].anchor_b != anchor)) continue;
        EntityResult joint_entity = entity_from_index_get(index);
        if(joint_entity.kind == ERROR_RESULT_VALUE) (void)entity_delete(joint_entity.result.value);
    }
    joint_anchor_used[slot] = false;
    joint_anchors[slot] = (JointAnchor){0};
    joint_anchor_generations[slot] += 1;
    if(joint_anchor_generations[slot] == 0) joint_anchor_generations[slot] = 1;
    return error_result_value(true);
}

static EngineResult physics_joint_anchors_set(
        Entity joint_entity,
        JointAnchorId anchor_a,
        JointAnchorId anchor_b,
        JointType type,
        float rest_length,
        float stiffness,
        float damping
) {
    uint32_t slot_a;
    uint32_t slot_b;
    EntityIndex a_index;
    EntityIndex b_index;
    JointAnchorPositionResult position_a;
    JointAnchorPositionResult position_b;

    if(!physics_joint_anchor_slot_get(anchor_a, &slot_a) ||
            !physics_joint_anchor_slot_get(anchor_b, &slot_b) ||
            !entity_index_get(joint_anchors[slot_a].entity, &a_index) ||
            !entity_index_get(joint_anchors[slot_b].entity, &b_index)) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    position_a = physics_joint_anchor_world_position_get(anchor_a);
    position_b = physics_joint_anchor_world_position_get(anchor_b);
    if(position_a.kind == ERROR_RESULT_ERROR || position_b.kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    return physics_joint_component_set(joint_entity, (Joint){
        .type = type,
        .anchor_a = anchor_a,
        .anchor_b = anchor_b,
        .a = joint_anchors[slot_a].entity,
        .b = joint_anchors[slot_b].entity,
        .rest_length = rest_length,
        .stiffness = stiffness,
        .damping = damping,
        .rest_angle = orientations[b_index] - orientations[a_index]
    });
}

EngineResult physics_joint_pin_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b) {
    return physics_joint_anchors_set(joint, anchor_a, anchor_b, JOINT_PIN, 0.0f, 0.0f, 0.0f);
}

EngineResult physics_joint_weld_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b) {
    return physics_joint_anchors_set(joint, anchor_a, anchor_b, JOINT_WELD, 0.0f, 0.0f, 0.0f);
}

EngineResult physics_joint_spring_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b,
        float rest_length, float stiffness, float damping) {
    if(rest_length < 0.0f || stiffness < 0.0f || damping < 0.0f) {
        return error_result_error(ERROR_ENGINE_STATE_INVALID);
    }
    return physics_joint_anchors_set(joint, anchor_a, anchor_b, JOINT_SPRING,
        rest_length, stiffness, damping);
}

EntityResult physics_joint_create(
    Entity a,
    Entity b,
    JointType type,
    Vec2D local_anchor_a,
    Vec2D local_anchor_b,
    float stiffness,
    float damping
) {
    EntityIndex a_index;
    EntityIndex b_index;
    EntityResult joint_result;
    EngineResult result;

    if(!(entity_index_get(a, &a_index) && entity_index_alive_check(a_index)) || !(entity_index_get(b, &b_index) && entity_index_alive_check(b_index))) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_INVALID_ENTITY);
    }

    joint_result = entity_add();
    if(joint_result.kind == ERROR_RESULT_ERROR) {
        return joint_result;
    }
    Entity joint = joint_result.result.value;
    EntityIndex joint_index;
    if(!(entity_index_get(joint, &joint_index) && entity_index_alive_check(joint_index))) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    }

    Vec2D world_anchor_a = {
        .x = positions[a_index].x + math_vector_rotate(local_anchor_a, orientations[a_index]).x,
        .y = positions[a_index].y + math_vector_rotate(local_anchor_a, orientations[a_index]).y
    };

    Vec2D world_anchor_b = {
        .x = positions[b_index].x + math_vector_rotate(local_anchor_b, orientations[b_index]).x,
        .y = positions[b_index].y + math_vector_rotate(local_anchor_b, orientations[b_index]).y
    };

    Vec2D delta = {
        .x = world_anchor_b.x - world_anchor_a.x,
        .y = world_anchor_b.y - world_anchor_a.y
    };

    result = physics_joint_component_set(joint, (Joint){
        .type = type,
        .a = a,
        .b = b,
        .local_anchor_a = local_anchor_a,
        .local_anchor_b = local_anchor_b,
        .rest_length = math_vector_magnitude(delta),
        .stiffness = stiffness,
        .damping = damping,
        .lock_angle = false,
        .rest_angle = orientations[b_index] - orientations[a_index],
        .angular_stiffness = 0.0f,
        .angular_damping = 0.0f
    });
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)entity_delete(joint);
        return ERROR_RESULT_MAKE_ERROR(EntityResult, result.result.error);
    }

    return ERROR_RESULT_MAKE_VALUE(EntityResult, joint);
}

EntityResult physics_soft_body_create(void) {
    EntityResult result = entity_add();
    EntityIndex index;

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!entity_index_get(result.result.value, &index) ||
            SoftBodyPool_store_at(&soft_bodies_pool, index, (SoftBody){0}).kind == ERROR_RESULT_ERROR) {
        (void)entity_delete(result.result.value);
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[index] |= SOFT_BODY;
    return result;
}

SoftBodyResult physics_soft_body_get(Entity soft_body) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(soft_body, &index);

    if(result.kind == ERROR_RESULT_ERROR) return ERROR_RESULT_MAKE_ERROR(SoftBodyResult, result.result.error);
    if(!entity_index_components_check(index, SOFT_BODY) || !soft_bodies_pool.used[index]) {
        return ERROR_RESULT_MAKE_ERROR(SoftBodyResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(SoftBodyResult, soft_bodies[index]);
}

EntityResult physics_soft_body_node_create(Entity soft_body, Position position,
        Mass node_mass, float radius) {
    EntityIndex body_index;
    EntityIndex node_index;
    EntityResult node;
    EngineResult result = physics_live_index_get(soft_body, &body_index);

    if(result.kind == ERROR_RESULT_ERROR || !entity_index_components_check(body_index, SOFT_BODY) ||
            !soft_bodies_pool.used[body_index] || node_mass <= 0.0f || radius <= 0.0f) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_STATE_INVALID);
    }
    if(soft_bodies[body_index].node_count >= SOFT_BODY_MAX_NODES) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
    }
    node = entity_add();
    if(node.kind == ERROR_RESULT_ERROR) return node;
    if(!entity_index_get(node.result.value, &node_index) ||
            physics_position_set(node.result.value, position).kind == ERROR_RESULT_ERROR ||
            physics_mass_set(node.result.value, node_mass).kind == ERROR_RESULT_ERROR ||
            physics_velocity_set(node.result.value, (Velocity){0}).kind == ERROR_RESULT_ERROR ||
            physics_acceleration_set(node.result.value, (Acceleration){0}).kind == ERROR_RESULT_ERROR ||
            physics_dynamic_set(node.result.value).kind == ERROR_RESULT_ERROR ||
            SoftBodyNodePool_store_at(&soft_body_nodes_pool, node_index, (SoftBodyNode){
                .soft_body = soft_body,
                .radius = radius,
                .category = ROHR_COLLISION_CATEGORY_DEFAULT,
                .collides_with = ROHR_COLLISION_CATEGORY_ALL
            }).kind == ERROR_RESULT_ERROR) {
        (void)entity_delete(node.result.value);
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[node_index] |= SOFT_BODY_NODE;
    soft_bodies[body_index].nodes[soft_bodies[body_index].node_count++] = node.result.value;
    return node;
}

SoftBodyNodeResult physics_soft_body_node_get(Entity node) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(node, &index);

    if(result.kind == ERROR_RESULT_ERROR) return ERROR_RESULT_MAKE_ERROR(SoftBodyNodeResult, result.result.error);
    if(!entity_index_components_check(index, SOFT_BODY_NODE) || !soft_body_nodes_pool.used[index]) {
        return ERROR_RESULT_MAKE_ERROR(SoftBodyNodeResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(SoftBodyNodeResult, soft_body_nodes[index]);
}

EngineResult physics_soft_body_node_collision_filter_set(Entity node,
        RohrCollisionCategoryMask category, RohrCollisionCategoryMask collides_with) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(node, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!entity_index_components_check(index, SOFT_BODY_NODE) || !soft_body_nodes_pool.used[index]) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    soft_body_nodes[index].category = category;
    soft_body_nodes[index].collides_with = collides_with;
    return error_result_value(true);
}

EngineResult physics_soft_body_node_force_for_one_tick_apply(Entity node, Force force) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(node, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!entity_index_components_check(index, SOFT_BODY_NODE)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    return physics_force_for_one_tick_apply(node, force);
}

EngineResult physics_soft_body_node_impulse_apply(Entity node, Vec2D impulse) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(node, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!entity_index_components_check(index, SOFT_BODY_NODE)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    return physics_impulse_apply(node, impulse);
}

EngineResult physics_soft_body_force_for_one_tick_apply(Entity soft_body, Force force) {
    SoftBodyResult body_result = physics_soft_body_get(soft_body);
    SoftBody body;
    float total_mass = 0.0f;

    if(body_result.kind == ERROR_RESULT_ERROR) return error_result_error(body_result.result.error);
    body = body_result.result.value;
    for(uint32_t i = 0; i < body.node_count; i += 1) {
        EntityIndex index;
        if(!entity_index_get(body.nodes[i], &index) || !entity_index_alive_check(index) || mass[index] <= 0.0f) {
            return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
        }
        total_mass += mass[index];
    }
    if(total_mass <= 0.0f) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    for(uint32_t i = 0; i < body.node_count; i += 1) {
        EntityIndex index;
        EngineResult result;
        (void)entity_index_get(body.nodes[i], &index);
        result = physics_soft_body_node_force_for_one_tick_apply(body.nodes[i], (Force){
            .x = force.x * mass[index] / total_mass,
            .y = force.y * mass[index] / total_mass
        });
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }
    return error_result_value(true);
}

EngineResult physics_soft_body_torque_for_one_tick_apply(Entity soft_body, Torque torque) {
    SoftBodyResult body_result = physics_soft_body_get(soft_body);
    SoftBody body;
    Position center = {0};
    float total_mass = 0.0f;
    float weighted_radius_squared = 0.0f;

    if(body_result.kind == ERROR_RESULT_ERROR) return error_result_error(body_result.result.error);
    body = body_result.result.value;
    for(uint32_t i = 0; i < body.node_count; i += 1) {
        EntityIndex index;
        if(!entity_index_get(body.nodes[i], &index) || !entity_index_alive_check(index) || mass[index] <= 0.0f) {
            return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
        }
        center.x += positions[index].x * mass[index];
        center.y += positions[index].y * mass[index];
        total_mass += mass[index];
    }
    if(total_mass <= 0.0f) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    center.x /= total_mass;
    center.y /= total_mass;
    for(uint32_t i = 0; i < body.node_count; i += 1) {
        EntityIndex index;
        Vec2D offset;
        (void)entity_index_get(body.nodes[i], &index);
        offset = math_vector_subtract(positions[index], center);
        weighted_radius_squared += mass[index] * math_dot_product(offset, offset);
    }
    if(weighted_radius_squared <= 0.0001f) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    for(uint32_t i = 0; i < body.node_count; i += 1) {
        EntityIndex index;
        Vec2D offset;
        float scale;
        EngineResult result;
        (void)entity_index_get(body.nodes[i], &index);
        offset = math_vector_subtract(positions[index], center);
        scale = torque * mass[index] / weighted_radius_squared;
        result = physics_soft_body_node_force_for_one_tick_apply(body.nodes[i], (Force){
            .x = -offset.y * scale,
            .y = offset.x * scale
        });
        if(result.kind == ERROR_RESULT_ERROR) return result;
    }
    return error_result_value(true);
}

SoftBodyNodeAnchorPinResult physics_soft_body_node_to_anchor_pin_create(
        Entity node, JointAnchorId anchor) {
    EntityIndex node_index;
    JointAnchorIdResult node_anchor;
    EntityResult joint;
    EngineResult pin_result;

    if(physics_live_index_get(node, &node_index).kind == ERROR_RESULT_ERROR ||
            !entity_index_components_check(node_index, SOFT_BODY_NODE) ||
            !soft_body_nodes_pool.used[node_index]) {
        return ERROR_RESULT_MAKE_ERROR(SoftBodyNodeAnchorPinResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    if(physics_joint_anchor_world_position_get(anchor).kind == ERROR_RESULT_ERROR) {
        return ERROR_RESULT_MAKE_ERROR(SoftBodyNodeAnchorPinResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    node_anchor = physics_joint_anchor_create(node, (Vec2D){0.0f, 0.0f});
    if(node_anchor.kind == ERROR_RESULT_ERROR) {
        return ERROR_RESULT_MAKE_ERROR(SoftBodyNodeAnchorPinResult, node_anchor.result.error);
    }
    joint = entity_add();
    if(joint.kind == ERROR_RESULT_ERROR) {
        (void)physics_joint_anchor_remove(node_anchor.result.value);
        return ERROR_RESULT_MAKE_ERROR(SoftBodyNodeAnchorPinResult, joint.result.error);
    }
    pin_result = physics_joint_pin_set(joint.result.value, node_anchor.result.value, anchor);
    if(pin_result.kind == ERROR_RESULT_ERROR) {
        (void)entity_delete(joint.result.value);
        (void)physics_joint_anchor_remove(node_anchor.result.value);
        return ERROR_RESULT_MAKE_ERROR(SoftBodyNodeAnchorPinResult, pin_result.result.error);
    }
    return ERROR_RESULT_MAKE_VALUE(SoftBodyNodeAnchorPinResult, ((SoftBodyNodeAnchorPin){
        .joint = joint.result.value,
        .node_anchor = node_anchor.result.value
    }));
}

EntityResult physics_soft_body_beam_create(Entity soft_body, Entity node_a, Entity node_b,
        float stiffness, float damping) {
    EntityIndex body_index;
    EntityIndex a_index;
    EntityIndex b_index;
    EntityIndex beam_index;
    EntityResult beam;
    Vec2D delta;

    if(physics_live_index_get(soft_body, &body_index).kind == ERROR_RESULT_ERROR ||
            physics_live_index_get(node_a, &a_index).kind == ERROR_RESULT_ERROR ||
            physics_live_index_get(node_b, &b_index).kind == ERROR_RESULT_ERROR || node_a == node_b ||
            !entity_index_components_check(body_index, SOFT_BODY) ||
            !entity_index_components_check(a_index, SOFT_BODY_NODE) ||
            !entity_index_components_check(b_index, SOFT_BODY_NODE) ||
            soft_body_nodes[a_index].soft_body != soft_body ||
            soft_body_nodes[b_index].soft_body != soft_body || stiffness < 0.0f || damping < 0.0f) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_STATE_INVALID);
    }
    if(soft_bodies[body_index].beam_count >= SOFT_BODY_MAX_BEAMS) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
    }
    beam = entity_add();
    if(beam.kind == ERROR_RESULT_ERROR) return beam;
    if(!entity_index_get(beam.result.value, &beam_index)) {
        (void)entity_delete(beam.result.value);
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    delta = math_vector_subtract(positions[b_index], positions[a_index]);
    if(SoftBodyBeamPool_store_at(&soft_body_beams_pool, beam_index, (SoftBodyBeam){
            .soft_body = soft_body,
            .node_a = node_a,
            .node_b = node_b,
            .rest_length = math_vector_magnitude(delta),
            .stiffness = stiffness,
            .damping = damping
        }).kind == ERROR_RESULT_ERROR) {
        (void)entity_delete(beam.result.value);
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[beam_index] |= SOFT_BODY_BEAM;
    soft_bodies[body_index].beams[soft_bodies[body_index].beam_count++] = beam.result.value;
    return beam;
}

SoftBodyBeamResult physics_soft_body_beam_get(Entity beam) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(beam, &index);
    if(result.kind == ERROR_RESULT_ERROR) return ERROR_RESULT_MAKE_ERROR(SoftBodyBeamResult, result.result.error);
    if(!entity_index_components_check(index, SOFT_BODY_BEAM) || !soft_body_beams_pool.used[index]) {
        return ERROR_RESULT_MAKE_ERROR(SoftBodyBeamResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(SoftBodyBeamResult, soft_body_beams[index]);
}

EntityResult physics_soft_body_triangle_create(Entity soft_body, Entity node_a, Entity node_b, Entity node_c) {
    EntityIndex body_index;
    EntityIndex indices[3];
    Entity nodes_to_check[3] = {node_a, node_b, node_c};
    EntityIndex triangle_index;
    EntityResult triangle;

    if(physics_live_index_get(soft_body, &body_index).kind == ERROR_RESULT_ERROR ||
            !entity_index_components_check(body_index, SOFT_BODY) ||
            node_a == node_b || node_b == node_c || node_a == node_c) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_STATE_INVALID);
    }
    for(uint32_t i = 0; i < 3; i += 1) {
        if(physics_live_index_get(nodes_to_check[i], &indices[i]).kind == ERROR_RESULT_ERROR ||
                !entity_index_components_check(indices[i], SOFT_BODY_NODE) ||
                soft_body_nodes[indices[i]].soft_body != soft_body) {
            return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_STATE_INVALID);
        }
    }
    if(soft_bodies[body_index].triangle_count >= SOFT_BODY_MAX_TRIANGLES) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
    }
    triangle = entity_add();
    if(triangle.kind == ERROR_RESULT_ERROR) return triangle;
    if(!entity_index_get(triangle.result.value, &triangle_index) ||
            SoftBodyTrianglePool_store_at(&soft_body_triangles_pool, triangle_index, (SoftBodyTriangle){
                .soft_body = soft_body,
                .node_a = node_a,
                .node_b = node_b,
                .node_c = node_c
            }).kind == ERROR_RESULT_ERROR) {
        (void)entity_delete(triangle.result.value);
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[triangle_index] |= SOFT_BODY_TRIANGLE;
    soft_bodies[body_index].triangles[soft_bodies[body_index].triangle_count++] = triangle.result.value;
    return triangle;
}

SoftBodyTriangleResult physics_soft_body_triangle_get(Entity triangle) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(triangle, &index);
    if(result.kind == ERROR_RESULT_ERROR) return ERROR_RESULT_MAKE_ERROR(SoftBodyTriangleResult, result.result.error);
    if(!entity_index_components_check(index, SOFT_BODY_TRIANGLE) || !soft_body_triangles_pool.used[index]) {
        return ERROR_RESULT_MAKE_ERROR(SoftBodyTriangleResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(SoftBodyTriangleResult, soft_body_triangles[index]);
}

static bool physics_interaction_entities_valid(Entity entity, Entity target) {
    EntityIndex index;
    EntityIndex target_index;

    return entity_index_get(entity, &index) && entity_index_alive_check(index) &&
        entity_index_get(target, &target_index) && entity_index_alive_check(target_index);
}

static bool physics_interaction_flag_check(
    Entity entity,
    Entity target,
    PhysicsInteractionFlags flags
) {
    return physics_interaction_entities_valid(entity, target) &&
        physics_interaction_current_check(entity, target, flags);
}

static OverlapInfo physics_interaction_overlap_get(
    Entity entity,
    Entity target,
    PhysicsInteractionFlags flags
) {
    PhysicsInteraction interaction;

    if(!physics_interaction_flag_check(entity, target, flags) ||
            !physics_interaction_current_get(entity, target, &interaction)) {
        return (OverlapInfo){.detected = false};
    }
    return interaction.overlap;
}

static bool physics_interaction_entered_check(
    Entity entity,
    Entity target,
    PhysicsInteractionFlags flags
) {
    if(!physics_interaction_entities_valid(entity, target)) return false;
    return physics_interaction_current_check(entity, target, flags) &&
        !physics_interaction_previous_check(entity, target, flags);
}

static bool physics_interaction_stayed_check(
    Entity entity,
    Entity target,
    PhysicsInteractionFlags flags
) {
    if(!physics_interaction_entities_valid(entity, target)) return false;
    return physics_interaction_current_check(entity, target, flags) &&
        physics_interaction_previous_check(entity, target, flags);
}

static bool physics_interaction_exited_check(
    Entity entity,
    Entity target,
    PhysicsInteractionFlags flags
) {
    if(!physics_interaction_entities_valid(entity, target)) return false;
    return !physics_interaction_current_check(entity, target, flags) &&
        physics_interaction_previous_check(entity, target, flags);
}

bool physics_overlap_check(Entity entity, Entity target) {
    return physics_interaction_flag_check(entity, target, PHYSICS_INTERACTION_OVERLAP);
}

OverlapInfo physics_overlap_get(Entity entity, Entity target) {
    return physics_interaction_overlap_get(entity, target, PHYSICS_INTERACTION_OVERLAP);
}

bool physics_overlap_entered_check(Entity entity, Entity target) {
    return physics_interaction_entered_check(entity, target, PHYSICS_INTERACTION_OVERLAP);
}

bool physics_overlap_stayed_check(Entity entity, Entity target) {
    return physics_interaction_stayed_check(entity, target, PHYSICS_INTERACTION_OVERLAP);
}

bool physics_overlap_exited_check(Entity entity, Entity target) {
    return physics_interaction_exited_check(entity, target, PHYSICS_INTERACTION_OVERLAP);
}

bool physics_contact_check(Entity entity, Entity target) {
    return physics_interaction_flag_check(entity, target, PHYSICS_INTERACTION_CONTACT);
}

OverlapInfo physics_contact_get(Entity entity, Entity target) {
    return physics_interaction_overlap_get(entity, target, PHYSICS_INTERACTION_CONTACT);
}

bool physics_contact_entered_check(Entity entity, Entity target) {
    return physics_interaction_entered_check(entity, target, PHYSICS_INTERACTION_CONTACT);
}

bool physics_contact_stayed_check(Entity entity, Entity target) {
    return physics_interaction_stayed_check(entity, target, PHYSICS_INTERACTION_CONTACT);
}

bool physics_contact_exited_check(Entity entity, Entity target) {
    return physics_interaction_exited_check(entity, target, PHYSICS_INTERACTION_CONTACT);
}

EngineResult physics_dt_per_tick_set(Time dt) {
    if(dt <= 0.0) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    physics_dt_per_tick = dt;
    physics_dt_overwritten = true;
    return error_result_value(true);
}

Time physics_dt_per_tick_get(void) {
    return physics_dt_overwritten ? physics_dt_per_tick : engine_time_per_tick_get();
}

void physics_engine_time_per_tick_use(void) {
    physics_dt_per_tick = 0.0;
    physics_dt_overwritten = false;
}

void physics_update(Tick ticks) {
    if(ticks == 0) return;
    system_physics_update(physics_dt_per_tick_get() * (Time)ticks);
}

void physics_dt_update(Time dt) {
    if(dt > 0.0) system_physics_update(dt);
}
