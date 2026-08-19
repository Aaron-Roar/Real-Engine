#include "rohr.h"
#include <stdarg.h>

void console_vwrite(LogSourceType source, const char *fmt, va_list args);
void console_debug_vwrite(LogSourceType source, const char *fmt, va_list args);

EngineResult rohr_engine_init(void) { return engine_init(); }
void rohr_engine_shutdown(void) { engine_shutdown(); }
void rohr_engine_time_update(void) { engine_time_update(); }
Time rohr_engine_time_get(void) { return engine_time_get(); }
Tick rohr_engine_tick_get(void) { return engine_tick_get(); }
void rohr_engine_pause(void) { engine_pause(); }
void rohr_engine_resume(void) { engine_resume(); }
EngineResult rohr_engine_time_per_tick_set(Time value) { return engine_time_per_tick_set(value); }
Time rohr_engine_time_per_tick_get(void) { return engine_time_per_tick_get(); }
SDL_Event rohr_engine_event_poll(void) { return engine_event_poll(); }
bool rohr_engine_paused_get(void) { return engine_paused_get(); }
void rohr_engine_clock_reset(void) { engine_clock_reset(); }

EngineResult rohr_error_result_value(bool value) { return error_result_value(value); }
EngineResult rohr_error_result_error(EngineError error) { return error_result_error(error); }
const char *rohr_error_code_message_get(EngineError error) { return error_code_message_get(error); }

void rohr_console_logs_print(void) { console_logs_print(); }
void rohr_console_init(void) { console_init(); }
void rohr_console_shutdown(void) { console_shutdown(); }
bool rohr_console_read(ConsoleLogString *input) { return console_read(input); }
void rohr_console_write(LogSourceType source, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    console_vwrite(source, fmt, args);
    va_end(args);
}
bool rohr_console_active_get(void) { return console_active_get(); }
void rohr_console_debug_write(LogSourceType source, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    console_debug_vwrite(source, fmt, args);
    va_end(args);
}
void rohr_console_debug_set(bool state) { console_debug_set(state); }

bool rohr_entity_alive_check(Entity entity) { return entity_alive_check(entity); }
bool rohr_entity_index_alive_check(EntityIndex index) { return entity_index_alive_check(index); }
uint32_t rohr_entity_alive_count_get(void) { return entity_alive_count_get(); }
EntityResult rohr_entity_alive_at_get(uint32_t position) { return entity_alive_at_get(position); }
EntityIndexResult rohr_entity_index_get(Entity entity) {
    EntityIndex index;
    if(!entity_index_get(entity, &index)) {
        return ERROR_RESULT_MAKE_ERROR(EntityIndexResult, ERROR_ENGINE_INVALID_ENTITY);
    }
    return ERROR_RESULT_MAKE_VALUE(EntityIndexResult, index);
}
EntityResult rohr_entity_from_index_get(EntityIndex index) { return entity_from_index_get(index); }
EntityResult rohr_entity_add(void) { return entity_add(); }
EngineResult rohr_entity_name_set(Entity entity, const char *name) { return entity_name_set(entity, name); }
EntityResult rohr_entity_by_name_get(const char *name) { return entity_by_name_get(name); }
EntityNameResult rohr_entity_name_get(Entity entity) { return entity_name_get(entity); }
EngineResult rohr_game_state_file_load(const char *path) { return game_state_file_load(path); }
EngineResult rohr_game_state_files_load(const char *const *paths, size_t path_count) { return game_state_files_load(paths, path_count); }
UIButtonDefinitionResult rohr_ui_button_by_name_get(const char *name) { return ui_button_by_name_get(name); }
UIFontDefinitionResult rohr_ui_font_by_name_get(const char *name) { return ui_font_by_name_get(name); }
UILabelDefinitionResult rohr_ui_label_by_name_get(const char *name) { return ui_label_by_name_get(name); }
UISliderDefinitionResult rohr_ui_slider_by_name_get(const char *name) { return ui_slider_by_name_get(name); }
EngineResult rohr_game_state_file_save(const char *path) { return game_state_file_save(path); }
EngineResult rohr_game_state_template_file_save(const char *path) { return game_state_template_file_save(path); }
EngineResult rohr_entity_delete(Entity entity) { return entity_delete(entity); }
EngineResult rohr_entity_components_add(Entity entity, RohrComponentMask mask) { return entity_components_add(entity, mask); }
bool rohr_entity_components_check(Entity entity, RohrComponentMask components) { return entity_components_check(entity, components); }
bool rohr_entity_index_components_check(EntityIndex index, RohrComponentMask components) { return entity_index_components_check(index, components); }
GroupIdResult rohr_entity_group_create(void) { return entity_group_create(); }
EngineResult rohr_entity_group_name_set(GroupId group, const char *name) { return entity_group_name_set(group, name); }
GroupIdResult rohr_entity_group_by_name_get(const char *name) { return entity_group_by_name_get(name); }
GroupNameResult rohr_entity_group_name_get(GroupId group) { return entity_group_name_get(group); }
EngineResult rohr_entity_group_destroy(GroupId group) { return entity_group_destroy(group); }
EngineResult rohr_entity_group_add(GroupId group, Entity entity) { return entity_group_add(group, entity); }
EngineResult rohr_entity_group_remove(GroupId group, Entity entity) { return entity_group_remove(group, entity); }
bool rohr_entity_group_entity_check(GroupId group, Entity entity) { return entity_group_entity_check(group, entity); }
EntityGroupResult rohr_entity_group_get(GroupId group) { return entity_group_get(group); }
EntityGroupMembershipResult rohr_entity_groups_get(Entity entity) { return entity_groups_get(entity); }
EngineResult rohr_entity_components_delete(Entity entity, RohrComponentMask mask) { return entity_components_delete(entity, mask); }
EngineResult rohr_entity_child_set(Entity parent, Entity child) { return entity_child_set(parent, child); }
EngineResult rohr_entity_parent_set(Entity child, Entity parent) { return entity_parent_set(child, parent); }
EngineResult rohr_entity_parent_remove(Entity child) { return entity_parent_remove(child); }
EngineResult rohr_entity_child_remove(Entity parent, Entity child) { return entity_child_remove(parent, child); }
ChildrenResult rohr_entity_children_get(Entity entity) { return entity_children_get(entity); }
ParentResult rohr_entity_parent_get(Entity entity) { return entity_parent_get(entity); }
EngineResult rohr_entity_life_time_set(Entity entity, Time expirey_time, Tick expirey_tick) { return entity_life_time_set(entity, expirey_time, expirey_tick); }
EngineResult rohr_entity_life_time_remove(Entity entity) { return entity_life_time_remove(entity); }

