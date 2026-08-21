/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics.h"

#include "graphics.h"
#include "physics/physics_internal.h"

#include <stdlib.h>

static HitboxAnimationBindingList *binding_list_get(EntityIndex index) {
    if(index >= hitbox_animation_bindings_pool.capacity ||
            !hitbox_animation_bindings_pool.used[index]) return NULL;
    return &hitbox_animation_bindings_pool.objects[index];
}

static bool hitbox_id_check(EntityIndex index, HitboxId id) {
    if(index >= hitbox_variants_pool.capacity ||
            !hitbox_variants_pool.used[index]) return false;
    HitboxVariantList *variants = &hitbox_variants_pool.objects[index];
    for(size_t i = 0; i < variants->count; i += 1)
        if(variants->values[i].id == id) return true;
    return false;
}

static bool animation_frame_id_check(EntityIndex index, AnimationId animation_id,
        AnimationFrameId frame_id) {
    if(index >= animated_sprites_pool.capacity ||
            !animated_sprites_pool.used[index]) return false;
    AnimatedSprite *sprite = &animated_sprites[index];
    if(sprite->animation.id != animation_id) return false;
    for(int i = 0; i < sprite->animation.texture_list.amount; i += 1)
        if(sprite->animation.texture_list.frame_ids[i] == frame_id) return true;
    return false;
}

EngineResult physics_hitbox_animation_binding_set(Entity entity,
        AnimationId animation_id, AnimationFrameId frame_id,
        HitboxId hitbox_id) {
    EntityIndex index;
    HitboxAnimationBindingList *list;
    HitboxAnimationBinding *values;
    EngineResult result = physics_live_index_get(entity, &index);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(!animation_frame_id_check(index, animation_id, frame_id) ||
            !hitbox_id_check(index, hitbox_id))
        return error_result_error(ERROR_ENGINE_INDEX_OUT_OF_RANGE);
    list = binding_list_get(index);
    if(list == NULL) {
        HitboxAnimationBindingList empty = {0};
        if(HitboxAnimationBindingListPool_store_at(
                &hitbox_animation_bindings_pool, index, empty).kind ==
                ERROR_RESULT_ERROR)
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        list = &hitbox_animation_bindings_pool.objects[index];
    }
    for(size_t i = 0; i < list->count; i += 1) {
        if(list->values[i].animation_id != animation_id ||
                list->values[i].frame_id != frame_id) continue;
        list->values[i].hitbox_id = hitbox_id;
        entity_mask[index] |= ROHR_HITBOX_ANIMATION_BINDING;
        return error_result_value(true);
    }
    if(list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        values = realloc(list->values, capacity * sizeof(*values));
        if(values == NULL)
            return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
        list->values = values;
        list->capacity = capacity;
    }
    list->values[list->count++] = (HitboxAnimationBinding){
        animation_id, frame_id, hitbox_id};
    entity_mask[index] |= ROHR_HITBOX_ANIMATION_BINDING;
    return error_result_value(true);
}

EngineResult physics_hitbox_animation_binding_remove(Entity entity,
        AnimationId animation_id, AnimationFrameId frame_id) {
    EntityIndex index;
    HitboxAnimationBindingList *list;
    EngineResult result = physics_live_index_get(entity, &index);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    list = binding_list_get(index);
    if(list == NULL) return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    for(size_t i = 0; i < list->count; i += 1) {
        if(list->values[i].animation_id != animation_id ||
                list->values[i].frame_id != frame_id) continue;
        for(size_t j = i + 1; j < list->count; j += 1)
            list->values[j - 1] = list->values[j];
        list->count -= 1;
        if(list->count == 0) physics_hitbox_animation_bindings_entity_clear(index);
        return error_result_value(true);
    }
    return error_result_error(ERROR_ENGINE_INDEX_OUT_OF_RANGE);
}

