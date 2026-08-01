#include "rohr.h"
#include <stdarg.h>

void console_vwrite(LogSourceType source, const char *fmt, va_list args);
void console_debug_vwrite(LogSourceType source, const char *fmt, va_list args);

EngineResult rohr_engine_init(void) { return engine_init(); }
void rohr_engine_shutdown(void) { engine_shutdown(); }
void rohr_engine_update_time(void) { engine_update_time(); }
Time rohr_engine_get_time(void) { return engine_get_time(); }
Tick rohr_engine_get_tick(void) { return engine_get_tick(); }
void rohr_engine_pause(void) { engine_pause(); }
void rohr_engine_resume(void) { engine_resume(); }
void rohr_engine_update_tick(void) { engine_update_tick(); }
SDL_Event rohr_engine_poll_event(void) { return engine_poll_event(); }
bool rohr_engine_is_paused(void) { return engine_is_paused(); }
Time rohr_engine_get_dt(void) { return engine_get_dt(); }
void rohr_engine_calculate_dt(void) { engine_calculate_dt(); }
void rohr_engine_set_dt(Time dt) { engine_set_dt(dt); }
void rohr_engine_reset_clock(void) { engine_reset_clock(); }

EngineResult rohr_error_result_value(bool value) { return error_result_value(value); }
EngineResult rohr_error_result_error(EngineError error) { return error_result_error(error); }
const char *rohr_error_default_message(EngineError error) { return error_default_message(error); }
const char *rohr_error_string(EngineError error) { return error_string(error); }

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
bool rohr_console_is_active(void) { return console_is_active(); }
void rohr_console_debug_write(LogSourceType source, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    console_debug_vwrite(source, fmt, args);
    va_end(args);
}
void rohr_console_set_debug(bool state) { console_set_debug(state); }

bool rohr_entity_is_alive(Entity entity) { return entity_is_alive(entity); }
bool rohr_entity_index_is_alive(EntityIndex index) { return entity_index_is_alive(index); }
uint32_t rohr_entity_alive_count(void) { return entity_alive_count(); }
EntityResult rohr_entity_alive_at(uint32_t position) { return entity_alive_at(position); }
bool rohr_entity_get_index(Entity entity, EntityIndex *index) { return entity_get_index(entity, index); }
EntityResult rohr_entity_from_index(EntityIndex index) { return entity_from_index(index); }
EntityResult rohr_entity_add(void) { return entity_add(); }
EngineResult rohr_entity_set_name(Entity entity, const char *name) { return entity_set_name(entity, name); }
EntityResult rohr_entity_find_by_name(const char *name) { return entity_find_by_name(name); }
EntityNameResult rohr_entity_get_name(Entity entity) { return entity_get_name(entity); }
EngineResult rohr_game_state_load_file(const char *path) { return game_state_load_file(path); }
EngineResult rohr_game_state_load_files(const char *const *paths, size_t path_count) { return game_state_load_files(paths, path_count); }
UIButtonDefinitionResult rohr_ui_button_find_by_name(const char *name) { return ui_button_find_by_name(name); }
UIFontDefinitionResult rohr_ui_font_find_by_name(const char *name) { return ui_font_find_by_name(name); }
UILabelDefinitionResult rohr_ui_label_find_by_name(const char *name) { return ui_label_find_by_name(name); }
UISliderDefinitionResult rohr_ui_slider_find_by_name(const char *name) { return ui_slider_find_by_name(name); }
EngineResult rohr_game_state_save_file(const char *path) { return game_state_save_file(path); }
EngineResult rohr_game_state_save_template_file(const char *path) { return game_state_save_template_file(path); }
EngineResult rohr_entity_delete(Entity entity) { return entity_delete(entity); }
EngineResult rohr_entity_add_components(Entity entity, CMask mask) { return entity_add_components(entity, mask); }
bool rohr_entity_has_components(Entity entity, CMask components) { return entity_has_components(entity, components); }
bool rohr_entity_index_has_components(EntityIndex index, CMask components) { return entity_index_has_components(index, components); }
GroupIdResult rohr_entity_group_create(void) { return entity_group_create(); }
EngineResult rohr_entity_group_set_name(GroupId group, const char *name) { return entity_group_set_name(group, name); }
GroupIdResult rohr_entity_group_find_by_name(const char *name) { return entity_group_find_by_name(name); }
GroupNameResult rohr_entity_group_get_name(GroupId group) { return entity_group_get_name(group); }
EngineResult rohr_entity_group_destroy(GroupId group) { return entity_group_destroy(group); }
EngineResult rohr_entity_group_add(GroupId group, Entity entity) { return entity_group_add(group, entity); }
EngineResult rohr_entity_group_remove(GroupId group, Entity entity) { return entity_group_remove(group, entity); }
bool rohr_entity_group_contains(GroupId group, Entity entity) { return entity_group_contains(group, entity); }
EntityGroupResult rohr_entity_group_get(GroupId group) { return entity_group_get(group); }
EntityGroupMembershipResult rohr_entity_get_groups(Entity entity) { return entity_get_groups(entity); }
EngineResult rohr_entity_delete_components(Entity entity, CMask mask) { return entity_delete_components(entity, mask); }
EngineResult rohr_entity_set_child(Entity parent, Entity child) { return entity_set_child(parent, child); }
EngineResult rohr_entity_set_parent(Entity child, Entity parent) { return entity_set_parent(child, parent); }
EngineResult rohr_entity_remove_parent(Entity child) { return entity_remove_parent(child); }
EngineResult rohr_entity_remove_child(Entity parent, Entity child) { return entity_remove_child(parent, child); }
ChildrenResult rohr_entity_get_children(Entity entity) { return entity_get_children(entity); }
ParentResult rohr_entity_get_parent(Entity entity) { return entity_get_parent(entity); }
EngineResult rohr_entity_set_life_time(Entity entity, Time expirey_time, Tick expirey_tick) { return entity_set_life_time(entity, expirey_time, expirey_tick); }
EngineResult rohr_entity_remove_life_time(Entity entity) { return entity_remove_life_time(entity); }

