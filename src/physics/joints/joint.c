/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics.h"

#include "physics/physics_internal.h"

#include <string.h>

static JointAnchor joint_anchors[MAX_JOINT_ANCHORS];
static uint32_t joint_anchor_generations[MAX_JOINT_ANCHORS];
static bool joint_anchor_used[MAX_JOINT_ANCHORS];

static JointAnchorId physics_joint_anchor_id_make(uint32_t slot) {
    return ((uint64_t)joint_anchor_generations[slot] << 32) |
        ((uint64_t)slot + 1);
}

static bool physics_joint_anchor_slot_get(
        JointAnchorId anchor, uint32_t *slot) {
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

void physics_joint_state_init(void) {
    memset(joint_anchors, 0, sizeof(joint_anchors));
    memset(joint_anchor_used, 0, sizeof(joint_anchor_used));
    for(uint32_t i = 0; i < MAX_JOINT_ANCHORS; i += 1)
        joint_anchor_generations[i] = 1;
}

void physics_joint_entity_clear(Entity entity, EntityIndex index) {
    for(uint32_t slot = 0; slot < MAX_JOINT_ANCHORS; slot += 1) {
        if(joint_anchor_used[slot] && joint_anchors[slot].entity == entity)
            (void)physics_joint_anchor_remove(
                physics_joint_anchor_id_make(slot));
    }
    for(EntityIndex joint_index = 0;
            joint_index < joints_pool.capacity;
            joint_index += 1) {
        EntityResult joint_entity;

        if(!joints_pool.used[joint_index] ||
                (joints[joint_index].a != entity &&
                    joints[joint_index].b != entity)) continue;
        joint_entity = entity_from_index_get(joint_index);
        if(joint_entity.kind == ERROR_RESULT_VALUE &&
                joint_entity.result.value != entity)
            (void)entity_delete(joint_entity.result.value);
    }
    if(index < joints_pool.capacity && joints_pool.used[index])
        (void)JointPool_release_at(&joints_pool, index);
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
    entity_mask[index] |= ROHR_JOINT;
    return error_result_value(true);
}

JointAnchorIdResult physics_joint_anchor_create(Entity entity, Vec2D local_offset) {
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
            .local_offset = local_offset
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

JointAnchorPositionResult physics_joint_anchor_local_position_get(JointAnchorId anchor) {
    uint32_t slot;

    if(!physics_joint_anchor_slot_get(anchor, &slot)) {
        return ERROR_RESULT_MAKE_ERROR(JointAnchorPositionResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    return ERROR_RESULT_MAKE_VALUE(JointAnchorPositionResult, joint_anchors[slot].local_offset);
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
    local_position = joint_anchors[slot].local_offset;
    rotated = math_vector_rotate(local_position, orientations[entity_index]);
    return ERROR_RESULT_MAKE_VALUE(JointAnchorPositionResult, ((Position){
        .x = positions[entity_index].x + rotated.x,
        .y = positions[entity_index].y + rotated.y
    }));
}

EngineResult physics_joint_anchor_local_position_set(JointAnchorId anchor, Vec2D local_offset) {
    uint32_t slot;

    if(!physics_joint_anchor_slot_get(anchor, &slot)) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    joint_anchors[slot].local_offset = local_offset;
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

