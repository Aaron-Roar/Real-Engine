#include "physics/physics_step_internal.h"
#include "systems.h"
#include "console.h"
#include "physics/collision/contact_manifold.h"
#include "math2d.h"
#include <float.h>
#include <math.h>

static double physics_rigid_elapsed_ms(uint64_t start) {
    return (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
        (double)SDL_GetPerformanceFrequency();
}

void system_generate_global_hitboxes(void) {
    RohrComponentMask filter = ROHR_HIT_BOX;

    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get(); alive_position += 1) {
        EntityIndex i;

        if(!physics_step_alive_index_at(alive_position, &i)) {
            continue;
        }
        if( entity_index_components_check(i, filter) ) {
            Position pos = positions[i];
            Orientation ort = orientations[i];
            Shape hit_box = hit_boxes[i];

            world_hit_boxes[i] = physics_shape_world_translate(hit_box, pos, ort);
        }
    }
}

Shape system_generate_global_hitbox(Entity entity) {
    RohrComponentMask filter = ROHR_HIT_BOX;
    EntityIndex index;

        if(entity_index_get(entity, &index) && entity_index_alive_check(index)) {
            if( entity_index_components_check(index, filter) ) {
                Position pos = positions[index];
                Orientation ort = orientations[index];
                Shape hit_box = hit_boxes[index];
                world_hit_boxes[index] = physics_shape_world_translate(hit_box, pos, ort);
                return world_hit_boxes[index];
            }
        }
        return (Shape){0};
}

void system_positions_update(double dt) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get(); alive_position += 1) {
        EntityIndex i;

        if(!physics_step_alive_index_at(alive_position, &i)) {
            continue;
        }
        if(physics_entity_movable_get(i)) {
            positions[i] = (Position){
                .x = positions[i].x + (velocities[i].x)*dt,
                .y = positions[i].y + (velocities[i].y)*dt
            };
        }
    }
}

void system_orientations_update(double dt) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get(); alive_position += 1) {
        EntityIndex i;

        if(!physics_step_alive_index_at(alive_position, &i)) {
            continue;
        }
        if(physics_entity_movable_get(i) &&
                !entity_index_components_check(i, ROHR_PARTICLE)) {
            orientations[i] = orientations[i] + angular_velocities[i]*dt;
        }
    }
}


void system_angular_velocities_update(double dt) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get(); alive_position += 1) {
        EntityIndex i;

        if(!physics_step_alive_index_at(alive_position, &i)) {
            continue;
        }
        if(physics_entity_movable_get(i)) {
            if(entity_index_components_check(i, ROHR_PARTICLE)) {
                angular_velocities[i] = 0.0f;
                continue;
            }
            angular_velocities[i] += (angular_accelerations[i] + torque_angular_accelerations[i]) * dt;
        }
    }
}

static void system_angular_velocity_maximums_apply(void) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get();
            alive_position += 1) {
        EntityIndex i;
        AngularVelocity maximum;

        if(!physics_step_alive_index_at(alive_position, &i) ||
                i >= angular_velocity_maximums_pool.capacity ||
                !angular_velocity_maximums_pool.used[i]) continue;
        maximum = angular_velocity_maximums[i];
        if(angular_velocities[i] > maximum) angular_velocities[i] = maximum;
        if(angular_velocities[i] < -maximum) angular_velocities[i] = -maximum;
    }
}

void system_velocities_update(double dt) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get(); alive_position += 1) {
        EntityIndex i;

        if(!physics_step_alive_index_at(alive_position, &i)) {
            continue;
        }
        if(physics_entity_movable_get(i)) {
            velocities[i] = (Velocity){
                .x = velocities[i].x + (accelerations[i].x + force_accelerations[i].x)*dt,
                .y = velocities[i].y + (accelerations[i].y + force_accelerations[i].y)*dt
            };
        }
    }
}

void system_forces_apply(void) {
  RohrComponentMask filter = ROHR_FORCE | ROHR_TARGETABLE;
  RohrComponentMask target_filter = ROHR_MASS;

  for(int i = 0; i < MAX_ENTITIES; i++) {
    if(entity_index_alive_check(i)) { //Check if this entity exists
        if(entity_index_components_check(i, ROHR_HOLD)) {
            continue;
        }
        if( entity_index_components_check(i, filter) ) { //Check if this entity is a targetable force
            EntityIndex target_index;
            if(entity_index_get(targets[i], &target_index) && entity_index_alive_check(target_index)) { //Check if the target to the force exists
                if(physics_entity_simulated_get(target_index) && entity_index_components_check(target_index, target_filter)) { //Check if the target is moveable
                    if(mass[target_index] != 0) {
                        force_accelerations[target_index].x += forces[i].x/mass[target_index];
                        force_accelerations[target_index].y += forces[i].y/mass[target_index];
                    } else {
                        //Force on massless entity
                        console_write(
                            LOG_ENGINE,
                            "Error: failed to update acceleration, force entity %d targets massless entity %u\n",
                            i,
                            targets[i]
                        );
                    }
                }
            }
            else {
                //Forces exist without targets
            }
        }
    }
  }
}