EngineResult rohr_physics_dt_per_tick_set(Time dt) { return physics_dt_per_tick_set(dt); }
Time rohr_physics_dt_per_tick_get(void) { return physics_dt_per_tick_get(); }
void rohr_physics_engine_time_per_tick_use(void) { physics_engine_time_per_tick_use(); }
EngineResult rohr_physics_solver_iterations_set(uint32_t iterations) { return physics_solver_iterations_set(iterations); }
uint32_t rohr_physics_solver_iterations_get(void) { return physics_solver_iterations_get(); }
EngineResult rohr_physics_substeps_set(uint32_t substeps) { return physics_substeps_set(substeps); }
uint32_t rohr_physics_substeps_get(void) { return physics_substeps_get(); }
EngineResult rohr_physics_gravity_set(Acceleration gravity) { return physics_gravity_set(gravity); }
Acceleration rohr_physics_gravity_get(void) { return physics_gravity_get(); }
EngineResult rohr_physics_gravity_enable(Entity entity) { return physics_gravity_enable(entity); }
EngineResult rohr_physics_gravity_disable(Entity entity) { return physics_gravity_disable(entity); }
bool rohr_physics_gravity_check(Entity entity) { return physics_gravity_check(entity); }
void rohr_physics_pipeline_step_begin(void) { physics_pipeline_step_begin(); }
void rohr_physics_pipeline_substep_begin(void) { physics_pipeline_substep_begin(); }
void rohr_physics_pipeline_accelerations_clear(void) { physics_pipeline_accelerations_clear(); }
void rohr_physics_pipeline_gravity_apply(void) { physics_pipeline_gravity_apply(); }
void rohr_physics_pipeline_forces_apply(void) { physics_pipeline_forces_apply(); }
void rohr_physics_pipeline_integrate(double dt) { physics_pipeline_integrate(dt); }
void rohr_physics_pipeline_contacts_gather(void) { physics_pipeline_contacts_gather(); }
void rohr_physics_pipeline_joints_gather(void) { physics_pipeline_joints_gather(); }
void rohr_physics_pipeline_constraints_solve(uint32_t iterations) { physics_pipeline_constraints_solve(iterations); }
void rohr_physics_pipeline_substep(double dt) { physics_pipeline_substep(dt); }
void rohr_physics_pipeline_update(double dt) { physics_pipeline_update(dt); }
void rohr_physics_update(Tick ticks) { physics_update(ticks); }
void rohr_physics_dt_update(Time dt) { physics_dt_update(dt); }
Shape rohr_physics_shape_world_translate(Shape shape, Position position, Orientation angle) { return physics_shape_world_translate(shape, position, angle); }
float rohr_physics_polygon_moment_of_inertia(Shape shape, Mass mass_value) { return physics_polygon_moment_of_inertia(shape, mass_value); }
OverlapInfo rohr_physics_sat_overlap_get(Shape shape_1, Shape shape_2) { return physics_sat_overlap_get(shape_1, shape_2); }
Vec1D rohr_physics_circle_moment_of_inertia(Shape circle, Mass mass_value) { return physics_circle_moment_of_inertia(circle, mass_value); }
bool rohr_physics_entity_held_get(EntityIndex index) { return physics_entity_held_get(index); }
EngineResult rohr_physics_acceleration_set(Entity entity, Acceleration a) { return physics_acceleration_set(entity, a); }
EngineResult rohr_physics_angular_acceleration_set(Entity entity, AngularAcceleration acceleration) {
    return physics_angular_acceleration_set(entity, acceleration);
}
EngineResult rohr_physics_acceleration_toward_position_set(Entity entity, float acceleration_magnitude, Position position) {
    return physics_acceleration_toward_position_set(entity, acceleration_magnitude, position);
}
EngineResult rohr_physics_acceleration_toward_entity_set(Entity entity, float acceleration_magnitude, Entity target) {
    return physics_acceleration_toward_entity_set(entity, acceleration_magnitude, target);
}
EngineResult rohr_physics_acceleration_away_from_position_set(Entity entity, float acceleration_magnitude, Position position) {
    return physics_acceleration_away_from_position_set(entity, acceleration_magnitude, position);
}
EngineResult rohr_physics_acceleration_away_from_entity_set(Entity entity, float acceleration_magnitude, Entity target) {
    return physics_acceleration_away_from_entity_set(entity, acceleration_magnitude, target);
}
EngineResult rohr_physics_group_acceleration_toward_entity_set(GroupId group, float acceleration_magnitude, Entity target) {
    return physics_group_acceleration_toward_entity_set(group, acceleration_magnitude, target);
}
EngineResult rohr_physics_group_acceleration_away_from_entity_set(GroupId group, float acceleration_magnitude, Entity target) {
    return physics_group_acceleration_away_from_entity_set(group, acceleration_magnitude, target);
}
EngineResult rohr_physics_velocity_set(Entity entity, Velocity v) { return physics_velocity_set(entity, v); }
EngineResult rohr_physics_velocity_toward_position_set(Entity entity, float speed, Position position) {
    return physics_velocity_toward_position_set(entity, speed, position);
}
EngineResult rohr_physics_velocity_toward_entity_set(Entity entity, float speed, Entity target) {
    return physics_velocity_toward_entity_set(entity, speed, target);
}
EngineResult rohr_physics_velocity_away_from_position_set(Entity entity, float speed, Position position) {
    return physics_velocity_away_from_position_set(entity, speed, position);
}
EngineResult rohr_physics_velocity_away_from_entity_set(Entity entity, float speed, Entity target) {
    return physics_velocity_away_from_entity_set(entity, speed, target);
}
EngineResult rohr_physics_group_velocity_toward_entity_set(GroupId group, float speed, Entity target) {
    return physics_group_velocity_toward_entity_set(group, speed, target);
}
EngineResult rohr_physics_group_velocity_away_from_entity_set(GroupId group, float speed, Entity target) {
    return physics_group_velocity_away_from_entity_set(group, speed, target);
}
EngineResult rohr_physics_entity_stop(Entity entity) { return physics_entity_stop(entity); }
EngineResult rohr_physics_group_entities_stop(GroupId group) { return physics_group_entities_stop(group); }
EngineResult rohr_physics_impulse_apply(Entity entity, Vec2D impulse) { return physics_impulse_apply(entity, impulse); }
EngineResult rohr_physics_position_set(Entity entity, Position p) { return physics_position_set(entity, p); }
PositionResult rohr_physics_position_get(Entity entity) { return physics_position_get(entity); }
EngineResult rohr_physics_mass_set(Entity entity, Mass m) { return physics_mass_set(entity, m); }
EngineResult rohr_physics_mass_remove(Entity entity) { return physics_mass_remove(entity); }
bool rohr_physics_mass_check(Entity entity) { return physics_mass_check(entity); }
EngineResult rohr_physics_kinematic_driven_set(Entity entity) { return physics_kinematic_driven_set(entity); }
EngineResult rohr_physics_kinematic_driven_remove(Entity entity) { return physics_kinematic_driven_remove(entity); }
bool rohr_physics_kinematic_driven_check(Entity entity) { return physics_kinematic_driven_check(entity); }
EntityResult rohr_physics_force_create(Entity entity, Force f) { return physics_force_create(entity, f); }
EngineResult rohr_physics_force_component_set(Entity entity, Force force) {
    return physics_force_component_set(entity, force);
}
EngineResult rohr_physics_force_for_one_tick_apply(Entity entity, Force f) { return physics_force_for_one_tick_apply(entity, f); }
EntityResult rohr_physics_torque_create(Entity entity, Torque t) { return physics_torque_create(entity, t); }
EngineResult rohr_physics_torque_component_set(Entity entity, Torque torque) {
    return physics_torque_component_set(entity, torque);
}
EngineResult rohr_physics_torque_for_one_tick_apply(Entity entity, Torque t) { return physics_torque_for_one_tick_apply(entity, t); }
EngineResult rohr_physics_hitbox_set(Entity entity, Shape hitbox) { return physics_hitbox_set(entity, hitbox); }
CollisionFilterConfig rohr_physics_collision_filter_config_default_get(void) { return physics_collision_filter_config_default_get(); }
EngineResult rohr_physics_collision_filter_set(Entity entity, CollisionFilterConfig config) { return physics_collision_filter_set(entity, config); }
CollisionFilterConfigResult rohr_physics_collision_filter_get(Entity entity) { return physics_collision_filter_get(entity); }
EngineResult rohr_physics_collision_category_set(Entity entity, RohrCollisionCategoryMask category) { return physics_collision_category_set(entity, category); }
EngineResult rohr_physics_collision_with_set(Entity entity, RohrCollisionCategoryMask categories) { return physics_collision_with_set(entity, categories); }
EngineResult rohr_physics_collision_with_all_set(Entity entity) { return physics_collision_with_all_set(entity); }
EngineResult rohr_physics_collision_with_none_set(Entity entity) { return physics_collision_with_none_set(entity); }
bool rohr_physics_collision_between_check(Entity entity_1, Entity entity_2) { return physics_collision_between_check(entity_1, entity_2); }
EngineResult rohr_physics_orientation_set(Entity entity, Orientation angle) { return physics_orientation_set(entity, angle); }
EngineResult rohr_physics_angular_velocity_set(Entity entity, AngularVelocity v) { return physics_angular_velocity_set(entity, v); }
AngularVelocityResult rohr_physics_angular_velocity_get(Entity entity) { return physics_angular_velocity_get(entity); }
EngineResult rohr_physics_angular_velocity_maximum_set(Entity entity, AngularVelocity maximum) { return physics_angular_velocity_maximum_set(entity, maximum); }
AngularVelocityResult rohr_physics_angular_velocity_maximum_get(Entity entity) { return physics_angular_velocity_maximum_get(entity); }
ShapeResult rohr_physics_global_hit_box_get(Entity entity) { return physics_global_hit_box_get(entity); }
EngineResult rohr_physics_restitution_set(Entity entity, Restitution restitution) { return physics_restitution_set(entity, restitution); }
EngineResult rohr_physics_dynamic_set(Entity entity) { return physics_dynamic_set(entity); }
EngineResult rohr_physics_static_set(Entity entity) { return physics_static_set(entity); }
EngineResult rohr_physics_entity_hold(Entity entity) { return physics_entity_hold(entity); }
EngineResult rohr_physics_entity_unhold(Entity entity) { return physics_entity_unhold(entity); }
EngineResult rohr_physics_group_entities_hold(GroupId group) { return physics_group_entities_hold(group); }
EngineResult rohr_physics_group_entities_unhold(GroupId group) { return physics_group_entities_unhold(group); }
EngineResult rohr_physics_angle_lock_set(Entity entity, Orientation min, Orientation max) { return physics_angle_lock_set(entity, min, max); }
EngineResult rohr_physics_axis_lock_set(Entity entity, Axis axis, Position axis_point) { return physics_axis_lock_set(entity, axis, axis_point); }
EngineResult rohr_physics_friction_set(Entity entity, float friction) { return physics_friction_set(entity, friction); }
EngineResult rohr_physics_transform_lock_set(Entity driven, Entity driver, Vec2D local_offset, Orientation local_angle, bool lock_position, bool lock_orientation, bool inherit_velocity) {
    return physics_transform_lock_set(driven, driver, local_offset, local_angle, lock_position, lock_orientation, inherit_velocity);
}
EngineResult rohr_physics_transform_lock_remove(Entity entity) { return physics_transform_lock_remove(entity); }
EngineResult rohr_physics_transform_lock_current_transform_set(Entity driven, Entity driver, bool lock_position, bool lock_orientation, bool inherit_velocity) {
    return physics_transform_lock_current_transform_set(driven, driver, lock_position, lock_orientation, inherit_velocity);
}
EngineResult rohr_physics_target_set(Entity entity, Entity target) {
    return physics_target_set(entity, target);
}
EngineResult rohr_physics_joint_component_set(Entity entity, Joint joint) {
    return physics_joint_component_set(entity, joint);
}
JointAnchorIdResult rohr_physics_joint_anchor_create(Entity entity, Vec2D local_offset) { return physics_joint_anchor_create(entity, local_offset); }
JointAnchorListResult rohr_physics_joint_anchors_get(Entity entity) { return physics_joint_anchors_get(entity); }
JointAnchorPositionResult rohr_physics_joint_anchor_local_position_get(JointAnchorId anchor) { return physics_joint_anchor_local_position_get(anchor); }
JointAnchorPositionResult rohr_physics_joint_anchor_world_position_get(JointAnchorId anchor) { return physics_joint_anchor_world_position_get(anchor); }
EngineResult rohr_physics_joint_anchor_local_position_set(JointAnchorId anchor, Vec2D local_offset) { return physics_joint_anchor_local_position_set(anchor, local_offset); }
EngineResult rohr_physics_joint_anchor_remove(JointAnchorId anchor) { return physics_joint_anchor_remove(anchor); }
EngineResult rohr_physics_joint_pin_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b) { return physics_joint_pin_set(joint, anchor_a, anchor_b); }
EngineResult rohr_physics_joint_weld_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b) { return physics_joint_weld_set(joint, anchor_a, anchor_b); }
EngineResult rohr_physics_joint_spring_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b, float rest_length, float stiffness, float damping) { return physics_joint_spring_set(joint, anchor_a, anchor_b, rest_length, stiffness, damping); }
EntityResult rohr_physics_soft_body_create(void) { return physics_soft_body_create(); }
SoftBodyResult rohr_physics_soft_body_get(Entity soft_body) { return physics_soft_body_get(soft_body); }
EntityResult rohr_physics_soft_body_node_create(Entity soft_body, Position position, Mass mass_value, float radius) { return physics_soft_body_node_create(soft_body, position, mass_value, radius); }
SoftBodyNodeResult rohr_physics_soft_body_node_get(Entity node) { return physics_soft_body_node_get(node); }
EngineResult rohr_physics_soft_body_node_collision_filter_set(Entity node, RohrCollisionCategoryMask category, RohrCollisionCategoryMask collides_with) { return physics_soft_body_node_collision_filter_set(node, category, collides_with); }
EngineResult rohr_physics_soft_body_node_force_for_one_tick_apply(Entity node, Force force) { return physics_soft_body_node_force_for_one_tick_apply(node, force); }
EngineResult rohr_physics_soft_body_node_impulse_apply(Entity node, Vec2D impulse) { return physics_soft_body_node_impulse_apply(node, impulse); }
EngineResult rohr_physics_soft_body_force_for_one_tick_apply(Entity soft_body, Force force) { return physics_soft_body_force_for_one_tick_apply(soft_body, force); }
EngineResult rohr_physics_soft_body_torque_for_one_tick_apply(Entity soft_body, Torque torque) { return physics_soft_body_torque_for_one_tick_apply(soft_body, torque); }
SoftBodyNodeAnchorPinResult rohr_physics_soft_body_node_to_anchor_pin_create(Entity node, JointAnchorId anchor) { return physics_soft_body_node_to_anchor_pin_create(node, anchor); }
EntityResult rohr_physics_soft_body_beam_create(Entity soft_body, Entity node_a, Entity node_b, float stiffness, float damping) { return physics_soft_body_beam_create(soft_body, node_a, node_b, stiffness, damping); }
SoftBodyBeamResult rohr_physics_soft_body_beam_get(Entity beam) { return physics_soft_body_beam_get(beam); }
EntityResult rohr_physics_soft_body_triangle_create(Entity soft_body, Entity node_a, Entity node_b, Entity node_c) { return physics_soft_body_triangle_create(soft_body, node_a, node_b, node_c); }
SoftBodyTriangleResult rohr_physics_soft_body_triangle_get(Entity triangle) { return physics_soft_body_triangle_get(triangle); }
EntityResult rohr_physics_joint_create(Entity a, Entity b, JointType type, Vec2D local_anchor_a, Vec2D local_anchor_b, float stiffness, float damping) {
    return physics_joint_create(a, b, type, local_anchor_a, local_anchor_b, stiffness, damping);
}
OverlapInfo rohr_physics_particle_overlap_get(Shape shape_1, Shape shape_2) { return physics_particle_overlap_get(shape_1, shape_2); }
bool rohr_physics_overlap_check(Entity entity, Entity target) { return physics_overlap_check(entity, target); }
OverlapInfo rohr_physics_overlap_get(Entity entity, Entity target) { return physics_overlap_get(entity, target); }
bool rohr_physics_overlap_entered_check(Entity entity, Entity target) { return physics_overlap_entered_check(entity, target); }
bool rohr_physics_overlap_stayed_check(Entity entity, Entity target) { return physics_overlap_stayed_check(entity, target); }
bool rohr_physics_overlap_exited_check(Entity entity, Entity target) { return physics_overlap_exited_check(entity, target); }
size_t rohr_physics_overlap_count_get(Entity entity) { return physics_overlap_count_get(entity); }
size_t rohr_physics_overlaps_get(Entity entity, EntityInteraction *results, size_t capacity) { return physics_overlaps_get(entity, results, capacity); }
bool rohr_physics_contact_check(Entity entity, Entity target) { return physics_contact_check(entity, target); }
ContactInfo rohr_physics_contact_get(Entity entity, Entity target) { return physics_contact_get(entity, target); }
Vec2D rohr_physics_contact_total_impulse_get(ContactInfo contact) { return physics_contact_total_impulse_get(contact); }
bool rohr_physics_contact_entered_check(Entity entity, Entity target) { return physics_contact_entered_check(entity, target); }
bool rohr_physics_contact_stayed_check(Entity entity, Entity target) { return physics_contact_stayed_check(entity, target); }
bool rohr_physics_contact_exited_check(Entity entity, Entity target) { return physics_contact_exited_check(entity, target); }
size_t rohr_physics_contact_count_get(Entity entity) { return physics_contact_count_get(entity); }
size_t rohr_physics_contacts_get(Entity entity, EntityContact *results, size_t capacity) { return physics_contacts_get(entity, results, capacity); }