Shape rohr_physics_shape_world_translate(Shape shape, Position position, Orientation angle) { return physics_shape_world_translate(shape, position, angle); }
float rohr_physics_polygon_moment_of_inertia(Shape shape, Mass mass_value) { return physics_polygon_moment_of_inertia(shape, mass_value); }
Collision rohr_physics_sat_collision(Shape shape_1, Shape shape_2) { return physics_sat_collision(shape_1, shape_2); }
Vec1D rohr_physics_circle_moment_of_inertia(Shape circle, Mass mass_value) { return physics_circle_moment_of_inertia(circle, mass_value); }
bool rohr_physics_entity_is_held(EntityIndex index) { return physics_entity_is_held(index); }
EngineResult rohr_physics_set_acceleration(Entity entity, Acceleration a) { return physics_set_acceleration(entity, a); }
EngineResult rohr_physics_set_angular_acceleration(Entity entity, AngularAcceleration acceleration) {
    return physics_set_angular_acceleration(entity, acceleration);
}
EngineResult rohr_physics_set_acceleration_toward_position(Entity entity, float acceleration_magnitude, Position position) {
    return physics_set_acceleration_toward_position(entity, acceleration_magnitude, position);
}
EngineResult rohr_physics_set_acceleration_toward_entity(Entity entity, float acceleration_magnitude, Entity target) {
    return physics_set_acceleration_toward_entity(entity, acceleration_magnitude, target);
}
EngineResult rohr_physics_set_acceleration_away_from_position(Entity entity, float acceleration_magnitude, Position position) {
    return physics_set_acceleration_away_from_position(entity, acceleration_magnitude, position);
}
EngineResult rohr_physics_set_acceleration_away_from_entity(Entity entity, float acceleration_magnitude, Entity target) {
    return physics_set_acceleration_away_from_entity(entity, acceleration_magnitude, target);
}
EngineResult rohr_physics_group_set_acceleration_toward_entity(GroupId group, float acceleration_magnitude, Entity target) {
    return physics_group_set_acceleration_toward_entity(group, acceleration_magnitude, target);
}
EngineResult rohr_physics_group_set_acceleration_away_from_entity(GroupId group, float acceleration_magnitude, Entity target) {
    return physics_group_set_acceleration_away_from_entity(group, acceleration_magnitude, target);
}
EngineResult rohr_physics_set_velocity(Entity entity, Velocity v) { return physics_set_velocity(entity, v); }
EngineResult rohr_physics_set_velocity_toward_position(Entity entity, float speed, Position position) {
    return physics_set_velocity_toward_position(entity, speed, position);
}
EngineResult rohr_physics_set_velocity_toward_entity(Entity entity, float speed, Entity target) {
    return physics_set_velocity_toward_entity(entity, speed, target);
}
EngineResult rohr_physics_set_velocity_away_from_position(Entity entity, float speed, Position position) {
    return physics_set_velocity_away_from_position(entity, speed, position);
}
EngineResult rohr_physics_set_velocity_away_from_entity(Entity entity, float speed, Entity target) {
    return physics_set_velocity_away_from_entity(entity, speed, target);
}
EngineResult rohr_physics_group_set_velocity_toward_entity(GroupId group, float speed, Entity target) {
    return physics_group_set_velocity_toward_entity(group, speed, target);
}
EngineResult rohr_physics_group_set_velocity_away_from_entity(GroupId group, float speed, Entity target) {
    return physics_group_set_velocity_away_from_entity(group, speed, target);
}
EngineResult rohr_physics_stop_entity(Entity entity) { return physics_stop_entity(entity); }
EngineResult rohr_physics_group_stop_entities(GroupId group) { return physics_group_stop_entities(group); }
EngineResult rohr_physics_apply_impulse(Entity entity, Vec2D impulse) { return physics_apply_impulse(entity, impulse); }
EngineResult rohr_physics_set_position(Entity entity, Position p) { return physics_set_position(entity, p); }
EngineResult rohr_physics_set_mass(Entity entity, Mass m) { return physics_set_mass(entity, m); }
EntityResult rohr_physics_set_force(Entity entity, Force f) { return physics_set_force(entity, f); }
EngineResult rohr_physics_set_force_component(Entity entity, Force force) {
    return physics_set_force_component(entity, force);
}
EngineResult rohr_physics_apply_force_for_one_tick(Entity entity, Force f) { return physics_apply_force_for_one_tick(entity, f); }
EntityResult rohr_physics_set_torque(Entity entity, Torque t) { return physics_set_torque(entity, t); }
EngineResult rohr_physics_set_torque_component(Entity entity, Torque torque) {
    return physics_set_torque_component(entity, torque);
}
EngineResult rohr_physics_apply_torque_for_one_tick(Entity entity, Torque t) { return physics_apply_torque_for_one_tick(entity, t); }
EngineResult rohr_physics_set_hitbox(Entity entity, Shape hitbox) { return physics_set_hitbox(entity, hitbox); }
EngineResult rohr_physics_set_orientation(Entity entity, Orientation angle) { return physics_set_orientation(entity, angle); }
EngineResult rohr_physics_set_angular_velocity(Entity entity, AngularVelocity v) { return physics_set_angular_velocity(entity, v); }
ShapeResult rohr_physics_get_global_hit_box(Entity entity) { return physics_get_global_hit_box(entity); }
EngineResult rohr_physics_set_restitution(Entity entity, Restitution restitution) { return physics_set_restitution(entity, restitution); }
EngineResult rohr_physics_set_dynamic(Entity entity) { return physics_set_dynamic(entity); }
EngineResult rohr_physics_set_static(Entity entity) { return physics_set_static(entity); }
EngineResult rohr_physics_hold_entity(Entity entity) { return physics_hold_entity(entity); }
EngineResult rohr_physics_unhold_entity(Entity entity) { return physics_unhold_entity(entity); }
EngineResult rohr_physics_group_hold_entities(GroupId group) { return physics_group_hold_entities(group); }
EngineResult rohr_physics_group_unhold_entities(GroupId group) { return physics_group_unhold_entities(group); }
EngineResult rohr_physics_set_angle_lock(Entity entity, Orientation min, Orientation max) { return physics_set_angle_lock(entity, min, max); }
EngineResult rohr_physics_set_axis_lock(Entity entity, Axis axis, Position axis_point) { return physics_set_axis_lock(entity, axis, axis_point); }
EngineResult rohr_physics_set_friction(Entity entity, float friction) { return physics_set_friction(entity, friction); }
EngineResult rohr_physics_set_transform_lock(Entity driven, Entity driver, Vec2D local_offset, Orientation local_angle, bool lock_position, bool lock_orientation, bool inherit_velocity) {
    return physics_set_transform_lock(driven, driver, local_offset, local_angle, lock_position, lock_orientation, inherit_velocity);
}
EngineResult rohr_physics_remove_transform_lock(Entity entity) { return physics_remove_transform_lock(entity); }
EngineResult rohr_physics_set_transform_lock_current_transform(Entity driven, Entity driver, bool lock_position, bool lock_orientation, bool inherit_velocity) {
    return physics_set_transform_lock_current_transform(driven, driver, lock_position, lock_orientation, inherit_velocity);
}
EngineResult rohr_physics_set_target(Entity entity, Entity target) {
    return physics_set_target(entity, target);
}
EngineResult rohr_physics_set_joint_component(Entity entity, Joint joint) {
    return physics_set_joint_component(entity, joint);
}
EntityResult rohr_physics_set_joint(Entity a, Entity b, JointType type, Vec2D local_anchor_a, Vec2D local_anchor_b, float stiffness, float damping) {
    return physics_set_joint(a, b, type, local_anchor_a, local_anchor_b, stiffness, damping);
}
Collision rohr_physics_particle_collision(Shape shape_1, Shape shape_2) { return physics_particle_collision(shape_1, shape_2); }
EngineResult rohr_physics_set_collision_report(Entity entity, Entity target, bool state) { return physics_set_collision_report(entity, target, state); }
bool rohr_physics_get_collision_report(Entity entity, Entity target) { return physics_get_collision_report(entity, target); }

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
void rohr_graphics_set_camera(Camera camera) { graphics_set_camera(camera); }
Camera rohr_graphics_get_camera(void) { return graphics_get_camera(); }
void rohr_graphics_move_camera(Vec2D translation) { graphics_move_camera(translation); }
void rohr_graphics_rotate_camera(Orientation radians) { graphics_rotate_camera(radians); }
EngineResult rohr_graphics_attach_camera(Entity entity, Vec2D position_offset, Orientation orientation_offset) { return graphics_attach_camera(entity, position_offset, orientation_offset); }
EngineResult rohr_graphics_attach_camera_with_options(Entity entity, Vec2D position_offset, Orientation orientation_offset, bool follow_position, bool follow_orientation) { return graphics_attach_camera_with_options(entity, position_offset, orientation_offset, follow_position, follow_orientation); }
void rohr_graphics_detach_camera(void) { graphics_detach_camera(); }
bool rohr_graphics_camera_is_attached(void) { return graphics_camera_is_attached(); }
bool rohr_graphics_get_camera_attachment(CameraAttachment *attachment) { return graphics_get_camera_attachment(attachment); }
Position rohr_graphics_world_to_screen(Position pos) { return graphics_world_to_screen(pos); }
Position rohr_graphics_screen_to_world(Position screen) { return graphics_screen_to_world(screen); }
Position rohr_graphics_get_mouse_screen_position(void) { return graphics_get_mouse_screen_position(); }
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