void system_torques_apply(void) {
    //Apply force offset from centroid and torque applied directly
  RohrComponentMask filter = ROHR_TORQUE | ROHR_TARGETABLE;
  RohrComponentMask target_filter = ROHR_MASS;

  for(int i = 0; i < MAX_ENTITIES; i++) {
    if(entity_index_alive_check(i)) { //Check if this entity exists
        if(entity_index_components_check(i, ROHR_HOLD)) {
            continue;
        }
        if( entity_index_components_check(i, filter) ) { //Check if this entity is a targetable force
            EntityIndex target_index;
            if(entity_index_get(targets[i], &target_index) && entity_index_alive_check(target_index)) { //Check if the target to the force exists
                if(physics_entity_simulated_get(target_index) && entity_index_components_check(target_index, target_filter)) { //Check if the target is moveable
                    if(mass[target_index] != 0) {
                        torque_angular_accelerations[target_index] += torques[i]/physics_polygon_moment_of_inertia(hit_boxes[target_index], mass[target_index]);
                    } else {
                        //Force on massless entity
                        console_write(
                            LOG_ENGINE,
                            "Error: failed to update angular acceleration, torque entity %d targets massless entity %u\n",
                            i,
                            targets[i]
                        );
                    }
                }
            }
            else {
                //Forces exist without targets
            }
        }
    }
  }
}

void system_force_torque_accelerations_clear(void) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get(); alive_position += 1) {
        EntityIndex i;

        if(!physics_step_alive_index_at(alive_position, &i)) {
            continue;
        }
        force_accelerations[i].x = 0;
        force_accelerations[i].y = 0;
        torque_angular_accelerations[i] = 0;
    }
}

void physics_rigid_gravity_apply(Acceleration gravity) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get();
            alive_position += 1) {
        EntityIndex index;

        if(!physics_step_alive_index_at(alive_position, &index) ||
                !physics_entity_simulated_get(index) ||
                !entity_index_components_check(index, ROHR_GRAVITY)) continue;
        force_accelerations[index].x += gravity.x;
        force_accelerations[index].y += gravity.y;
    }
}

OverlapInfo system_entity_overlap_get(Entity entity_1, Entity entity_2) {
    Shape shape1 = world_hit_boxes[entity_1];
    Shape shape2 = world_hit_boxes[entity_2];
    if(entity_index_components_check(entity_1, ROHR_PARTICLE) && entity_index_components_check(entity_2, ROHR_PARTICLE)) {
        return physics_particle_overlap_get(shape1, shape2);
    }
    return physics_sat_overlap_get(shape1, shape2);
}

void system_separate_entities_tuned(
    Entity entity_1,
    Entity entity_2,
    OverlapInfo collision
) {
    bool dynamic_1 = physics_entity_simulated_get(entity_1);
    bool dynamic_2 = physics_entity_simulated_get(entity_2);

    float inv_mass_1 =
        dynamic_1 && entity_index_components_check(entity_1, ROHR_MASS) &&
            mass[entity_1] > 0.0f
        ? 1.0f / mass[entity_1]
        : 0.0f;

    float inv_mass_2 =
        dynamic_2 && entity_index_components_check(entity_2, ROHR_MASS) &&
            mass[entity_2] > 0.0f
        ? 1.0f / mass[entity_2]
        : 0.0f;

    float inv_mass_sum = inv_mass_1 + inv_mass_2;

    if(inv_mass_sum <= 0.0f) {
        return;
    }

    Vec2D correction = {
        .x = collision.normal.x * collision.depth,
        .y = collision.normal.y * collision.depth
    };

    float share_1 = inv_mass_1 / inv_mass_sum;
    float share_2 = inv_mass_2 / inv_mass_sum;

    positions[entity_1].x -= correction.x * share_1;
    positions[entity_1].y -= correction.y * share_1;

    positions[entity_2].x += correction.x * share_2;
    positions[entity_2].y += correction.y * share_2;
}

