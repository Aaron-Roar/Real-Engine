#include "rohr.h"

int main(void) {
    EntityResult body;
    EntityResult node_a;
    EntityResult node_b;
    EntityResult node_c;
    EntityResult beam;
    EntityResult triangle;
    SoftBodyNodeAnchorPinResult attachment;
    EntityResult rigid_body;
    JointAnchorIdResult rigid_anchor;
    EntityIndexResult index_a;
    EntityIndexResult index_b;

    {
        Color white = rohr_graphics_color_hex_create(UINT32_C(0xffffffff));
        Color black = rohr_graphics_color_hex_create(UINT32_C(0x000000ff));
        Color yellow = rohr_graphics_color_hex_create(UINT32_C(0xffff00ff));
        Color orange = rohr_graphics_color_hex_create(UINT32_C(0xff800080));
        if(white.red != 255 || white.green != 255 || white.blue != 255 ||
                white.alpha != 255 || black.red != 0 || black.green != 0 ||
                black.blue != 0 || black.alpha != 255 || yellow.red != 255 ||
                yellow.green != 255 || yellow.blue != 0 || orange.red != 255 ||
                orange.green != 128 || orange.blue != 0 || orange.alpha != 128) return 1;
    }
    if(rohr_error_check(rohr_engine_init())) return 1;
    body = rohr_physics_soft_body_create();
    if(rohr_error_check(body)) goto fail;
    node_a = rohr_physics_soft_body_node_create(body.result.value, (Position){-10.0f, 0.0f}, 1.0f, 2.0f);
    node_b = rohr_physics_soft_body_node_create(body.result.value, (Position){10.0f, 0.0f}, 1.0f, 2.0f);
    node_c = rohr_physics_soft_body_node_create(body.result.value, (Position){0.0f, 15.0f}, 1.0f, 2.0f);
    if(rohr_error_check(node_a) || rohr_error_check(node_b) || rohr_error_check(node_c)) goto fail;
    {
        CollisionFilterConfigResult filter =
            rohr_physics_collision_filter_get(node_a.result.value);
        if(rohr_error_check(filter) ||
                !rohr_entity_components_check(node_a.result.value,
                    ROHR_SOFT_BODY_NODE | ROHR_PARTICLE | ROHR_HIT_BOX | ROHR_COLLISION) ||
                filter.result.value.category !=
                    ROHR_COLLISION_CATEGORY_SOFT_BODY_NODE ||
                (filter.result.value.collides_with &
                    ROHR_COLLISION_CATEGORY_SOFT_BODY_NODE) != 0) goto fail;
    }
    beam = rohr_physics_soft_body_beam_create(
        body.result.value, node_a.result.value, node_b.result.value, 10.0f, 1.0f);
    triangle = rohr_physics_soft_body_triangle_create(
        body.result.value, node_a.result.value, node_b.result.value, node_c.result.value);
    if(rohr_error_check(beam) || rohr_error_check(triangle)) goto fail;
    {
        Color node_color = {10, 20, 30, 255};
        Color beam_color = {40, 50, 60, 255};
        Color area_color = {70, 80, 90, 255};
        SoftBodyNodeResult styled_node;
        SoftBodyBeamResult styled_beam;
        SoftBodyTriangleResult styled_area;
        if(rohr_error_check(rohr_graphics_soft_body_node_color_set(
                    body.result.value, node_a.result.value, node_color)) ||
                rohr_error_check(rohr_graphics_soft_body_beam_color_set(
                    body.result.value, node_b.result.value, node_a.result.value,
                    beam_color)) ||
                rohr_error_check(rohr_graphics_soft_body_area_color_set(
                    body.result.value, node_c.result.value, node_a.result.value,
                    node_b.result.value, area_color))) goto fail;
        styled_node = rohr_physics_soft_body_node_get(node_a.result.value);
        styled_beam = rohr_physics_soft_body_beam_get(beam.result.value);
        styled_area = rohr_physics_soft_body_triangle_get(triangle.result.value);
        if(rohr_error_check(styled_node) || rohr_error_check(styled_beam) ||
                rohr_error_check(styled_area) ||
                !styled_node.result.value.draw_color_overridden ||
                styled_node.result.value.draw_color.red != node_color.red ||
                !styled_beam.result.value.draw_color_overridden ||
                styled_beam.result.value.draw_color.green != beam_color.green ||
                !styled_area.result.value.draw_color_overridden ||
                styled_area.result.value.draw_color.blue != area_color.blue) goto fail;
    }
    {
        SoftBodyResult topology = rohr_physics_soft_body_get(body.result.value);
        if(rohr_error_check(topology) || topology.result.value.node_count != 3 ||
                topology.result.value.beam_count != 1 || topology.result.value.triangle_count != 1) goto fail;
    }
    if(rohr_error_check(rohr_physics_position_set(node_b.result.value, (Position){20.0f, 0.0f})) ||
            rohr_error_check(rohr_physics_angular_velocity_set(
                node_a.result.value, 10.0f))) goto fail;
    rohr_system_physics_update(0.1);
    index_a = rohr_entity_index_get(node_a.result.value);
    index_b = rohr_entity_index_get(node_b.result.value);
    if(rohr_error_check(index_a) || rohr_error_check(index_b) ||
            velocities[index_a.result.value].x <= 0.0f ||
            velocities[index_b.result.value].x >= 0.0f ||
            angular_velocities[index_a.result.value] != 0.0f) goto fail;
    if(rohr_error_check(rohr_physics_soft_body_node_impulse_apply(
                node_c.result.value, (Vec2D){0.0f, 2.0f})) ||
            rohr_error_check(rohr_physics_soft_body_node_force_for_one_tick_apply(
                node_c.result.value, (Force){0.0f, 1.0f})) ||
            rohr_error_check(rohr_physics_soft_body_force_for_one_tick_apply(
                body.result.value, (Force){3.0f, 0.0f})) ||
            rohr_error_check(rohr_physics_soft_body_torque_for_one_tick_apply(
                body.result.value, 2.0f))) goto fail;
    rigid_body = rohr_entity_add();
    if(rohr_error_check(rigid_body) ||
            rohr_error_check(rohr_physics_position_set(rigid_body.result.value, (Position){0.0f, 0.0f})) ||
            rohr_error_check(rohr_physics_mass_set(rigid_body.result.value, 4.0f)) ||
            rohr_error_check(rohr_physics_velocity_set(rigid_body.result.value, (Velocity){0})) ||
            rohr_error_check(rohr_physics_angular_velocity_set(rigid_body.result.value, 10.0f)) ||
            rohr_error_check(rohr_physics_dynamic_set(rigid_body.result.value))) goto fail;
    {
        AngularVelocityResult angular_velocity =
            rohr_physics_angular_velocity_get(rigid_body.result.value);
        if(rohr_error_check(angular_velocity) || angular_velocity.result.value != 10.0f) goto fail;
        if(rohr_error_check(rohr_physics_angular_velocity_maximum_set(
                    rigid_body.result.value, 1.0f))) goto fail;
        rohr_system_physics_update(0.1);
        angular_velocity = rohr_physics_angular_velocity_get(rigid_body.result.value);
        if(rohr_error_check(angular_velocity) ||
                angular_velocity.result.value > 1.0f ||
                angular_velocity.result.value < -1.0f) goto fail;
    }
    rigid_anchor = rohr_physics_joint_anchor_create(
        rigid_body.result.value, (Vec2D){0.0f, 15.0f});
    if(rohr_error_check(rigid_anchor)) goto fail;
    attachment = rohr_physics_soft_body_node_to_anchor_pin_create(
        node_c.result.value, rigid_anchor.result.value);
    if(rohr_error_check(attachment) ||
            !rohr_entity_alive_check(attachment.result.value.joint)) goto fail;
    if(rohr_error_check(rohr_entity_delete(body.result.value)) ||
            rohr_entity_alive_check(node_a.result.value) || rohr_entity_alive_check(node_b.result.value) ||
            rohr_entity_alive_check(node_c.result.value) || rohr_entity_alive_check(beam.result.value) ||
            rohr_entity_alive_check(triangle.result.value) ||
            rohr_entity_alive_check(attachment.result.value.joint)) goto fail;
    {
        EntityResult collision_body = rohr_physics_soft_body_create();
        EntityResult collision_node;
        EntityResult wall = rohr_entity_add();
        EntityIndexResult collision_node_index;
        PositionResult wall_position;
        if(rohr_error_check(collision_body) || rohr_error_check(wall)) goto fail;
        collision_node = rohr_physics_soft_body_node_create(
            collision_body.result.value, (Position){11.0f, 0.0f}, 1.0f, 3.0f);
        if(rohr_error_check(collision_node)) goto fail;
        if(rohr_error_check(rohr_physics_friction_set(collision_node.result.value, 0.5f)) ||
                rohr_error_check(rohr_physics_restitution_set(collision_node.result.value, 0.3f)) ||
                rohr_error_check(rohr_physics_friction_set(wall.result.value, 0.5f)) ||
                rohr_error_check(rohr_physics_restitution_set(wall.result.value, 0.3f))) goto fail;
        if(rohr_error_check(rohr_physics_position_set(wall.result.value, (Position){0.0f, 0.0f}))) goto fail;
        if(rohr_error_check(rohr_physics_hitbox_set(
                    wall.result.value, rohr_math_square_create(20.0f, 20.0f)))) goto fail;
        if(rohr_error_check(rohr_physics_dynamic_set(wall.result.value))) goto fail;
        rohr_system_physics_update(0.0);
        collision_node_index = rohr_entity_index_get(collision_node.result.value);
        wall_position = rohr_physics_position_get(wall.result.value);
        if(rohr_error_check(collision_node_index) ||
                rohr_error_check(wall_position) ||
                positions[collision_node_index.result.value].x <= 11.0f ||
                wall_position.result.value.x != 0.0f ||
                wall_position.result.value.y != 0.0f ||
                frictions[collision_node_index.result.value] != 0.5f ||
                restitutions[collision_node_index.result.value] != 0.3f) goto fail;
    }
    {
        EntityResult boundary_body = rohr_physics_soft_body_create();
        EntityResult boundary_a;
        EntityResult boundary_b;
        EntityResult boundary_c;
        EntityResult boundary_triangle;
        EntityResult object = rohr_entity_add();
        EntityIndexResult object_index;
        PositionResult object_position;

        if(rohr_error_check(boundary_body) || rohr_error_check(object)) goto fail;
        boundary_a = rohr_physics_soft_body_node_create(
            boundary_body.result.value, (Position){-10.0f, 0.0f}, 1.0f, 1.0f);
        boundary_b = rohr_physics_soft_body_node_create(
            boundary_body.result.value, (Position){10.0f, 0.0f}, 1.0f, 1.0f);
        boundary_c = rohr_physics_soft_body_node_create(
            boundary_body.result.value, (Position){0.0f, 10.0f}, 1.0f, 1.0f);
        if(rohr_error_check(boundary_a) || rohr_error_check(boundary_b) ||
                rohr_error_check(boundary_c)) goto fail;
        if(rohr_error_check(rohr_physics_friction_set(
                    boundary_a.result.value, 1.0f)) ||
                rohr_error_check(rohr_physics_friction_set(
                    boundary_b.result.value, 1.0f))) goto fail;
        boundary_triangle = rohr_physics_soft_body_triangle_create(
            boundary_body.result.value, boundary_a.result.value,
            boundary_b.result.value, boundary_c.result.value);
        if(rohr_error_check(boundary_triangle) ||
                rohr_error_check(rohr_physics_position_set(
                    object.result.value, (Position){0.0f, -1.25f})) ||
                rohr_error_check(rohr_physics_hitbox_set(
                    object.result.value, rohr_math_square_create(1.0f, 1.0f))) ||
                rohr_error_check(rohr_physics_mass_set(object.result.value, 1.0f)) ||
                rohr_error_check(rohr_physics_velocity_set(
                    object.result.value, (Velocity){0.0f, 1.0f})) ||
                rohr_error_check(rohr_physics_angular_velocity_set(
                    object.result.value, 4.0f)) ||
                rohr_error_check(rohr_physics_dynamic_set(object.result.value)) ||
                rohr_error_check(rohr_physics_restitution_set(
                    object.result.value, 0.0f)) ||
                rohr_error_check(rohr_physics_friction_set(
                    object.result.value, 1.0f))) goto fail;
        rohr_system_physics_update(0.0);
        object_index = rohr_entity_index_get(object.result.value);
        object_position = rohr_physics_position_get(object.result.value);
        if(rohr_error_check(object_index) || rohr_error_check(object_position) ||
                object_position.result.value.y >= -1.25f ||
                velocities[object_index.result.value].x <= 0.0f ||
                angular_velocities[object_index.result.value] >= 4.0f) goto fail;
    }
    rohr_engine_shutdown();
    return 0;

fail:
    rohr_engine_shutdown();
    return 1;
}
