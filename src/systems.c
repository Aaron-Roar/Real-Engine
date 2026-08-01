#include "entity_components.h"
#include "systems.h"
#include "console.h"
#include "grid.h"
#include "math2d.h"
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <time.h>

Shape system_generate_global_hitbox(Entity entity);

static bool system_entity_from_index(EntityIndex index, Entity *entity) {
    EntityResult result = entity_from_index(index);

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
    result = entity_alive_at(alive_position);
    if(result.kind == ERROR_RESULT_ERROR) {
        return false;
    }
    return entity_get_index(result.result.value, index) && entity_index_is_alive(*index);
}

static void system_set_collision_report_by_index(EntityIndex entity_1, EntityIndex entity_2, bool state) {
    Entity entity_1_id;
    Entity entity_2_id;

    if(!system_entity_from_index(entity_1, &entity_1_id) || !system_entity_from_index(entity_2, &entity_2_id)) {
        return;
    }
    physics_set_collision_report(entity_1_id, entity_2_id, state);
}

static void system_generate_global_hitbox_by_index(EntityIndex index) {
    Entity entity;

    if(!system_entity_from_index(index, &entity)) {
        return;
    }
    system_generate_global_hitbox(entity);
}

static void system_delete_by_index(EntityIndex index) {
    Entity entity;

    if(!system_entity_from_index(index, &entity)) {
        return;
    }
    entity_delete(entity);
}

static void system_remove_transform_lock_by_index(EntityIndex index) {
    Entity entity;

    if(!system_entity_from_index(index, &entity)) {
        return;
    }
    physics_remove_transform_lock(entity);
}

void system_generate_global_hitboxes(void) {
    RohrComponentMask filter = HIT_BOX;

    for(uint32_t alive_position = 0; alive_position < entity_alive_count(); alive_position += 1) {
        EntityIndex i;

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        if( entity_index_has_components(i, filter) ) {
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

        if(entity_get_index(entity, &index) && entity_index_is_alive(index)) {
            if( entity_index_has_components(index, filter) ) {
                Position pos = positions[index];
                Orientation ort = orientations[index];
                Shape hit_box = hit_boxes[index];
                world_hit_boxes[index] = physics_shape_world_translate(hit_box, pos, ort);
                return world_hit_boxes[index];
            }
        }
        return (Shape){0};
}

void system_update_positions(double dt) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count(); alive_position += 1) {
        EntityIndex i;

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        if(physics_entity_can_move(i)) {
            positions[i] = (Position){
                .x = positions[i].x + (velocities[i].x)*dt,
                .y = positions[i].y + (velocities[i].y)*dt
            };
        }
    }
}

void system_update_orientations(double dt) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count(); alive_position += 1) {
        EntityIndex i;

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        if(physics_entity_can_move(i)) {
            orientations[i] = orientations[i] + angular_velocities[i]*dt;
        }
    }
}


void system_update_angular_velocities(double dt) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count(); alive_position += 1) {
        EntityIndex i;

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        if(physics_entity_can_move(i)) {
            angular_velocities[i] += (angular_accelerations[i] + torque_angular_accelerations[i]) * dt;
        }
    }
}

void system_update_velocities(double dt) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count(); alive_position += 1) {
        EntityIndex i;

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        if(physics_entity_can_move(i)) {
            velocities[i] = (Velocity){
                .x = velocities[i].x + (accelerations[i].x + force_accelerations[i].x)*dt,
                .y = velocities[i].y + (accelerations[i].y + force_accelerations[i].y)*dt
            };
        }
    }
}