void system_separate_entities(Entity entity_1, Entity entity_2, OverlapInfo collision)
{
    bool entity_1_dynamic = physics_entity_movable_get(entity_1);
    bool entity_2_dynamic = physics_entity_movable_get(entity_2);

    Vec2D correction = {
        .x = collision.normal.x * collision.depth,
        .y = collision.normal.y * collision.depth
    };
    Mass mass_1 = mass[entity_1];
    Mass mass_2 = mass[entity_2];
    Mass mass_sum = mass_1 + mass_2;

    if(entity_1_dynamic && entity_2_dynamic) {
        positions[entity_1].x -= ( (correction.x)*(mass_2/(mass_sum)) );
        positions[entity_1].y -= ( (correction.y)*(mass_2/(mass_sum)) );
        positions[entity_2].x += ( (correction.x)*(mass_1/(mass_sum)) );
        positions[entity_2].y += ( (correction.y)*(mass_1/(mass_sum)) );

    }

    else if(entity_1_dynamic && !entity_2_dynamic) {
        positions[entity_1].x -= (correction.x);
        positions[entity_1].y -= (correction.y);
    }
    else if(!entity_1_dynamic && entity_2_dynamic) {
        positions[entity_2].x += (correction.x);
        positions[entity_2].y += (correction.y);
    }
}

Position system_support_point_average(Shape shape, Vec2D direction)
{
    float best_projection = -FLT_MAX;
    Position sum = {0};
    int count = 0;

    const float epsilon = 0.0001f;

    for(int i = 0; i < shape.amount_of_vertices; i += 1) {
        Position vertex = shape.vertices[i];
        float projection = math_dot_product(vertex, direction);

        if(projection > best_projection + epsilon) {
            best_projection = projection;
            sum = vertex;
            count = 1;
        }
        else if(fabsf(projection - best_projection) <= epsilon) {
            sum.x += vertex.x;
            sum.y += vertex.y;
            count += 1;
        }
    }

    if(count > 0) {
        sum.x /= count;
        sum.y /= count;
    }

    return sum;
}

Position system_particle_edge_get(Entity entity, Vec2D normal, Vec1D radius) {
    return (Position) {
        .x = positions[entity].x + normal.x*radius,
        .y = positions[entity].y + normal.y*radius,
    };
}

Position system_collision_contact_point(Entity entity_1, Entity entity_2, OverlapInfo collision)
{
    Shape shape_1 = world_hit_boxes[entity_1];
    Shape shape_2 = world_hit_boxes[entity_2];

    bool entity_1_dynamic = physics_entity_movable_get(entity_1);
    bool entity_2_dynamic = physics_entity_movable_get(entity_2);

    Vec2D normal = collision.normal;

    Vec2D opposite_normal = {
        .x = -normal.x,
        .y = -normal.y
    };

    Position point_1 = {0};
    Position point_2 = {0};
    bool entity_1_particle =
        entity_index_components_check(entity_1, ROHR_PARTICLE);
    bool entity_2_particle =
        entity_index_components_check(entity_2, ROHR_PARTICLE);

    if(entity_1_particle) {
        Vec1D r1 = math_circle_radius(shape_1, math_polygon_centroid(shape_1));
        point_1 = system_particle_edge_get(entity_1, normal, r1);
    } else {
        point_1 = system_support_point_average(shape_1, normal);
    }
    if(entity_2_particle) {
        Vec1D r2 = math_circle_radius(shape_2, math_polygon_centroid(shape_2));
        point_2 = system_particle_edge_get(entity_2, opposite_normal, r2);
    } else {
        point_2 = system_support_point_average(shape_2, opposite_normal);
    }

    /*
     * A frictionless normal impulse on a particle must pass through its
     * center. Polygon support averaging can otherwise create a false lever
     * arm and convert translational bounce energy into particle spin.
     */
    if(entity_1_particle && !entity_2_particle) {
        return point_1;
    }
    if(!entity_1_particle && entity_2_particle) {
        return point_2;
    }

    //Use dynamic entities contact point
    if(entity_1_dynamic && !entity_2_dynamic) {
        return point_1;
    }

    if(!entity_1_dynamic && entity_2_dynamic) {
        return point_2;
    }

    //Midpoint is good enough for now
    return (Position){
        .x = (point_1.x + point_2.x) * 0.5f,
        .y = (point_1.y + point_2.y) * 0.5f
    };
}

