#include "rohr.h"
#include <stdarg.h>

void console_vwrite(LogSourceType source, const char *fmt, va_list args);
void console_debug_vwrite(LogSourceType source, const char *fmt, va_list args);

EngineResult rohr_engine_init(void) { return engine_init(); }
void rohr_engine_shutdown(void) { engine_shutdown(); }
void rohr_engine_update_time(void) { engine_update_time(); }
Time rohr_engine_time_get(void) { return engine_time_get(); }
Tick rohr_engine_tick_get(void) { return engine_tick_get(); }
void rohr_engine_pause(void) { engine_pause(); }
void rohr_engine_resume(void) { engine_resume(); }
Tick rohr_engine_update_tick(void) { return engine_update_tick(); }
EngineResult rohr_engine_time_per_tick_set(Time value) { return engine_time_per_tick_set(value); }
Time rohr_engine_time_per_tick_get(void) { return engine_time_per_tick_get(); }
SDL_Event rohr_engine_poll_event(void) { return engine_poll_event(); }
bool rohr_engine_paused_is(void) { return engine_paused_is(); }
void rohr_engine_reset_clock(void) { engine_reset_clock(); }

EngineResult rohr_error_result_value(bool value) { return error_result_value(value); }
EngineResult rohr_error_result_error(EngineError error) { return error_result_error(error); }
const char *rohr_error_default_message(EngineError error) { return error_default_message(error); }
const char *rohr_error_string(EngineError error) { return error_string(error); }
void rohr_error_print_stderr(EngineError error) { error_print_stderr(error); }

void rohr_console_print_logs(void) { console_print_logs(); }
void rohr_console_init(void) { console_init(); }
void rohr_console_shutdown(void) { console_shutdown(); }
bool rohr_console_read(ConsoleLogString *input) { return console_read(input); }
void rohr_console_write(LogSourceType source, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    console_vwrite(source, fmt, args);
    va_end(args);
}
bool rohr_console_active_is(void) { return console_active_is(); }
void rohr_console_debug_write(LogSourceType source, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    console_debug_vwrite(source, fmt, args);
    va_end(args);
}
void rohr_console_debug_set(bool state) { console_debug_set(state); }

