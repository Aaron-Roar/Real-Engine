#include "rohr.h"
#include "example_runtime.h"

#include <stdio.h>

#define BODY_COUNT 5

static const Color background_color = {18, 22, 30, 255};
static const Color wall_color = {90, 100, 115, 255};
static const Color pin_color = {70, 170, 255, 255};
static const Color weld_color = {255, 105, 120, 255};
static const Color spring_color = {245, 190, 65, 255};
static const Color joint_color = {235, 240, 245, 255};
static const RohrCollisionCategoryMask room_category = UINT64_C(1) << 1;
static const RohrCollisionCategoryMask pin_category = UINT64_C(1) << 2;
static const RohrCollisionCategoryMask weld_category = UINT64_C(1) << 3;
static const RohrCollisionCategoryMask spring_category = UINT64_C(1) << 4;

static bool result_ok(EngineResult result) {
    if(!rohr_error_check(result)) return true;
    rohr_error_print_stderr(result.result.error);
    return false;
}

static Entity body_create(Position position, Vec2D dimensions,
        Mass body_mass, RohrCollisionCategoryMask category, bool dynamic) {
    EntityResult result = rohr_entity_add();
    Entity entity;

    if(rohr_error_check(result)) return ENTITY_INVALID;
    entity = result.result.value;
    if(!result_ok(rohr_physics_position_set(entity, position)) ||
            !result_ok(rohr_physics_orientation_set(entity, 0.0f)) ||
            !result_ok(rohr_physics_hitbox_set(
                entity,
                rohr_math_create_square(dimensions.x, dimensions.y)
            )) ||
            !result_ok(rohr_physics_restitution_set(entity, 0.75f)) ||
            !result_ok(rohr_physics_friction_set(entity, 0.35f)) ||
            !result_ok(rohr_physics_collision_category_set(entity, category)) ||
            !result_ok(rohr_physics_collision_with_set(
                entity,
                dynamic ? room_category : ROHR_COLLISION_CATEGORY_ALL
            ))) return ENTITY_INVALID;
    if(dynamic) {
        if(!result_ok(rohr_physics_mass_set(entity, body_mass)) ||
                !result_ok(rohr_physics_velocity_set(entity, (Velocity){0.0f, 0.0f})) ||
                !result_ok(rohr_physics_angular_velocity_set(entity, 0.0f)) ||
                !result_ok(rohr_physics_dynamic_set(entity))) return ENTITY_INVALID;
    } else if(!result_ok(rohr_physics_static_set(entity))) {
        return ENTITY_INVALID;
    }
    return entity;
}

static Entity anchor_entity_create(Position position) {
    EntityResult result = rohr_entity_add();

    if(rohr_error_check(result) ||
            !result_ok(rohr_physics_position_set(result.result.value, position)) ||
            !result_ok(rohr_physics_orientation_set(result.result.value, 0.0f)) ||
            !result_ok(rohr_physics_static_set(result.result.value))) return ENTITY_INVALID;
    return result.result.value;
}

static Entity joint_entity_create(void) {
    EntityResult result = rohr_entity_add();
    return rohr_error_check(result) ? ENTITY_INVALID : result.result.value;
}

static bool room_create(Entity walls[4]) {
    walls[0] = body_create((Position){0.0f, -225.0f}, (Vec2D){620.0f, 20.0f}, 0.0f, room_category, false);
    walls[1] = body_create((Position){0.0f, 225.0f}, (Vec2D){620.0f, 20.0f}, 0.0f, room_category, false);
    walls[2] = body_create((Position){-305.0f, 0.0f}, (Vec2D){20.0f, 470.0f}, 0.0f, room_category, false);
    walls[3] = body_create((Position){305.0f, 0.0f}, (Vec2D){20.0f, 470.0f}, 0.0f, room_category, false);
    return walls[0] != ENTITY_INVALID && walls[1] != ENTITY_INVALID &&
        walls[2] != ENTITY_INVALID && walls[3] != ENTITY_INVALID;
}

