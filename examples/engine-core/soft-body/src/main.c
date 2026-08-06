#include "rohr.h"
#include "example_runtime.h"

#include <math.h>
#include <stdio.h>

#define WHEEL_COUNT 2
#define ANCHOR_NODE_COUNT 10
#define OUTER_NODE_COUNT 20
#define OUTER_NODE_START ANCHOR_NODE_COUNT
#define WHEEL_NODE_COUNT (ANCHOR_NODE_COUNT + OUTER_NODE_COUNT)
#define RAMP_TRIANGLE_COUNT 4
#define LEVEL_WALL_COUNT 5
#define PIT_PARTICLE_COUNT 200
#define PIT_PARTICLE_COLUMNS 20

typedef struct Wheel {
    Entity hub;
    Entity disk;
    Entity soft_body;
    Entity nodes[WHEEL_NODE_COUNT];
} Wheel;

static const Color background_color;
static const Color wall_color;
static const Color chassis_color;
static const Color cabin_color;
static const Color hub_color;
static const Color disk_color;
static const Color surface_color;
static const Color beam_color;
static const Color node_color;
static const Color ramp_color;
static const Color particle_color;
static const RohrCollisionCategoryMask room_category;
static const RohrCollisionCategoryMask soft_node_category;
static const RohrCollisionCategoryMask disk_category;
static const RohrCollisionCategoryMask chassis_category;
static const RohrCollisionCategoryMask particle_category;
static const float inner_beam_stiffness;
static const float outer_beam_stiffness;
static const float support_beam_stiffness;
static const float physics_tick_time;
static const Acceleration gravity;
static const float rigid_friction;
static const float node_friction;
static const Restitution collision_restitution;
static const float cabin_half_side;
static const float cabin_height;
static const float wheel_inner_radius;
static const float wheel_outer_radius;
static const float inner_node_radius;
static const float outer_node_radius;
static const Mass node_mass;
static const float inner_beam_damping;
static const float outer_beam_damping;
static const float support_beam_damping;
static const float hub_radius;
static const Mass hub_mass;
static const float disk_radius;
static const Mass disk_mass;
static const Torque control_torque;
static const AngularVelocity maximum_wheel_angular_velocity;
static const Vec2D chassis_dimensions;
static const Mass chassis_mass;
static const Mass cabin_mass;
static const float wheel_center_y;
static const float wheel_horizontal_position_ratio;
static const float wheel_vertical_position_ratio;
static const float cabin_right_recess_ratio;
static const Position level_wall_positions[LEVEL_WALL_COUNT];
static const Vec2D level_wall_dimensions[LEVEL_WALL_COUNT];
static const Position ramp_vertices[RAMP_TRIANGLE_COUNT][3];
static const Position ramp_weld_points[RAMP_TRIANGLE_COUNT - 1];
static const float particle_radius;
static const Mass particle_mass;
static const float particle_spacing;
static const Position particle_spawn_origin;
static const float camera_default_zoom;
static const float camera_collision_zoom;
static const Time camera_collision_zoom_duration;
static const Time camera_collision_zoom_out_duration;
static const float camera_collision_physics_scale;

static bool result_ok(EngineResult result) {
    if(!rohr_error_check(result)) return true;
    rohr_error_stderr_print(result.result.error);
    return false;
}

static Entity wall_create(Position position, Vec2D dimensions) {
    EntityResult result = rohr_entity_add();
    Entity wall;
    if(rohr_error_check(result)) return ENTITY_INVALID;
    wall = result.result.value;
    if(!result_ok(rohr_physics_position_set(wall, position)) ||
            !result_ok(rohr_physics_hitbox_set(
                wall, rohr_math_square_create(dimensions.x, dimensions.y))) ||
            !result_ok(rohr_physics_static_set(wall)) ||
            !result_ok(rohr_physics_friction_set(wall, rigid_friction)) ||
            !result_ok(rohr_physics_restitution_set(wall, collision_restitution)) ||
            !result_ok(rohr_physics_collision_category_set(wall, room_category)) ||
            !result_ok(rohr_physics_collision_with_all_set(wall))) return ENTITY_INVALID;
    return wall;
}