bool rohr_entity_alive_is(Entity entity) { return entity_alive_is(entity); }
bool rohr_entity_index_alive_is(EntityIndex index) { return entity_index_alive_is(index); }
uint32_t rohr_entity_alive_count_get(void) { return entity_alive_count_get(); }
EntityResult rohr_entity_alive_at_get(uint32_t position) { return entity_alive_at_get(position); }
EntityIndexResult rohr_entity_index_get(Entity entity) {
    EntityIndex index;
    if(!entity_index_get(entity, &index)) {
        return ERROR_RESULT_MAKE_ERROR(EntityIndexResult, ERROR_ENGINE_INVALID_ENTITY);
    }
    return ERROR_RESULT_MAKE_VALUE(EntityIndexResult, index);
}
EntityResult rohr_entity_from_index(EntityIndex index) { return entity_from_index(index); }
EntityResult rohr_entity_add(void) { return entity_add(); }
EngineResult rohr_entity_name_set(Entity entity, const char *name) { return entity_name_set(entity, name); }
EntityResult rohr_entity_by_name_get(const char *name) { return entity_by_name_get(name); }
EntityNameResult rohr_entity_name_get(Entity entity) { return entity_name_get(entity); }
EngineResult rohr_game_state_load_file(const char *path) { return game_state_load_file(path); }
EngineResult rohr_game_state_load_files(const char *const *paths, size_t path_count) { return game_state_load_files(paths, path_count); }
UIButtonDefinitionResult rohr_ui_button_by_name_get(const char *name) { return ui_button_by_name_get(name); }
UIFontDefinitionResult rohr_ui_font_by_name_get(const char *name) { return ui_font_by_name_get(name); }
UILabelDefinitionResult rohr_ui_label_by_name_get(const char *name) { return ui_label_by_name_get(name); }
UISliderDefinitionResult rohr_ui_slider_by_name_get(const char *name) { return ui_slider_by_name_get(name); }
EngineResult rohr_game_state_save_file(const char *path) { return game_state_save_file(path); }
EngineResult rohr_game_state_save_template_file(const char *path) { return game_state_save_template_file(path); }
EngineResult rohr_entity_delete(Entity entity) { return entity_delete(entity); }
EngineResult rohr_entity_add_components(Entity entity, RohrComponentMask mask) { return entity_add_components(entity, mask); }
bool rohr_entity_components_has(Entity entity, RohrComponentMask components) { return entity_components_has(entity, components); }
bool rohr_entity_index_components_has(EntityIndex index, RohrComponentMask components) { return entity_index_components_has(index, components); }
GroupIdResult rohr_entity_group_create(void) { return entity_group_create(); }
EngineResult rohr_entity_group_name_set(GroupId group, const char *name) { return entity_group_name_set(group, name); }
GroupIdResult rohr_entity_group_by_name_get(const char *name) { return entity_group_by_name_get(name); }
GroupNameResult rohr_entity_group_name_get(GroupId group) { return entity_group_name_get(group); }
EngineResult rohr_entity_group_destroy(GroupId group) { return entity_group_destroy(group); }
EngineResult rohr_entity_group_add(GroupId group, Entity entity) { return entity_group_add(group, entity); }
EngineResult rohr_entity_group_remove(GroupId group, Entity entity) { return entity_group_remove(group, entity); }
bool rohr_entity_group_entity_has(GroupId group, Entity entity) { return entity_group_entity_has(group, entity); }
EntityGroupResult rohr_entity_group_get(GroupId group) { return entity_group_get(group); }
EntityGroupMembershipResult rohr_entity_groups_get(Entity entity) { return entity_groups_get(entity); }
EngineResult rohr_entity_delete_components(Entity entity, RohrComponentMask mask) { return entity_delete_components(entity, mask); }
EngineResult rohr_entity_child_set(Entity parent, Entity child) { return entity_child_set(parent, child); }
EngineResult rohr_entity_parent_set(Entity child, Entity parent) { return entity_parent_set(child, parent); }
EngineResult rohr_entity_remove_parent(Entity child) { return entity_remove_parent(child); }
EngineResult rohr_entity_remove_child(Entity parent, Entity child) { return entity_remove_child(parent, child); }
ChildrenResult rohr_entity_children_get(Entity entity) { return entity_children_get(entity); }
ParentResult rohr_entity_parent_get(Entity entity) { return entity_parent_get(entity); }
EngineResult rohr_entity_life_time_set(Entity entity, Time expirey_time, Tick expirey_tick) { return entity_life_time_set(entity, expirey_time, expirey_tick); }
EngineResult rohr_entity_remove_life_time(Entity entity) { return entity_remove_life_time(entity); }