Color rohr_graphics_color_hex_create(uint32_t hex_color_code) { return graphics_color_hex_create(hex_color_code); }
EngineResult rohr_graphics_start(void) { return graphics_start(); }
void rohr_graphics_end(void) { graphics_end(); }
bool rohr_graphics_events_poll(SDL_Event *event) { return graphics_events_poll(event); }
void rohr_graphics_background_draw(Color color) { graphics_background_draw(color); }
void rohr_graphics_layer_set(int layer) { graphics_layer_set(layer); }
int rohr_graphics_layer_get(void) { return graphics_layer_get(); }
bool rohr_graphics_screen_rect_draw(float x, float y, float width, float height, Color color) { return graphics_screen_rect_draw(x, y, width, height, color); }
Scale rohr_graphics_render_output_size_get(void) { return graphics_render_output_size_get(); }
bool rohr_graphics_logical_size_set(int width, int height) { return graphics_logical_size_set(width, height); }
bool rohr_graphics_aspect_ratio_set(int width, int height) { return graphics_aspect_ratio_set(width, height); }
bool rohr_graphics_aspect_ratio_auto_set(bool enabled) { return graphics_aspect_ratio_auto_set(enabled); }
GraphicsWindowPresentationConfig rohr_graphics_window_presentation_default_get(void) { return graphics_window_presentation_default_get(); }
GraphicsWindowPresentationConfig rohr_graphics_window_presentation_get(void) { return graphics_window_presentation_get(); }
EngineResult rohr_graphics_window_presentation_set(GraphicsWindowPresentationConfig config) { return graphics_window_presentation_set(config); }
bool rohr_graphics_screen_clip_set(float x, float y, float width, float height) { return graphics_screen_clip_set(x, y, width, height); }
void rohr_graphics_screen_clip_clear(void) { graphics_screen_clip_clear(); }
bool rohr_graphics_screen_quad_draw(Position center, float width, float height, float angle, Color color) { return graphics_screen_quad_draw(center, width, height, angle, color); }
void rohr_graphics_show(void) { graphics_show(); }
EngineResult rohr_graphics_vsync_set(bool enabled) { return graphics_vsync_set(enabled); }
EngineResult rohr_graphics_frame_limit_set(int frames_per_second) { return graphics_frame_limit_set(frames_per_second); }
void rohr_graphics_hit_box_draw(Entity entity, Fill fill_type) { graphics_hit_box_draw(entity, fill_type); }
void rohr_graphics_hit_box_colored_draw(Entity entity, Fill fill_type, Color color) { graphics_hit_box_colored_draw(entity, fill_type, color); }
void rohr_graphics_hit_boxes_draw(void) { graphics_hit_boxes_draw(); }
bool rohr_graphics_joint_draw(Entity joint, Color color) { return graphics_joint_draw(joint, color); }
void rohr_graphics_joints_draw(Color color) { graphics_joints_draw(color); }
bool rohr_graphics_soft_body_draw(Entity soft_body, Color surface, Color beam, Color node) { return graphics_soft_body_draw(soft_body, surface, beam, node); }
EngineResult rohr_graphics_soft_body_node_color_set(Entity soft_body, Entity node, Color color) { return graphics_soft_body_node_color_set(soft_body, node, color); }
EngineResult rohr_graphics_soft_body_beam_color_set(Entity soft_body, Entity node_a, Entity node_b, Color color) { return graphics_soft_body_beam_color_set(soft_body, node_a, node_b, color); }
EngineResult rohr_graphics_soft_body_area_color_set(Entity soft_body, Entity node_a, Entity node_b, Entity node_c, Color color) { return graphics_soft_body_area_color_set(soft_body, node_a, node_b, node_c, color); }
TextureAssetResult rohr_graphics_texture_load(TextureDescriptor text_desc) { return graphics_texture_load(text_desc); }
void rohr_graphics_texture_draw(TextureAsset texture, Position position,
    Orientation orientation) { graphics_texture_draw(texture, position, orientation); }