static Shape truck_cabin_shape_create(void) {
    return (Shape){
        .amount_of_vertices = 3,
        .vertices = {
            {-cabin_half_side, -cabin_height / 3.0f},
            {cabin_half_side, -cabin_height / 3.0f},
            {0.0f, cabin_height * 2.0f / 3.0f}
        }
    };
}

static Entity rigid_body_create(Position position, Shape hitbox, Mass body_mass,
        RohrCollisionCategoryMask category, RohrCollisionCategoryMask collides_with) {
    EntityResult result = rohr_entity_add();
    Entity body;
    if(rohr_error_check(result)) return ENTITY_INVALID;
    body = result.result.value;
    if(!result_ok(rohr_physics_position_set(body, position)) ||
            !result_ok(rohr_physics_hitbox_set(body, hitbox)) ||
            !result_ok(rohr_physics_mass_set(body, body_mass)) ||
            !result_ok(rohr_physics_velocity_set(body, (Velocity){0})) ||
            !result_ok(rohr_physics_angular_velocity_set(body, 0.0f)) ||
            !result_ok(rohr_physics_acceleration_set(body, gravity)) ||
            !result_ok(rohr_physics_dynamic_set(body)) ||
            !result_ok(rohr_physics_friction_set(body, rigid_friction)) ||
            !result_ok(rohr_physics_restitution_set(body, collision_restitution)) ||
            !result_ok(rohr_physics_collision_category_set(body, category)) ||
            !result_ok(rohr_physics_collision_with_set(body, collides_with))) return ENTITY_INVALID;
    return body;
}

static Entity static_triangle_create(Position a, Position b, Position c) {
    Position center = {
        (a.x + b.x + c.x) / 3.0f,
        (a.y + b.y + c.y) / 3.0f
    };
    Shape triangle = {
        .amount_of_vertices = 3,
        .vertices = {
            {a.x - center.x, a.y - center.y},
            {b.x - center.x, b.y - center.y},
            {c.x - center.x, c.y - center.y}
        }
    };
    EntityResult result = rohr_entity_add();
    Entity entity;

    if(rohr_error_check(result)) return ENTITY_INVALID;
    entity = result.result.value;
    if(!result_ok(rohr_physics_position_set(entity, center)) ||
            !result_ok(rohr_physics_hitbox_set(entity, triangle)) ||
            !result_ok(rohr_physics_dynamic_set(entity)) ||
            !result_ok(rohr_physics_friction_set(entity, rigid_friction)) ||
            !result_ok(rohr_physics_restitution_set(entity, collision_restitution)) ||
            !result_ok(rohr_physics_collision_category_set(entity, room_category)) ||
            !result_ok(rohr_physics_collision_with_all_set(entity))) return ENTITY_INVALID;
    return entity;
}

static Entity particle_create(Position position) {
    EntityResult result = rohr_entity_add();
    Entity particle;

    if(rohr_error_check(result)) return ENTITY_INVALID;
    particle = result.result.value;
    if(!result_ok(rohr_physics_position_set(particle, position)) ||
            !result_ok(rohr_physics_hitbox_set(
                particle, rohr_math_circle_create(particle_radius, 8))) ||
            !result_ok(rohr_physics_mass_set(particle, particle_mass)) ||
            !result_ok(rohr_physics_velocity_set(particle, (Velocity){0})) ||
            !result_ok(rohr_physics_acceleration_set(particle, gravity)) ||
            !result_ok(rohr_physics_dynamic_set(particle)) ||
            !result_ok(rohr_physics_friction_set(particle, rigid_friction)) ||
            !result_ok(rohr_physics_restitution_set(
                particle, collision_restitution)) ||
            !result_ok(rohr_physics_collision_category_set(
                particle, particle_category)) ||
            !result_ok(rohr_physics_collision_with_all_set(particle)) ||
            !result_ok(rohr_entity_components_add(particle, PARTICLE))) {
        return ENTITY_INVALID;
    }
    return particle;
}