EngineResult rohr_physics_dt_per_tick_set(Time dt) { return physics_dt_per_tick_set(dt); }
Time rohr_physics_dt_per_tick_get(void) { return physics_dt_per_tick_get(); }
void rohr_physics_use_engine_time_per_tick(void) { physics_use_engine_time_per_tick(); }
void rohr_physics_update(Tick ticks) { physics_update(ticks); }
void rohr_physics_update_dt(Time dt) { physics_update_dt(dt); }
Shape rohr_physics_shape_world_translate(Shape shape, Position position, Orientation angle) { return physics_shape_world_translate(shape, position, angle); }
float rohr_physics_polygon_moment_of_inertia(Shape shape, Mass mass_value) { return physics_polygon_moment_of_inertia(shape, mass_value); }
Collision rohr_physics_sat_collision(Shape shape_1, Shape shape_2) { return physics_sat_collision(shape_1, shape_2); }
Vec1D rohr_physics_circle_moment_of_inertia(Shape circle, Mass mass_value) { return physics_circle_moment_of_inertia(circle, mass_value); }
bool rohr_physics_entity_held_is(EntityIndex index) { return physics_entity_held_is(index); }
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
EngineResult rohr_physics_stop_entity(Entity entity) { return physics_stop_entity(entity); }
EngineResult rohr_physics_group_stop_entities(GroupId group) { return physics_group_stop_entities(group); }
EngineResult rohr_physics_apply_impulse(Entity entity, Vec2D impulse) { return physics_apply_impulse(entity, impulse); }
EngineResult rohr_physics_position_set(Entity entity, Position p) { return physics_position_set(entity, p); }
EngineResult rohr_physics_mass_set(Entity entity, Mass m) { return physics_mass_set(entity, m); }
EntityResult rohr_physics_force_create(Entity entity, Force f) { return physics_force_create(entity, f); }
EngineResult rohr_physics_force_component_set(Entity entity, Force force) {
    return physics_force_component_set(entity, force);
}
EngineResult rohr_physics_apply_force_for_one_tick(Entity entity, Force f) { return physics_apply_force_for_one_tick(entity, f); }
EntityResult rohr_physics_torque_create(Entity entity, Torque t) { return physics_torque_create(entity, t); }
EngineResult rohr_physics_torque_component_set(Entity entity, Torque torque) {
    return physics_torque_component_set(entity, torque);
}
EngineResult rohr_physics_apply_torque_for_one_tick(Entity entity, Torque t) { return physics_apply_torque_for_one_tick(entity, t); }
EngineResult rohr_physics_hitbox_set(Entity entity, Shape hitbox) { return physics_hitbox_set(entity, hitbox); }
CollisionFilterConfig rohr_physics_collision_filter_default_config(void) { return physics_collision_filter_default_config(); }
EngineResult rohr_physics_collision_filter_set(Entity entity, CollisionFilterConfig config) { return physics_collision_filter_set(entity, config); }
CollisionFilterConfigResult rohr_physics_collision_filter_get(Entity entity) { return physics_collision_filter_get(entity); }
EngineResult rohr_physics_collision_category_set(Entity entity, RohrCollisionCategoryMask category) { return physics_collision_category_set(entity, category); }
EngineResult rohr_physics_collision_with_set(Entity entity, RohrCollisionCategoryMask categories) { return physics_collision_with_set(entity, categories); }
EngineResult rohr_physics_collision_with_all_set(Entity entity) { return physics_collision_with_all_set(entity); }
EngineResult rohr_physics_collision_with_none_set(Entity entity) { return physics_collision_with_none_set(entity); }
bool rohr_physics_collision_between_is(Entity entity_1, Entity entity_2) { return physics_collision_between_is(entity_1, entity_2); }
EngineResult rohr_physics_orientation_set(Entity entity, Orientation angle) { return physics_orientation_set(entity, angle); }
EngineResult rohr_physics_angular_velocity_set(Entity entity, AngularVelocity v) { return physics_angular_velocity_set(entity, v); }
ShapeResult rohr_physics_global_hit_box_get(Entity entity) { return physics_global_hit_box_get(entity); }
EngineResult rohr_physics_restitution_set(Entity entity, Restitution restitution) { return physics_restitution_set(entity, restitution); }
EngineResult rohr_physics_dynamic_set(Entity entity) { return physics_dynamic_set(entity); }
EngineResult rohr_physics_static_set(Entity entity) { return physics_static_set(entity); }
EngineResult rohr_physics_hold_entity(Entity entity) { return physics_hold_entity(entity); }
EngineResult rohr_physics_unhold_entity(Entity entity) { return physics_unhold_entity(entity); }
EngineResult rohr_physics_group_hold_entities(GroupId group) { return physics_group_hold_entities(group); }
EngineResult rohr_physics_group_unhold_entities(GroupId group) { return physics_group_unhold_entities(group); }
EngineResult rohr_physics_angle_lock_set(Entity entity, Orientation min, Orientation max) { return physics_angle_lock_set(entity, min, max); }
EngineResult rohr_physics_axis_lock_set(Entity entity, Axis axis, Position axis_point) { return physics_axis_lock_set(entity, axis, axis_point); }
EngineResult rohr_physics_friction_set(Entity entity, float friction) { return physics_friction_set(entity, friction); }
EngineResult rohr_physics_transform_lock_set(Entity driven, Entity driver, Vec2D local_offset, Orientation local_angle, bool lock_position, bool lock_orientation, bool inherit_velocity) {
    return physics_transform_lock_set(driven, driver, local_offset, local_angle, lock_position, lock_orientation, inherit_velocity);
}
EngineResult rohr_physics_remove_transform_lock(Entity entity) { return physics_remove_transform_lock(entity); }
EngineResult rohr_physics_transform_lock_current_transform_set(Entity driven, Entity driver, bool lock_position, bool lock_orientation, bool inherit_velocity) {
    return physics_transform_lock_current_transform_set(driven, driver, lock_position, lock_orientation, inherit_velocity);
}
EngineResult rohr_physics_target_set(Entity entity, Entity target) {
    return physics_target_set(entity, target);
}
EngineResult rohr_physics_joint_component_set(Entity entity, Joint joint) {
    return physics_joint_component_set(entity, joint);
}
JointAnchorIdResult rohr_physics_joint_anchor_create(Entity entity, Vec2D centroid_offset) { return physics_joint_anchor_create(entity, centroid_offset); }
JointAnchorListResult rohr_physics_joint_anchors_get(Entity entity) { return physics_joint_anchors_get(entity); }
JointAnchorPositionResult rohr_physics_joint_anchor_position_get(JointAnchorId anchor) { return physics_joint_anchor_position_get(anchor); }
JointAnchorPositionResult rohr_physics_joint_anchor_world_position_get(JointAnchorId anchor) { return physics_joint_anchor_world_position_get(anchor); }
EngineResult rohr_physics_joint_anchor_position_set(JointAnchorId anchor, Vec2D centroid_offset) { return physics_joint_anchor_position_set(anchor, centroid_offset); }
EngineResult rohr_physics_joint_anchor_remove(JointAnchorId anchor) { return physics_joint_anchor_remove(anchor); }
EngineResult rohr_physics_joint_pin_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b) { return physics_joint_pin_set(joint, anchor_a, anchor_b); }
EngineResult rohr_physics_joint_weld_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b) { return physics_joint_weld_set(joint, anchor_a, anchor_b); }
EngineResult rohr_physics_joint_spring_set(Entity joint, JointAnchorId anchor_a, JointAnchorId anchor_b, float rest_length, float stiffness, float damping) { return physics_joint_spring_set(joint, anchor_a, anchor_b, rest_length, stiffness, damping); }
EntityResult rohr_physics_joint_create(Entity a, Entity b, JointType type, Vec2D local_anchor_a, Vec2D local_anchor_b, float stiffness, float damping) {
    return physics_joint_create(a, b, type, local_anchor_a, local_anchor_b, stiffness, damping);
}
Collision rohr_physics_particle_collision(Shape shape_1, Shape shape_2) { return physics_particle_collision(shape_1, shape_2); }
EngineResult rohr_physics_collision_report_set(Entity entity, Entity target, bool state) { return physics_collision_report_set(entity, target, state); }
bool rohr_physics_collision_report_get(Entity entity, Entity target) { return physics_collision_report_get(entity, target); }

