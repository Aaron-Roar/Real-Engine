#include "entity_components.h"
#include "engine_internal.h"
#include "systems.h"
#include "console.h"
#include "grid.h"
#include "math2d.h"
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <time.h>

Shape system_generate_global_hitbox(Entity entity);

static bool system_entity_from_index_get(EntityIndex index, Entity *entity) {
    EntityResult result = entity_from_index_get(index);

    if(entity == NULL || result.kind == ERROR_RESULT_ERROR) {
        return false;
    }
    *entity = result.result.value;
    return true;
}

static bool system_alive_index_at(uint32_t alive_position, EntityIndex *index) {
    EntityResult result;

    if(index == NULL) {
        return false;
    }
    result = entity_alive_at_get(alive_position);
    if(result.kind == ERROR_RESULT_ERROR) {
        return false;
    }
    return entity_index_get(result.result.value, index) && entity_index_alive_check(*index);
}

static void system_interaction_by_index_record(
    EntityIndex entity_1,
    EntityIndex entity_2,
    OverlapInfo overlap,
    ContactInfo contact,
    PhysicsInteractionFlags flags
) {
    Entity entity_1_id;
    Entity entity_2_id;

    if(!system_entity_from_index_get(entity_1, &entity_1_id) ||
            !system_entity_from_index_get(entity_2, &entity_2_id)) return;
    (void)physics_interaction_record(
        entity_1_id, entity_2_id, overlap, contact, flags
    );
}

static void system_generate_global_hitbox_by_index(EntityIndex index) {
    Entity entity;

    if(!system_entity_from_index_get(index, &entity)) {
        return;
    }
    system_generate_global_hitbox(entity);
}

static void system_by_index_delete(EntityIndex index) {
    Entity entity;

    if(!system_entity_from_index_get(index, &entity)) {
        return;
    }
    entity_delete(entity);
}

static void system_transform_lock_by_index_remove(EntityIndex index) {
    Entity entity;

    if(!system_entity_from_index_get(index, &entity)) {
        return;
    }
    physics_transform_lock_remove(entity);
}

void system_generate_global_hitboxes(void) {
    RohrComponentMask filter = HIT_BOX;

    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get(); alive_position += 1) {
        EntityIndex i;

        if(!system_alive_index_at(alive_position, &i)) {
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
    RohrComponentMask filter = HIT_BOX;
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

        if(!system_alive_index_at(alive_position, &i)) {
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

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        if(physics_entity_movable_get(i)) {
            orientations[i] = orientations[i] + angular_velocities[i]*dt;
        }
    }
}


void system_angular_velocities_update(double dt) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get(); alive_position += 1) {
        EntityIndex i;

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        if(physics_entity_movable_get(i)) {
            angular_velocities[i] += (angular_accelerations[i] + torque_angular_accelerations[i]) * dt;
        }
    }
}