Vec2D system_friction_impulse_apply(
    Entity entity_1,
    Entity entity_2,
    OverlapInfo collision,
    Vec2D r1,
    Vec2D r2,
    float normal_impulse_magnitude,
    float inv_mass_1,
    float inv_mass_2,
    float inv_inertia_1,
    float inv_inertia_2
) {
    bool moving_1 = physics_entity_movable_get(entity_1);
    bool moving_2 = physics_entity_movable_get(entity_2);
    Vec2D angular_v1 = !moving_1 || entity_index_components_check(entity_1, ROHR_PARTICLE)
        ? (Vec2D){0}
        : math_angular_velocity_cross_vec(angular_velocities[entity_1], r1);
    Vec2D angular_v2 = !moving_2 || entity_index_components_check(entity_2, ROHR_PARTICLE)
        ? (Vec2D){0}
        : math_angular_velocity_cross_vec(angular_velocities[entity_2], r2);

    Vec2D contact_v1 = {
        .x = (moving_1 ? velocities[entity_1].x : 0.0f) + angular_v1.x,
        .y = (moving_1 ? velocities[entity_1].y : 0.0f) + angular_v1.y
    };

    Vec2D contact_v2 = {
        .x = (moving_2 ? velocities[entity_2].x : 0.0f) + angular_v2.x,
        .y = (moving_2 ? velocities[entity_2].y : 0.0f) + angular_v2.y
    };

    Vec2D rel_v = {
        .x = contact_v2.x - contact_v1.x,
        .y = contact_v2.y - contact_v1.y
    };

    float rel_v_along_normal = math_dot_product(rel_v, collision.normal);

    Vec2D tangent = {
        .x = rel_v.x - collision.normal.x * rel_v_along_normal,
        .y = rel_v.y - collision.normal.y * rel_v_along_normal
    };

    float tangent_mag = sqrtf(tangent.x * tangent.x + tangent.y * tangent.y);

    if(tangent_mag <= 0) {
        return (Vec2D){0};
    }

    tangent.x /= tangent_mag;
    tangent.y /= tangent_mag;

    float r1_cross_t = math_cross_2d(r1, tangent);
    float r2_cross_t = math_cross_2d(r2, tangent);

    float denominator =
        inv_mass_1 +
        inv_mass_2 +
        (r1_cross_t * r1_cross_t) * inv_inertia_1 +
        (r2_cross_t * r2_cross_t) * inv_inertia_2;

    if(denominator <= 0) {
        return (Vec2D){0};
    }

    float jt = -math_dot_product(rel_v, tangent) / denominator;

    float mu = sqrtf(frictions[entity_1] * frictions[entity_2]);

    float max_friction = fabsf(normal_impulse_magnitude) * mu;

    if(jt > max_friction) {
        jt = max_friction;
    }
    else if(jt < -max_friction) {
        jt = -max_friction;
    }

    Vec2D friction_impulse = {
        .x = tangent.x * jt,
        .y = tangent.y * jt
    };

    velocities[entity_1].x -= friction_impulse.x * inv_mass_1;
    velocities[entity_1].y -= friction_impulse.y * inv_mass_1;

    velocities[entity_2].x += friction_impulse.x * inv_mass_2;
    velocities[entity_2].y += friction_impulse.y * inv_mass_2;

    angular_velocities[entity_1] -= math_cross_2d(r1, friction_impulse) * inv_inertia_1;
    angular_velocities[entity_2] += math_cross_2d(r2, friction_impulse) * inv_inertia_2;
    return friction_impulse;
}