void system_apply_forces(void) {
  RohrComponentMask filter = FORCE | TARGETABLE;
  RohrComponentMask target_filter = MASS;

  for(int i = 0; i < MAX_ENTITIES; i++) {
    if(entity_index_is_alive(i)) { //Check if this entity exists
        if(entity_index_has_components(i, HOLD)) {
            continue;
        }
        if( entity_index_has_components(i, filter) ) { //Check if this entity is a targetable force
            EntityIndex target_index;
            if(entity_get_index(targets[i], &target_index) && entity_index_is_alive(target_index)) { //Check if the target to the force exists
                if(physics_entity_can_move(target_index) && entity_index_has_components(target_index, target_filter)) { //Check if the target is moveable
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

void system_apply_torques(void) {
    //Apply force offset from centroid and torque applied directly
  RohrComponentMask filter = TORQUE | TARGETABLE;
  RohrComponentMask target_filter = MASS;

  for(int i = 0; i < MAX_ENTITIES; i++) {
    if(entity_index_is_alive(i)) { //Check if this entity exists
        if(entity_index_has_components(i, HOLD)) {
            continue;
        }
        if( entity_index_has_components(i, filter) ) { //Check if this entity is a targetable force
            EntityIndex target_index;
            if(entity_get_index(targets[i], &target_index) && entity_index_is_alive(target_index)) { //Check if the target to the force exists
                if(physics_entity_can_move(target_index) && entity_index_has_components(target_index, target_filter)) { //Check if the target is moveable
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

void system_clear_force_torque_accelerations(void) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count(); alive_position += 1) {
        EntityIndex i;

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        force_accelerations[i].x = 0;
        force_accelerations[i].y = 0;
        torque_angular_accelerations[i] = 0;
    }
}

Collision system_get_entity_collision(Entity entity_1, Entity entity_2) {
    Shape shape1 = world_hit_boxes[entity_1];
    Shape shape2 = world_hit_boxes[entity_2];
    if(entity_index_has_components(entity_1, PARTICLE) && entity_index_has_components(entity_2, PARTICLE)) {
        return physics_particle_collision(shape1, shape2);
    }
    return physics_sat_collision(shape1, shape2);
}

void system_separate_entities_tuned(
    Entity entity_1,
    Entity entity_2,
    Collision collision
) {
    bool dynamic_1 = physics_entity_can_move(entity_1);
    bool dynamic_2 = physics_entity_can_move(entity_2);

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

void system_separate_entities(Entity entity_1, Entity entity_2, Collision collision)
{
    bool entity_1_dynamic = physics_entity_can_move(entity_1);
    bool entity_2_dynamic = physics_entity_can_move(entity_2);

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

Position system_get_particle_edge(Entity entity, Vec2D normal, Vec1D radius) {
    return (Position) {
        .x = positions[entity].x + normal.x*radius,
        .y = positions[entity].y + normal.y*radius,
    };
}

Position system_collision_contact_point(Entity entity_1, Entity entity_2, Collision collision)
{
    Shape shape_1 = world_hit_boxes[entity_1];
    Shape shape_2 = world_hit_boxes[entity_2];

    bool entity_1_dynamic = physics_entity_can_move(entity_1);
    bool entity_2_dynamic = physics_entity_can_move(entity_2);

    Vec2D normal = collision.normal;

    Vec2D opposite_normal = {
        .x = -normal.x,
        .y = -normal.y
    };

    Position point_1 = {0};
    Position point_2 = {0};
    bool entity_1_particle =
        entity_index_has_components(entity_1, PARTICLE);
    bool entity_2_particle =
        entity_index_has_components(entity_2, PARTICLE);

    if(entity_1_particle) {
        Vec1D r1 = math_circle_radius(shape_1, math_polygon_centroid(shape_1));
        point_1 = system_get_particle_edge(entity_1, normal, r1);
    } else {
        point_1 = system_support_point_average(shape_1, normal);
    }
    if(entity_2_particle) {
        Vec1D r2 = math_circle_radius(shape_2, math_polygon_centroid(shape_2));
        point_2 = system_get_particle_edge(entity_2, opposite_normal, r2);
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

void system_apply_friction_impulse(
    Entity entity_1,
    Entity entity_2,
    Collision collision,
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
        return;
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
        return;
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
}

void system_resolve_collision(Entity entity_1, Entity entity_2, Collision collision) {
    //Assume collision.normal points from entity_1 -> entity_2
    bool entity_1_movable = physics_entity_can_move(entity_1);
    bool entity_2_movable = physics_entity_can_move(entity_2);

    float inv_mass_1 = 0.0f;
    float inv_mass_2 = 0.0f;

    if (entity_1_movable && mass[entity_1] > 0.0f) {
        inv_mass_1 = 1.0f / mass[entity_1];
    }

    if (entity_2_movable && mass[entity_2] > 0.0f) {
        inv_mass_2 = 1.0f / mass[entity_2];
    }

    //If neither body can move, no velocity response is needed
    if (inv_mass_1 + inv_mass_2 <= 0.0f) {
        return;
    }

    //Position contact = approximate_contact_point(positions[entity_1], positions[entity_2]);
    Position contact = system_collision_contact_point(entity_1, entity_2, collision);

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

    float v_normal = math_dot_product(v_rel, collision.normal);

    if (v_normal > 0.0f) {
        return;
    }

    float restitution = fminf(restitutions[entity_1], restitutions[entity_2]);

    if(fabsf(v_normal) < 1.0f) {
      restitution = 0.0f;
    }

    float inv_inertia_1 = 0.0f;
    float inv_inertia_2 = 0.0f;

    if (entity_1_movable) {
        float inertia_1 = 0;
        if(entity_index_has_components(entity_1, PARTICLE)) {
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

        if(entity_index_has_components(entity_2, PARTICLE)) {
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
        return;
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

        system_apply_friction_impulse(
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
}

void system_add_entities_to_grid(void) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count(); alive_position += 1) {
        EntityIndex i;

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        if( entity_index_has_components(i, HIT_BOX)) {
            add_entity_to_grids(i);
        }
    }
}

void system_apply_collisions_tuned(void) {
    for(int row = 0; row < GRID_ROWS; row += 1) {
        for(int col = 0; col < GRID_COLS; col += 1) {

            for(int i = 0; i < MAX_ENTITIES; i += 1) {
                if(grid.cells[row][col].entity_present[i]) {
                    for(int j = 0; j < MAX_ENTITIES; j +=1 ) {
                        if(i == j) {
                            continue;
                        }
                        if(grid.cells[row][col].entity_present[j]) {
                            Entity entity_1 = grid.cells[row][col].entities[i];
                            Entity entity_2 = grid.cells[row][col].entities[j];
                            if(checked_pair(entity_1,entity_2)) {
                                continue;
                            }
                            add_pair(entity_1,entity_2);
                            Collision collision = system_get_entity_collision(entity_1, entity_2);
                            if(collision.overlap == true) {
                                system_set_collision_report_by_index(entity_1, entity_2, true);
                                system_set_collision_report_by_index(entity_2, entity_1, true);
                                if(entity_index_has_components(entity_1, COLLISION) && entity_index_has_components(entity_2, COLLISION)) {
                                    system_resolve_collision(entity_1, entity_2, collision);
                                    system_separate_entities(entity_1,entity_2, collision);

                                    system_generate_global_hitbox_by_index(entity_1);
                                    system_generate_global_hitbox_by_index(entity_2);
                                    grid_update_aabb(entity_1);
                                    grid_update_aabb(entity_2);
                                }

                            } else {
                                system_set_collision_report_by_index(entity_1, entity_2, false);
                                system_set_collision_report_by_index(entity_2, entity_1, false);

                            }

                        }
                    }
                }

            }

        }
    }
}

void system_apply_collisions(void) {
    for(int i = 0; i < MAX_ENTITIES; i += 1) {
        if(!entity_index_is_alive(i)) {
            continue;
        }

        for(int j = i + 1; j < MAX_ENTITIES; j += 1) {
            if(!entity_index_is_alive(j)) {
                continue;
            }
            if(i == j) {
                continue;
            }

            if(!entity_index_has_components(i, HIT_BOX) || !entity_index_has_components(j, HIT_BOX)) {
                continue;
            }
            Collision collision = system_get_entity_collision(i, j);


            if(collision.overlap == true) {
                system_set_collision_report_by_index(i, j, true);
                system_set_collision_report_by_index(j, i, true);
                if(entity_index_has_components(i, COLLISION) && entity_index_has_components(j, COLLISION)) {
                    system_resolve_collision(i, j, collision);
                }

            }
            else {
                system_set_collision_report_by_index(i, j, false);
                system_set_collision_report_by_index(j, i, false);

            }
        }
    }
}

void system_apply_angle_locks(void) {
    for(Entity entity = 0; entity < MAX_ENTITIES; entity += 1) {
        if(!entity_index_is_alive(entity)) {
            continue;
        }

        if(!physics_entity_can_move(entity)) {
            continue;
        }

        if(!entity_index_has_components(entity, ANGLE_LOCK)) {
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

void system_apply_axis_locks(void) {
    for(Entity entity = 0; entity < MAX_ENTITIES; entity += 1) {
        if(!entity_index_is_alive(entity)) {
            continue;
        }

        if(!physics_entity_can_move(entity)) {
            continue;
        }

        if(!entity_index_has_components(entity, AXIS_LOCK)) {
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

void system_apply_transform_locks(void) {
    for(Entity driven = 0; driven < MAX_ENTITIES; driven += 1) {
        if(!entity_index_is_alive(driven)) {
            continue;
        }

        if(!physics_entity_can_move(driven)) {
            continue;
        }

        if(!entity_index_has_components(driven, TRANSFORM_LOCK)) {
            continue;
        }

        Entity driver = transform_locks[driven].driver;
        EntityIndex driver_index;

        if(!entity_get_index(driver, &driver_index) || !entity_index_is_alive(driver_index)) {
            system_remove_transform_lock_by_index(driven);
            continue;
        }

        Vec2D world_offset = math_rotate_vector(
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

void system_clean_entities_past_lifetime(void) {
    for(int i = 0; i < MAX_ENTITIES; i += 1) {
        if(!entity_index_is_alive(i)) {
            continue;
        }
        if( entity_index_has_components(i, LIFETIME) ) {

            if( (life_times[i].expirey_time != 0 && life_times[i].expirey_time <= engine_get_time()) ) {
                system_delete_by_index(i);
            }
            else if( (life_times[i].expirey_tick != 0 && life_times[i].expirey_tick <= engine_get_tick()) ) {
                system_delete_by_index(i);
            }
        }
    }
}

static Velocity system_point_velocity(Entity entity, Vec2D world_offset) {
    EntityIndex index;

    if(!entity_get_index(entity, &index) || !entity_index_is_alive(index)) {
        return (Velocity){0};
    }
    if(!physics_entity_can_move(index)) {
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
static void system_add_joint_force_for_one_tick(Entity target, Force force) {
    EntityIndex target_index;

    if(!entity_get_index(target, &target_index) || !entity_index_is_alive(target_index)) {
        return;
    }
    if(!physics_entity_can_move(target_index)) {
        return;
    }
    if(!entity_index_has_components(target_index, MASS) || mass[target_index] == 0.0f) {
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
    //    engine_get_tick() + 1
    //);
}
static void system_add_joint_torque_for_one_tick(Entity target, Torque torque) {
    EntityIndex target_index;

    if(!entity_get_index(target, &target_index) || !entity_index_is_alive(target_index)) {
        return;
    }
    if(!physics_entity_can_move(target_index)) {
        return;
    }
    if(!entity_index_has_components(target_index, MASS) || mass[target_index] == 0.0f) {
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
    //    engine_get_tick() + 1
    //);
}

static void system_add_joint_force_at_point_for_one_tick(Entity target, Position world_point, Force force) {
    EntityIndex target_index;

    if(!entity_get_index(target, &target_index) || !entity_index_is_alive(target_index)) {
        return;
    }

    if(!physics_entity_can_move(target_index)) {
        return;
    }

    EntityResult force_result = physics_set_force(target, force);

    if(force_result.kind == ERROR_RESULT_VALUE) {
        entity_set_life_time(force_result.result.value, 0.0, engine_get_tick() + 1);
    }

    Vec2D r = {
        .x = world_point.x - positions[target_index].x,
        .y = world_point.y - positions[target_index].y
    };

    Torque torque = math_cross_2d(r, force);

    EntityResult torque_result = physics_set_torque(target, torque);

    if(torque_result.kind == ERROR_RESULT_VALUE) {
        entity_set_life_time(torque_result.result.value, 0.0, engine_get_tick() + 1);
    }
}

static void system_apply_pin_joint(Entity joint_entity) {
    Joint joint = joints[joint_entity];

    Entity a = joint.a;
    Entity b = joint.b;
    EntityIndex a_index;
    EntityIndex b_index;

    if(!entity_get_index(a, &a_index) || !entity_index_is_alive(a_index) || !entity_get_index(b, &b_index) || !entity_index_is_alive(b_index)) {
        system_delete_by_index(joint_entity);
        return;
    }

    Vec2D offset_a = math_rotate_vector(
        joint.local_anchor_a,
        orientations[a_index]
    );

    Vec2D offset_b = math_rotate_vector(
        joint.local_anchor_b,
        orientations[b_index]
    );

    Position world_anchor_a = {
        .x = positions[a_index].x + offset_a.x,
        .y = positions[a_index].y + offset_a.y
    };

    Position world_anchor_b = {
        .x = positions[b_index].x + offset_b.x,
        .y = positions[b_index].y + offset_b.y
    };

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

    Force force_on_a = {
        .x = joint.stiffness * error.x + joint.damping * relative_velocity.x,
        .y = joint.stiffness * error.y + joint.damping * relative_velocity.y
    };

    Force force_on_b = {
        .x = -force_on_a.x,
        .y = -force_on_a.y
    };

    system_add_joint_force_at_point_for_one_tick(
        a,
        world_anchor_a,
        force_on_a
    );

    system_add_joint_force_at_point_for_one_tick(
        b,
        world_anchor_b,
        force_on_b
    );
}

void system_apply_distance_joint(Entity joint_entity) {
        Joint joint = joints[joint_entity];
        if(joint.type != JOINT_DISTANCE) {
            return;
        }

        Entity a = joint.a;
        Entity b = joint.b;
        EntityIndex a_index;
        EntityIndex b_index;

        if(!entity_get_index(a, &a_index) || !entity_index_is_alive(a_index) || !entity_get_index(b, &b_index) || !entity_index_is_alive(b_index)) {
            system_delete_by_index(joint_entity);
            return;
        }

        Vec2D offset_a = math_rotate_vector(
            joint.local_anchor_a,
            orientations[a_index]
        );

        Vec2D offset_b = math_rotate_vector(
            joint.local_anchor_b,
            orientations[b_index]
        );

        Position world_anchor_a = {
            .x = positions[a_index].x + offset_a.x,
            .y = positions[a_index].y + offset_a.y
        };

        Position world_anchor_b = {
            .x = positions[b_index].x + offset_b.x,
            .y = positions[b_index].y + offset_b.y
        };

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

        system_add_joint_force_for_one_tick(a, force_on_a);
        system_add_joint_force_for_one_tick(b, force_on_b);

        system_add_joint_torque_for_one_tick(a, torque_on_a);
        system_add_joint_torque_for_one_tick(b, torque_on_b);

}
void system_apply_joints(void)
{
    for(Entity joint_entity = 0; joint_entity < MAX_ENTITIES; joint_entity += 1) {
        if(!entity_index_is_alive(joint_entity)) {
            continue;
        }
        if(!entity_index_has_components(joint_entity, JOINT)) {
            continue;
        }
        Joint joint = joints[joint_entity];
        switch(joint.type) {
            case JOINT_PIN:
                system_apply_pin_joint(joint_entity);
                break;
            case JOINT_DISTANCE:
                system_apply_distance_joint(joint_entity);
                break;
            case JOINT_WELD:
                //Not implemented
                break;
            default:
                //Not implemented
                break;
        }

    }
}

void system_update_aabbs(void) {
    for(uint32_t alive_position = 0; alive_position < entity_alive_count(); alive_position += 1) {
        EntityIndex i;

        if(!system_alive_index_at(alive_position, &i)) {
            continue;
        }
        if( entity_index_has_components(i, HIT_BOX)) {
            grid_update_aabb(i);
        }
    }
}

void system_update_physics(double dt) {
    system_clear_force_torque_accelerations();
    system_apply_joints();

    system_apply_forces();
    system_apply_torques();
    system_update_velocities(dt);
    system_update_angular_velocities(dt);
    system_update_orientations(dt);
    system_update_positions(dt);
    system_apply_axis_locks();
    system_apply_angle_locks();
    system_apply_transform_locks();

    system_generate_global_hitboxes();
    system_update_aabbs();
    system_add_entities_to_grid();
    system_apply_collisions_tuned();
    clear_grid();
}

void print_entity_movement(Entity entity) {
    console_write(LOG_ENGINE, "---Movement Log---\n");
    console_write(LOG_ENGINE, "Entity: %d\n", entity);
    console_write(LOG_ENGINE, "Position: {x: %f, y: %f}\n", positions[entity].x, positions[entity].y);
    console_write(LOG_ENGINE, "Velocity: {x: %f, y: %f}\n", velocities[entity].x, velocities[entity].y);
    console_write(LOG_ENGINE, "Acceleration: {x: %f, y: %f}\n", accelerations[entity].x, accelerations[entity].y);
    console_write(LOG_ENGINE, "---Movement Log---\n");
}
