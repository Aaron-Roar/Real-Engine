/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "rohr.h"

#include <stdio.h>

static Shape triangle(float x) {
    return (Shape){.amount_of_vertices = 3,
        .vertices = {{x, 0.0f}, {x + 1.0f, 0.0f}, {x, 1.0f}}};
}

int main(void) {
    EntityResult added;
    Entity entity;
    AnimatedSprite sprite = {0};
    HitboxIdResult first_id, second_id;
    HitboxIndexResult active;

    if(rohr_error_check(rohr_engine_init())) return 1;
    added = rohr_entity_add();
    if(rohr_error_check(added)) goto fail;
    entity = added.result.value;
    if(rohr_error_check(rohr_physics_hitbox_add(entity, triangle(0.0f))) ||
            rohr_error_check(rohr_physics_hitbox_add(entity, triangle(2.0f))))
        goto fail;
    first_id = rohr_physics_hitbox_id_at_get(entity, 0);
    second_id = rohr_physics_hitbox_id_at_get(entity, 1);
    if(rohr_error_check(first_id) || rohr_error_check(second_id)) goto fail;

    sprite.animation.id = 41;
    sprite.animation.texture_list.amount = 3;
    sprite.animation.texture_list.frame_ids[0] = 101;
    sprite.animation.texture_list.frame_ids[1] = 102;
    sprite.animation.texture_list.frame_ids[2] = 103;
    sprite.animation_frame = 0;
    if(rohr_error_check(rohr_graphics_animated_sprite_add(entity, sprite)) ||
            rohr_error_check(rohr_physics_hitbox_animation_binding_set(
                entity, 41, 101, second_id.result.value)) ||
            rohr_error_check(rohr_physics_hitbox_animation_binding_set(
                entity, 41, 102, second_id.result.value)) ||
            rohr_error_check(rohr_physics_hitbox_animation_binding_set(
                entity, 41, 103, first_id.result.value))) goto fail;
    {
        HitboxIdResult by_id = rohr_physics_hitbox_animation_binding_get(
            entity, 41, 102);
        HitboxIndexResult by_index =
            rohr_physics_hitbox_animation_binding_at_get(entity, 1);
        if(rohr_error_check(by_id) ||
                by_id.result.value != second_id.result.value ||
                rohr_error_check(by_index) || by_index.result.value != 1)
            goto fail;
    }

    rohr_physics_hitbox_animation_bindings_update();
    active = rohr_physics_hitbox_active_index_get(entity);
    if(rohr_error_check(active) || active.result.value != 1) goto fail;
    if(rohr_error_check(rohr_graphics_animated_sprite_frame_index_set(entity, 1)))
        goto fail;
    rohr_physics_hitbox_animation_bindings_update();
    active = rohr_physics_hitbox_active_index_get(entity);
    if(rohr_error_check(active) || active.result.value != 1) goto fail;
    if(rohr_error_check(rohr_graphics_animated_sprite_frame_index_set(entity, 2)))
        goto fail;
    rohr_physics_hitbox_animation_bindings_update();
    active = rohr_physics_hitbox_active_index_get(entity);
    if(rohr_error_check(active) || active.result.value != 0) goto fail;

    if(rohr_error_check(rohr_physics_hitbox_by_id_remove(
            entity, second_id.result.value))) goto fail;
    if(rohr_error_check(rohr_physics_hitbox_remove(entity)) ||
            rohr_entity_components_check(entity,
                ROHR_HITBOX_ANIMATION_BINDING)) goto fail;
    rohr_engine_shutdown();
    return 0;
fail:
    fprintf(stderr, "hitbox animation binding test failed\n");
    rohr_engine_shutdown();
    return 1;
}