Color rohr_graphics_create_color_hex(uint32_t hex_color_code) { return graphics_creat_color_hex(hex_color_code); }
EngineResult rohr_graphics_start(void) { return graphics_start(); }
void rohr_graphics_end(void) { graphics_end(); }
bool rohr_graphics_poll_events(SDL_Event *event) { return graphics_poll_events(event); }
void rohr_graphics_draw_background(Color color) { graphics_draw_background(color); }
bool rohr_graphics_draw_screen_rect(float x, float y, float width, float height, Color color) { return graphics_draw_screen_rect(x, y, width, height, color); }
bool rohr_graphics_draw_screen_quad(Position center, float width, float height, float angle, Color color) { return graphics_draw_screen_quad(center, width, height, angle, color); }
void rohr_graphics_show(void) { graphics_show(); }
void rohr_graphics_draw_hit_box(Entity entity, Fill fill_type) { graphics_draw_hit_box(entity, fill_type); }
void rohr_graphics_draw_hit_box_colored(Entity entity, Fill fill_type, Color color) { graphics_draw_hit_box_colored(entity, fill_type, color); }
void rohr_graphics_draw_hit_boxes(void) { graphics_draw_hit_boxes(); }
bool rohr_graphics_draw_joint(Entity joint, Color color) { return graphics_draw_joint(joint, color); }
void rohr_graphics_draw_joints(Color color) { graphics_draw_joints(color); }
TextureAssetResult rohr_graphics_load_texture(TextureDescriptor text_desc) { return graphics_load_texture(text_desc); }
FontAssetResult rohr_graphics_load_font(FontDescriptor descriptor) { return graphics_load_font(descriptor); }
void rohr_graphics_destroy_font(FontAsset *font) { graphics_destroy_font(font); }
TextAssetResult rohr_graphics_create_text(const FontAsset *font, const char *value, Color color) { return graphics_create_text(font, value, color); }
void rohr_graphics_destroy_text(TextAsset *text) { graphics_destroy_text(text); }
bool rohr_graphics_draw_text(const TextAsset *text, Position position) { return graphics_draw_text(text, position); }
AnimationAssetResult rohr_graphics_load_animation(AnimationDescriptor anim_desc) { return graphics_load_animation(anim_desc); }
AnimatedSprite rohr_graphics_create_animated_sprite(AnimationAsset asset_ptr, Scale scale) { return graphics_create_animated_sprite(asset_ptr, scale); }
EngineResult rohr_graphics_add_animated_sprite(Entity entity, AnimatedSprite sprite) { return graphics_add_animated_sprite(entity, sprite); }
void rohr_graphics_draw_animated_sprites(void) { graphics_draw_animated_sprites(); }
void rohr_graphics_update_sprite_frames(Tick current_tick, Time current_time) { graphics_update_sprite_frames(current_tick, current_time); }
void rohr_graphics_scale_textures(Entity entity, Scale scale) { graphics_scale_textures(entity, scale); }
void rohr_graphics_active_camera_set(Camera camera) { graphics_active_camera_set(camera); }
Camera rohr_graphics_active_camera_get(void) { return graphics_active_camera_get(); }
void rohr_graphics_move_camera(Vec2D translation) { graphics_move_camera(translation); }
void rohr_graphics_rotate_camera(Orientation radians) { graphics_rotate_camera(radians); }
EngineResult rohr_graphics_attach_camera(Entity entity, Vec2D position_offset, Orientation orientation_offset) { return graphics_attach_camera(entity, position_offset, orientation_offset); }
EngineResult rohr_graphics_attach_camera_with_options(Entity entity, Vec2D position_offset, Orientation orientation_offset, bool follow_position, bool follow_orientation) { return graphics_attach_camera_with_options(entity, position_offset, orientation_offset, follow_position, follow_orientation); }
void rohr_graphics_detach_camera(void) { graphics_detach_camera(); }
bool rohr_graphics_camera_attached_is(void) { return graphics_camera_attached_is(); }
CameraAttachmentResult rohr_graphics_camera_attachment_get(void) {
    CameraAttachment attachment;
    if(!graphics_camera_attachment_get(&attachment)) {
        return ERROR_RESULT_MAKE_ERROR(CameraAttachmentResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(CameraAttachmentResult, attachment);
}
CameraConfig rohr_camera_default_config(void) { return graphics_camera_default_config(); }
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
    return graphics_camera_attach_to(
        camera_id,
        entity,
        position_offset,
        orientation_offset,
        follow_position,
        follow_orientation
    );
}
EngineResult rohr_camera_detach(CameraId camera_id) { return graphics_camera_detach_from(camera_id); }
EngineResult rohr_camera_render_callback_set(CameraId camera, CameraRenderCallback callback, void *context) { return graphics_camera_render_callback_set(camera, callback, context); }
EngineResult rohr_camera_enable_set(CameraId camera) { return graphics_camera_enable_set(camera); }
EngineResult rohr_camera_disable_set(CameraId camera) { return graphics_camera_disable_set(camera); }
EngineResult rohr_camera_pause_with_engine_set(CameraId camera) { return graphics_camera_pause_with_engine_set(camera); }
EngineResult rohr_camera_render_when_paused_set(CameraId camera) { return graphics_camera_render_when_paused_set(camera); }
EngineResult rohr_camera_position_move(CameraId camera, Vec2D translation, Time duration) { return graphics_camera_position_move(camera, translation, duration); }
EngineResult rohr_camera_position_set(CameraId camera, Position position, Time duration) { return graphics_camera_position_set(camera, position, duration); }
EngineResult rohr_camera_position_from_entity_set(CameraId camera, Entity entity, Time duration) { return graphics_camera_position_from_entity_set(camera, entity, duration); }
EngineResult rohr_camera_entity_attachment_set(CameraId camera, Entity entity) { return graphics_camera_entity_attachment_set(camera, entity); }
EngineResult rohr_camera_moving_is(CameraId camera) { return graphics_camera_moving_is(camera); }
EngineResult rohr_camera_zoom_set(CameraId camera, float zoom, Time duration) { return graphics_camera_zoom_set(camera, zoom, duration); }
CameraZoomResult rohr_camera_zoom_get(CameraId camera) { return graphics_camera_zoom_get(camera); }
ViewportConfig rohr_viewport_default_config(void) { return graphics_viewport_default_config(); }
ViewportIdResult rohr_viewport_create(ViewportConfig config) { return graphics_viewport_create(config); }
EngineResult rohr_viewport_destroy(ViewportId viewport) { return graphics_viewport_destroy(viewport); }
EngineResult rohr_viewport_camera_set(ViewportId viewport, CameraId camera) { return graphics_viewport_camera_set(viewport, camera); }
EngineResult rohr_viewport_clear_camera(ViewportId viewport) { return graphics_viewport_clear_camera(viewport); }
EngineResult rohr_viewport_enable_set(ViewportId viewport) { return graphics_viewport_enable_set(viewport); }
EngineResult rohr_viewport_disable_set(ViewportId viewport) { return graphics_viewport_disable_set(viewport); }
Position rohr_graphics_world_to_screen(Position pos) { return graphics_world_to_screen(pos); }
Position rohr_graphics_screen_to_world(Position screen) { return graphics_screen_to_world(screen); }
Position rohr_graphics_mouse_screen_position_get(void) { return graphics_mouse_screen_position_get(); }
void rohr_graphics_draw_grid(void) { graphics_draw_grid(); }
bool rohr_graphics_recording_start(const char *output_path, int fps) { return graphics_recording_start(output_path, fps); }
void rohr_graphics_draw_particles(void) { graphics_draw_particles(); }
void rohr_graphics_draw_local_origins(void) { graphics_draw_local_origins(); }

Vec2DList rohr_math_create_normals(Shape shape) { return math_create_normals(shape); }
Vec2D rohr_math_normalize_vector(Vec2D vector) { return math_normalize_vector(vector); }
Vec2DList rohr_math_normalize_vectors(Vec2DList vectors) { return math_normalize_vectors(vectors); }
float rohr_math_dot_product(Vec2D vector_1, Vec2D vector_2) { return math_dot_product(vector_1, vector_2); }
Shape rohr_math_create_square(float width, float height) { return math_create_square(width, height); }
Shape rohr_math_create_circle(float radius, uint8_t verticies) { return math_create_circle(radius, verticies); }
Projection rohr_math_project_shape_on_axis(Shape shape, Axis axis) { return math_project_shape_on_axis(shape, axis); }
float rohr_math_cross_2d(Vec2D a, Vec2D b) { return math_cross_2d(a, b); }
Vec2D rohr_math_angular_velocity_cross_vec(float omega, Vec2D r) { return math_angular_velocity_cross_vec(omega, r); }
Vec2D rohr_math_project_onto_axis(Vec2D v, Axis axis) { return math_project_onto_axis(v, axis); }
float rohr_math_axis_magnitude(Axis axis) { return math_axis_magnitude(axis); }
float rohr_math_vector_magnitude(Vec2D vector) { return math_vector_magnitude(vector); }
Vec2D rohr_math_rotate_vector(Vec2D vector, float angle) { return math_rotate_vector(vector, angle); }
Vec1D rohr_math_circle_radius(Shape circle, Vec2D centroid) { return math_circle_radius(circle, centroid); }
Vec2D rohr_math_vector_subtract(Vec2D vector_a, Vec2D vector_b) { return math_vector_subtract(vector_a, vector_b); }
Vec1D rohr_math_circle_overlap_depth(Vec2D centroid_1, Vec1D radius_1, Vec2D centroid_2, Vec1D radius_2) { return math_circle_overlap_depth(centroid_1, radius_1, centroid_2, radius_2); }
float rohr_math_projection_overlap(Projection projection_1, Projection projection_2) { return math_projection_overlap(projection_1, projection_2); }
Shape rohr_math_scale_shape(Shape shape, float scale) { return math_scale_shape(shape, scale); }
Shape rohr_math_scale_shape_y(Shape shape, float scale) { return math_scale_shape_y(shape, scale); }
Shape rohr_math_scale_shape_x(Shape shape, float scale) { return math_scale_shape_x(shape, scale); }
Vec2D rohr_math_polygon_centroid(Shape shape) { return math_polygon_centroid(shape); }
Shape rohr_math_add_vertex(Shape shape) { return math_add_vertex(shape); }
Shape rohr_math_delete_vertex(Shape shape) { return math_delete_vertex(shape); }
AABB rohr_math_create_aabb(Shape world_shape) { return math_create_aabb(world_shape); }

void rohr_system_update_physics(double dt) { system_update_physics(dt); }
void rohr_system_clean_entities_past_lifetime(void) { system_clean_entities_past_lifetime(); }

void rohr_controller_update_key_states(KeyboardState *keyboard) { update_key_states(keyboard); }
void rohr_controller_add_key_event(KeyboardState *keyboard, KeyboardEvent key_event) { add_key_event(keyboard, key_event); }
KeyboardEvent rohr_controller_capture_keyboard_event(const SDL_Event *sdl_event) { return capture_keyboard_event(sdl_event); }
bool rohr_controller_key_down_is(const KeyboardState *keyboard, SDL_Keycode keycode) { return controller_key_down_is(keyboard, keycode); }
bool rohr_controller_key_pressed_is(const KeyboardState *keyboard, SDL_Keycode keycode) { return controller_key_pressed_is(keyboard, keycode); }
bool rohr_controller_key_released_is(const KeyboardState *keyboard, SDL_Keycode keycode) { return controller_key_released_is(keyboard, keycode); }
Vec2D rohr_controller_axis_from_keycodes(
        const KeyboardState *keyboard,
        SDL_Keycode up,
        SDL_Keycode left,
        SDL_Keycode down,
        SDL_Keycode right
        ) {
    return controller_axis_from_keycodes(keyboard, up, left, down, right);
}
Vec2D rohr_controller_wasd_axis_get(const KeyboardState *keyboard) { return controller_wasd_axis_get(keyboard); }
Vec2D rohr_controller_arrow_axis_get(const KeyboardState *keyboard) { return controller_arrow_axis_get(keyboard); }
Controller rohr_controller_default(void) { return controller_default(); }
Controller rohr_controller_default_wasd(void) { return controller_default_wasd(); }
Controller rohr_controller_default_arrows(void) { return controller_default_arrows(); }
void rohr_controller_axis_binding_set(Controller *controller, ControllerAxisBinding binding) {
    controller_axis_binding_set(controller, binding);
}
Vec2D rohr_controller_default_axis_get(const KeyboardState *keyboard, const Controller *controller) {
    return controller_default_axis_get(keyboard, controller);
}
bool rohr_controller_add_axis(Controller *controller, const char *name, ControllerAxisBinding binding) {
    return controller_add_axis(controller, name, binding);
}
bool rohr_controller_add_button(Controller *controller, const char *name, SDL_Keycode keycode) {
    return controller_add_button(controller, name, keycode);
}
Vec2D rohr_controller_axis_get(
        const KeyboardState *keyboard,
        const Controller *controller,
        const char *name
        ) {
    return controller_axis_get(keyboard, controller, name);
}
bool rohr_controller_button_down_is(
        const KeyboardState *keyboard,
        const Controller *controller,
        const char *name
        ) {
    return controller_button_down_is(keyboard, controller, name);
}
bool rohr_controller_button_pressed_is(
        const KeyboardState *keyboard,
        const Controller *controller,
        const char *name
        ) {
    return controller_button_pressed_is(keyboard, controller, name);
}
bool rohr_controller_button_released_is(
        const KeyboardState *keyboard,
        const Controller *controller,
        const char *name
        ) {
    return controller_button_released_is(keyboard, controller, name);
}
void rohr_controller_print_mouse_event(MouseEvent event) { print_mouse_event(event); }
void rohr_controller_update_mouse_states(MouseState *mouse) { update_mouse_states(mouse); }
void rohr_controller_add_mouse_event(MouseState *mouse, MouseEvent mouse_event) { add_mouse_event(mouse, mouse_event); }
MouseEvent rohr_controller_capture_mouse_event(const SDL_Event *sdl_event) { return capture_mouse_event(sdl_event); }
Position rohr_controller_mouse_world_position_get(const MouseState *mouse) {
    if(mouse == NULL) {
        return (Position){0};
    }
    return graphics_screen_to_world(mouse->position);
}

void rohr_grid_add_entity_to_grids(Entity entity) { add_entity_to_grids(entity); }
bool rohr_grid_pair_checked_is(Entity entity_1, Entity entity_2) { return grid_pair_checked_is(entity_1, entity_2); }
void rohr_grid_add_pair(Entity entity_1, Entity entity_2) { add_pair(entity_1, entity_2); }
void rohr_grid_clear(void) { clear_grid(); }
void rohr_grid_update_aabb(Entity entity) { grid_update_aabb(entity); }

void rohr_tools_delay(int seconds) { delay(seconds); }
void rohr_tools_binary_to_string(uint32_t value, char *buffer, size_t size) { binary_to_string(value, buffer, size); }
void rohr_tools_append_string(char *src, char *dst, size_t src_size, size_t dst_size) { tools_append_string(src, dst, src_size, dst_size); }
uint32_t rohr_tools_sizeof_string(char *str, char delimiter) { return tool_sizeof_string(str, delimiter); }
int rohr_tools_random_range(int min, int max) { return tools_random_range(min, max); }
float rohr_tools_random_range_float(float min, float max) { return tools_random_range_float(min, max); }

void rohr_ui_begin_frame(UIInput input) { ui_begin_frame(input); }
UIButtonResult rohr_ui_button(const char *id, const TextAsset *label, UIRect bounds, const UIButtonStyle *style) { return ui_button(id, label, bounds, style); }
void rohr_ui_label(const TextAsset *text, UIRect bounds) { ui_label(text, bounds); }
void rohr_ui_button_disabled(UIRect bounds, const UIButtonStyle *style) { ui_button_disabled(bounds, style); }
bool rohr_ui_pointer_consumed_is(void) { return ui_pointer_consumed_is(); }
void rohr_ui_end_frame(void) { ui_end_frame(); }
UIButtonStyle rohr_ui_default_button_style(void) { return ui_default_button_style(); }
UISliderConfig rohr_ui_default_slider_config(void) { return ui_default_slider_config(); }
UISliderResult rohr_ui_slider(const char *id, float value, const UISliderConfig *config) { return ui_slider(id, value, config); }
UISliderResult rohr_ui_slider_with_text(const char *id, float value, const UISliderConfig *config, const UISliderText *text) { return ui_slider_with_text(id, value, config, text); }