HitboxIdResult physics_hitbox_animation_binding_get(Entity entity,
        AnimationId animation_id, AnimationFrameId frame_id) {
    EntityIndex index;
    HitboxAnimationBindingList *list;
    EngineResult result = physics_live_index_get(entity, &index);
    if(result.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(HitboxIdResult, result.result.error);
    list = binding_list_get(index);
    if(list == NULL)
        return ERROR_RESULT_MAKE_ERROR(HitboxIdResult,
            ERROR_ENGINE_COMPONENT_MISSING);
    for(size_t i = 0; i < list->count; i += 1)
        if(list->values[i].animation_id == animation_id &&
                list->values[i].frame_id == frame_id)
            return ERROR_RESULT_MAKE_VALUE(HitboxIdResult,
                list->values[i].hitbox_id);
    return ERROR_RESULT_MAKE_ERROR(HitboxIdResult,
        ERROR_ENGINE_INDEX_OUT_OF_RANGE);
}

EngineResult physics_hitbox_animation_binding_at_set(Entity entity,
        size_t frame_index, size_t hitbox_index) {
    EntityIndex index;
    AnimatedSprite *sprite;
    HitboxVariantList *variants;
    EngineResult result = physics_live_index_get(entity, &index);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(index >= animated_sprites_pool.capacity ||
            !animated_sprites_pool.used[index])
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    sprite = &animated_sprites[index];
    variants = index < hitbox_variants_pool.capacity &&
        hitbox_variants_pool.used[index] ? &hitbox_variants_pool.objects[index] : NULL;
    if(frame_index >= (size_t)sprite->animation.texture_list.amount ||
            variants == NULL || hitbox_index >= variants->count)
        return error_result_error(ERROR_ENGINE_INDEX_OUT_OF_RANGE);
    return physics_hitbox_animation_binding_set(entity, sprite->animation.id,
        sprite->animation.texture_list.frame_ids[frame_index],
        variants->values[hitbox_index].id);
}

EngineResult physics_hitbox_animation_binding_at_remove(Entity entity,
        size_t frame_index) {
    EntityIndex index;
    AnimatedSprite *sprite;
    EngineResult result = physics_live_index_get(entity, &index);
    if(result.kind == ERROR_RESULT_ERROR) return result;
    if(index >= animated_sprites_pool.capacity ||
            !animated_sprites_pool.used[index])
        return error_result_error(ERROR_ENGINE_COMPONENT_MISSING);
    sprite = &animated_sprites[index];
    if(frame_index >= (size_t)sprite->animation.texture_list.amount)
        return error_result_error(ERROR_ENGINE_INDEX_OUT_OF_RANGE);
    return physics_hitbox_animation_binding_remove(entity, sprite->animation.id,
        sprite->animation.texture_list.frame_ids[frame_index]);
}

HitboxIndexResult physics_hitbox_animation_binding_at_get(Entity entity,
        size_t frame_index) {
    EntityIndex index;
    AnimatedSprite *sprite;
    HitboxVariantList *variants;
    HitboxIdResult id;
    EngineResult result = physics_live_index_get(entity, &index);
    if(result.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(HitboxIndexResult, result.result.error);
    if(index >= animated_sprites_pool.capacity ||
            !animated_sprites_pool.used[index])
        return ERROR_RESULT_MAKE_ERROR(HitboxIndexResult,
            ERROR_ENGINE_COMPONENT_MISSING);
    sprite = &animated_sprites[index];
    if(frame_index >= (size_t)sprite->animation.texture_list.amount)
        return ERROR_RESULT_MAKE_ERROR(HitboxIndexResult,
            ERROR_ENGINE_INDEX_OUT_OF_RANGE);
    id = physics_hitbox_animation_binding_get(entity, sprite->animation.id,
        sprite->animation.texture_list.frame_ids[frame_index]);
    if(id.kind == ERROR_RESULT_ERROR)
        return ERROR_RESULT_MAKE_ERROR(HitboxIndexResult, id.result.error);
    variants = index < hitbox_variants_pool.capacity &&
        hitbox_variants_pool.used[index] ? &hitbox_variants_pool.objects[index] : NULL;
    if(variants != NULL) for(size_t i = 0; i < variants->count; i += 1)
        if(variants->values[i].id == id.result.value)
            return ERROR_RESULT_MAKE_VALUE(HitboxIndexResult, i);
    return ERROR_RESULT_MAKE_ERROR(HitboxIndexResult,
        ERROR_ENGINE_INDEX_OUT_OF_RANGE);
}

void physics_hitbox_animation_binding_hitbox_remove(EntityIndex index,
        HitboxId hitbox_id) {
    HitboxAnimationBindingList *list = binding_list_get(index);
    if(list == NULL) return;
    for(size_t i = 0; i < list->count;) {
        if(list->values[i].hitbox_id != hitbox_id) { i += 1; continue; }
        for(size_t j = i + 1; j < list->count; j += 1)
            list->values[j - 1] = list->values[j];
        list->count -= 1;
    }
    if(list->count == 0) physics_hitbox_animation_bindings_entity_clear(index);
}

void physics_hitbox_animation_bindings_entity_clear(EntityIndex index) {
    HitboxAnimationBindingList *list = binding_list_get(index);
    if(list != NULL) {
        free(list->values);
        (void)HitboxAnimationBindingListPool_release_at(
            &hitbox_animation_bindings_pool, index);
    }
    if(index < MAX_ENTITIES)
        entity_mask[index] &= ~ROHR_HITBOX_ANIMATION_BINDING;
}

void physics_hitbox_animation_bindings_update(void) {
    for(EntityIndex index = 0; index < MAX_ENTITIES; index += 1) {
        HitboxAnimationBindingList *list;
        AnimatedSprite *sprite;
        AnimationFrameId frame_id;
        EntityResult entity;
        if(!entity_index_alive_check(index) ||
                !entity_index_components_check(index,
                    ROHR_HITBOX_ANIMATION_BINDING | ROHR_ANIMATED_SPRITE))
            continue;
        list = binding_list_get(index);
        if(list == NULL || index >= animated_sprites_pool.capacity ||
                !animated_sprites_pool.used[index]) continue;
        sprite = &animated_sprites[index];
        if(sprite->animation_frame < 0 || sprite->animation_frame >=
                sprite->animation.texture_list.amount) continue;
        frame_id = sprite->animation.texture_list.frame_ids[
            sprite->animation_frame];
        entity = entity_from_index_get(index);
        if(entity.kind == ERROR_RESULT_ERROR) continue;
        for(size_t i = 0; i < list->count; i += 1) {
            if(list->values[i].animation_id != sprite->animation.id ||
                    list->values[i].frame_id != frame_id) continue;
            (void)physics_hitbox_active_id_set(entity.result.value,
                list->values[i].hitbox_id);
            break;
        }
    }
}
