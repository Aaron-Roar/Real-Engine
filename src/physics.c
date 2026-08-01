#include "physics.h"
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
MEMORY_DEFINE_OBJECT_POOL(CollisionReportPool, CollisionReport)
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

PositionPool positions_pool = {0};
OrientationPool orientations_pool = {0};
VelocityPool velocities_pool = {0};
AccelerationPool accelerations_pool = {0};
AccelerationPool force_accelerations_pool = {0};
MassPool mass_pool = {0};
ForcePool forces_pool = {0};
ShapePool hit_boxes_pool = {0};
ShapePool world_hit_boxes_pool = {0};
CollisionReportPool collision_reports_pool = {0};
AngularVelocityPool angular_velocities_pool = {0};
AngularAccelerationPool angular_accelerations_pool = {0};
AngularVelocityPool torque_angular_accelerations_pool = {0};
TorquePool torques_pool = {0};
FrictionPool frictions_pool = {0};
RestitutionPool restitutions_pool = {0};
AngleLockPool angle_locks_pool = {0};
AxisLockPool axis_locks_pool = {0};
TransformLockPool transform_locks_pool = {0};
JointPool joints_pool = {0};

EngineResult physics_tables_init(void) {
    physics_dt_per_tick = 0.0;
    physics_dt_overwritten = false;
    if(PositionPool_init(&positions_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(OrientationPool_init(&orientations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(VelocityPool_init(&velocities_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AccelerationPool_init(&accelerations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AccelerationPool_init(&force_accelerations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(MassPool_init(&mass_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(ForcePool_init(&forces_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(ShapePool_init(&hit_boxes_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(ShapePool_init(&world_hit_boxes_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(CollisionReportPool_init(&collision_reports_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngularVelocityPool_init(&angular_velocities_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngularAccelerationPool_init(&angular_accelerations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngularVelocityPool_init(&torque_angular_accelerations_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(TorquePool_init(&torques_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(FrictionPool_init(&frictions_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(RestitutionPool_init(&restitutions_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AngleLockPool_init(&angle_locks_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(AxisLockPool_init(&axis_locks_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(TransformLockPool_init(&transform_locks_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
    if(JointPool_init(&joints_pool, 0).kind == ERROR_RESULT_ERROR) { goto fail; }
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
    if(new_capacity > collision_reports_pool.capacity && CollisionReportPool_expand(&collision_reports_pool, new_capacity - collision_reports_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > angular_velocities_pool.capacity && AngularVelocityPool_expand(&angular_velocities_pool, new_capacity - angular_velocities_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > angular_accelerations_pool.capacity && AngularAccelerationPool_expand(&angular_accelerations_pool, new_capacity - angular_accelerations_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > torque_angular_accelerations_pool.capacity && AngularVelocityPool_expand(&torque_angular_accelerations_pool, new_capacity - torque_angular_accelerations_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > torques_pool.capacity && TorquePool_expand(&torques_pool, new_capacity - torques_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > frictions_pool.capacity && FrictionPool_expand(&frictions_pool, new_capacity - frictions_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > restitutions_pool.capacity && RestitutionPool_expand(&restitutions_pool, new_capacity - restitutions_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > angle_locks_pool.capacity && AngleLockPool_expand(&angle_locks_pool, new_capacity - angle_locks_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > axis_locks_pool.capacity && AxisLockPool_expand(&axis_locks_pool, new_capacity - axis_locks_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > transform_locks_pool.capacity && TransformLockPool_expand(&transform_locks_pool, new_capacity - transform_locks_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > joints_pool.capacity && JointPool_expand(&joints_pool, new_capacity - joints_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    return error_result_value(true);
}

void physics_tables_destroy(void) {
    (void)PositionPool_destroy(&positions_pool);
    (void)OrientationPool_destroy(&orientations_pool);
    (void)VelocityPool_destroy(&velocities_pool);
    (void)AccelerationPool_destroy(&accelerations_pool);
    (void)AccelerationPool_destroy(&force_accelerations_pool);
    (void)MassPool_destroy(&mass_pool);
    (void)ForcePool_destroy(&forces_pool);
    (void)ShapePool_destroy(&hit_boxes_pool);
    (void)ShapePool_destroy(&world_hit_boxes_pool);
    (void)CollisionReportPool_destroy(&collision_reports_pool);
    (void)AngularVelocityPool_destroy(&angular_velocities_pool);
    (void)AngularAccelerationPool_destroy(&angular_accelerations_pool);
    (void)AngularVelocityPool_destroy(&torque_angular_accelerations_pool);
    (void)TorquePool_destroy(&torques_pool);
    (void)FrictionPool_destroy(&frictions_pool);
    (void)RestitutionPool_destroy(&restitutions_pool);
    (void)AngleLockPool_destroy(&angle_locks_pool);
    (void)AxisLockPool_destroy(&axis_locks_pool);
    (void)TransformLockPool_destroy(&transform_locks_pool);
    (void)JointPool_destroy(&joints_pool);
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
Collision physics_sat_collision_on_axes(Shape shape_1, Shape shape_2, Vec2DList axes, Collision collision) {
    for (int i = 0; i < axes.amount_of_vectors; i += 1) {
        Axis axis = axes.vectors[i];

        Projection p1 = math_project_shape_on_axis(shape_1, axis);
        Projection p2 = math_project_shape_on_axis(shape_2, axis);

        float overlap = math_projection_overlap(p1, p2);

        if (overlap <= 0.0f) {
            return (Collision){ .overlap = false };
        }

        if (overlap < collision.depth) {
            collision.depth = overlap;
            collision.normal = axis;
        }
    }

    return collision;
}
Collision physics_particle_collision(Shape shape_1, Shape shape_2)
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
        return (Collision){
            .overlap = false
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

    return (Collision){
        .overlap = true,
        .normal = normal,
        .depth = radius_sum - distance
    };
}

Collision physics_sat_collision(Shape shape_1, Shape shape_2)
{
    Collision collision = {
        .overlap = true,
        .normal = {0},
        .depth = FLT_MAX
    };

    Vec2DList shape1_axes = math_normalize_vectors(math_create_normals(shape_1));
    Vec2DList shape2_axes = math_normalize_vectors(math_create_normals(shape_2));

    collision = physics_sat_collision_on_axes(shape_1, shape_2, shape1_axes, collision);

    if (!collision.overlap) {
        return collision;
    }

    collision = physics_sat_collision_on_axes(shape_1, shape_2, shape2_axes, collision);

    if (!collision.overlap) {
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
    if(!entity_index_alive_is(*index)) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    return error_result_value(true);
}

bool physics_entity_held_is(EntityIndex index) {
    if(!entity_index_alive_is(index)) {
        return false;
    }
    return entity_index_components_has(index, HOLD);
}

bool physics_entity_movable_is(EntityIndex index) {
    if(!entity_index_alive_is(index)) {
        return false;
    }
    return entity_index_components_has(index, DYNAMIC)
        && !entity_index_components_has(index, STATIC)
        && !physics_entity_held_is(index);
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

static EngineResult physics_group_apply_entity_target(GroupId group, float magnitude, Entity target, PhysicsGroupEntityTargetFn fn) {
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
        if(!entity_alive_is(entity)) {
            continue;
        }
        result = fn(entity, magnitude, target);
        if(result.kind == ERROR_RESULT_ERROR) {
            return result;
        }
    }
    return error_result_value(true);
}

static EngineResult physics_group_apply_entity(GroupId group, PhysicsGroupEntityFn fn) {
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
        if(!entity_alive_is(entity)) {
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
    return physics_group_apply_entity_target(group, speed, target, physics_velocity_toward_entity_set);
}

EngineResult physics_group_velocity_away_from_entity_set(GroupId group, float speed, Entity target) {
    return physics_group_apply_entity_target(group, speed, target, physics_velocity_away_from_entity_set);
}

EngineResult physics_stop_entity(Entity entity) {
    return physics_velocity_set(entity, (Velocity){0});
}

EngineResult physics_group_stop_entities(GroupId group) {
    return physics_group_apply_entity(group, physics_stop_entity);
}

EngineResult physics_apply_impulse(Entity entity, Vec2D impulse) {
    EntityIndex index;
    Velocity velocity;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(!entity_index_components_has(index, MASS)) {
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
    if(!(entity_index_get(force_entity, &force_index) && entity_index_alive_is(force_index))) {
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

EngineResult physics_apply_force_for_one_tick(Entity entity, Force f) {
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
    return physics_group_apply_entity_target(group, acceleration_magnitude, target, physics_acceleration_toward_entity_set);
}

EngineResult physics_group_acceleration_away_from_entity_set(GroupId group, float acceleration_magnitude, Entity target) {
    return physics_group_apply_entity_target(group, acceleration_magnitude, target, physics_acceleration_away_from_entity_set);
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
    if(!(entity_index_get(torque_entity, &torque_index) && entity_index_alive_is(torque_index))) {
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

EngineResult physics_apply_torque_for_one_tick(Entity entity, Torque t) {
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
    entity_mask[index] |= COLLISION | HIT_BOX;
    (void)ShapePool_store_at(&hit_boxes_pool, index, hitbox);
    console_debug_write(LOG_ENGINE, "Set Entity: %d to have a hit box\n", entity);
    return error_result_value(true);
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

    if((entity_index_get(entity, &index) && entity_index_alive_is(index))) {
        if( entity_index_components_has(index, filter) ) {
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
     return entity_add_components(entity, COLLISION);
}
EngineResult physics_dynamic_set(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_add_components(entity, DYNAMIC);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_delete_components(entity, STATIC);
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
    result = entity_add_components(entity, STATIC);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_delete_components(entity, DYNAMIC);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    console_debug_write(LOG_ENGINE, "Set Entity: %d to STATIC\n", entity);
    return error_result_value(true);
}

EngineResult physics_hold_entity(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_add_components(entity, HOLD);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    console_debug_write(LOG_ENGINE, "Hold Entity: %d\n", entity);
    return error_result_value(true);
}

EngineResult physics_unhold_entity(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_delete_components(entity, HOLD);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    console_debug_write(LOG_ENGINE, "Unhold Entity: %d\n", entity);
    return error_result_value(true);
}

EngineResult physics_group_hold_entities(GroupId group) {
    return physics_group_apply_entity(group, physics_hold_entity);
}

EngineResult physics_group_unhold_entities(GroupId group) {
    return physics_group_apply_entity(group, physics_unhold_entity);
}

EngineResult physics_angle_lock_set(Entity entity, Orientation min, Orientation max) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_add_components(entity, ANGLE_LOCK);
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
    result = entity_add_components(entity, AXIS_LOCK);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    Axis normalized_axis = math_normalize_vector(axis);
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
    result = entity_add_components(driven, TRANSFORM_LOCK);
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
EngineResult physics_remove_transform_lock(Entity entity) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(entity, &index);

    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_delete_components(entity, TRANSFORM_LOCK);
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

    Vec2D local_offset = math_rotate_vector(
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
    if(joint.type < JOINT_DISTANCE || joint.type > JOINT_PIN) {
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

    if(!(entity_index_get(a, &a_index) && entity_index_alive_is(a_index)) || !(entity_index_get(b, &b_index) && entity_index_alive_is(b_index))) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_INVALID_ENTITY);
    }

    joint_result = entity_add();
    if(joint_result.kind == ERROR_RESULT_ERROR) {
        return joint_result;
    }
    Entity joint = joint_result.result.value;
    EntityIndex joint_index;
    if(!(entity_index_get(joint, &joint_index) && entity_index_alive_is(joint_index))) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    }

    Vec2D world_anchor_a = {
        .x = positions[a_index].x + math_rotate_vector(local_anchor_a, orientations[a_index]).x,
        .y = positions[a_index].y + math_rotate_vector(local_anchor_a, orientations[a_index]).y
    };

    Vec2D world_anchor_b = {
        .x = positions[b_index].x + math_rotate_vector(local_anchor_b, orientations[b_index]).x,
        .y = positions[b_index].y + math_rotate_vector(local_anchor_b, orientations[b_index]).y
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

EngineResult physics_collision_report_set(Entity entity, Entity target, bool state) {
    EntityIndex index;
    EntityIndex target_index;
    EngineResult result;

    result = physics_live_index_get(entity, &index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = physics_live_index_get(target, &target_index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(index >= collision_reports_pool.capacity) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    if(collision_reports_pool.used[index] == 0) {
        (void)CollisionReportPool_store_at(&collision_reports_pool, index, (CollisionReport){0});
    }
    collision_reports[index].collisions[target_index] = state;
    return error_result_value(true);
}
bool physics_collision_report_get(Entity entity, Entity target) {
    EntityIndex index;
    EntityIndex target_index;

    if(!(entity_index_get(entity, &index) && entity_index_alive_is(index)) || !(entity_index_get(target, &target_index) && entity_index_alive_is(target_index))) {
        return false;
    }
    if(collision_reports[index].collisions[target_index] && collision_reports[target_index].collisions[index]) {
        return true;
    }
    return false;
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

void physics_use_engine_time_per_tick(void) {
    physics_dt_per_tick = 0.0;
    physics_dt_overwritten = false;
}

void physics_update(Tick ticks) {
    if(ticks == 0) return;
    system_update_physics(physics_dt_per_tick_get() * (Time)ticks);
}

void physics_update_dt(Time dt) {
    if(dt > 0.0) system_update_physics(dt);
}
