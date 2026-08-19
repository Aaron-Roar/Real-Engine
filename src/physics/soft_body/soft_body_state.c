#include "physics.h"

#include "physics/physics_internal.h"

static void physics_soft_body_entity_list_remove(
        Entity *values, uint32_t *count, Entity entity) {
    if(values == NULL || count == NULL) return;
    for(uint32_t i = 0; i < *count; i += 1) {
        if(values[i] != entity) continue;
        values[i] = values[*count - 1];
        values[*count - 1] = ENTITY_INVALID;
        *count -= 1;
        return;
    }
}

void physics_soft_body_entity_clear(Entity entity, EntityIndex index) {
    if(index < soft_bodies_pool.capacity && soft_bodies_pool.used[index]) {
        SoftBody body = soft_bodies[index];

        for(uint32_t i = 0; i < body.triangle_count; i += 1)
            if(entity_alive_check(body.triangles[i]))
                (void)entity_delete(body.triangles[i]);
        for(uint32_t i = 0; i < body.beam_count; i += 1)
            if(entity_alive_check(body.beams[i]))
                (void)entity_delete(body.beams[i]);
        for(uint32_t i = 0; i < body.node_count; i += 1)
            if(entity_alive_check(body.nodes[i]))
                (void)entity_delete(body.nodes[i]);
        (void)SoftBodyPool_release_at(&soft_bodies_pool, index);
    }
    if(index < soft_body_nodes_pool.capacity &&
            soft_body_nodes_pool.used[index]) {
        EntityIndex body_index;
        Entity owner = soft_body_nodes[index].soft_body;

        for(EntityIndex connected = 0;
                connected < soft_body_beams_pool.capacity;
                connected += 1) {
            EntityResult connected_entity;

            if(!soft_body_beams_pool.used[connected] ||
                    (soft_body_beams[connected].node_a != entity &&
                        soft_body_beams[connected].node_b != entity)) continue;
            connected_entity = entity_from_index_get(connected);
            if(connected_entity.kind == ERROR_RESULT_VALUE)
                (void)entity_delete(connected_entity.result.value);
        }
        for(EntityIndex connected = 0;
                connected < soft_body_triangles_pool.capacity;
                connected += 1) {
            SoftBodyTriangle triangle;
            EntityResult connected_entity;

            if(!soft_body_triangles_pool.used[connected]) continue;
            triangle = soft_body_triangles[connected];
            if(triangle.node_a != entity &&
                    triangle.node_b != entity &&
                    triangle.node_c != entity) continue;
            connected_entity = entity_from_index_get(connected);
            if(connected_entity.kind == ERROR_RESULT_VALUE)
                (void)entity_delete(connected_entity.result.value);
        }
        if(entity_index_get(owner, &body_index) &&
                body_index < soft_bodies_pool.capacity &&
                soft_bodies_pool.used[body_index])
            physics_soft_body_entity_list_remove(
                soft_bodies[body_index].nodes,
                &soft_bodies[body_index].node_count,
                entity);
        (void)SoftBodyNodePool_release_at(&soft_body_nodes_pool, index);
    }
    if(index < soft_body_beams_pool.capacity &&
            soft_body_beams_pool.used[index]) {
        EntityIndex body_index;
        Entity owner = soft_body_beams[index].soft_body;

        if(entity_index_get(owner, &body_index) &&
                body_index < soft_bodies_pool.capacity &&
                soft_bodies_pool.used[body_index])
            physics_soft_body_entity_list_remove(
                soft_bodies[body_index].beams,
                &soft_bodies[body_index].beam_count,
                entity);
        (void)SoftBodyBeamPool_release_at(&soft_body_beams_pool, index);
    }
    if(index < soft_body_triangles_pool.capacity &&
            soft_body_triangles_pool.used[index]) {
        EntityIndex body_index;
        Entity owner = soft_body_triangles[index].soft_body;

        if(entity_index_get(owner, &body_index) &&
                body_index < soft_bodies_pool.capacity &&
                soft_bodies_pool.used[body_index])
            physics_soft_body_entity_list_remove(
                soft_bodies[body_index].triangles,
                &soft_bodies[body_index].triangle_count,
                entity);
        (void)SoftBodyTrianglePool_release_at(
            &soft_body_triangles_pool, index);
    }
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
    entity_mask[index] |= ROHR_SOFT_BODY;
    return result;
}

