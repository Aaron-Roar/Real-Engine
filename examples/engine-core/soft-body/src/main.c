#include "rohr.h"
#include "example_runtime.h"

#include <math.h>
#include <stdio.h>

#define OUTER_NODE_COUNT 10
#define NODE_COUNT (OUTER_NODE_COUNT + 1)

static const Color background_color = {18, 22, 30, 255};
static const Color wall_color = {90, 100, 115, 255};
static const Color surface_color = {55, 125, 175, 150};
static const Color beam_color = {235, 240, 245, 255};
static const Color node_color = {255, 170, 70, 255};
static const RohrCollisionCategoryMask room_category = UINT64_C(1) << 1;
static const RohrCollisionCategoryMask soft_node_category = UINT64_C(1) << 2;

static bool result_ok(EngineResult result) {
    if(!rohr_error_check(result)) return true;
    rohr_error_print_stderr(result.result.error);
    return false;
}

static Entity wall_create(Position position, Vec2D dimensions) {
    EntityResult result = rohr_entity_add();
    Entity wall;
    if(rohr_error_check(result)) return ENTITY_INVALID;
    wall = result.result.value;
    if(!result_ok(rohr_physics_position_set(wall, position)) ||
            !result_ok(rohr_physics_hitbox_set(wall, rohr_math_create_square(dimensions.x, dimensions.y))) ||
            !result_ok(rohr_physics_static_set(wall)) ||
            !result_ok(rohr_physics_collision_category_set(wall, room_category)) ||
            !result_ok(rohr_physics_collision_with_all_set(wall))) return ENTITY_INVALID;
    return wall;
}

int main(void) {
    Entity walls[4];
    Entity soft_body;
    Entity nodes[NODE_COUNT];
    KeyboardState keyboard = {0};

    if(!example_use_executable_directory() || !result_ok(rohr_engine_init())) return 1;
    if(!result_ok(rohr_engine_time_per_tick_set(1.0 / 240.0)) ||
            !result_ok(rohr_graphics_start())) goto fail;
    walls[0] = wall_create((Position){0.0f, -225.0f}, (Vec2D){620.0f, 20.0f});
    walls[1] = wall_create((Position){0.0f, 225.0f}, (Vec2D){620.0f, 20.0f});
    walls[2] = wall_create((Position){-305.0f, 0.0f}, (Vec2D){20.0f, 470.0f});
    walls[3] = wall_create((Position){305.0f, 0.0f}, (Vec2D){20.0f, 470.0f});
    for(uint32_t i = 0; i < 4; i += 1) if(walls[i] == ENTITY_INVALID) goto fail;

    {
        EntityResult result = rohr_physics_soft_body_create();
        if(rohr_error_check(result)) goto fail;
        soft_body = result.result.value;
    }
    {
        const Position center = {0.0f, 105.0f};
        const float radius = 58.0f;
        for(uint32_t i = 0; i < OUTER_NODE_COUNT; i += 1) {
            float angle = 2.0f * PI_F * (float)i / (float)OUTER_NODE_COUNT;
            Position position = {
                .x = center.x + cosf(angle) * radius,
                .y = center.y + sinf(angle) * radius
            };
            EntityResult result = rohr_physics_soft_body_node_create(
                soft_body, position, 1.0f, 7.0f);
            if(rohr_error_check(result)) goto fail;
            nodes[i] = result.result.value;
            if(!result_ok(rohr_physics_acceleration_set(nodes[i], (Acceleration){0.0f, -175.0f})) ||
                    !result_ok(rohr_physics_soft_body_node_collision_filter_set(
                        nodes[i], soft_node_category, room_category))) goto fail;
        }
        {
            EntityResult result = rohr_physics_soft_body_node_create(
                soft_body, center, 2.0f, 4.0f);
            if(rohr_error_check(result)) goto fail;
            nodes[OUTER_NODE_COUNT] = result.result.value;
            if(!result_ok(rohr_physics_acceleration_set(
                        nodes[OUTER_NODE_COUNT], (Acceleration){0.0f, -175.0f})) ||
                    !result_ok(rohr_physics_soft_body_node_collision_filter_set(
                        nodes[OUTER_NODE_COUNT], soft_node_category, ROHR_COLLISION_CATEGORY_NONE))) goto fail;
        }
    }
    for(uint32_t i = 0; i < OUTER_NODE_COUNT; i += 1) {
        Entity next = nodes[(i + 1) % OUTER_NODE_COUNT];
        EntityResult perimeter = rohr_physics_soft_body_beam_create(
            soft_body, nodes[i], next, 170.0f, 8.0f);
        EntityResult radial = rohr_physics_soft_body_beam_create(
            soft_body, nodes[i], nodes[OUTER_NODE_COUNT], 125.0f, 7.0f);
        EntityResult surface = rohr_physics_soft_body_triangle_create(
            soft_body, nodes[OUTER_NODE_COUNT], nodes[i], next);
        if(rohr_error_check(perimeter) || rohr_error_check(radial) ||
                rohr_error_check(surface)) goto fail;
    }

    rohr_engine_reset_clock();
    while(true) {
        SDL_Event event = rohr_engine_poll_event();
        KeyboardEvent key_event = rohr_controller_capture_keyboard_event(&event);
        rohr_controller_update_key_states(&keyboard);
        rohr_controller_add_key_event(&keyboard, key_event);
        if(event.type == SDL_EVENT_QUIT || rohr_controller_key_pressed_is(&keyboard, SDLK_ESCAPE)) break;
        rohr_physics_update(rohr_engine_update_tick());
        rohr_graphics_draw_background(background_color);
        for(uint32_t i = 0; i < 4; i += 1) {
            rohr_graphics_draw_hit_box_colored(walls[i], GRAPHICS_FILLED, wall_color);
        }
        (void)rohr_graphics_draw_soft_body(soft_body, surface_color, beam_color, node_color);
        rohr_graphics_show();
    }
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;

fail:
    fprintf(stderr, "soft-body example failed\n");
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