void rohr_graphics_screen_texture_draw(TextureAsset texture, Position center,
    Scale size, Orientation orientation) {
    graphics_screen_texture_draw(texture, center, size, orientation);
}
FontAssetResult rohr_graphics_font_load(FontDescriptor descriptor) { return graphics_font_load(descriptor); }
void rohr_graphics_font_destroy(FontAsset *font) { graphics_font_destroy(font); }
TextAssetResult rohr_graphics_text_create(const FontAsset *font, const char *value, Color color) { return graphics_text_create(font, value, color); }
bool rohr_graphics_text_value_set(TextAsset *text, const char *value) { return graphics_text_value_set(text, value); }
void rohr_graphics_text_destroy(TextAsset *text) { graphics_text_destroy(text); }
bool rohr_graphics_text_draw(const TextAsset *text, Position position) { return graphics_text_draw(text, position); }
AnimationAssetResult rohr_graphics_animation_load(AnimationDescriptor anim_desc) { return graphics_animation_load(anim_desc); }
AnimatedSprite rohr_graphics_animated_sprite_create(AnimationAsset asset_ptr, Scale scale) { return graphics_animated_sprite_create(asset_ptr, scale); }
void rohr_graphics_animated_sprite_update(AnimatedSprite *sprite, Tick current_tick, Time current_time) { graphics_animated_sprite_update(sprite, current_tick, current_time); }
EngineResult rohr_graphics_animated_sprite_add(Entity entity, AnimatedSprite sprite) { return graphics_animated_sprite_add(entity, sprite); }
Sprite rohr_graphics_sprite_create(TextureAsset asset, Scale scale) { return graphics_sprite_create(asset, scale); }
EngineResult rohr_graphics_sprite_add(Entity entity, Sprite sprite) { return graphics_sprite_add(entity, sprite); }
bool rohr_graphics_sprite_draw(Entity entity) { return graphics_sprite_draw(entity); }
void rohr_graphics_sprites_draw(void) { graphics_sprites_draw(); }
bool rohr_graphics_animated_sprite_draw(Entity entity) { return graphics_animated_sprite_draw(entity); }
void rohr_graphics_animated_sprites_draw(void) { graphics_animated_sprites_draw(); }
void rohr_graphics_sprite_frames_update(Tick current_tick, Time current_time) { graphics_sprite_frames_update(current_tick, current_time); }
void rohr_graphics_textures_scale(Entity entity, Scale scale) { graphics_textures_scale(entity, scale); }
void rohr_graphics_camera_move(Vec2D translation) { graphics_camera_move(translation); }
void rohr_graphics_camera_rotate(Orientation radians) { graphics_camera_rotate(radians); }
EngineResult rohr_graphics_camera_attach(Entity entity, Vec2D position_offset, Orientation orientation_offset) { return graphics_camera_attach(entity, position_offset, orientation_offset); }
EngineResult rohr_graphics_camera_with_options_attach(Entity entity, Vec2D position_offset, Orientation orientation_offset, bool follow_position, bool follow_orientation) { return graphics_camera_with_options_attach(entity, position_offset, orientation_offset, follow_position, follow_orientation); }
void rohr_graphics_camera_detach(void) { graphics_camera_detach(); }
bool rohr_graphics_camera_attached_get(void) { return graphics_camera_attached_get(); }
CameraAttachmentResult rohr_graphics_camera_attachment_get(void) {
    CameraAttachment attachment;
    if(!graphics_camera_attachment_get(&attachment)) {
        return ERROR_RESULT_MAKE_ERROR(CameraAttachmentResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(CameraAttachmentResult, attachment);
}
CameraConfig rohr_camera_config_default_get(void) { return graphics_camera_config_default_get(); }
CameraIdResult rohr_camera_create(CameraConfig config) { return graphics_camera_create(config); }
EngineResult rohr_camera_destroy(CameraId camera_id) { return graphics_camera_destroy(camera_id); }
EngineResult rohr_camera_active_set(CameraId camera_id) { return graphics_camera_active_set(camera_id); }
CameraId rohr_camera_active_get(void) { return graphics_camera_active_get(); }
CameraResult rohr_camera_get(CameraId camera_id) { return graphics_camera_get(camera_id); }
EngineResult rohr_camera_set(CameraId camera_id, Camera value) { return graphics_camera_set(camera_id, value); }
EngineResult rohr_camera_attach(
        CameraId camera_id,
        Entity entity,
        Vec2D position_offset,
        Orientation orientation_offset,
        bool follow_position,
        bool follow_orientation
        ) {
    return graphics_camera_attachment_set(
        camera_id,
        entity,
        position_offset,
        orientation_offset,
        follow_position,
        follow_orientation
    );
}
EngineResult rohr_camera_detach(CameraId camera_id) { return graphics_camera_attachment_remove(camera_id); }
EngineResult rohr_camera_render_callback_set(CameraId camera, CameraRenderCallback callback, void *context) { return graphics_camera_render_callback_set(camera, callback, context); }
EngineResult rohr_camera_enable_set(CameraId camera) { return graphics_camera_enable_set(camera); }
EngineResult rohr_camera_disable_set(CameraId camera) { return graphics_camera_disable_set(camera); }
EngineResult rohr_camera_pause_with_engine_set(CameraId camera) { return graphics_camera_pause_with_engine_set(camera); }
EngineResult rohr_camera_render_when_paused_set(CameraId camera) { return graphics_camera_render_when_paused_set(camera); }
EngineResult rohr_camera_position_move(CameraId camera, Vec2D translation, Time duration) { return graphics_camera_position_move(camera, translation, duration); }
EngineResult rohr_camera_position_set(CameraId camera, Position position, Time duration) { return graphics_camera_position_set(camera, position, duration); }
EngineResult rohr_camera_position_from_entity_set(CameraId camera, Entity entity, Time duration) { return graphics_camera_position_from_entity_set(camera, entity, duration); }
EngineResult rohr_camera_entity_attachment_set(CameraId camera, Entity entity) { return graphics_camera_entity_attachment_set(camera, entity); }
EngineResult rohr_camera_moving_get(CameraId camera) { return graphics_camera_moving_get(camera); }
EngineResult rohr_camera_zoom_set(CameraId camera, float zoom, Time duration) { return graphics_camera_zoom_set(camera, zoom, duration); }
CameraZoomResult rohr_camera_zoom_get(CameraId camera) { return graphics_camera_zoom_get(camera); }
ViewportConfig rohr_viewport_config_default_get(void) { return graphics_viewport_config_default_get(); }
ViewportIdResult rohr_viewport_create(ViewportConfig config) { return graphics_viewport_create(config); }
EngineResult rohr_viewport_destroy(ViewportId viewport) { return graphics_viewport_destroy(viewport); }
EngineResult rohr_viewport_camera_set(ViewportId viewport, CameraId camera) { return graphics_viewport_camera_set(viewport, camera); }
EngineResult rohr_viewport_camera_clear(ViewportId viewport) { return graphics_viewport_camera_clear(viewport); }
EngineResult rohr_viewport_enable_set(ViewportId viewport) { return graphics_viewport_enable_set(viewport); }
EngineResult rohr_viewport_disable_set(ViewportId viewport) { return graphics_viewport_disable_set(viewport); }
Position rohr_graphics_world_to_screen_get(Position pos) { return graphics_world_to_screen_get(pos); }
Position rohr_graphics_screen_to_world_get(Position screen) { return graphics_screen_to_world_get(screen); }
Position rohr_graphics_mouse_screen_position_get(void) { return graphics_mouse_screen_position_get(); }
void rohr_graphics_aabb_tree_debug_set(bool enabled) { graphics_aabb_tree_debug_set(enabled); }
bool rohr_graphics_aabb_tree_debug_check(void) { return graphics_aabb_tree_debug_check(); }
void rohr_graphics_aabb_tree_draw(void) { graphics_aabb_tree_draw(); }
bool rohr_graphics_recording_start(const char *output_path, int fps) { return graphics_recording_start(output_path, fps); }
void rohr_graphics_particles_draw(void) { graphics_particles_draw(); }
void rohr_graphics_local_origins_draw(void) { graphics_local_origins_draw(); }

Vec2DList rohr_math_normals_create(Shape shape) { return math_normals_create(shape); }
Vec2D rohr_math_vector_normalize(Vec2D vector) { return math_vector_normalize(vector); }
Vec2DList rohr_math_vectors_normalize(Vec2DList vectors) { return math_vectors_normalize(vectors); }
float rohr_math_dot_product(Vec2D vector_1, Vec2D vector_2) { return math_dot_product(vector_1, vector_2); }
Shape rohr_math_square_create(float width, float height) { return math_square_create(width, height); }
Shape rohr_math_circle_create(float radius, uint8_t verticies) { return math_circle_create(radius, verticies); }
Projection rohr_math_project_shape_on_axis(Shape shape, Axis axis) { return math_project_shape_on_axis(shape, axis); }
float rohr_math_cross_2d(Vec2D a, Vec2D b) { return math_cross_2d(a, b); }
Vec2D rohr_math_angular_velocity_cross_vec(float omega, Vec2D r) { return math_angular_velocity_cross_vec(omega, r); }
Vec2D rohr_math_project_onto_axis(Vec2D v, Axis axis) { return math_project_onto_axis(v, axis); }
float rohr_math_axis_magnitude(Axis axis) { return math_axis_magnitude(axis); }
float rohr_math_vector_magnitude(Vec2D vector) { return math_vector_magnitude(vector); }
Vec2D rohr_math_vector_rotate(Vec2D vector, float angle) { return math_vector_rotate(vector, angle); }
Vec1D rohr_math_circle_radius(Shape circle, Vec2D centroid) { return math_circle_radius(circle, centroid); }
Vec2D rohr_math_vector_subtract(Vec2D vector_a, Vec2D vector_b) { return math_vector_subtract(vector_a, vector_b); }
Vec1D rohr_math_circle_overlap_depth(Vec2D centroid_1, Vec1D radius_1, Vec2D centroid_2, Vec1D radius_2) { return math_circle_overlap_depth(centroid_1, radius_1, centroid_2, radius_2); }
float rohr_math_projection_overlap(Projection projection_1, Projection projection_2) { return math_projection_overlap(projection_1, projection_2); }
Shape rohr_math_shape_scale(Shape shape, float scale) { return math_shape_scale(shape, scale); }
Shape rohr_math_shape_y_scale(Shape shape, float scale) { return math_shape_y_scale(shape, scale); }
Shape rohr_math_shape_x_scale(Shape shape, float scale) { return math_shape_x_scale(shape, scale); }
Vec2D rohr_math_polygon_centroid(Shape shape) { return math_polygon_centroid(shape); }
Shape rohr_math_vertex_add(Shape shape) { return math_vertex_add(shape); }
Shape rohr_math_vertex_delete(Shape shape) { return math_vertex_delete(shape); }
AABB rohr_math_aabb_create(Shape world_shape) { return math_aabb_create(world_shape); }

void rohr_system_physics_update(double dt) { system_physics_update(dt); }
Tick rohr_system_tick_update(void) { return system_tick_update(); }
void rohr_system_entities_past_lifetime_clean(void) { system_entities_past_lifetime_clean(); }

void rohr_controller_key_states_update(KeyboardState *keyboard) { controller_key_states_update(keyboard); }
void rohr_controller_key_event_add(KeyboardState *keyboard, KeyboardEvent key_event) { controller_key_event_add(keyboard, key_event); }
KeyboardEvent rohr_controller_keyboard_event_capture(const SDL_Event *sdl_event) { return controller_keyboard_event_capture(sdl_event); }
bool rohr_controller_key_down_get(const KeyboardState *keyboard, SDL_Keycode keycode) { return controller_key_down_get(keyboard, keycode); }
bool rohr_controller_key_pressed_get(const KeyboardState *keyboard, SDL_Keycode keycode) { return controller_key_pressed_get(keyboard, keycode); }
bool rohr_controller_key_released_get(const KeyboardState *keyboard, SDL_Keycode keycode) { return controller_key_released_get(keyboard, keycode); }
Vec2D rohr_controller_axis_from_keycodes_get(
        const KeyboardState *keyboard,
        SDL_Keycode up,
        SDL_Keycode left,
        SDL_Keycode down,
        SDL_Keycode right
        ) {
    return controller_axis_from_keycodes_get(keyboard, up, left, down, right);
}
Vec2D rohr_controller_wasd_axis_get(const KeyboardState *keyboard) { return controller_wasd_axis_get(keyboard); }
Vec2D rohr_controller_arrow_axis_get(const KeyboardState *keyboard) { return controller_arrow_axis_get(keyboard); }
Controller rohr_controller_default_get(void) { return controller_default_get(); }
Controller rohr_controller_wasd_default_get(void) { return controller_wasd_default_get(); }
Controller rohr_controller_arrows_default_get(void) { return controller_arrows_default_get(); }
void rohr_controller_axis_binding_set(Controller *controller, ControllerAxisBinding binding) {
    controller_axis_binding_set(controller, binding);
}
Vec2D rohr_controller_default_axis_get(const KeyboardState *keyboard, const Controller *controller) {
    return controller_default_axis_get(keyboard, controller);
}
bool rohr_controller_axis_add(Controller *controller, const char *name, ControllerAxisBinding binding) {
    return controller_axis_add(controller, name, binding);
}
bool rohr_controller_button_add(Controller *controller, const char *name, SDL_Keycode keycode) {
    return controller_button_add(controller, name, keycode);
}
Vec2D rohr_controller_axis_get(
        const KeyboardState *keyboard,
        const Controller *controller,
        const char *name
        ) {
    return controller_axis_get(keyboard, controller, name);
}
bool rohr_controller_button_down_get(
        const KeyboardState *keyboard,
        const Controller *controller,
        const char *name
        ) {
    return controller_button_down_get(keyboard, controller, name);
}
bool rohr_controller_button_pressed_get(
        const KeyboardState *keyboard,
        const Controller *controller,
        const char *name
        ) {
    return controller_button_pressed_get(keyboard, controller, name);
}
bool rohr_controller_button_released_get(
        const KeyboardState *keyboard,
        const Controller *controller,
        const char *name
        ) {
    return controller_button_released_get(keyboard, controller, name);
}
void rohr_controller_mouse_event_print(MouseEvent event) { controller_mouse_event_print(event); }
void rohr_controller_mouse_states_update(MouseState *mouse) { controller_mouse_states_update(mouse); }
void rohr_controller_mouse_event_add(MouseState *mouse, MouseEvent mouse_event) { controller_mouse_event_add(mouse, mouse_event); }
MouseEvent rohr_controller_mouse_event_capture(const SDL_Event *sdl_event) { return controller_mouse_event_capture(sdl_event); }
Position rohr_controller_mouse_world_position_get(const MouseState *mouse) {
    if(mouse == NULL) {
        return (Position){0};
    }
    return graphics_screen_to_world_get(mouse->position);
}

void rohr_tools_delay(int seconds) { delay(seconds); }
void rohr_tools_binary_to_string(uint32_t value, char *buffer, size_t size) { binary_to_string(value, buffer, size); }
void rohr_tools_append_string(char *src, char *dst, size_t src_size, size_t dst_size) { tools_append_string(src, dst, src_size, dst_size); }
uint32_t rohr_tools_sizeof_string(char *str, char delimiter) { return tool_sizeof_string(str, delimiter); }
int rohr_tools_random_range(int min, int max) { return tools_random_range(min, max); }
float rohr_tools_random_range_float(float min, float max) { return tools_random_range_float(min, max); }

void rohr_ui_frame_begin(UIInput input) { ui_frame_begin(input); }
void rohr_ui_modal_set(UIRect bounds) { ui_modal_set(bounds); }
void rohr_ui_modal_controls_begin(void) { ui_modal_controls_begin(); }
void rohr_ui_modal_controls_end(void) { ui_modal_controls_end(); }
UIRect rohr_ui_component_bounds_get(UIRect bounds, const TextAsset *const *texts, size_t text_count, UIComponentConfig config) { return ui_component_bounds_get(bounds, texts, text_count, config); }
void rohr_ui_event_add(const SDL_Event *event) { ui_event_add(event); }
void rohr_ui_field_event_add(const SDL_Event *event) { ui_field_event_add(event); }
void rohr_ui_field_focus_clear(void) { ui_field_focus_clear(); }
UIFieldResult rohr_ui_field(const char *id, UIFieldBinding binding, TextAsset *display, UIRect bounds, const UIButtonStyle *style) { return ui_field(id, binding, display, bounds, style); }
UIFieldResult rohr_ui_multiline_field(const char *id, UIFieldBinding binding, TextAsset *display, UIRect bounds, const UIButtonStyle *style) { return ui_multiline_field(id, binding, display, bounds, style); }
UIButtonResult rohr_ui_button(const char *id, const TextAsset *label, UIRect bounds, const UIButtonStyle *style) { return ui_button(id, label, bounds, style); }
UIDropdownResult rohr_ui_dropdown(const char *id, const TextAsset *const *options, size_t option_count, size_t selected_index, UIRect bounds, const UIButtonStyle *style) { return ui_dropdown(id, options, option_count, selected_index, bounds, style); }
UIDropdownResult rohr_ui_menu(const char *id, const TextAsset *label, const TextAsset *const *options, size_t option_count, UIRect bounds, const UIButtonStyle *style) { return ui_menu(id, label, options, option_count, bounds, style); }
UIScrollRegionResult rohr_ui_scroll_region_begin(const char *id, UIRect bounds, float content_height, float offset, float wheel_step) { return ui_scroll_region_begin(id, bounds, content_height, offset, wheel_step); }
void rohr_ui_scroll_region_end(void) { ui_scroll_region_end(); }
UIButtonResult rohr_ui_interaction(const char *id, UIRect bounds) { return ui_interaction(id, bounds); }
void rohr_ui_surface(UIRect bounds, Color color) { ui_surface(bounds, color); }
void rohr_ui_border(UIRect bounds, float thickness, Color color) { ui_border(bounds, thickness, color); }
void rohr_ui_content(const TextAsset *text, UIRect bounds) { ui_content(text, bounds); }
void rohr_ui_quad(Position center, float width, float height, float angle, Color color) { ui_quad(center, width, height, angle, color); }
bool rohr_ui_clip_begin(UIRect bounds) { return ui_clip_begin(bounds); }
void rohr_ui_clip_end(void) { ui_clip_end(); }
bool rohr_ui_navigation_move(UINavigationDirection direction) { return ui_navigation_move(direction); }
bool rohr_ui_navigation_activate(void) { return ui_navigation_activate(); }
bool rohr_ui_navigation_focus_bounds_get(UIRect *bounds) { return ui_navigation_focus_bounds_get(bounds); }
void rohr_ui_label(const TextAsset *text, UIRect bounds) { ui_label(text, bounds); }
EngineResult rohr_ui_physics_debug_panel_init(UIPhysicsDebugPanel *panel, FontDescriptor font) { return ui_physics_debug_panel_init(panel, font); }
void rohr_ui_physics_debug_panel_draw(UIPhysicsDebugPanel *panel) { ui_physics_debug_panel_draw(panel); }
void rohr_ui_physics_debug_panel_destroy(UIPhysicsDebugPanel *panel) { ui_physics_debug_panel_destroy(panel); }
PhysicsDebugStats rohr_physics_debug_stats_get(void) { return physics_debug_stats_get(); }
void rohr_physics_debug_stats_enabled_set(bool enabled) { physics_debug_stats_enabled_set(enabled); }
void rohr_ui_button_disabled(UIRect bounds, const UIButtonStyle *style) { ui_button_disabled(bounds, style); }
bool rohr_ui_pointer_consumed_get(void) { return ui_pointer_consumed_get(); }
void rohr_ui_frame_end(void) { ui_frame_end(); }
UIButtonStyle rohr_ui_button_style_default_get(void) { return ui_button_style_default_get(); }
UISliderConfig rohr_ui_slider_config_default_get(void) { return ui_slider_config_default_get(); }
UISliderResult rohr_ui_slider(const char *id, float value, const UISliderConfig *config) { return ui_slider(id, value, config); }
UISliderResult rohr_ui_slider_with_text(const char *id, float value, const UISliderConfig *config, const UISliderText *text) { return ui_slider_with_text(id, value, config, text); }