int main(void) {
    Entity walls[4];
    Entity bodies[BODY_COUNT];
    Entity pin_anchor_entity;
    Entity pin_joint;
    Entity weld_joint;
    Entity spring_joint;
    JointAnchorIdResult anchor_a;
    JointAnchorIdResult anchor_b;
    KeyboardState keyboard = {0};
    Time next_throw = 1.0;
    uint32_t throw_index = 0;
    const Force throws[] = {
        {900.0f, 500.0f},
        {-700.0f, 850.0f},
        {800.0f, -650.0f},
        {-950.0f, -400.0f},
        {600.0f, 900.0f}
    };

    if(!example_use_executable_directory() || !result_ok(rohr_engine_init())) return 1;
    if(!result_ok(rohr_engine_time_per_tick_set(1.0 / 120.0)) ||
            !result_ok(rohr_graphics_start())) goto fail;
    if(!room_create(walls)) goto fail;

    pin_anchor_entity = anchor_entity_create((Position){-180.0f, 120.0f});
    bodies[0] = body_create((Position){-180.0f, 70.0f}, (Vec2D){70.0f, 24.0f}, 3.0f, pin_category, true);
    pin_joint = joint_entity_create();
    anchor_a = rohr_physics_joint_anchor_create(pin_anchor_entity, (Vec2D){0.0f, 0.0f});
    anchor_b = rohr_physics_joint_anchor_create(bodies[0], (Vec2D){0.0f, 50.0f});
    if(pin_anchor_entity == ENTITY_INVALID || bodies[0] == ENTITY_INVALID || pin_joint == ENTITY_INVALID ||
            rohr_error_check(anchor_a) || rohr_error_check(anchor_b) ||
            !result_ok(rohr_physics_joint_pin_set(pin_joint, anchor_a.result.value, anchor_b.result.value))) goto fail;

    bodies[1] = body_create((Position){-45.0f, 60.0f}, (Vec2D){55.0f, 30.0f}, 2.0f, weld_category, true);
    bodies[2] = body_create((Position){10.0f, 60.0f}, (Vec2D){55.0f, 30.0f}, 4.0f, weld_category, true);
    weld_joint = joint_entity_create();
    anchor_a = rohr_physics_joint_anchor_create(bodies[1], (Vec2D){27.5f, 0.0f});
    anchor_b = rohr_physics_joint_anchor_create(bodies[2], (Vec2D){-27.5f, 0.0f});
    if(bodies[1] == ENTITY_INVALID || bodies[2] == ENTITY_INVALID || weld_joint == ENTITY_INVALID ||
            rohr_error_check(anchor_a) || rohr_error_check(anchor_b) ||
            !result_ok(rohr_physics_joint_weld_set(weld_joint, anchor_a.result.value, anchor_b.result.value))) goto fail;

    bodies[3] = body_create((Position){105.0f, -70.0f}, (Vec2D){38.0f, 38.0f}, 2.0f, spring_category, true);
    bodies[4] = body_create((Position){215.0f, -70.0f}, (Vec2D){38.0f, 38.0f}, 2.0f, spring_category, true);
    spring_joint = joint_entity_create();
    anchor_a = rohr_physics_joint_anchor_create(bodies[3], (Vec2D){0.0f, 0.0f});
    anchor_b = rohr_physics_joint_anchor_create(bodies[4], (Vec2D){0.0f, 0.0f});
    if(bodies[3] == ENTITY_INVALID || bodies[4] == ENTITY_INVALID || spring_joint == ENTITY_INVALID ||
            rohr_error_check(anchor_a) || rohr_error_check(anchor_b) ||
            !result_ok(rohr_physics_joint_spring_set(
                spring_joint,
                anchor_a.result.value,
                anchor_b.result.value,
                110.0f,
                14.0f,
                2.5f
            ))) goto fail;

    rohr_engine_reset_clock();
    while(true) {
        SDL_Event event = rohr_engine_poll_event();
        KeyboardEvent key_event = rohr_controller_capture_keyboard_event(&event);
        rohr_controller_update_key_states(&keyboard);
        rohr_controller_add_key_event(&keyboard, key_event);
        if(event.type == SDL_EVENT_QUIT || rohr_controller_key_pressed_is(&keyboard, SDLK_ESCAPE)) break;

        if(rohr_engine_time_get() >= next_throw) {
            Entity body = bodies[throw_index % BODY_COUNT];
            Force impulse = throws[throw_index % (sizeof(throws) / sizeof(throws[0]))];
            if(!result_ok(rohr_physics_apply_impulse(body, impulse)) ||
                    !result_ok(rohr_physics_apply_torque_for_one_tick(
                        body,
                        (throw_index % 2 == 0 ? 1.0f : -1.0f) * 1800.0f
                    ))) goto fail;
            throw_index += 1;
            next_throw += 1.5;
        }

        rohr_physics_update(rohr_engine_update_tick());
        rohr_graphics_draw_background(background_color);
        for(uint32_t i = 0; i < 4; i += 1) rohr_graphics_draw_hit_box_colored(walls[i], GRAPHICS_FILLED, wall_color);
        rohr_graphics_draw_hit_box_colored(bodies[0], GRAPHICS_FILLED, pin_color);
        rohr_graphics_draw_hit_box_colored(bodies[1], GRAPHICS_FILLED, weld_color);
        rohr_graphics_draw_hit_box_colored(bodies[2], GRAPHICS_FILLED, weld_color);
        rohr_graphics_draw_hit_box_colored(bodies[3], GRAPHICS_FILLED, spring_color);
        rohr_graphics_draw_hit_box_colored(bodies[4], GRAPHICS_FILLED, spring_color);
        rohr_graphics_draw_joints(joint_color);
        rohr_graphics_show();
    }

    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;

fail:
    fprintf(stderr, "joints example failed\n");
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