static bool anchored_joint_set(Entity a, Vec2D offset_a, Entity b, Vec2D offset_b,
        JointType type) {
    JointAnchorIdResult anchor_a = rohr_physics_joint_anchor_create(a, offset_a);
    JointAnchorIdResult anchor_b = rohr_physics_joint_anchor_create(b, offset_b);
    EntityResult joint = rohr_entity_add();
    if(rohr_error_check(anchor_a) || rohr_error_check(anchor_b) || rohr_error_check(joint)) {
        return false;
    }
    if(type == JOINT_WELD) {
        return result_ok(rohr_physics_joint_weld_set(
            joint.result.value, anchor_a.result.value, anchor_b.result.value));
    }
    return result_ok(rohr_physics_joint_pin_set(
        joint.result.value, anchor_a.result.value, anchor_b.result.value));
}

static bool wheel_soft_body_create(Wheel *wheel, Position center) {
    EntityResult body = rohr_physics_soft_body_create();

    if(wheel == NULL || rohr_error_check(body)) return false;
    wheel->soft_body = body.result.value;
    for(uint32_t i = 0; i < WHEEL_NODE_COUNT; i += 1) {
        bool anchored = i < ANCHOR_NODE_COUNT;
        uint32_t ring_index = anchored ? i : i - OUTER_NODE_START;
        uint32_t ring_count = anchored ? ANCHOR_NODE_COUNT : OUTER_NODE_COUNT;
        float radius = anchored ? wheel_inner_radius : wheel_outer_radius;
        float angle = 2.0f * PI_F * (float)ring_index / (float)ring_count;
        Vec2D offset = {cosf(angle) * radius, sinf(angle) * radius};
        EntityResult node = rohr_physics_soft_body_node_create(
            wheel->soft_body,
            (Position){center.x + offset.x, center.y + offset.y},
            node_mass,
            anchored ? inner_node_radius : outer_node_radius
        );
        if(rohr_error_check(node)) return false;
        wheel->nodes[i] = node.result.value;
        if(!result_ok(rohr_physics_acceleration_set(
                    wheel->nodes[i], gravity)) ||
                !result_ok(rohr_physics_friction_set(wheel->nodes[i], node_friction)) ||
                !result_ok(rohr_physics_restitution_set(
                    wheel->nodes[i], collision_restitution)) ||
                !result_ok(rohr_physics_soft_body_node_collision_filter_set(
                    wheel->nodes[i], soft_node_category,
                    room_category | disk_category | particle_category))) return false;
        if(anchored) {
            JointAnchorIdResult anchor = rohr_physics_joint_anchor_create(wheel->disk, offset);
            SoftBodyNodeAnchorPinResult connection;
            if(rohr_error_check(anchor)) return false;
            connection = rohr_physics_soft_body_node_to_anchor_pin_create(
                wheel->nodes[i], anchor.result.value);
            if(rohr_error_check(connection)) return false;
        }
    }
    for(uint32_t i = 0; i < ANCHOR_NODE_COUNT; i += 1) {
        uint32_t next = (i + 1) % ANCHOR_NODE_COUNT;
        if(rohr_error_check(rohr_physics_soft_body_beam_create(
                    wheel->soft_body, wheel->nodes[i], wheel->nodes[next],
                    inner_beam_stiffness, inner_beam_damping))) return false;
    }
    for(uint32_t i = 0; i < OUTER_NODE_COUNT; i += 1) {
        uint32_t next = (i + 1) % OUTER_NODE_COUNT;
        uint32_t anchor = i * ANCHOR_NODE_COUNT / OUTER_NODE_COUNT;
        uint32_t previous_anchor =
            (anchor + ANCHOR_NODE_COUNT - 1) % ANCHOR_NODE_COUNT;
        uint32_t next_anchor = (anchor + 1) % ANCHOR_NODE_COUNT;
        if(rohr_error_check(rohr_physics_soft_body_beam_create(
                    wheel->soft_body, wheel->nodes[OUTER_NODE_START + i],
                    wheel->nodes[OUTER_NODE_START + next], outer_beam_stiffness,
                    outer_beam_damping)) ||
                rohr_error_check(rohr_physics_soft_body_beam_create(
                    wheel->soft_body, wheel->nodes[anchor],
                    wheel->nodes[OUTER_NODE_START + i], support_beam_stiffness,
                    support_beam_damping)) ||
                rohr_error_check(rohr_physics_soft_body_beam_create(
                    wheel->soft_body, wheel->nodes[next_anchor],
                    wheel->nodes[OUTER_NODE_START + i], support_beam_stiffness,
                    support_beam_damping)) ||
                rohr_error_check(rohr_physics_soft_body_beam_create(
                    wheel->soft_body, wheel->nodes[previous_anchor],
                    wheel->nodes[OUTER_NODE_START + i], support_beam_stiffness,
                    support_beam_damping)) ||
                rohr_error_check(rohr_physics_soft_body_triangle_create(
                    wheel->soft_body, wheel->nodes[anchor],
                    wheel->nodes[OUTER_NODE_START + i],
                    wheel->nodes[OUTER_NODE_START + next]))) return false;
    }
    for(uint32_t i = 0; i < ANCHOR_NODE_COUNT; i += 1) {
        uint32_t next_anchor = (i + 1) % ANCHOR_NODE_COUNT;
        uint32_t outer = OUTER_NODE_START +
            (((i + 1) * OUTER_NODE_COUNT / ANCHOR_NODE_COUNT) % OUTER_NODE_COUNT);
        if(rohr_error_check(rohr_physics_soft_body_triangle_create(
                    wheel->soft_body, wheel->nodes[i], wheel->nodes[outer],
                    wheel->nodes[next_anchor]))) return false;
    }
    return true;
}