static void system_angular_velocity_maximums_apply(void) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get();
            alive_position += 1) {
        EntityIndex i;
        AngularVelocity maximum;

        if(!system_alive_index_at(alive_position, &i) ||
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

        if(!system_alive_index_at(alive_position, &i)) {
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
  RohrComponentMask filter = FORCE | TARGETABLE;
  RohrComponentMask target_filter = MASS;

  for(int i = 0; i < MAX_ENTITIES; i++) {
    if(entity_index_alive_check(i)) { //Check if this entity exists
        if(entity_index_components_check(i, HOLD)) {
            continue;
        }
        if( entity_index_components_check(i, filter) ) { //Check if this entity is a targetable force
            EntityIndex target_index;
            if(entity_index_get(targets[i], &target_index) && entity_index_alive_check(target_index)) { //Check if the target to the force exists
                if(physics_entity_movable_get(target_index) && entity_index_components_check(target_index, target_filter)) { //Check if the target is moveable
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
  RohrComponentMask filter = TORQUE | TARGETABLE;
  RohrComponentMask target_filter = MASS;

  for(int i = 0; i < MAX_ENTITIES; i++) {
    if(entity_index_alive_check(i)) { //Check if this entity exists
        if(entity_index_components_check(i, HOLD)) {
            continue;
        }
        if( entity_index_components_check(i, filter) ) { //Check if this entity is a targetable force
            EntityIndex target_index;
            if(entity_index_get(targets[i], &target_index) && entity_index_alive_check(target_index)) { //Check if the target to the force exists
                if(physics_entity_movable_get(target_index) && entity_index_components_check(target_index, target_filter)) { //Check if the target is moveable
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

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        force_accelerations[i].x = 0;
        force_accelerations[i].y = 0;
        torque_angular_accelerations[i] = 0;
    }
}

OverlapInfo system_entity_overlap_get(Entity entity_1, Entity entity_2) {
    Shape shape1 = world_hit_boxes[entity_1];
    Shape shape2 = world_hit_boxes[entity_2];
    if(entity_index_components_check(entity_1, PARTICLE) && entity_index_components_check(entity_2, PARTICLE)) {
        return physics_particle_overlap_get(shape1, shape2);
    }
    return physics_sat_overlap_get(shape1, shape2);
}

void system_separate_entities_tuned(
    Entity entity_1,
    Entity entity_2,
    OverlapInfo collision
) {
    bool dynamic_1 = physics_entity_movable_get(entity_1);
    bool dynamic_2 = physics_entity_movable_get(entity_2);

    float inv_mass_1 =
        dynamic_1 && mass[entity_1] > 0.0f
        ? 1.0f / mass[entity_1]
        : 0.0f;

    float inv_mass_2 =
        dynamic_2 && mass[entity_2] > 0.0f
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
        entity_index_components_check(entity_1, PARTICLE);
    bool entity_2_particle =
        entity_index_components_check(entity_2, PARTICLE);

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
    Vec2D angular_v1 = math_angular_velocity_cross_vec(angular_velocities[entity_1], r1);
    Vec2D angular_v2 = math_angular_velocity_cross_vec(angular_velocities[entity_2], r2);

    Vec2D contact_v1 = {
        .x = velocities[entity_1].x + angular_v1.x,
        .y = velocities[entity_1].y + angular_v1.y
    };

    Vec2D contact_v2 = {
        .x = velocities[entity_2].x + angular_v2.x,
        .y = velocities[entity_2].y + angular_v2.y
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

ContactInfo system_resolve_collision(Entity entity_1, Entity entity_2, OverlapInfo collision) {
    //Assume collision.normal points from entity_1 -> entity_2
    bool entity_1_movable = physics_entity_movable_get(entity_1);
    bool entity_2_movable = physics_entity_movable_get(entity_2);

    float inv_mass_1 = 0.0f;
    float inv_mass_2 = 0.0f;
    ContactInfo contact_info = {
        .detected = true,
        .normal = collision.normal,
        .depth = collision.depth,
        .point = system_collision_contact_point(entity_1, entity_2, collision)
    };

    if (entity_1_movable && mass[entity_1] > 0.0f) {
        inv_mass_1 = 1.0f / mass[entity_1];
    }

    if (entity_2_movable && mass[entity_2] > 0.0f) {
        inv_mass_2 = 1.0f / mass[entity_2];
    }

    //If neither body can move, no velocity response is needed
    if (inv_mass_1 + inv_mass_2 <= 0.0f) {
        return contact_info;
    }

    Position contact = contact_info.point;

    Vec2D r1 = {
        .x = contact.x - positions[entity_1].x,
        .y = contact.y - positions[entity_1].y
    };

    Vec2D r2 = {
        .x = contact.x - positions[entity_2].x,
        .y = contact.y - positions[entity_2].y
    };

    Vec2D rotational_velocity_1 = {0};
    Vec2D rotational_velocity_2 = {0};

    if (entity_1_movable) {
        rotational_velocity_1 =
            math_angular_velocity_cross_vec(angular_velocities[entity_1], r1);
    }

    if (entity_2_movable) {
        rotational_velocity_2 =
            math_angular_velocity_cross_vec(angular_velocities[entity_2], r2);
    }

    Vec2D contact_velocity_1 = {0};
    Vec2D contact_velocity_2 = {0};

    if (entity_1_movable) {
        contact_velocity_1.x = velocities[entity_1].x + rotational_velocity_1.x;
        contact_velocity_1.y = velocities[entity_1].y + rotational_velocity_1.y;
    }

    if (entity_2_movable) {
        contact_velocity_2.x = velocities[entity_2].x + rotational_velocity_2.x;
        contact_velocity_2.y = velocities[entity_2].y + rotational_velocity_2.y;
    }

    Vec2D v_rel = {
        .x = contact_velocity_2.x - contact_velocity_1.x,
        .y = contact_velocity_2.y - contact_velocity_1.y
    };
    contact_info.relative_velocity = v_rel;

    float v_normal = math_dot_product(v_rel, collision.normal);

    if (v_normal > 0.0f) {
        return contact_info;
    }

    float restitution = fminf(restitutions[entity_1], restitutions[entity_2]);

    if(fabsf(v_normal) < 1.0f) {
      restitution = 0.0f;
    }

    float inv_inertia_1 = 0.0f;
    float inv_inertia_2 = 0.0f;

    if (entity_1_movable) {
        float inertia_1 = 0;
        if(entity_index_components_check(entity_1, PARTICLE)) {
            inertia_1 = physics_circle_moment_of_inertia(hit_boxes[entity_1], mass[entity_1]);
        } else {
            inertia_1 =
            physics_polygon_moment_of_inertia(hit_boxes[entity_1], mass[entity_1]);

        }
        if (inertia_1 > 0.0f) {
            inv_inertia_1 = 1.0f / inertia_1;
        }
    }

    if (entity_2_movable) {
        float inertia_2 = 0.0f;

        if(entity_index_components_check(entity_2, PARTICLE)) {
            inertia_2 = physics_circle_moment_of_inertia(
                hit_boxes[entity_2],
                mass[entity_2]
            );
        } else {
            inertia_2 = physics_polygon_moment_of_inertia(
                hit_boxes[entity_2],
                mass[entity_2]
            );
        }

        if(inertia_2 > 0.0f) {
            inv_inertia_2 = 1.0f / inertia_2;
        }
    }

    float r1_cross_n = math_cross_2d(r1, collision.normal);
    float r2_cross_n = math_cross_2d(r2, collision.normal);

    float denominator =
        inv_mass_1 +
        inv_mass_2 +
        (r1_cross_n * r1_cross_n) * inv_inertia_1 +
        (r2_cross_n * r2_cross_n) * inv_inertia_2;

    if (denominator <= 0.0f) {
        return contact_info;
    }

    float impulse_magnitude =
        (-(1.0f + restitution) * v_normal) / denominator;

    Vec2D impulse = {
        .x = collision.normal.x * impulse_magnitude,
        .y = collision.normal.y * impulse_magnitude
    };

    velocities[entity_1].x -= impulse.x * inv_mass_1;
    velocities[entity_1].y -= impulse.y * inv_mass_1;

    velocities[entity_2].x += impulse.x * inv_mass_2;
    velocities[entity_2].y += impulse.y * inv_mass_2;

    angular_velocities[entity_1] -= math_cross_2d(r1, impulse) * inv_inertia_1;
    angular_velocities[entity_2] += math_cross_2d(r2, impulse) * inv_inertia_2;

    {
        Vec2D friction_impulse = system_friction_impulse_apply(
        entity_1,
        entity_2,
        collision,
        r1,
        r2,
        impulse_magnitude,              // pass normal impulse magnitude
        inv_mass_1,
        inv_mass_2,
        inv_inertia_1,
        inv_inertia_2
        );
        contact_info.applied_impulse = (Vec2D){
            .x = impulse.x + friction_impulse.x,
            .y = impulse.y + friction_impulse.y
        };
    }
    return contact_info;
}

void system_entities_to_grid_add(void) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get(); alive_position += 1) {
        EntityIndex i;

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        if( entity_index_components_check(i, HIT_BOX)) {
            grid_entity_add(i);
        }
    }
}

void system_collisions_tuned_apply(void) {
    for(int row = 0; row < GRID_ROWS; row += 1) {
        for(int col = 0; col < GRID_COLS; col += 1) {
            Cell *cell = &grid.cells[row][col];

            for(uint16_t i = 0; i < cell->entity_count; i += 1) {
                    for(uint16_t j = i + 1; j < cell->entity_count; j += 1) {
                            EntityIndex entity_1 = grid.cells[row][col].entities[i];
                            EntityIndex entity_2 = grid.cells[row][col].entities[j];
                            Entity entity_1_id;
                            Entity entity_2_id;
                            if(grid_pair_checked_get(entity_1,entity_2)) {
                                continue;
                            }
                            grid_pair_add(entity_1,entity_2);
                            if(!system_entity_from_index_get(entity_1, &entity_1_id) ||
                                    !system_entity_from_index_get(entity_2, &entity_2_id)) {
                                continue;
                            }
                            if(!physics_collision_between_check(entity_1_id, entity_2_id)) {
                                continue;
                            }
                            OverlapInfo collision = system_entity_overlap_get(entity_1, entity_2);
                            if(collision.detected == true) {
                                bool responds = entity_index_components_check(entity_1, COLLISION) && entity_index_components_check(entity_2, COLLISION);
                                ContactInfo contact = responds
                                    ? system_resolve_collision(entity_1, entity_2, collision)
                                    : (ContactInfo){0};
                                system_interaction_by_index_record(
                                    entity_1,
                                    entity_2,
                                    collision,
                                    contact,
                                    PHYSICS_INTERACTION_OVERLAP |
                                        (responds ? PHYSICS_INTERACTION_CONTACT : 0)
                                );
                                if(responds) {
                                    system_separate_entities(entity_1,entity_2, collision);

                                    system_generate_global_hitbox_by_index(entity_1);
                                    system_generate_global_hitbox_by_index(entity_2);
                                    grid_aabb_update(entity_1);
                                    grid_aabb_update(entity_2);
                                }

                            }
                    }
            }
        }
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

            if(!entity_index_components_check(i, HIT_BOX) || !entity_index_components_check(j, HIT_BOX)) {
                continue;
            }
            if(!system_entity_from_index_get(i, &entity_1) || !system_entity_from_index_get(j, &entity_2)) {
                continue;
            }
            if(!physics_collision_between_check(entity_1, entity_2)) {
                continue;
            }
            OverlapInfo collision = system_entity_overlap_get(i, j);


            if(collision.detected == true) {
                bool responds = entity_index_components_check(i, COLLISION) && entity_index_components_check(j, COLLISION);
                ContactInfo contact = responds
                    ? system_resolve_collision(i, j, collision)
                    : (ContactInfo){0};
                system_interaction_by_index_record(
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

        if(!entity_index_components_check(entity, ANGLE_LOCK)) {
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

        if(!entity_index_components_check(entity, AXIS_LOCK)) {
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

        if(!entity_index_components_check(driven, TRANSFORM_LOCK)) {
            continue;
        }

        Entity driver = transform_locks[driven].driver;
        EntityIndex driver_index;

        if(!entity_index_get(driver, &driver_index) || !entity_index_alive_check(driver_index)) {
            system_transform_lock_by_index_remove(driven);
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

void system_entities_past_lifetime_clean(void) {
    for(int i = 0; i < MAX_ENTITIES; i += 1) {
        if(!entity_index_alive_check(i)) {
            continue;
        }
        if( entity_index_components_check(i, LIFETIME) ) {

            if( (life_times[i].expirey_time != 0 && life_times[i].expirey_time <= engine_time_get()) ) {
                system_by_index_delete(i);
            }
            else if( (life_times[i].expirey_tick != 0 && life_times[i].expirey_tick <= engine_tick_get()) ) {
                system_by_index_delete(i);
            }
        }
    }
}

static Velocity system_point_velocity(Entity entity, Vec2D world_offset) {
    EntityIndex index;

    if(!entity_index_get(entity, &index) || !entity_index_alive_check(index)) {
        return (Velocity){0};
    }
    if(!physics_entity_movable_get(index)) {
        return (Velocity){0};
    }
    Vec2D angular_part = math_angular_velocity_cross_vec(
        angular_velocities[index],
        world_offset
    );

    return (Velocity){
        .x = velocities[index].x + angular_part.x,
        .y = velocities[index].y + angular_part.y
    };
}
static void system_joint_force_for_one_tick_add(Entity target, Force force) {
    EntityIndex target_index;

    if(!entity_index_get(target, &target_index) || !entity_index_alive_check(target_index)) {
        return;
    }
    if(!physics_entity_movable_get(target_index)) {
        return;
    }
    if(!entity_index_components_check(target_index, MASS) || mass[target_index] == 0.0f) {
        return;
    }
    //Entity force_entity = set_force(target, force);
    force_accelerations[target_index].x += force.x/mass[target_index];
    force_accelerations[target_index].y += force.y/mass[target_index];

    //if(force_entity == 0) {
    //    return;
    //}

    //set_life_time(
    //    force_entity,
    //    0.0,
    //    engine_tick_get() + 1
    //);
}
static void system_joint_torque_for_one_tick_add(Entity target, Torque torque) {
    EntityIndex target_index;

    if(!entity_index_get(target, &target_index) || !entity_index_alive_check(target_index)) {
        return;
    }
    if(!physics_entity_movable_get(target_index)) {
        return;
    }
    if(!entity_index_components_check(target_index, MASS) || mass[target_index] == 0.0f) {
        return;
    }
    torque_angular_accelerations[target_index] += torque/physics_polygon_moment_of_inertia(hit_boxes[target_index], mass[target_index]);
    //Entity torque_entity = set_torque(target, torque);

    //if(torque_entity == 0) {
    //    return;
    //}

    //set_life_time(
    //    torque_entity,
    //    0.0,
    //    engine_tick_get() + 1
    //);
}

static void system_joint_force_at_point_for_one_tick_add(Entity target, Position world_point, Force force) {
    EntityIndex target_index;

    if(!entity_index_get(target, &target_index) || !entity_index_alive_check(target_index)) {
        return;
    }

    if(!physics_entity_movable_get(target_index)) {
        return;
    }

    EntityResult force_result = physics_force_create(target, force);

    if(force_result.kind == ERROR_RESULT_VALUE) {
        entity_life_time_set(force_result.result.value, 0.0, engine_tick_get() + 1);
    }

    Vec2D r = {
        .x = world_point.x - positions[target_index].x,
        .y = world_point.y - positions[target_index].y
    };

    Torque torque = math_cross_2d(r, force);

    EntityResult torque_result = physics_torque_create(target, torque);

    if(torque_result.kind == ERROR_RESULT_VALUE) {
        entity_life_time_set(torque_result.result.value, 0.0, engine_tick_get() + 1);
    }
}

static float system_joint_inverse_inertia(EntityIndex index) {
    float inertia;

    if(!physics_entity_movable_get(index) ||
            !entity_index_components_check(index, MASS | HIT_BOX) || mass[index] <= 0.0f) return 0.0f;
    inertia = physics_polygon_moment_of_inertia(hit_boxes[index], mass[index]);
    return inertia > 0.0f ? 1.0f / inertia : 0.0f;
}

static void system_rigid_anchor_axis_solve(
        EntityIndex a,
        EntityIndex b,
        Vec2D offset_a,
        Vec2D offset_b,
        Vec2D position_error,
        Vec2D velocity_error,
        Vec2D axis
) {
    float inverse_mass_a = physics_entity_movable_get(a) && mass[a] > 0.0f ? 1.0f / mass[a] : 0.0f;
    float inverse_mass_b = physics_entity_movable_get(b) && mass[b] > 0.0f ? 1.0f / mass[b] : 0.0f;
    float inverse_inertia_a = system_joint_inverse_inertia(a);
    float inverse_inertia_b = system_joint_inverse_inertia(b);
    float lever_a = math_cross_2d(offset_a, axis);
    float lever_b = math_cross_2d(offset_b, axis);
    float effective_inverse_mass = inverse_mass_a + inverse_mass_b +
        lever_a * lever_a * inverse_inertia_a + lever_b * lever_b * inverse_inertia_b;
    float position_impulse;
    float velocity_impulse;

    if(effective_inverse_mass <= 0.0f) return;
    position_impulse = math_dot_product(position_error, axis) / effective_inverse_mass;
    velocity_impulse = math_dot_product(velocity_error, axis) / effective_inverse_mass;
    positions[a].x += axis.x * position_impulse * inverse_mass_a;
    positions[a].y += axis.y * position_impulse * inverse_mass_a;
    orientations[a] += lever_a * position_impulse * inverse_inertia_a;
    positions[b].x -= axis.x * position_impulse * inverse_mass_b;
    positions[b].y -= axis.y * position_impulse * inverse_mass_b;
    orientations[b] -= lever_b * position_impulse * inverse_inertia_b;
    velocities[a].x += axis.x * velocity_impulse * inverse_mass_a;
    velocities[a].y += axis.y * velocity_impulse * inverse_mass_a;
    angular_velocities[a] += lever_a * velocity_impulse * inverse_inertia_a;
    velocities[b].x -= axis.x * velocity_impulse * inverse_mass_b;
    velocities[b].y -= axis.y * velocity_impulse * inverse_mass_b;
    angular_velocities[b] -= lever_b * velocity_impulse * inverse_inertia_b;
}

static void system_pin_joint_apply(Entity joint_entity) {
    Joint joint = joints[joint_entity];

    Entity a = joint.a;
    Entity b = joint.b;
    EntityIndex a_index;
    EntityIndex b_index;

    if(!entity_index_get(a, &a_index) || !entity_index_alive_check(a_index) || !entity_index_get(b, &b_index) || !entity_index_alive_check(b_index)) {
        system_by_index_delete(joint_entity);
        return;
    }

    Vec2D offset_a;
    Vec2D offset_b;
    Position world_anchor_a;
    Position world_anchor_b;

    if(joint.anchor_a != JOINT_ANCHOR_INVALID && joint.anchor_b != JOINT_ANCHOR_INVALID) {
        JointAnchorPositionResult anchor_a = physics_joint_anchor_world_position_get(joint.anchor_a);
        JointAnchorPositionResult anchor_b = physics_joint_anchor_world_position_get(joint.anchor_b);
        if(anchor_a.kind == ERROR_RESULT_ERROR || anchor_b.kind == ERROR_RESULT_ERROR) {
            system_by_index_delete(joint_entity);
            return;
        }
        world_anchor_a = anchor_a.result.value;
        world_anchor_b = anchor_b.result.value;
        offset_a = math_vector_subtract(world_anchor_a, positions[a_index]);
        offset_b = math_vector_subtract(world_anchor_b, positions[b_index]);
    } else {
        offset_a = math_vector_rotate(joint.local_anchor_a, orientations[a_index]);
        offset_b = math_vector_rotate(joint.local_anchor_b, orientations[b_index]);
        world_anchor_a = (Position){positions[a_index].x + offset_a.x, positions[a_index].y + offset_a.y};
        world_anchor_b = (Position){positions[b_index].x + offset_b.x, positions[b_index].y + offset_b.y};
    }

    Vec2D error = {
        .x = world_anchor_b.x - world_anchor_a.x,
        .y = world_anchor_b.y - world_anchor_a.y
    };

    Velocity velocity_a = system_point_velocity(a, offset_a);
    Velocity velocity_b = system_point_velocity(b, offset_b);

    Vec2D relative_velocity = {
        .x = velocity_b.x - velocity_a.x,
        .y = velocity_b.y - velocity_a.y
    };

    if(joint.anchor_a != JOINT_ANCHOR_INVALID && joint.anchor_b != JOINT_ANCHOR_INVALID) {
        system_rigid_anchor_axis_solve(a_index, b_index, offset_a, offset_b,
            error, relative_velocity, (Vec2D){1.0f, 0.0f});
        system_rigid_anchor_axis_solve(a_index, b_index, offset_a, offset_b,
            error, relative_velocity, (Vec2D){0.0f, 1.0f});
        return;
    }

    Force force_on_a = {
        .x = joint.stiffness * error.x + joint.damping * relative_velocity.x,
        .y = joint.stiffness * error.y + joint.damping * relative_velocity.y
    };

    Force force_on_b = {
        .x = -force_on_a.x,
        .y = -force_on_a.y
    };

    system_joint_force_at_point_for_one_tick_add(
        a,
        world_anchor_a,
        force_on_a
    );

    system_joint_force_at_point_for_one_tick_add(
        b,
        world_anchor_b,
        force_on_b
    );
}

static void system_weld_joint_apply(Entity joint_entity) {
    Joint joint = joints[joint_entity];
    EntityIndex a_index;
    EntityIndex b_index;
    float inverse_inertia_a = 0.0f;
    float inverse_inertia_b = 0.0f;
    float inverse_inertia_sum;
    float angle_error;
    float angular_velocity_error;

    system_pin_joint_apply(joint_entity);
    if(!entity_index_get(joint.a, &a_index) || !entity_index_alive_check(a_index) ||
            !entity_index_get(joint.b, &b_index) || !entity_index_alive_check(b_index)) return;
    if(physics_entity_movable_get(a_index) && entity_index_components_check(a_index, MASS | HIT_BOX)) {
        float inertia = physics_polygon_moment_of_inertia(hit_boxes[a_index], mass[a_index]);
        if(inertia > 0.0f) inverse_inertia_a = 1.0f / inertia;
    }
    if(physics_entity_movable_get(b_index) && entity_index_components_check(b_index, MASS | HIT_BOX)) {
        float inertia = physics_polygon_moment_of_inertia(hit_boxes[b_index], mass[b_index]);
        if(inertia > 0.0f) inverse_inertia_b = 1.0f / inertia;
    }
    inverse_inertia_sum = inverse_inertia_a + inverse_inertia_b;
    if(inverse_inertia_sum <= 0.0f) return;
    angle_error = (orientations[b_index] - orientations[a_index]) - joint.rest_angle;
    angular_velocity_error = angular_velocities[b_index] - angular_velocities[a_index];
    orientations[a_index] += angle_error * inverse_inertia_a / inverse_inertia_sum;
    orientations[b_index] -= angle_error * inverse_inertia_b / inverse_inertia_sum;
    angular_velocities[a_index] += angular_velocity_error * inverse_inertia_a / inverse_inertia_sum;
    angular_velocities[b_index] -= angular_velocity_error * inverse_inertia_b / inverse_inertia_sum;
}

static void system_spring_joint_apply(Entity joint_entity) {
        Joint joint = joints[joint_entity];
        if(joint.type != JOINT_SPRING) {
            return;
        }

        Entity a = joint.a;
        Entity b = joint.b;
        EntityIndex a_index;
        EntityIndex b_index;

        if(!entity_index_get(a, &a_index) || !entity_index_alive_check(a_index) || !entity_index_get(b, &b_index) || !entity_index_alive_check(b_index)) {
            system_by_index_delete(joint_entity);
            return;
        }

        Vec2D offset_a;
        Vec2D offset_b;
        Position world_anchor_a;
        Position world_anchor_b;
        if(joint.anchor_a != JOINT_ANCHOR_INVALID && joint.anchor_b != JOINT_ANCHOR_INVALID) {
            JointAnchorPositionResult anchor_a = physics_joint_anchor_world_position_get(joint.anchor_a);
            JointAnchorPositionResult anchor_b = physics_joint_anchor_world_position_get(joint.anchor_b);
            if(anchor_a.kind == ERROR_RESULT_ERROR || anchor_b.kind == ERROR_RESULT_ERROR) {
                system_by_index_delete(joint_entity);
                return;
            }
            world_anchor_a = anchor_a.result.value;
            world_anchor_b = anchor_b.result.value;
            offset_a = math_vector_subtract(world_anchor_a, positions[a_index]);
            offset_b = math_vector_subtract(world_anchor_b, positions[b_index]);
        } else {
            offset_a = math_vector_rotate(joint.local_anchor_a, orientations[a_index]);
            offset_b = math_vector_rotate(joint.local_anchor_b, orientations[b_index]);
            world_anchor_a = (Position){positions[a_index].x + offset_a.x, positions[a_index].y + offset_a.y};
            world_anchor_b = (Position){positions[b_index].x + offset_b.x, positions[b_index].y + offset_b.y};
        }

        Vec2D delta = {
            .x = world_anchor_b.x - world_anchor_a.x,
            .y = world_anchor_b.y - world_anchor_a.y
        };

        float length = math_vector_magnitude(delta);

        if(length <= 0.0) {
            return;
        }

        Vec2D normal = {
            .x = delta.x / length,
            .y = delta.y / length
        };

        Velocity velocity_a = system_point_velocity(a, offset_a);
        Velocity velocity_b = system_point_velocity(b, offset_b);

        Vec2D relative_velocity = {
            .x = velocity_b.x - velocity_a.x,
            .y = velocity_b.y - velocity_a.y
        };

        float relative_speed = math_dot_product(relative_velocity, normal);

        float stretch = length - joint.rest_length;

        float force_magnitude =
            joint.stiffness * stretch +
            joint.damping * relative_speed;

        Force force_on_a = {
            .x = normal.x * force_magnitude,
            .y = normal.y * force_magnitude
        };

        Force force_on_b = {
            .x = -force_on_a.x,
            .y = -force_on_a.y
        };

        Vec2D r_a = {
            .x = world_anchor_a.x - positions[a_index].x,
            .y = world_anchor_a.y - positions[a_index].y
        };

        Vec2D r_b = {
            .x = world_anchor_b.x - positions[b_index].x,
            .y = world_anchor_b.y - positions[b_index].y
        };

        Torque torque_on_a = math_cross_2d(r_a, force_on_a);
        Torque torque_on_b = math_cross_2d(r_b, force_on_b);

        system_joint_force_for_one_tick_add(a, force_on_a);
        system_joint_force_for_one_tick_add(b, force_on_b);

        system_joint_torque_for_one_tick_add(a, torque_on_a);
        system_joint_torque_for_one_tick_add(b, torque_on_b);

}
void system_joints_apply(void)
{
    for(Entity joint_entity = 0; joint_entity < MAX_ENTITIES; joint_entity += 1) {
        if(!entity_index_alive_check(joint_entity)) {
            continue;
        }
        if(!entity_index_components_check(joint_entity, JOINT)) {
            continue;
        }
        Joint joint = joints[joint_entity];
        switch(joint.type) {
            case JOINT_PIN:
                system_pin_joint_apply(joint_entity);
                break;
            case JOINT_SPRING:
                system_spring_joint_apply(joint_entity);
                break;
            case JOINT_WELD:
                system_weld_joint_apply(joint_entity);
                break;
            default:
                //Not implemented
                break;
        }

    }
}

void system_aabbs_update(void) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count_get(); alive_position += 1) {
        EntityIndex i;

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        if( entity_index_components_check(i, HIT_BOX)) {
            grid_aabb_update(i);
        }
    }
}

static void system_soft_body_beams_apply(void) {
    for(EntityIndex beam_index = 0; beam_index < soft_body_beams_pool.capacity; beam_index += 1) {
        SoftBodyBeam beam;
        EntityIndex a;
        EntityIndex b;
        Vec2D delta;
        Vec2D normal;
        Vec2D relative_velocity;
        float length;
        float relative_speed;
        float force_magnitude;
        Vec2D force;

        if(!soft_body_beams_pool.used[beam_index] || !entity_index_alive_check(beam_index) ||
                !entity_index_components_check(beam_index, SOFT_BODY_BEAM)) continue;
        beam = soft_body_beams[beam_index];
        if(!entity_index_get(beam.node_a, &a) || !entity_index_alive_check(a) ||
                !entity_index_get(beam.node_b, &b) || !entity_index_alive_check(b) ||
                mass[a] <= 0.0f || mass[b] <= 0.0f) continue;
        delta = math_vector_subtract(positions[b], positions[a]);
        length = math_vector_magnitude(delta);
        if(length <= 0.0001f) continue;
        normal = (Vec2D){delta.x / length, delta.y / length};
        relative_velocity = math_vector_subtract(velocities[b], velocities[a]);
        relative_speed = math_dot_product(relative_velocity, normal);
        force_magnitude = beam.stiffness * (length - beam.rest_length) + beam.damping * relative_speed;
        force = (Vec2D){normal.x * force_magnitude, normal.y * force_magnitude};
        force_accelerations[a].x += force.x / mass[a];
        force_accelerations[a].y += force.y / mass[a];
        force_accelerations[b].x -= force.x / mass[b];
        force_accelerations[b].y -= force.y / mass[b];
    }
}

static bool system_soft_node_rigid_filter_allows(EntityIndex node, EntityIndex rigid) {
    CollisionFilterConfig rigid_filter = physics_collision_filter_config_default_get();

    if(entity_index_components_check(rigid, COLLISION_FILTER) &&
            rigid < collision_filters_pool.capacity && collision_filters_pool.used[rigid]) {
        rigid_filter = collision_filters[rigid];
    }
    return (soft_body_nodes[node].collides_with & rigid_filter.category) != 0 &&
        (rigid_filter.collides_with & soft_body_nodes[node].category) != 0;
}

static void system_soft_body_node_rigid_collisions_apply(void) {
    for(EntityIndex node = 0; node < soft_body_nodes_pool.capacity; node += 1) {
        Shape node_shape;

        if(!soft_body_nodes_pool.used[node] || !entity_index_alive_check(node) ||
                !entity_index_components_check(node, SOFT_BODY_NODE)) continue;
        node_shape = physics_shape_world_translate(
            math_circle_create(soft_body_nodes[node].radius, 8),
            positions[node],
            0.0f
        );
        for(EntityIndex rigid = 0; rigid < world_hit_boxes_pool.capacity; rigid += 1) {
            OverlapInfo collision;
            float inverse_mass_node;
            float inverse_mass_rigid;
            float inverse_mass_sum;
            Vec2D relative_velocity;
            float normal_velocity;
            float impulse_magnitude;
            float restitution;
            ContactInfo contact_info;

            if(node == rigid || !entity_index_alive_check(rigid) ||
                    !entity_index_components_check(rigid, HIT_BOX | COLLISION) ||
                    entity_index_components_check(rigid, SOFT_BODY_NODE) ||
                    !system_soft_node_rigid_filter_allows(node, rigid)) continue;
            collision = physics_sat_overlap_get(node_shape, world_hit_boxes[rigid]);
            if(!collision.detected) continue;
            relative_velocity = math_vector_subtract(
                velocities[rigid], velocities[node]
            );
            contact_info = (ContactInfo){
                .detected = true,
                .normal = collision.normal,
                .depth = collision.depth,
                .point = {
                    positions[node].x +
                        collision.normal.x * soft_body_nodes[node].radius,
                    positions[node].y +
                        collision.normal.y * soft_body_nodes[node].radius
                },
                .relative_velocity = relative_velocity
            };
            system_interaction_by_index_record(
                node,
                rigid,
                collision,
                contact_info,
                PHYSICS_INTERACTION_OVERLAP | PHYSICS_INTERACTION_CONTACT
            );
            inverse_mass_node = physics_entity_movable_get(node) && mass[node] > 0.0f ? 1.0f / mass[node] : 0.0f;
            inverse_mass_rigid = physics_entity_movable_get(rigid) && mass[rigid] > 0.0f ? 1.0f / mass[rigid] : 0.0f;
            inverse_mass_sum = inverse_mass_node + inverse_mass_rigid;
            if(inverse_mass_sum <= 0.0f) continue;
            positions[node].x -= collision.normal.x * collision.depth * inverse_mass_node / inverse_mass_sum;
            positions[node].y -= collision.normal.y * collision.depth * inverse_mass_node / inverse_mass_sum;
            positions[rigid].x += collision.normal.x * collision.depth * inverse_mass_rigid / inverse_mass_sum;
            positions[rigid].y += collision.normal.y * collision.depth * inverse_mass_rigid / inverse_mass_sum;
            normal_velocity = math_dot_product(relative_velocity, collision.normal);
            if(normal_velocity >= 0.0f) continue;
            restitution = fminf(
                restitutions_pool.used[node] ? restitutions[node] : 0.25f,
                restitutions_pool.used[rigid] ? restitutions[rigid] : 0.25f
            );
            impulse_magnitude = -(1.0f + restitution) * normal_velocity / inverse_mass_sum;
            velocities[node].x -= collision.normal.x * impulse_magnitude * inverse_mass_node;
            velocities[node].y -= collision.normal.y * impulse_magnitude * inverse_mass_node;
            velocities[rigid].x += collision.normal.x * impulse_magnitude * inverse_mass_rigid;
            velocities[rigid].y += collision.normal.y * impulse_magnitude * inverse_mass_rigid;
            contact_info.applied_impulse = (Vec2D){
                collision.normal.x * impulse_magnitude,
                collision.normal.y * impulse_magnitude
            };
            system_interaction_by_index_record(
                node,
                rigid,
                collision,
                contact_info,
                PHYSICS_INTERACTION_OVERLAP | PHYSICS_INTERACTION_CONTACT
            );
            {
                float node_friction = frictions_pool.used[node] ? frictions[node] : 0.0f;
                float rigid_friction = frictions_pool.used[rigid] ? frictions[rigid] : 0.0f;
                float coefficient = sqrtf(node_friction * rigid_friction);
                Vec2D tangent = {-collision.normal.y, collision.normal.x};
                Vec2D rigid_offset = {
                    positions[node].x + collision.normal.x * soft_body_nodes[node].radius - positions[rigid].x,
                    positions[node].y + collision.normal.y * soft_body_nodes[node].radius - positions[rigid].y
                };
                float inverse_inertia_rigid = 0.0f;
                float tangent_speed;
                float tangent_impulse;
                float maximum_friction;
                float denominator;

                if(physics_entity_movable_get(rigid) &&
                        entity_index_components_check(rigid, MASS | HIT_BOX)) {
                    float inertia = physics_polygon_moment_of_inertia(
                        hit_boxes[rigid], mass[rigid]);
                    if(inertia > 0.0f) inverse_inertia_rigid = 1.0f / inertia;
                }
                {
                    Vec2D angular_velocity = math_angular_velocity_cross_vec(
                        angular_velocities[rigid], rigid_offset);
                    relative_velocity = (Vec2D){
                        velocities[rigid].x + angular_velocity.x - velocities[node].x,
                        velocities[rigid].y + angular_velocity.y - velocities[node].y
                    };
                }
                tangent_speed = math_dot_product(relative_velocity, tangent);
                denominator = inverse_mass_sum +
                    powf(math_cross_2d(rigid_offset, tangent), 2.0f) * inverse_inertia_rigid;
                if(coefficient <= 0.0f || denominator <= 0.0f) continue;
                tangent_impulse = -tangent_speed / denominator;
                maximum_friction = fabsf(impulse_magnitude) * coefficient;
                tangent_impulse = fmaxf(-maximum_friction,
                    fminf(tangent_impulse, maximum_friction));
                velocities[node].x -= tangent.x * tangent_impulse * inverse_mass_node;
                velocities[node].y -= tangent.y * tangent_impulse * inverse_mass_node;
                velocities[rigid].x += tangent.x * tangent_impulse * inverse_mass_rigid;
                velocities[rigid].y += tangent.y * tangent_impulse * inverse_mass_rigid;
                angular_velocities[rigid] += math_cross_2d(
                    rigid_offset,
                    (Vec2D){tangent.x * tangent_impulse, tangent.y * tangent_impulse}
                ) * inverse_inertia_rigid;
                contact_info.applied_impulse.x += tangent.x * tangent_impulse;
                contact_info.applied_impulse.y += tangent.y * tangent_impulse;
                system_interaction_by_index_record(
                    node,
                    rigid,
                    collision,
                    contact_info,
                    PHYSICS_INTERACTION_OVERLAP | PHYSICS_INTERACTION_CONTACT
                );
            }
        }
    }
}

void system_physics_update(double dt) {
    physics_interactions_step_begin();
    system_force_torque_accelerations_clear();
    system_joints_apply();
    system_soft_body_beams_apply();

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

    system_generate_global_hitboxes();
    system_soft_body_node_rigid_collisions_apply();
    system_aabbs_update();
    system_entities_to_grid_add();
    system_collisions_tuned_apply();
    grid_clear();
}

void print_entity_movement(Entity entity) {
    console_write(LOG_ENGINE, "---Movement Log---\n");
    console_write(LOG_ENGINE, "Entity: %d\n", entity);
    console_write(LOG_ENGINE, "Position: {x: %f, y: %f}\n", positions[entity].x, positions[entity].y);
    console_write(LOG_ENGINE, "Velocity: {x: %f, y: %f}\n", velocities[entity].x, velocities[entity].y);
    console_write(LOG_ENGINE, "Acceleration: {x: %f, y: %f}\n", accelerations[entity].x, accelerations[entity].y);
    console_write(LOG_ENGINE, "---Movement Log---\n");
}