SoftBodyResult physics_soft_body_get(Entity soft_body) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(soft_body, &index);

    if(result.kind == ERROR_RESULT_ERROR) return ERROR_RESULT_MAKE_ERROR(SoftBodyResult, result.result.error);
    if(!entity_index_components_check(index, ROHR_SOFT_BODY) || !soft_bodies_pool.used[index]) {
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

    if(result.kind == ERROR_RESULT_ERROR || !entity_index_components_check(body_index, ROHR_SOFT_BODY) ||
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
            physics_hitbox_set(node.result.value,
                math_circle_create(radius, 8)).kind == ERROR_RESULT_ERROR ||
            physics_restitution_set(node.result.value, 0.25f).kind == ERROR_RESULT_ERROR ||
            physics_friction_set(node.result.value, 0.0f).kind == ERROR_RESULT_ERROR ||
            physics_collision_filter_set(node.result.value, (CollisionFilterConfig){
                .category = ROHR_COLLISION_CATEGORY_SOFT_BODY_NODE,
                .collides_with = ROHR_COLLISION_CATEGORY_ALL &
                    ~ROHR_COLLISION_CATEGORY_SOFT_BODY_NODE
            }).kind == ERROR_RESULT_ERROR ||
            SoftBodyNodePool_store_at(&soft_body_nodes_pool, node_index, (SoftBodyNode){
                .soft_body = soft_body,
                .radius = radius,
                .category = ROHR_COLLISION_CATEGORY_SOFT_BODY_NODE,
                .collides_with = ROHR_COLLISION_CATEGORY_ALL &
                    ~ROHR_COLLISION_CATEGORY_SOFT_BODY_NODE
            }).kind == ERROR_RESULT_ERROR) {
        (void)entity_delete(node.result.value);
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[node_index] |= ROHR_SOFT_BODY_NODE | ROHR_PARTICLE;
    soft_bodies[body_index].nodes[soft_bodies[body_index].node_count++] = node.result.value;
    return node;
}

SoftBodyNodeResult physics_soft_body_node_get(Entity node) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(node, &index);

    if(result.kind == ERROR_RESULT_ERROR) return ERROR_RESULT_MAKE_ERROR(SoftBodyNodeResult, result.result.error);
    if(!entity_index_components_check(index, ROHR_SOFT_BODY_NODE) || !soft_body_nodes_pool.used[index]) {
        return ERROR_RESULT_MAKE_ERROR(SoftBodyNodeResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(SoftBodyNodeResult, soft_body_nodes[index]);
}

EngineResult physics_soft_body_node_collision_filter_set(Entity node,
        RohrCollisionCategoryMask category, RohrCollisionCategoryMask collides_with) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(node, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!entity_index_components_check(index, ROHR_SOFT_BODY_NODE) || !soft_body_nodes_pool.used[index]) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    result = physics_collision_filter_set(node, (CollisionFilterConfig){
        .category = category,
        .collides_with = collides_with
    });
    if(result.kind == ERROR_RESULT_ERROR) return result;
    soft_body_nodes[index].category = category;
    soft_body_nodes[index].collides_with = collides_with;
    return result;
}

EngineResult physics_soft_body_node_force_for_one_tick_apply(Entity node, Force force) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(node, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!entity_index_components_check(index, ROHR_SOFT_BODY_NODE)) {
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    }
    return physics_force_for_one_tick_apply(node, force);
}

EngineResult physics_soft_body_node_impulse_apply(Entity node, Vec2D impulse) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(node, &index);

    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!entity_index_components_check(index, ROHR_SOFT_BODY_NODE)) {
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
            !entity_index_components_check(node_index, ROHR_SOFT_BODY_NODE) ||
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
            !entity_index_components_check(body_index, ROHR_SOFT_BODY) ||
            !entity_index_components_check(a_index, ROHR_SOFT_BODY_NODE) ||
            !entity_index_components_check(b_index, ROHR_SOFT_BODY_NODE) ||
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
    entity_mask[beam_index] |= ROHR_SOFT_BODY_BEAM;
    soft_bodies[body_index].beams[soft_bodies[body_index].beam_count++] = beam.result.value;
    return beam;
}

SoftBodyBeamResult physics_soft_body_beam_get(Entity beam) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(beam, &index);
    if(result.kind == ERROR_RESULT_ERROR) return ERROR_RESULT_MAKE_ERROR(SoftBodyBeamResult, result.result.error);
    if(!entity_index_components_check(index, ROHR_SOFT_BODY_BEAM) || !soft_body_beams_pool.used[index]) {
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
            !entity_index_components_check(body_index, ROHR_SOFT_BODY) ||
            node_a == node_b || node_b == node_c || node_a == node_c) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_STATE_INVALID);
    }
    for(uint32_t i = 0; i < 3; i += 1) {
        if(physics_live_index_get(nodes_to_check[i], &indices[i]).kind == ERROR_RESULT_ERROR ||
                !entity_index_components_check(indices[i], ROHR_SOFT_BODY_NODE) ||
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
    entity_mask[triangle_index] |= ROHR_SOFT_BODY_TRIANGLE;
    soft_bodies[body_index].triangles[soft_bodies[body_index].triangle_count++] = triangle.result.value;
    return triangle;
}

SoftBodyTriangleResult physics_soft_body_triangle_get(Entity triangle) {
    EntityIndex index;
    EngineResult result = physics_live_index_get(triangle, &index);
    if(result.kind == ERROR_RESULT_ERROR) return ERROR_RESULT_MAKE_ERROR(SoftBodyTriangleResult, result.result.error);
    if(!entity_index_components_check(index, ROHR_SOFT_BODY_TRIANGLE) || !soft_body_triangles_pool.used[index]) {
        return ERROR_RESULT_MAKE_ERROR(SoftBodyTriangleResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(SoftBodyTriangleResult, soft_body_triangles[index]);
}