static bool wheel_create(Wheel *wheel, Entity chassis, Position center,
        Vec2D chassis_hub_offset) {
    if(wheel == NULL) return false;
    wheel->hub = rigid_body_create(center, rohr_math_circle_create(hub_radius, 12), hub_mass,
        chassis_category, particle_category);
    wheel->disk = rigid_body_create(center, rohr_math_circle_create(disk_radius, 16), disk_mass,
        disk_category, room_category | soft_node_category | particle_category);
    if(wheel->hub == ENTITY_INVALID || wheel->disk == ENTITY_INVALID ||
            !anchored_joint_set(chassis, chassis_hub_offset, wheel->hub,
                (Vec2D){0}, JOINT_WELD) ||
            !anchored_joint_set(wheel->hub, (Vec2D){0}, wheel->disk,
                (Vec2D){0}, JOINT_PIN)) return false;
    return wheel_soft_body_create(wheel, center);
}

static const Color background_color = {18, 22, 30, 255};
static const Color wall_color = {90, 100, 115, 255};
static const Color chassis_color = {65, 145, 105, 255};
static const Color cabin_color = {80, 175, 125, 255};
static const Color hub_color = {75, 80, 90, 255};
static const Color disk_color = {190, 75, 65, 255};
static const Color surface_color = {55, 125, 175, 150};
static const Color beam_color = {235, 240, 245, 255};
static const Color node_color = {255, 170, 70, 255};
static const Color ramp_color = {145, 120, 85, 255};
static const Color particle_color = {45, 120, 230, 255};
static const RohrCollisionCategoryMask room_category = UINT64_C(1) << 1;
static const RohrCollisionCategoryMask soft_node_category = UINT64_C(1) << 2;
static const RohrCollisionCategoryMask disk_category = UINT64_C(1) << 3;
static const RohrCollisionCategoryMask chassis_category = UINT64_C(1) << 4;
static const RohrCollisionCategoryMask particle_category = UINT64_C(1) << 5;
static const float inner_beam_stiffness = 1280.0f;
static const float outer_beam_stiffness = 1377.5f;
static const float support_beam_stiffness = 1118.75f;
static const float physics_tick_time = 1.0f / 240.0f;
static const Acceleration gravity = {0.0f, -175.0f};
static const float rigid_friction = 3.0f;
static const float node_friction = 1.0f;
static const Restitution collision_restitution = 0.3f;
static const float cabin_half_side = 30.0f;
static const float cabin_height = 51.961524f;
static const float wheel_inner_radius = 17.5f;
static const float wheel_outer_radius = 31.0f;
static const float inner_node_radius = 2.0f;
static const float outer_node_radius = 2.5f;
static const Mass node_mass = 0.70f;
static const float inner_beam_damping = 10.0f;
static const float outer_beam_damping = 8.0f;
static const float support_beam_damping = 7.0f;
static const float hub_radius = 5.0f;
static const Mass hub_mass = 1.0f;
static const float disk_radius = 15.0f;
static const Mass disk_mass = 3.0f;
static const Torque control_torque = 600000.0f;
static const AngularVelocity maximum_wheel_angular_velocity = 5.0f;
static const Vec2D chassis_dimensions = {170.0f, 40.0f};
static const Mass chassis_mass = 5.0f;
static const Mass cabin_mass = 0.10f;
static const float wheel_center_y = -175.5f;
static const float wheel_horizontal_position_ratio = 65.0f / 190.0f;
static const float wheel_vertical_position_ratio = 1.0f;
static const float cabin_right_recess_ratio = 0.1f;
static const Position level_wall_positions[LEVEL_WALL_COUNT] = {
    {-60.0f, -225.0f},
    {900.0f, 225.0f},
    {-305.0f, 0.0f},
    {600.0f, -370.0f},
    {800.0f, -320.0f}
};
static const Vec2D level_wall_dimensions[LEVEL_WALL_COUNT] = {
    {480.0f, 20.0f},
    {2400.0f, 20.0f},
    {20.0f, 470.0f},
    {400.0f, 10.0f},
    {10.0f, 100.0f}
};
static const Position ramp_vertices[RAMP_TRIANGLE_COUNT][3] = {
    {{180.0f, -215.0f}, {260.0f, -215.0f}, {260.0f, -180.0f}},
    {{260.0f, -215.0f}, {340.0f, -215.0f}, {260.0f, -180.0f}},
    {{260.0f, -180.0f}, {340.0f, -215.0f}, {340.0f, -170.0f}},
    {{340.0f, -215.0f}, {420.0f, -215.0f}, {340.0f, -170.0f}}
};
static const Position ramp_weld_points[RAMP_TRIANGLE_COUNT - 1] = {
    {260.0f, -197.5f},
    {300.0f, -197.5f},
    {340.0f, -180.0f}
};
static const float particle_radius = 3.0f;
static const Mass particle_mass = 0.3;
static const float particle_spacing = 17.0f;
static const Position particle_spawn_origin = {438.5f, -170.0f};
static const float camera_default_zoom = 1.0f;
static const float camera_collision_zoom = 2.0f;
static const Time camera_collision_zoom_duration = 5.0;
static const Time camera_collision_zoom_out_duration = 3.0;
static const float camera_collision_physics_scale = 0.2f;