static void system_contact_point_solve(
    Entity first,
    Entity second,
    OverlapInfo overlap,
    Position point,
    bool restitution_enabled,
    float inverse_mass_first,
    float inverse_mass_second,
    float inverse_inertia_first,
    float inverse_inertia_second,
    Velocity *relative_velocity,
    Vec2D *normal_impulse,
    Vec2D *friction_impulse
) {
    bool moving_first = physics_entity_movable_get(first);
    bool moving_second = physics_entity_movable_get(second);
    Vec2D first_offset = math_vector_subtract(point, positions[first]);
    Vec2D second_offset = math_vector_subtract(point, positions[second]);
    Vec2D first_angular_velocity = !moving_first ||
            entity_index_components_check(first, ROHR_PARTICLE)
        ? (Vec2D){0}
        : math_angular_velocity_cross_vec(angular_velocities[first], first_offset);
    Vec2D second_angular_velocity = !moving_second ||
            entity_index_components_check(second, ROHR_PARTICLE)
        ? (Vec2D){0}
        : math_angular_velocity_cross_vec(angular_velocities[second], second_offset);
    Velocity current_relative_velocity = {
        (moving_second ? velocities[second].x : 0.0f) +
            second_angular_velocity.x -
            (moving_first ? velocities[first].x : 0.0f) -
            first_angular_velocity.x,
        (moving_second ? velocities[second].y : 0.0f) +
            second_angular_velocity.y -
            (moving_first ? velocities[first].y : 0.0f) -
            first_angular_velocity.y
    };
    float normal_velocity = math_dot_product(current_relative_velocity, overlap.normal);
    float restitution;
    float first_lever;
    float second_lever;
    float denominator;
    float impulse_magnitude;
    Vec2D impulse;

    if(relative_velocity != NULL) *relative_velocity = current_relative_velocity;
    if(normal_velocity > 0.0f) return;
    restitution = restitution_enabled
        ? fminf(restitutions[first], restitutions[second]) : 0.0f;
    if(fabsf(normal_velocity) < 1.0f) restitution = 0.0f;
    first_lever = math_cross_2d(first_offset, overlap.normal);
    second_lever = math_cross_2d(second_offset, overlap.normal);
    denominator = inverse_mass_first + inverse_mass_second +
        first_lever * first_lever * inverse_inertia_first +
        second_lever * second_lever * inverse_inertia_second;
    if(denominator <= 0.0f) return;
    impulse_magnitude = -(1.0f + restitution) * normal_velocity / denominator;
    impulse = (Vec2D){
        overlap.normal.x * impulse_magnitude,
        overlap.normal.y * impulse_magnitude
    };
    velocities[first].x -= impulse.x * inverse_mass_first;
    velocities[first].y -= impulse.y * inverse_mass_first;
    velocities[second].x += impulse.x * inverse_mass_second;
    velocities[second].y += impulse.y * inverse_mass_second;
    angular_velocities[first] -= math_cross_2d(first_offset, impulse) *
        inverse_inertia_first;
    angular_velocities[second] += math_cross_2d(second_offset, impulse) *
        inverse_inertia_second;
    if(normal_impulse != NULL) *normal_impulse = impulse;
    if(friction_impulse != NULL) {
        *friction_impulse = system_friction_impulse_apply(
            first, second, overlap, first_offset, second_offset,
            impulse_magnitude, inverse_mass_first, inverse_mass_second,
            inverse_inertia_first, inverse_inertia_second);
    }
}

ContactInfo system_resolve_collision(
    Entity first,
    Entity second,
    OverlapInfo overlap,
    bool restitution_enabled
) {
    bool first_particle = entity_index_components_check(first, ROHR_PARTICLE);
    bool second_particle = entity_index_components_check(second, ROHR_PARTICLE);
    float inverse_mass_first = physics_entity_simulated_get(first)
        ? 1.0f / mass[first] : 0.0f;
    float inverse_mass_second = physics_entity_simulated_get(second)
        ? 1.0f / mass[second] : 0.0f;
    float inverse_inertia_first = 0.0f;
    float inverse_inertia_second = 0.0f;
    ContactInfo result = {
        .detected = true,
        .normal = overlap.normal,
        .depth = overlap.depth
    };
    ContactManifold manifold = {0};

    if(!first_particle && !second_particle) {
        manifold = contact_manifold_polygon_get(
            world_hit_boxes[first], world_hit_boxes[second], overlap.normal);
    }
    if(manifold.count == 0) {
        manifold.points[0] = system_collision_contact_point(first, second, overlap);
        manifold.count = 1;
    }
    result.point_count = manifold.count;
    for(uint8_t i = 0; i < manifold.count; i += 1) {
        result.points[i].position = manifold.points[i];
    }
    if(inverse_mass_first + inverse_mass_second <= 0.0f) return result;
    if(!first_particle && inverse_mass_first > 0.0f) {
        float inertia = physics_polygon_moment_of_inertia(hit_boxes[first], mass[first]);
        if(inertia > 0.0f) inverse_inertia_first = 1.0f / inertia;
    }
    if(!second_particle && inverse_mass_second > 0.0f) {
        float inertia = physics_polygon_moment_of_inertia(hit_boxes[second], mass[second]);
        if(inertia > 0.0f) inverse_inertia_second = 1.0f / inertia;
    }
    for(uint8_t i = 0; i < result.point_count; i += 1) {
        system_contact_point_solve(
            first, second, overlap, result.points[i].position, restitution_enabled,
            inverse_mass_first, inverse_mass_second,
            inverse_inertia_first, inverse_inertia_second,
            &result.points[i].relative_velocity,
            &result.points[i].normal_impulse,
            &result.points[i].friction_impulse);
    }
    return result;
}

typedef struct SystemBroadphaseQuery {
    Entity source;
    EntityIndex source_index;
} SystemBroadphaseQuery;