EngineResult rohr_level_editor_init(void) { return level_editor_init(); }
EngineResult rohr_level_editor_update(void) { return level_editor_update(); }

void rohr_controller_update_key_states(KeyboardState *keyboard) { update_key_states(keyboard); }
void rohr_controller_add_key_event(KeyboardState *keyboard, KeyboardEvent key_event) { add_key_event(keyboard, key_event); }
KeyboardEvent rohr_controller_capture_keyboard_event(const SDL_Event *sdl_event) { return capture_keyboard_event(sdl_event); }
bool rohr_controller_key_down(const KeyboardState *keyboard, SDL_Keycode keycode) { return controller_key_down(keyboard, keycode); }
bool rohr_controller_key_pressed(const KeyboardState *keyboard, SDL_Keycode keycode) { return controller_key_pressed(keyboard, keycode); }
bool rohr_controller_key_released(const KeyboardState *keyboard, SDL_Keycode keycode) { return controller_key_released(keyboard, keycode); }
Vec2D rohr_controller_axis_from_keycodes(
        const KeyboardState *keyboard,
        SDL_Keycode up,
        SDL_Keycode left,
        SDL_Keycode down,
        SDL_Keycode right
        ) {
    return controller_axis_from_keycodes(keyboard, up, left, down, right);
}
Vec2D rohr_controller_wasd_axis(const KeyboardState *keyboard) { return controller_wasd_axis(keyboard); }
Vec2D rohr_controller_arrow_axis(const KeyboardState *keyboard) { return controller_arrow_axis(keyboard); }
void rohr_controller_print_mouse_event(MouseEvent event) { print_mouse_event(event); }
void rohr_controller_update_mouse_states(MouseState *mouse) { update_mouse_states(mouse); }
void rohr_controller_add_mouse_event(MouseState *mouse, MouseEvent mouse_event) { add_mouse_event(mouse, mouse_event); }
MouseEvent rohr_controller_capture_mouse_event(const SDL_Event *sdl_event) { return capture_mouse_event(sdl_event); }
Position rohr_controller_mouse_world_position(const MouseState *mouse) {
    if(mouse == NULL) {
        return (Position){0};
    }
    return graphics_screen_to_world(mouse->position);
}

void rohr_grid_add_entity_to_grids(Entity entity) { add_entity_to_grids(entity); }
bool rohr_grid_checked_pair(Entity entity_1, Entity entity_2) { return checked_pair(entity_1, entity_2); }
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
bool rohr_ui_pointer_consumed(void) { return ui_pointer_consumed(); }
void rohr_ui_end_frame(void) { ui_end_frame(); }
UIButtonStyle rohr_ui_default_button_style(void) { return ui_default_button_style(); }
UISliderConfig rohr_ui_default_slider_config(void) { return ui_default_slider_config(); }
UISliderResult rohr_ui_slider(const char *id, float value, const UISliderConfig *config) { return ui_slider(id, value, config); }
UISliderResult rohr_ui_slider_with_text(const char *id, float value, const UISliderConfig *config, const UISliderText *text) { return ui_slider_with_text(id, value, config, text); }