int main(void) {
    UIPhysicsDebugPanel debug_panel = {0};
    const float wheel_horizontal_offset =
        chassis_dimensions.x * wheel_horizontal_position_ratio;
    const float chassis_wheel_vertical_offset =
        chassis_dimensions.y * wheel_vertical_position_ratio;
    const float chassis_center_y = wheel_center_y + chassis_wheel_vertical_offset;
    const Vec2D cabin_offset = {
        chassis_dimensions.x * (0.5f - cabin_right_recess_ratio) - cabin_half_side,
        chassis_dimensions.y * 0.5f + cabin_height / 3.0f
    };
    Entity walls[LEVEL_WALL_COUNT];
    Entity ramp[RAMP_TRIANGLE_COUNT];
    Entity particles[PIT_PARTICLE_COUNT];
    Entity chassis;
    Entity cabin;
    Wheel wheels[WHEEL_COUNT] = {0};
    KeyboardState keyboard = {0};
    Controller controller = rohr_controller_default_get();
    Tick zoom_end_tick = 0;
    bool collision_zoom_started = false;
    bool collision_slow_motion_active = false;
    bool broadphase_debug = true;

    if(!example_use_executable_directory() || !result_ok(rohr_engine_init())) return 1;
    if(!result_ok(rohr_engine_time_per_tick_set(physics_tick_time)) ||
            !result_ok(rohr_graphics_start())) goto fail;
    if(!result_ok(rohr_ui_physics_debug_panel_init(&debug_panel,
            (FontDescriptor){"assets/debug/JetBrainsMono-BoldItalic.ttf", 11.0f}))) goto fail;
    if(!rohr_controller_axis_add(&controller, "torque", (ControllerAxisBinding){
                .positive_x = SDLK_D,
                .negative_x = SDLK_A,
                .positive_y = SDLK_UNKNOWN,
                .negative_y = SDLK_UNKNOWN
            })) goto fail;
    rohr_graphics_aabb_tree_debug_set(broadphase_debug);
    for(uint32_t i = 0; i < LEVEL_WALL_COUNT; i += 1) {
        walls[i] = wall_create(level_wall_positions[i], level_wall_dimensions[i]);
        if(walls[i] == ENTITY_INVALID) goto fail;
    }

    {
        Position centers[RAMP_TRIANGLE_COUNT];

        for(uint32_t i = 0; i < RAMP_TRIANGLE_COUNT; i += 1) {
            centers[i] = (Position){
                (ramp_vertices[i][0].x + ramp_vertices[i][1].x +
                    ramp_vertices[i][2].x) / 3.0f,
                (ramp_vertices[i][0].y + ramp_vertices[i][1].y +
                    ramp_vertices[i][2].y) / 3.0f
            };
            ramp[i] = static_triangle_create(
                ramp_vertices[i][0], ramp_vertices[i][1], ramp_vertices[i][2]);
            if(ramp[i] == ENTITY_INVALID) goto fail;
        }
        for(uint32_t i = 0; i + 1 < RAMP_TRIANGLE_COUNT; i += 1) {
            if(!anchored_joint_set(
                    ramp[i],
                    (Vec2D){ramp_weld_points[i].x - centers[i].x,
                        ramp_weld_points[i].y - centers[i].y},
                    ramp[i + 1],
                    (Vec2D){ramp_weld_points[i].x - centers[i + 1].x,
                        ramp_weld_points[i].y - centers[i + 1].y},
                    JOINT_WELD)) goto fail;
        }
    }
    chassis = rigid_body_create((Position){0.0f, chassis_center_y},
        rohr_math_square_create(chassis_dimensions.x, chassis_dimensions.y), chassis_mass,
        chassis_category, room_category | particle_category);
    cabin = rigid_body_create(
        (Position){cabin_offset.x, chassis_center_y + cabin_offset.y},
        truck_cabin_shape_create(), cabin_mass,
        chassis_category, room_category | particle_category);
    for(uint32_t i = 0; i < PIT_PARTICLE_COUNT; i += 1) {
        uint32_t column = i % PIT_PARTICLE_COLUMNS;
        uint32_t row = i / PIT_PARTICLE_COLUMNS;
        particles[i] = particle_create((Position){
            particle_spawn_origin.x + (float)column * particle_spacing,
            particle_spawn_origin.y + (float)row * particle_spacing
        });
        if(particles[i] == ENTITY_INVALID) goto fail;
    }
    if(chassis == ENTITY_INVALID || cabin == ENTITY_INVALID ||
            !anchored_joint_set(chassis, cabin_offset, cabin,
                (Vec2D){0}, JOINT_WELD) ||
            !wheel_create(&wheels[0], chassis,
                (Position){-wheel_horizontal_offset, wheel_center_y},
                (Vec2D){-wheel_horizontal_offset, -chassis_wheel_vertical_offset}) ||
            !wheel_create(&wheels[1], chassis,
                (Position){wheel_horizontal_offset, wheel_center_y},
                (Vec2D){wheel_horizontal_offset, -chassis_wheel_vertical_offset})) goto fail;
    for(uint32_t i = 0; i < WHEEL_COUNT; i += 1) {
        if(!result_ok(rohr_physics_angular_velocity_maximum_set(
                    wheels[i].disk, maximum_wheel_angular_velocity))) goto fail;
    }
    if(!result_ok(rohr_graphics_camera_with_options_attach(
                chassis, (Vec2D){0}, 0.0f, true, false))) goto fail;
    if(!result_ok(rohr_camera_zoom_set(
                rohr_camera_active_get(), camera_default_zoom, 0.0))) goto fail;

    rohr_engine_clock_reset();
    while(true) {
        SDL_Event event;
        bool exit_requested = false;

        rohr_controller_key_states_update(&keyboard);
        while((event = rohr_engine_event_poll()).type != 0) {
            KeyboardEvent key_event =
                rohr_controller_keyboard_event_capture(&event);
            rohr_controller_key_event_add(&keyboard, key_event);
            if(key_event.keycode == SDLK_B &&
                    key_event.state == KEY_STATE_PRESSED) {
                broadphase_debug = !broadphase_debug;
                rohr_graphics_aabb_tree_debug_set(broadphase_debug);
            }
            if(event.type == SDL_EVENT_QUIT ||
                    rohr_controller_key_pressed_get(
                        &keyboard, SDLK_ESCAPE)) exit_requested = true;
        }
        if(exit_requested) break;
        {
            Tick ticks = rohr_system_tick_update();
            Tick current_tick = rohr_engine_tick_get();
            if(collision_slow_motion_active && current_tick >= zoom_end_tick) {
                rohr_physics_engine_time_per_tick_use();
                if(!result_ok(rohr_camera_zoom_set(
                            rohr_camera_active_get(), camera_default_zoom,
                            camera_collision_zoom_out_duration))) goto fail;
                collision_slow_motion_active = false;
            }
            if(ticks > 0) {
                Vec2D torque_axis = rohr_controller_axis_get(
                    &keyboard, &controller, "torque");
                if(!result_ok(rohr_physics_torque_for_one_tick_apply(
                            wheels[0].disk, -torque_axis.x * control_torque))) goto fail;
            }
            rohr_physics_update(ticks);
            if(!collision_zoom_started) {
                for(uint32_t i = 0; i < PIT_PARTICLE_COUNT; i += 1) {
                    if(!rohr_physics_contact_check(chassis, particles[i])) continue;
                    if(!result_ok(rohr_camera_zoom_set(
                                rohr_camera_active_get(), camera_collision_zoom,
                                camera_collision_zoom_duration)) ||
                            !result_ok(rohr_physics_dt_per_tick_set(
                                physics_tick_time * camera_collision_physics_scale))) goto fail;
                    zoom_end_tick = current_tick + (Tick)ceil(
                        camera_collision_zoom_duration / physics_tick_time);
                    collision_zoom_started = true;
                    collision_slow_motion_active = true;
                    break;
                }
            }
        }
        rohr_graphics_background_draw(background_color);
        rohr_graphics_aabb_tree_draw();
        for(uint32_t i = 0; i < LEVEL_WALL_COUNT; i += 1) {
            rohr_graphics_hit_box_colored_draw(walls[i], GRAPHICS_FILLED, wall_color);
        }
        for(uint32_t i = 0; i < RAMP_TRIANGLE_COUNT; i += 1) {
            rohr_graphics_hit_box_colored_draw(ramp[i], GRAPHICS_FILLED, ramp_color);
        }
        for(uint32_t i = 0; i < PIT_PARTICLE_COUNT; i += 1) {
            rohr_graphics_hit_box_colored_draw(
                particles[i], GRAPHICS_FILLED, particle_color);
        }
        for(uint32_t i = 0; i < WHEEL_COUNT; i += 1) {
            (void)rohr_graphics_soft_body_draw(
                wheels[i].soft_body, surface_color, beam_color, node_color);
            rohr_graphics_hit_box_colored_draw(
                wheels[i].disk, GRAPHICS_FILLED, disk_color);
        }
        rohr_graphics_hit_box_colored_draw(chassis, GRAPHICS_FILLED, chassis_color);
        rohr_graphics_hit_box_colored_draw(cabin, GRAPHICS_FILLED, cabin_color);
        for(uint32_t i = 0; i < WHEEL_COUNT; i += 1) {
            rohr_graphics_hit_box_colored_draw(
                wheels[i].hub, GRAPHICS_FILLED, hub_color);
        }
        rohr_ui_physics_debug_panel_draw(&debug_panel);
        rohr_graphics_show();
    }
    rohr_graphics_end();
    rohr_ui_physics_debug_panel_destroy(&debug_panel);
    rohr_engine_shutdown();
    return 0;

fail:
    rohr_ui_physics_debug_panel_destroy(&debug_panel);
    fprintf(stderr, "soft-body example failed\n");
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