static bool system_broadphase_pair_apply(Entity target, void *context) {
    SystemBroadphaseQuery *query = context;
    EntityIndex target_index;
    OverlapInfo overlap;
    bool responds;
    uint64_t started;

    if(query == NULL || target <= query->source ||
            !entity_index_get(target, &target_index) ||
            !entity_index_alive_check(target_index)) return true;
    if(physics_step_debug_stats_enabled) physics_step_debug_stats.candidate_pair_count += 1;
    if(!physics_collision_between_check(query->source, target)) return true;
    if(physics_step_debug_stats_enabled) {
        physics_step_debug_stats.narrowphase_test_count += 1;
        started = SDL_GetPerformanceCounter();
    }
    overlap = system_entity_overlap_get(query->source_index, target_index);
    if(physics_step_debug_stats_enabled) physics_step_debug_stats.narrowphase_ms += physics_rigid_elapsed_ms(started);
    if(!overlap.detected) return true;
    if(physics_step_debug_stats_enabled) physics_step_debug_stats.overlap_count += 1;
    responds = entity_index_components_check(query->source_index, ROHR_COLLISION) &&
        entity_index_components_check(target_index, ROHR_COLLISION);
    return contact_constraint_list_append(&physics_step_contact_constraints,
        (SystemContactConstraint){
        .type = SYSTEM_CONTACT_CONSTRAINT_RIGID_PAIR,
        .value.rigid = {
            .first = query->source,
            .second = target,
            .first_index = query->source_index,
            .second_index = target_index,
            .overlap = overlap,
            .responds = responds
        }
    });
}

void physics_rigid_contact_point_impulses_accumulate(
    ContactInfo *current,
    const ContactInfo *previous
) {
    bool matched[PHYSICS_CONTACT_POINT_MAX] = {0};

    if(current == NULL || previous == NULL) return;
    for(uint8_t previous_index = 0;
            previous_index < previous->point_count;
            previous_index += 1) {
        uint8_t nearest = PHYSICS_CONTACT_POINT_MAX;
        float nearest_distance = FLT_MAX;

        for(uint8_t current_index = 0;
                current_index < current->point_count;
                current_index += 1) {
            Vec2D delta;
            float distance;

            if(matched[current_index]) continue;
            delta = math_vector_subtract(
                current->points[current_index].position,
                previous->points[previous_index].position);
            distance = math_dot_product(delta, delta);
            if(distance < nearest_distance) {
                nearest = current_index;
                nearest_distance = distance;
            }
        }
        if(nearest >= current->point_count) continue;
        matched[nearest] = true;
        current->points[nearest].normal_impulse.x +=
            previous->points[previous_index].normal_impulse.x;
        current->points[nearest].normal_impulse.y +=
            previous->points[previous_index].normal_impulse.y;
        current->points[nearest].friction_impulse.x +=
            previous->points[previous_index].friction_impulse.x;
        current->points[nearest].friction_impulse.y +=
            previous->points[previous_index].friction_impulse.y;
    }
}

static void system_rigid_contact_constraint_solve(
    SystemContactConstraint *constraint,
    float position_fraction
) {
    ContactInfo result;
    ContactInfo previous_contact;
    EntityIndex first;
    EntityIndex second;
    OverlapInfo overlap;
    bool responds;
    bool first_solve;

    if(constraint == NULL) return;
    first = constraint->value.rigid.first_index;
    second = constraint->value.rigid.second_index;
    overlap = system_entity_overlap_get(first, second);
    if(!overlap.detected) return;
    constraint->value.rigid.overlap = overlap;
    responds = constraint->value.rigid.responds;
    first_solve = !constraint->value.rigid.solved;
    previous_contact = constraint->value.rigid.contact;
    result = responds
        ? system_resolve_collision(first, second, overlap,
            first_solve)
        : (ContactInfo){0};
    constraint->value.rigid.contact = result;
    physics_rigid_contact_point_impulses_accumulate(
        &constraint->value.rigid.contact,
        &previous_contact);
    constraint->value.rigid.solved = true;
    if(responds) {
        if(physics_step_debug_stats_enabled && first_solve) {
            physics_step_debug_stats.contact_count += 1;
        }
        overlap.depth *= position_fraction;
        system_separate_entities_tuned(first, second, overlap);
        physics_step_hitbox_dirty_add(first);
        physics_step_hitbox_dirty_add(second);
    }
}

static void system_broadphase_build(void) {
    aabb_tree_clear(&physics_broadphase_tree);
    for(uint32_t alive_position = 0;
            alive_position < entity_alive_count_get();
            alive_position += 1) {
        EntityIndex index;
        Entity entity;

        if(!physics_step_alive_index_at(alive_position, &index) ||
                !entity_index_components_check(index, ROHR_HIT_BOX) ||
                !physics_step_entity_from_index_get(index, &entity)) continue;
        if(physics_step_debug_stats_enabled) physics_step_debug_stats.collider_count += 1;
        (void)aabb_tree_insert(
            &physics_broadphase_tree,
            entity,
            math_aabb_create(world_hit_boxes[index])
        );
    }
}

