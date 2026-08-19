#include "rohr.h"

#include <math.h>

int main(void) {
    EntityResult transient;
    Tick ticks;
    AnimatedSprite preview;
    Sprite static_sprite;

    if(rohr_error_check(rohr_engine_init()) ||
            rohr_error_check(rohr_engine_time_per_tick_set(0.01))) {
        return 1;
    }
    rohr_engine_clock_reset();
    preview = rohr_graphics_animated_sprite_create((AnimationAsset){
        .texture_list.amount = 3, .ticks_per_frame = 2}, (Scale){1.0f, 1.0f});
    rohr_graphics_animated_sprite_update(&preview, 1, 0.0);
    if(preview.animation_frame != 0) {
        rohr_engine_shutdown();
        return 1;
    }
    rohr_graphics_animated_sprite_update(&preview, 2, 0.0);
    if(preview.animation_frame != 1) {
        rohr_engine_shutdown();
        return 1;
    }
    transient = rohr_entity_add();
    static_sprite = rohr_graphics_sprite_create((TextureAsset){0},
        (Scale){2.0f, 3.0f});
    if(rohr_error_check(transient) || static_sprite.scale.x != 2.0f ||
            !static_sprite.follow_entity_rotation ||
            rohr_error_check(rohr_graphics_sprite_add(
                transient.result.value, static_sprite)) ||
            !rohr_entity_components_check(transient.result.value, ROHR_SPRITE)) {
        rohr_engine_shutdown();
        return 1;
    }
    (void)rohr_entity_delete(transient.result.value);
    if(rohr_system_tick_update() != 0) {
        rohr_engine_shutdown();
        return 1;
    }
    transient = rohr_entity_add();
    if(rohr_error_check(transient) ||
            rohr_error_check(rohr_entity_life_time_set(
                transient.result.value, 0.0, rohr_engine_tick_get() + 1))) {
        rohr_engine_shutdown();
        return 1;
    }
    SDL_Delay(30);
    ticks = rohr_system_tick_update();
    if(ticks < 2 || rohr_physics_dt_per_tick_get() != 0.01 ||
            rohr_entity_alive_check(transient.result.value)) {
        rohr_engine_shutdown();
        return 1;
    }
    if(rohr_error_check(rohr_physics_dt_per_tick_set(0.5))) {
        rohr_engine_shutdown();
        return 1;
    }
    rohr_engine_clock_reset();
    SDL_Delay(25);
    ticks = rohr_system_tick_update();
    if(ticks < 2 || rohr_physics_dt_per_tick_get() != 0.5) {
        rohr_engine_shutdown();
        return 1;
    }
    rohr_physics_engine_time_per_tick_use();
    if(rohr_physics_dt_per_tick_get() != rohr_engine_time_per_tick_get()) {
        rohr_engine_shutdown();
        return 1;
    }
    rohr_engine_shutdown();
    return 0;
}