static void system_broadphase_collisions_apply(void) {
    for(uint32_t alive_position = 0;
            alive_position < entity_alive_count_get();
            alive_position += 1) {
        EntityIndex index;
        Entity entity;
        SystemBroadphaseQuery query;

        if(!physics_step_alive_index_at(alive_position, &index) ||
                !entity_index_components_check(index, ROHR_HIT_BOX) ||
                !physics_step_entity_from_index_get(index, &entity)) continue;
        query = (SystemBroadphaseQuery){
            .source = entity,
            .source_index = index
        };
        (void)aabb_tree_query(
            &physics_broadphase_tree,
            math_aabb_create(world_hit_boxes[index]),
            system_broadphase_pair_apply,
            &query
        );
    }
}

void system_collisions_apply(void) {
    for(int i = 0; i < MAX_ENTITIES; i += 1) {
        if(!entity_index_alive_check(i)) {
            continue;
        }

        for(int j = i + 1; j < MAX_ENTITIES; j += 1) {
            Entity entity_1;
            Entity entity_2;

            if(!entity_index_alive_check(j)) {
                continue;
            }
            if(i == j) {
                continue;
            }

            if(!entity_index_components_check(i, ROHR_HIT_BOX) || !entity_index_components_check(j, ROHR_HIT_BOX)) {
                continue;
            }
            if(!physics_step_entity_from_index_get(i, &entity_1) || !physics_step_entity_from_index_get(j, &entity_2)) {
                continue;
            }
            if(!physics_collision_between_check(entity_1, entity_2)) {
                continue;
            }
            OverlapInfo collision = system_entity_overlap_get(i, j);


            if(collision.detected == true) {
                bool responds = entity_index_components_check(i, ROHR_COLLISION) && entity_index_components_check(j, ROHR_COLLISION);
                ContactInfo contact = responds
                    ? system_resolve_collision(i, j, collision, true)
                    : (ContactInfo){0};
                physics_step_interaction_by_index_record(
                    i,
                    j,
                    collision,
                    contact,
                    PHYSICS_INTERACTION_OVERLAP |
                        (responds ? PHYSICS_INTERACTION_CONTACT : 0)
                );

            }
        }
    }
}

void system_angle_locks_apply(void) {
    for(Entity entity = 0; entity < MAX_ENTITIES; entity += 1) {
        if(!entity_index_alive_check(entity)) {
            continue;
        }

        if(!physics_entity_movable_get(entity)) {
            continue;
        }

        if(!entity_index_components_check(entity, ROHR_ANGLE_LOCK)) {
            continue;
        }

        Orientation min = angle_locks[entity].min;
        Orientation max = angle_locks[entity].max;

        if(min > max) {
            Orientation temp = min;
            min = max;
            max = temp;
        }

        if(fabsf(max - min) <= 0) {
            orientations[entity] = min;
            angular_velocities[entity] = 0.0f;
            angular_accelerations[entity] = 0.0f;
            torque_angular_accelerations[entity] = 0.0f;
            torques[entity] = 0.0f;
            continue;
        }

        if(orientations[entity] < min) {
            orientations[entity] = min;

            if(angular_velocities[entity] < 0.0f) {
                angular_velocities[entity] = 0.0f;
            }

            if(angular_accelerations[entity] < 0.0f) {
                angular_accelerations[entity] = 0.0f;
            }

            if(torque_angular_accelerations[entity] < 0.0f) {
                torque_angular_accelerations[entity] = 0.0f;
            }

            if(torques[entity] < 0.0f) {
                torques[entity] = 0.0f;
            }
        }

        if(orientations[entity] > max) {
            orientations[entity] = max;

            if(angular_velocities[entity] > 0.0f) {
                angular_velocities[entity] = 0.0f;
            }

            if(angular_accelerations[entity] > 0.0f) {
                angular_accelerations[entity] = 0.0f;
            }

            if(torque_angular_accelerations[entity] > 0.0f) {
                torque_angular_accelerations[entity] = 0.0f;
            }

            if(torques[entity] > 0.0f) {
                torques[entity] = 0.0f;
            }
        }
    }
}

void system_axis_locks_apply(void) {
    for(Entity entity = 0; entity < MAX_ENTITIES; entity += 1) {
        if(!entity_index_alive_check(entity)) {
            continue;
        }

        if(!physics_entity_movable_get(entity)) {
            continue;
        }

        if(!entity_index_components_check(entity, ROHR_AXIS_LOCK)) {
            continue;
        }

        Axis axis = axis_locks[entity].axis;

        float mag = math_axis_magnitude(axis);

        if(mag <= 0) {
            continue;
        }

        axis.x /= mag;
        axis.y /= mag;

        Position point_on_axis = axis_locks[entity].point_on_axis;

        Vec2D relative = {
            .x = positions[entity].x - point_on_axis.x,
            .y = positions[entity].y - point_on_axis.y
        };

        float distance_along_axis = math_dot_product(relative, axis);

        positions[entity].x = point_on_axis.x + axis.x * distance_along_axis;
        positions[entity].y = point_on_axis.y + axis.y * distance_along_axis;

        velocities[entity] = math_project_onto_axis(velocities[entity], axis);

        accelerations[entity] = math_project_onto_axis(accelerations[entity], axis);
        force_accelerations[entity] = math_project_onto_axis(force_accelerations[entity], axis);

        forces[entity] = math_project_onto_axis(forces[entity], axis);
    }
}

void system_transform_locks_apply(void) {
    for(Entity driven = 0; driven < MAX_ENTITIES; driven += 1) {
        if(!entity_index_alive_check(driven)) {
            continue;
        }

        if(!physics_entity_movable_get(driven)) {
            continue;
        }

        if(!entity_index_components_check(driven, ROHR_TRANSFORM_LOCK)) {
            continue;
        }

        Entity driver = transform_locks[driven].driver;
        EntityIndex driver_index;

        if(!entity_index_get(driver, &driver_index) || !entity_index_alive_check(driver_index)) {
            physics_step_transform_lock_by_index_remove(driven);
            continue;
        }

        Vec2D world_offset = math_vector_rotate(
            transform_locks[driven].local_offset,
            orientations[driver_index]
        );

        if(transform_locks[driven].lock_position) {
            positions[driven].x = positions[driver_index].x + world_offset.x;
            positions[driven].y = positions[driver_index].y + world_offset.y;
        }

        if(transform_locks[driven].lock_orientation) {
            orientations[driven] =
                orientations[driver_index] + transform_locks[driven].local_angle;
        }

        if(transform_locks[driven].inherit_velocity) {
            velocities[driven] = velocities[driver_index];

            Vec2D rotational_velocity = math_angular_velocity_cross_vec(
                angular_velocities[driver_index],
                world_offset
            );

            velocities[driven].x += rotational_velocity.x;
            velocities[driven].y += rotational_velocity.y;

            if(transform_locks[driven].lock_orientation) {
                angular_velocities[driven] = angular_velocities[driver_index];
            }
        }
    }
}

void physics_rigid_integrate(double dt) {
    system_forces_apply();
    system_torques_apply();
    system_velocities_update(dt);
    system_angular_velocities_update(dt);
    system_angular_velocity_maximums_apply();
    system_orientations_update(dt);
    system_positions_update(dt);
    system_axis_locks_apply();
    system_angle_locks_apply();
    system_transform_locks_apply();
}

void physics_rigid_accelerations_clear(void) {
    system_force_torque_accelerations_clear();
}

void physics_rigid_constraints_gather(void) {
    uint64_t started = 0;

    system_generate_global_hitboxes();
    if(physics_step_debug_stats_enabled) started = SDL_GetPerformanceCounter();
    system_broadphase_build();
    if(physics_step_debug_stats_enabled) {
        physics_step_debug_stats.broadphase_build_ms +=
            physics_rigid_elapsed_ms(started);
        physics_step_debug_stats.tree_node_count = physics_broadphase_tree.count;
        physics_step_debug_stats.tree_height =
            physics_broadphase_tree.root == AABB_TREE_NODE_INVALID
                ? 0
                : physics_broadphase_tree.nodes[physics_broadphase_tree.root].height;
    }
    system_broadphase_collisions_apply();
}

void physics_rigid_contact_constraint_solve(
    SystemContactConstraint *constraint,
    float position_fraction
) {
    system_rigid_contact_constraint_solve(constraint, position_fraction);
}

void physics_rigid_contact_constraint_finalize(
    const SystemContactConstraint *constraint
) {
    if(constraint == NULL) return;
    physics_step_interaction_by_index_record(
        constraint->value.rigid.first_index,
        constraint->value.rigid.second_index,
        constraint->value.rigid.overlap,
        constraint->value.rigid.contact,
        PHYSICS_INTERACTION_OVERLAP |
            (constraint->value.rigid.responds
                ? PHYSICS_INTERACTION_CONTACT : 0));
}
