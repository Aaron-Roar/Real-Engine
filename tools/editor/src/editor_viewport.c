#include "editor_viewport.h"
#include "editor_layout.h"

#include <math.h>

static Position editor_view_origin;
static float editor_view_scale = 1.0f;

static void editor_view_transform_set(const EditorViewportState *state,
    const EditorObject *object) {
    Position center = {EDITOR_VIEWPORT_WIDTH * 0.5f,
        EDITOR_MENU_HEIGHT + (WINDOW_HEIGHT - EDITOR_MENU_HEIGHT) * 0.5f};

    editor_view_origin = center;
    editor_view_scale = state != NULL && state->camera_zoom > 0.0f ?
        state->camera_zoom : 1.0f;
    if(state != NULL) {
        editor_view_origin.x += state->camera_offset.x;
        editor_view_origin.y += state->camera_offset.y;
    }
    if(state != NULL && state->local_view && object != NULL &&
            state->mode != EDITOR_VIEWPORT_HIERARCHY) {
        editor_view_origin.x -= object->position.x * editor_view_scale;
        editor_view_origin.y += object->position.y * editor_view_scale;
    }
}

static Position editor_view_world_to_screen(Position world) {
    return (Position){editor_view_origin.x + world.x * editor_view_scale,
        editor_view_origin.y - world.y * editor_view_scale};
}

static Position editor_view_screen_to_world(Position screen) {
    return (Position){(screen.x - editor_view_origin.x) / editor_view_scale,
        (editor_view_origin.y - screen.y) / editor_view_scale};
}

static Position editor_hitbox_vertex_world_get(const EditorObject *object,
    const EditorRigidBody *body, const EditorHitbox *hitbox, uint32_t vertex) {
    float cosine = cosf(body->rotation);
    float sine = sinf(body->rotation);
    Position local = hitbox->vertices[vertex].position;
    return (Position){
        object->position.x + body->position.x + local.x * cosine - local.y * sine,
        object->position.y + body->position.y + local.x * sine + local.y * cosine
    };
}

static Position editor_soft_node_world_get(const EditorObject *object,
    const EditorSoftBody *body, const EditorSoftNode *node) {
    float cosine = cosf(body->rotation);
    float sine = sinf(body->rotation);
    return (Position){object->position.x + body->position.x +
            node->position.x * cosine - node->position.y * sine,
        object->position.y + body->position.y +
            node->position.x * sine + node->position.y * cosine};
}

static Position editor_anchor_world_get(const EditorObject *object,
    const EditorAnchor *anchor) {
    const EditorRigidBody *body = NULL;
    if(object == NULL || anchor == NULL) return (Position){0};
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        if(object->rigid_bodies[i].id == anchor->rigid_body) body = &object->rigid_bodies[i];
    }
    if(body != NULL && anchor->position_follows_body) {
        float cosine = cosf(body->rotation);
        float sine = sinf(body->rotation);
        return (Position){object->position.x + body->position.x +
                anchor->position.x * cosine - anchor->position.y * sine,
            object->position.y + body->position.y +
                anchor->position.x * sine + anchor->position.y * cosine};
    }
    return (Position){object->position.x + anchor->position.x,
        object->position.y + anchor->position.y};
}

static void editor_anchor_world_set(const EditorObject *object, EditorAnchor *anchor,
    const EditorRigidBody *body, Position world) {
    Position local = {world.x - object->position.x, world.y - object->position.y};
    if(body != NULL && anchor->position_follows_body) {
        float cosine = cosf(-body->rotation);
        float sine = sinf(-body->rotation);
        local.x -= body->position.x;
        local.y -= body->position.y;
        anchor->position = (Position){local.x * cosine - local.y * sine,
            local.x * sine + local.y * cosine};
    } else {
        anchor->position = local;
    }
}

static float editor_body_radius_get(const EditorRigidBody *body) {
    float radius = 30.0f;
    if(body == NULL) return radius;
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        for(uint32_t j = 0; j < body->hitboxes[i].vertex_count; j += 1) {
            Position point = body->hitboxes[i].vertices[j].position;
            float distance = sqrtf(point.x * point.x + point.y * point.y);
            if(distance > radius) radius = distance;
        }
    }
    return radius;
}

static float editor_soft_body_radius_get(const EditorSoftBody *body) {
    float radius = 30.0f;
    if(body == NULL) return radius;
    for(size_t i = 0; i < body->node_count; i += 1) {
        float distance = hypotf(body->nodes[i].position.x, body->nodes[i].position.y);
        if(distance > radius) radius = distance;
    }
    return radius;
}

static Position editor_soft_body_rotation_handle_get(const EditorObject *object,
    const EditorSoftBody *body) {
    float distance = editor_soft_body_radius_get(body) + 28.0f;
    return (Position){object->position.x + body->position.x +
            sinf(body->rotation) * distance,
        object->position.y + body->position.y - cosf(body->rotation) * distance};
}

static Position editor_body_rotation_handle_get(const EditorObject *object,
    const EditorRigidBody *body) {
    float distance = editor_body_radius_get(body) + 28.0f;
    return (Position){object->position.x + body->position.x + sinf(body->rotation) * distance,
        object->position.y + body->position.y - cosf(body->rotation) * distance};
}

static float editor_segment_distance_squared(Position point, Position start, Position end) {
    Vec2D edge = {end.x - start.x, end.y - start.y};
    float length_squared = edge.x * edge.x + edge.y * edge.y;
    float amount = length_squared <= 0.001f ? 0.0f :
        ((point.x - start.x) * edge.x + (point.y - start.y) * edge.y) / length_squared;
    Position nearest;
    Vec2D distance;
    if(amount < 0.0f) amount = 0.0f;
    if(amount > 1.0f) amount = 1.0f;
    nearest = (Position){start.x + edge.x * amount, start.y + edge.y * amount};
    distance = (Vec2D){point.x - nearest.x, point.y - nearest.y};
    return distance.x * distance.x + distance.y * distance.y;
}

static bool editor_soft_body_area_contains(const EditorObject *object,
    const EditorSoftBody *body, Position point) {
    bool inside = false;
    size_t previous;

    if(object == NULL || body == NULL || body->node_count < 3) return false;
    for(size_t i = 0; i < body->node_count; i += 1) {
        EditorSoftNodeId first = body->nodes[i].id;
        EditorSoftNodeId second = body->nodes[(i + 1) % body->node_count].id;
        bool connected = false;
        for(size_t beam = 0; beam < body->beam_count; beam += 1) {
            const EditorSoftBeam *edge = &body->beams[beam];
            if((edge->node_a == first && edge->node_b == second) ||
                    (edge->node_a == second && edge->node_b == first)) {
                connected = true;
                break;
            }
        }
        if(!connected) return false;
    }
    previous = body->node_count - 1;
    for(size_t i = 0; i < body->node_count; i += 1) {
        Position current = editor_soft_node_world_get(object, body, &body->nodes[i]);
        Position prior = editor_soft_node_world_get(object, body, &body->nodes[previous]);
        bool crosses = (current.y > point.y) != (prior.y > point.y) &&
            point.x < (prior.x - current.x) * (point.y - current.y) /
                (prior.y - current.y) + current.x;
        if(crosses) inside = !inside;
        previous = i;
    }
    return inside;
}

static void editor_line_draw(Position start, Position end, Color color) {
    start = editor_view_world_to_screen(start);
    end = editor_view_world_to_screen(end);
    Vec2D delta = {end.x - start.x, end.y - start.y};
    float length = sqrtf(delta.x * delta.x + delta.y * delta.y);

    if(length <= 0.0f) return;
    (void)rohr_graphics_screen_quad_draw(
        (Position){(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f},
        length, 2.0f, -atan2f(delta.y, delta.x), color);
}

static void editor_triangle_filled_draw(Position a, Position b, Position c, Color color) {
    Position points[3] = {
        editor_view_world_to_screen(a),
        editor_view_world_to_screen(b),
        editor_view_world_to_screen(c)
    };
    float minimum_y = fminf(points[0].y, fminf(points[1].y, points[2].y));
    float maximum_y = fmaxf(points[0].y, fmaxf(points[1].y, points[2].y));
    int first_row = (int)floorf(fmaxf(minimum_y, EDITOR_MENU_HEIGHT));
    int last_row = (int)ceilf(fminf(maximum_y, WINDOW_HEIGHT - 1.0f));

    for(int row = first_row; row <= last_row; row += 1) {
        float scan_y = (float)row + 0.5f;
        float intersections[3];
        size_t count = 0;
        for(size_t edge = 0; edge < 3; edge += 1) {
            Position start = points[edge];
            Position end = points[(edge + 1) % 3];
            float low = fminf(start.y, end.y);
            float high = fmaxf(start.y, end.y);
            if(scan_y < low || scan_y >= high || fabsf(end.y - start.y) <= 0.0001f) {
                continue;
            }
            intersections[count++] = start.x + (scan_y - start.y) *
                (end.x - start.x) / (end.y - start.y);
        }
        if(count >= 2) {
            float left = fmaxf(fminf(intersections[0], intersections[1]), 0.0f);
            float right = fminf(fmaxf(intersections[0], intersections[1]),
                EDITOR_VIEWPORT_WIDTH);
            if(right > left) {
                (void)rohr_graphics_screen_rect_draw(
                    left, (float)row, right - left, 1.0f, color);
            }
        }
    }
}

static void editor_quad_draw(Position center, float width, float height,
    float rotation, Color color) {
    (void)rohr_graphics_screen_quad_draw(editor_view_world_to_screen(center),
        width, height, -rotation, color);
}

static void editor_circle_draw(Position center, float radius, Color color) {
    Position previous = {center.x + radius, center.y};
    for(uint32_t i = 1; i <= 16; i += 1) {
        float angle = 6.28318530718f * (float)i / 16.0f;
        Position current = {center.x + cosf(angle) * radius,
            center.y + sinf(angle) * radius};
        editor_line_draw(previous, current, color);
        previous = current;
    }
}

static void editor_joint_symbol_draw(EditorJointKind kind, Position start,
    Position end, float scale, Color color) {
    Position center = {(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f};
    float radius = 9.0f * scale;

    if(kind == EDITOR_JOINT_REVOLUTE) {
        editor_circle_draw(center, radius, color);
        editor_quad_draw(center, 6.0f * scale, 6.0f * scale, 0.0f, color);
    } else if(kind == EDITOR_JOINT_WELD) {
        Position top_left = {center.x - radius, center.y - radius};
        Position top_right = {center.x + radius, center.y - radius};
        Position bottom_right = {center.x + radius, center.y + radius};
        Position bottom_left = {center.x - radius, center.y + radius};
        editor_line_draw(top_left, top_right, color);
        editor_line_draw(top_right, bottom_right, color);
        editor_line_draw(bottom_right, bottom_left, color);
        editor_line_draw(bottom_left, top_left, color);
        editor_line_draw(top_left, bottom_right, color);
        editor_line_draw(top_right, bottom_left, color);
    } else {
        Position points[10];
        Vec2D delta = {end.x - start.x, end.y - start.y};
        float length = sqrtf(delta.x * delta.x + delta.y * delta.y);
        if(length <= 0.001f) {
            editor_circle_draw(start, radius, color);
            return;
        }
        Vec2D perpendicular = {-delta.y / length, delta.x / length};
        for(uint32_t i = 0; i < 10; i += 1) {
            float amount = (float)i / 9.0f;
            float offset = i == 0 || i == 9 ? 0.0f :
                (i % 2 == 0 ? 7.0f * scale : -7.0f * scale);
            points[i] = (Position){start.x + delta.x * amount + perpendicular.x * offset,
                start.y + delta.y * amount + perpendicular.y * offset};
            if(i > 0) editor_line_draw(points[i - 1], points[i], color);
        }
    }
}

static void editor_body_origin_draw(const EditorObject *object,
    const EditorRigidBody *body) {
    Position center;
    Position x_end;
    Position y_end;
    const float axis_length = 16.0f;

    if(object == NULL || body == NULL) return;
    center = (Position){object->position.x + body->position.x,
        object->position.y + body->position.y};
    x_end = (Position){center.x + cosf(body->rotation) * axis_length,
        center.y + sinf(body->rotation) * axis_length};
    y_end = (Position){center.x - sinf(body->rotation) * axis_length,
        center.y + cosf(body->rotation) * axis_length};
    editor_line_draw(center, x_end, (Color){235, 95, 95, 255});
    editor_line_draw(center, y_end, (Color){95, 220, 135, 255});
    editor_circle_draw(center, 5.0f, (Color){245, 245, 250, 255});
    editor_quad_draw(center, 3.0f, 3.0f, 0.0f, (Color){245, 245, 250, 255});
}

static bool editor_hitbox_point_contains(const EditorObject *object,
    const EditorRigidBody *body, const EditorHitbox *hitbox, Position point) {
    bool inside = false;
    uint32_t previous;

    if(object == NULL || body == NULL || hitbox == NULL || hitbox->vertex_count < 3) {
        return false;
    }
    previous = hitbox->vertex_count - 1;
    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
        Position current = editor_hitbox_vertex_world_get(object, body, hitbox, i);
        Position prior = editor_hitbox_vertex_world_get(object, body, hitbox, previous);
        bool crosses = (current.y > point.y) != (prior.y > point.y) &&
            point.x < (prior.x - current.x) * (point.y - current.y) /
                (prior.y - current.y) + current.x;
        if(crosses) inside = !inside;
        previous = i;
    }
    return inside;
}

static EditorRigidBody *editor_selected_body_get(EditorObject *object,
    const EditorViewportState *state) {
    return object == NULL || state == NULL ? NULL :
        editor_project_rigid_body_get(object, state->selected_rigid_body);
}

static EditorHitbox *editor_selected_hitbox_get(EditorObject *object,
    const EditorViewportState *state) {
    EditorRigidBody *body = editor_selected_body_get(object, state);
    return body == NULL ? NULL : editor_project_hitbox_get(body, state->selected_hitbox);
}

void editor_viewport_state_init(EditorViewportState *state) {
    if(state == NULL) return;
    *state = (EditorViewportState){.dragged_vertex = -1, .camera_zoom = 1.0f};
}

void editor_viewport_hitbox_editor_enter(EditorViewportState *state) {
    if(state == NULL) return;
    state->mode = EDITOR_VIEWPORT_HITBOX;
    state->selection = EDITOR_SELECTION_HITBOX;
    state->dragged_vertex = -1;
}

void editor_viewport_object_editor_enter(EditorViewportState *state) {
    if(state == NULL) return;
    state->mode = EDITOR_VIEWPORT_OBJECT;
    state->selection = EDITOR_SELECTION_OBJECT;
    state->dragged_vertex = -1;
}

void editor_viewport_hitbox_editor_exit(EditorViewportState *state) {
    if(state == NULL) return;
    state->mode = EDITOR_VIEWPORT_HIERARCHY;
    state->selection = EDITOR_SELECTION_NONE;
    state->dragged_vertex = -1;
}

bool editor_viewport_hitbox_editor_active_get(const EditorViewportState *state) {
    return state != NULL && state->mode != EDITOR_VIEWPORT_HIERARCHY;
}

void editor_viewport_line_editor_enter(EditorViewportState *state, uint32_t line) {
    if(state == NULL) return;
    state->mode = EDITOR_VIEWPORT_LINE;
    state->selection = EDITOR_SELECTION_LINE;
    state->selected_line = line;
    state->dragged_vertex = -1;
}

void editor_viewport_vertex_editor_enter(EditorViewportState *state, uint32_t vertex) {
    if(state == NULL) return;
    state->mode = EDITOR_VIEWPORT_VERTEX;
    state->selection = EDITOR_SELECTION_VERTEX;
    state->selected_vertex = vertex;
    state->dragged_vertex = -1;
}

void editor_viewport_back(EditorViewportState *state) {
    if(state == NULL) return;
    if(state->mode == EDITOR_VIEWPORT_LINE || state->mode == EDITOR_VIEWPORT_VERTEX) {
        state->mode = EDITOR_VIEWPORT_HITBOX;
        state->selection = EDITOR_SELECTION_HITBOX;
    } else if(state->mode == EDITOR_VIEWPORT_HITBOX) {
        state->mode = EDITOR_VIEWPORT_RIGID_BODY;
        state->selection = EDITOR_SELECTION_RIGID_BODY;
    } else if(state->mode == EDITOR_VIEWPORT_ANCHOR) {
        state->mode = EDITOR_VIEWPORT_JOINT;
        state->selection = EDITOR_SELECTION_ANCHOR;
    } else if(state->mode == EDITOR_VIEWPORT_RIGID_BODY ||
            state->mode == EDITOR_VIEWPORT_JOINT ||
            state->mode == EDITOR_VIEWPORT_SOFT_BODY) {
        state->mode = EDITOR_VIEWPORT_OBJECT;
        state->selection = EDITOR_SELECTION_OBJECT;
    } else if(state->mode == EDITOR_VIEWPORT_SOFT_NODE ||
            state->mode == EDITOR_VIEWPORT_SOFT_BEAM ||
            state->mode == EDITOR_VIEWPORT_SOFT_AREA) {
        state->mode = EDITOR_VIEWPORT_SOFT_BODY;
        state->selection = EDITOR_SELECTION_SOFT_BODY;
    } else if(state->mode == EDITOR_VIEWPORT_ORIGIN) {
        state->mode = state->selected_origin_kind == EDITOR_ORIGIN_RIGID_BODY ?
            EDITOR_VIEWPORT_RIGID_BODY : EDITOR_VIEWPORT_SOFT_BODY;
        state->selection = state->selected_origin_kind == EDITOR_ORIGIN_RIGID_BODY ?
            EDITOR_SELECTION_RIGID_BODY : EDITOR_SELECTION_SOFT_BODY;
    } else if(state->mode == EDITOR_VIEWPORT_OBJECT) {
        state->mode = EDITOR_VIEWPORT_HIERARCHY;
        state->selection = EDITOR_SELECTION_OBJECT;
    }
    state->dragged_vertex = -1;
}

static const EditorSoftNode *editor_soft_node_get(const EditorSoftBody *body,
        EditorSoftNodeId id) {
    if(body == NULL) return NULL;
    for(size_t i = 0; i < body->node_count; i += 1) {
        if(body->nodes[i].id == id) return &body->nodes[i];
    }
    return NULL;
}

static bool editor_soft_area_point_contains(const EditorObject *object,
        const EditorSoftBody *body, const EditorSoftArea *area, Position point) {
    const EditorSoftNode *nodes[3] = {
        editor_soft_node_get(body, area->node_a),
        editor_soft_node_get(body, area->node_b),
        editor_soft_node_get(body, area->node_c)
    };
    Position world[3];
    float signs[3];
    if(nodes[0] == NULL || nodes[1] == NULL || nodes[2] == NULL) return false;
    for(size_t i = 0; i < 3; i += 1) {
        size_t next = (i + 1) % 3;
        world[i] = editor_soft_node_world_get(object, body, nodes[i]);
        world[next] = editor_soft_node_world_get(object, body, nodes[next]);
        signs[i] = (point.x - world[next].x) * (world[i].y - world[next].y) -
            (world[i].x - world[next].x) * (point.y - world[next].y);
    }
    return (signs[0] >= 0.0f && signs[1] >= 0.0f && signs[2] >= 0.0f) ||
        (signs[0] <= 0.0f && signs[1] <= 0.0f && signs[2] <= 0.0f);
}

bool editor_viewport_update(EditorViewportState *state, EditorProject *project,
    Position pointer, MouseButtonState primary_button,
    MouseButtonState pan_button, bool pan_modifier, float wheel_y,
    bool pointer_consumed) {
    EditorObject *object;
    EditorRigidBody *body;
    EditorHitbox *hitbox;

    if(state == NULL || project == NULL) return false;
    object = editor_project_selected_get(project);
    if(pan_button == MOUSE_BUTTON_STATE_RELEASED ||
            (state->camera_pan_with_primary &&
                primary_button == MOUSE_BUTTON_STATE_RELEASED)) {
        state->camera_panning = false;
        state->camera_pan_with_primary = false;
    }
    if(pointer_consumed || pointer.x < 0.0f ||
            pointer.x >= EDITOR_VIEWPORT_WIDTH) return false;
    editor_view_transform_set(state, object);
    if(wheel_y != 0.0f) {
        Position world = editor_view_screen_to_world(pointer);
        Position desired_origin;
        Position candidate_origin;
        float factor = powf(1.1f, wheel_y);
        state->camera_zoom = fminf(8.0f, fmaxf(0.1f,
            state->camera_zoom * factor));
        editor_view_transform_set(state, object);
        candidate_origin = editor_view_origin;
        desired_origin = (Position){pointer.x - world.x * editor_view_scale,
            pointer.y + world.y * editor_view_scale};
        state->camera_offset.x += desired_origin.x - candidate_origin.x;
        state->camera_offset.y += desired_origin.y - candidate_origin.y;
        editor_view_transform_set(state, object);
        return true;
    }
    if(pan_button == MOUSE_BUTTON_STATE_PRESSED ||
            (pan_modifier && primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        state->camera_panning = true;
        state->camera_pan_with_primary = pan_modifier &&
            primary_button == MOUSE_BUTTON_STATE_PRESSED;
        state->camera_pointer = pointer;
        return true;
    }
    if(state->camera_panning &&
            ((!state->camera_pan_with_primary && pan_button == MOUSE_BUTTON_STATE_DOWN) ||
            (state->camera_pan_with_primary && primary_button == MOUSE_BUTTON_STATE_DOWN))) {
        state->camera_offset.x += pointer.x - state->camera_pointer.x;
        state->camera_offset.y += pointer.y - state->camera_pointer.y;
        state->camera_pointer = pointer;
        return true;
    }
    pointer = editor_view_screen_to_world(pointer);
    if(state->mode == EDITOR_VIEWPORT_HIERARCHY &&
            primary_button == MOUSE_BUTTON_STATE_PRESSED) {
        for(size_t object_index = project->object_count; object_index > 0; object_index -= 1) {
            EditorObject *candidate_object = &project->objects[object_index - 1];
            if(!candidate_object->visible) continue;
            for(size_t body_index = 0; body_index < candidate_object->rigid_body_count;
                    body_index += 1) {
                EditorRigidBody *candidate_body =
                    &candidate_object->rigid_bodies[body_index];
                if(!candidate_body->visible) continue;
                for(size_t hitbox_index = 0; hitbox_index < candidate_body->hitbox_count;
                        hitbox_index += 1) {
                    EditorHitbox *candidate_hitbox =
                        &candidate_body->hitboxes[hitbox_index];
                    if(candidate_hitbox->visible && editor_hitbox_point_contains(
                            candidate_object, candidate_body, candidate_hitbox, pointer)) {
                        (void)editor_project_object_select(project, candidate_object->id);
                        editor_viewport_object_editor_enter(state);
                        return true;
                    }
                }
            }
        }
        return false;
    }
    if(object == NULL) return false;
    if(primary_button == MOUSE_BUTTON_STATE_RELEASED) {
        state->dragged_vertex = -1;
        state->dragged_body = false;
        state->rotated_body = false;
        state->dragged_anchor = false;
        state->dragged_soft_node = false;
        state->dragged_soft_body = false;
        state->rotated_soft_body = false;
        state->dragged_origin = false;
        return false;
    }
    body = editor_selected_body_get(object, state);
    hitbox = editor_selected_hitbox_get(object, state);
    if(state->dragged_anchor && (primary_button == MOUSE_BUTTON_STATE_DOWN ||
            primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        EditorAnchor *anchor = editor_project_anchor_get(object, state->selected_anchor);
        EditorRigidBody *anchor_body = anchor == NULL ? NULL :
            editor_project_rigid_body_get(object, anchor->rigid_body);
        if(anchor != NULL) editor_anchor_world_set(object, anchor, anchor_body,
            (Position){pointer.x - state->drag_offset.x, pointer.y - state->drag_offset.y});
        if(anchor != NULL) editor_project_anchor_constraints_apply(object, anchor->id);
        return true;
    }
    if(state->dragged_soft_node &&
            (primary_button == MOUSE_BUTTON_STATE_DOWN ||
                primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        EditorSoftBody *soft_body = NULL;
        for(size_t i = 0; i < object->soft_body_count; i += 1) {
            if(object->soft_body_items[i].id == state->selected_soft_body) {
                soft_body = &object->soft_body_items[i];
            }
        }
        if(soft_body != NULL) {
            for(size_t i = 0; i < soft_body->node_count; i += 1) {
                if(soft_body->nodes[i].id != state->selected_soft_node) continue;
                Position local = {
                    pointer.x - object->position.x - soft_body->position.x -
                        state->drag_offset.x,
                    pointer.y - object->position.y - soft_body->position.y -
                        state->drag_offset.y
                };
                float cosine = cosf(-soft_body->rotation);
                float sine = sinf(-soft_body->rotation);
                soft_body->nodes[i].position = (Position){
                    local.x * cosine - local.y * sine,
                    local.x * sine + local.y * cosine
                };
                break;
            }
        }
        return true;
    }
    if(state->dragged_soft_body && (primary_button == MOUSE_BUTTON_STATE_DOWN ||
            primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        for(size_t i = 0; i < object->soft_body_count; i += 1) {
            EditorSoftBody *soft_body = &object->soft_body_items[i];
            if(soft_body->id != state->selected_soft_body) continue;
            soft_body->position = (Position){
                pointer.x - object->position.x - state->drag_offset.x,
                pointer.y - object->position.y - state->drag_offset.y
            };
            return true;
        }
    }
    if(state->dragged_origin && (primary_button == MOUSE_BUTTON_STATE_DOWN ||
            primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        Position position = {pointer.x - object->position.x - state->drag_offset.x,
            pointer.y - object->position.y - state->drag_offset.y};
        if(state->selected_origin_kind == EDITOR_ORIGIN_RIGID_BODY) {
            EditorRigidBody *origin_body = editor_project_rigid_body_get(
                object, state->selected_rigid_body);
            return editor_project_rigid_body_origin_set(object, origin_body, position);
        }
        if(state->selected_origin_kind == EDITOR_ORIGIN_SOFT_BODY) {
            for(size_t i = 0; i < object->soft_body_count; i += 1) {
                if(object->soft_body_items[i].id == state->selected_soft_body)
                    return editor_project_soft_body_origin_set(
                        &object->soft_body_items[i], position);
            }
        }
        return false;
    }
    if(state->rotated_soft_body && (primary_button == MOUSE_BUTTON_STATE_DOWN ||
            primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        for(size_t i = 0; i < object->soft_body_count; i += 1) {
            EditorSoftBody *soft_body = &object->soft_body_items[i];
            Position center;
            if(soft_body->id != state->selected_soft_body) continue;
            center = (Position){object->position.x + soft_body->position.x,
                object->position.y + soft_body->position.y};
            soft_body->rotation = atan2f(pointer.y - center.y, pointer.x - center.x) +
                state->rotation_pointer_offset;
            return true;
        }
    }
    if(body != NULL && state->dragged_body && (primary_button == MOUSE_BUTTON_STATE_DOWN ||
            primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        body->position = (Position){pointer.x - object->position.x - state->drag_offset.x,
            pointer.y - object->position.y - state->drag_offset.y};
        editor_project_rigid_body_constraints_apply(object, body->id);
        return true;
    }
    if(body != NULL && state->rotated_body && (primary_button == MOUSE_BUTTON_STATE_DOWN ||
            primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        Position center = {object->position.x + body->position.x,
            object->position.y + body->position.y};
        body->rotation = atan2f(pointer.y - center.y, pointer.x - center.x) +
            state->rotation_pointer_offset;
        editor_project_rigid_body_constraints_apply(object, body->id);
        return true;
    }
    if(hitbox != NULL && state->dragged_vertex >= 0 &&
            (primary_button == MOUSE_BUTTON_STATE_DOWN ||
                primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        if(hitbox->vertices[state->dragged_vertex].position_locked) return true;
        {
            Position local = {pointer.x - object->position.x - body->position.x,
                pointer.y - object->position.y - body->position.y};
            float cosine = cosf(-body->rotation);
            float sine = sinf(-body->rotation);
            hitbox->vertices[state->dragged_vertex].position = (Position){
                local.x * cosine - local.y * sine, local.x * sine + local.y * cosine};
        }
        return true;
    }
    if(primary_button != MOUSE_BUTTON_STATE_PRESSED) return false;

    if(body != NULL && body->visible &&
            (state->mode == EDITOR_VIEWPORT_RIGID_BODY ||
            state->mode == EDITOR_VIEWPORT_HITBOX ||
            (state->mode == EDITOR_VIEWPORT_ORIGIN &&
                state->selected_origin_kind == EDITOR_ORIGIN_RIGID_BODY))) {
        Position center = {object->position.x + body->position.x,
            object->position.y + body->position.y};
        if((pointer.x - center.x) * (pointer.x - center.x) +
                (pointer.y - center.y) * (pointer.y - center.y) <= 100.0f) {
            Uint64 now = SDL_GetTicks();
            bool editing = state->mode == EDITOR_VIEWPORT_ORIGIN &&
                state->selected_origin_kind == EDITOR_ORIGIN_RIGID_BODY;
            bool double_clicked = state->last_viewport_click_selection ==
                    EDITOR_SELECTION_ORIGIN &&
                state->last_viewport_click_object == object->id &&
                state->last_viewport_click_index == body->id &&
                now - state->last_viewport_click_at <= 400;
            if(editing || double_clicked) {
                state->selection = EDITOR_SELECTION_ORIGIN;
                state->selected_origin_kind = EDITOR_ORIGIN_RIGID_BODY;
                state->mode = EDITOR_VIEWPORT_ORIGIN;
                state->dragged_origin = true;
                state->drag_offset = (Vec2D){pointer.x - center.x,
                    pointer.y - center.y};
                state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
            } else {
                state->last_viewport_click_selection = EDITOR_SELECTION_ORIGIN;
                state->last_viewport_click_object = object->id;
                state->last_viewport_click_index = body->id;
                state->last_viewport_click_at = now;
            }
            return true;
        }
    }

    if(object->visible && (state->mode == EDITOR_VIEWPORT_SOFT_BODY ||
            state->mode == EDITOR_VIEWPORT_SOFT_NODE ||
            (state->mode == EDITOR_VIEWPORT_ORIGIN &&
                state->selected_origin_kind == EDITOR_ORIGIN_SOFT_BODY))) {
        for(size_t i = 0; i < object->soft_body_count; i += 1) {
            EditorSoftBody *soft_body = &object->soft_body_items[i];
            Position center = {object->position.x + soft_body->position.x,
                object->position.y + soft_body->position.y};
            Uint64 now;
            bool editing;
            bool double_clicked;
            if(soft_body->id != state->selected_soft_body || !soft_body->visible ||
                    (pointer.x - center.x) * (pointer.x - center.x) +
                    (pointer.y - center.y) * (pointer.y - center.y) > 100.0f) continue;
            now = SDL_GetTicks();
            editing = state->mode == EDITOR_VIEWPORT_ORIGIN &&
                state->selected_origin_kind == EDITOR_ORIGIN_SOFT_BODY;
            double_clicked = state->last_viewport_click_selection ==
                    EDITOR_SELECTION_ORIGIN &&
                state->last_viewport_click_object == object->id &&
                state->last_viewport_click_index == soft_body->id &&
                now - state->last_viewport_click_at <= 400;
            if(editing || double_clicked) {
                state->selection = EDITOR_SELECTION_ORIGIN;
                state->selected_origin_kind = EDITOR_ORIGIN_SOFT_BODY;
                state->mode = EDITOR_VIEWPORT_ORIGIN;
                state->dragged_origin = true;
                state->drag_offset = (Vec2D){pointer.x - center.x,
                    pointer.y - center.y};
                state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
            } else {
                state->last_viewport_click_selection = EDITOR_SELECTION_ORIGIN;
                state->last_viewport_click_object = object->id;
                state->last_viewport_click_index = soft_body->id;
                state->last_viewport_click_at = now;
            }
            return true;
        }
    }

    if(object->visible && state->mode == EDITOR_VIEWPORT_SOFT_BODY) {
        for(size_t i = 0; i < object->soft_body_count; i += 1) {
            EditorSoftBody *soft_body = &object->soft_body_items[i];
            Position handle;
            Position center;
            if(soft_body->id != state->selected_soft_body || !soft_body->visible) continue;
            handle = editor_soft_body_rotation_handle_get(object, soft_body);
            if((pointer.x - handle.x) * (pointer.x - handle.x) +
                    (pointer.y - handle.y) * (pointer.y - handle.y) > 144.0f) break;
            center = (Position){object->position.x + soft_body->position.x,
                object->position.y + soft_body->position.y};
            state->rotated_soft_body = true;
            state->rotation_pointer_offset = soft_body->rotation -
                atan2f(pointer.y - center.y, pointer.x - center.x);
            return true;
        }
    }

    if(object->visible) {
        for(size_t i = 0; i < object->anchor_count; i += 1) {
            EditorAnchor *anchor = &object->anchors[i];
            Position world = editor_anchor_world_get(object, anchor);
            if(!anchor->visible || (pointer.x - world.x) * (pointer.x - world.x) +
                    (pointer.y - world.y) * (pointer.y - world.y) > 100.0f) continue;
            state->selection = EDITOR_SELECTION_ANCHOR;
            state->selected_anchor = anchor->id;
            state->mode = EDITOR_VIEWPORT_ANCHOR;
            state->dragged_anchor = true;
            state->drag_offset = (Vec2D){pointer.x - world.x, pointer.y - world.y};
            return true;
        }
    }

    if(body != NULL && body->visible && state->mode == EDITOR_VIEWPORT_RIGID_BODY) {
        Position handle = editor_body_rotation_handle_get(object, body);
        if((pointer.x - handle.x) * (pointer.x - handle.x) +
                (pointer.y - handle.y) * (pointer.y - handle.y) <= 144.0f) {
            Position center = {object->position.x + body->position.x,
                object->position.y + body->position.y};
            state->rotated_body = true;
            state->selection = EDITOR_SELECTION_RIGID_BODY;
            state->rotation_pointer_offset = body->rotation -
                atan2f(pointer.y - center.y, pointer.x - center.x);
            return true;
        }
        for(size_t i = 0; i < body->hitbox_count; i += 1) {
            if(body->hitboxes[i].visible && editor_hitbox_point_contains(
                    object, body, &body->hitboxes[i], pointer)) {
                Uint64 now = SDL_GetTicks();
                bool double_clicked = state->last_viewport_click_selection ==
                        EDITOR_SELECTION_RIGID_BODY &&
                    state->last_viewport_click_object == object->id &&
                    state->last_viewport_click_index == body->id &&
                    now - state->last_viewport_click_at <= 400;
                if(double_clicked) {
                    state->selected_hitbox = body->hitboxes[i].id;
                    editor_viewport_hitbox_editor_enter(state);
                    state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
                    return true;
                }
                state->last_viewport_click_selection = EDITOR_SELECTION_RIGID_BODY;
                state->last_viewport_click_object = object->id;
                state->last_viewport_click_index = body->id;
                state->last_viewport_click_at = now;
                state->dragged_body = true;
                state->selection = EDITOR_SELECTION_RIGID_BODY;
                state->drag_offset = (Vec2D){pointer.x - object->position.x - body->position.x,
                    pointer.y - object->position.y - body->position.y};
                return true;
            }
        }
    }

    if(object->visible) {
        for(size_t soft_index = 0; soft_index < object->soft_body_count; soft_index += 1) {
            EditorSoftBody *soft_body = &object->soft_body_items[soft_index];
            if(!soft_body->visible) continue;
            for(size_t i = 0; i < soft_body->node_count; i += 1) {
                EditorSoftNode *node = &soft_body->nodes[i];
                Position world = editor_soft_node_world_get(object, soft_body, node);
                if(!node->visible || (pointer.x - world.x) * (pointer.x - world.x) +
                        (pointer.y - world.y) * (pointer.y - world.y) > 100.0f) continue;
                if(state->mode == EDITOR_VIEWPORT_SOFT_NODE &&
                        state->selected_soft_body == soft_body->id &&
                        state->selected_soft_node == node->id) {
                    state->dragged_soft_node = true;
                    state->drag_offset = (Vec2D){pointer.x - world.x,
                        pointer.y - world.y};
                    return true;
                }
                state->selected_soft_body = soft_body->id;
                {
                    Uint64 now = SDL_GetTicks();
                    bool double_clicked = state->last_viewport_click_selection ==
                            EDITOR_SELECTION_SOFT_NODE &&
                        state->last_viewport_click_object == object->id &&
                        state->last_viewport_click_index == node->id &&
                        now - state->last_viewport_click_at <= 400;
                    if(double_clicked) {
                        state->selection = EDITOR_SELECTION_SOFT_NODE;
                        state->selected_soft_node = node->id;
                        state->mode = EDITOR_VIEWPORT_SOFT_NODE;
                        state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
                    } else {
                        state->selection = EDITOR_SELECTION_SOFT_BODY;
                        state->mode = EDITOR_VIEWPORT_SOFT_BODY;
                        state->dragged_soft_body = true;
                        state->drag_offset = (Vec2D){
                            pointer.x - object->position.x - soft_body->position.x,
                            pointer.y - object->position.y - soft_body->position.y
                        };
                        state->last_viewport_click_selection = EDITOR_SELECTION_SOFT_NODE;
                        state->last_viewport_click_object = object->id;
                        state->last_viewport_click_index = node->id;
                        state->last_viewport_click_at = now;
                    }
                }
                return true;
            }
            for(size_t i = 0; i < soft_body->beam_count; i += 1) {
                EditorSoftBeam *beam = &soft_body->beams[i];
                EditorSoftNode *a = NULL;
                EditorSoftNode *b = NULL;
                if(!beam->visible) continue;
                for(size_t j = 0; j < soft_body->node_count; j += 1) {
                    if(soft_body->nodes[j].id == beam->node_a) a = &soft_body->nodes[j];
                    if(soft_body->nodes[j].id == beam->node_b) b = &soft_body->nodes[j];
                }
                if(a == NULL || b == NULL || editor_segment_distance_squared(pointer,
                        editor_soft_node_world_get(object, soft_body, a),
                        editor_soft_node_world_get(object, soft_body, b)) > 36.0f) continue;
                state->selected_soft_body = soft_body->id;
                {
                    Uint64 now = SDL_GetTicks();
                    bool double_clicked = state->last_viewport_click_selection ==
                            EDITOR_SELECTION_SOFT_BEAM &&
                        state->last_viewport_click_object == object->id &&
                        state->last_viewport_click_index == beam->id &&
                        now - state->last_viewport_click_at <= 400;
                    if(double_clicked) {
                        state->selection = EDITOR_SELECTION_SOFT_BEAM;
                        state->selected_soft_beam = beam->id;
                        state->mode = EDITOR_VIEWPORT_SOFT_BEAM;
                        state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
                    } else {
                        state->selection = EDITOR_SELECTION_SOFT_BODY;
                        state->mode = EDITOR_VIEWPORT_SOFT_BODY;
                        state->dragged_soft_body = true;
                        state->drag_offset = (Vec2D){
                            pointer.x - object->position.x - soft_body->position.x,
                            pointer.y - object->position.y - soft_body->position.y
                        };
                        state->last_viewport_click_selection = EDITOR_SELECTION_SOFT_BEAM;
                        state->last_viewport_click_object = object->id;
                        state->last_viewport_click_index = beam->id;
                        state->last_viewport_click_at = now;
                    }
                }
                return true;
            }
            {
                bool parent_editor_active = state->mode == EDITOR_VIEWPORT_SOFT_BODY &&
                    state->selected_soft_body == soft_body->id;
                state->soft_area_candidate_count = 0;
                for(size_t i = 0; i < soft_body->area_count; i += 1) {
                    EditorSoftArea *area = &soft_body->areas[i];
                    if(!area->visible || !editor_soft_area_point_contains(
                            object, soft_body, area, pointer)) continue;
                    state->soft_area_candidates[state->soft_area_candidate_count++] = area->id;
                }
                if(state->soft_area_candidate_count > 0 && !parent_editor_active) {
                    state->selected_soft_body = soft_body->id;
                    state->selection = EDITOR_SELECTION_SOFT_BODY;
                    state->mode = EDITOR_VIEWPORT_SOFT_BODY;
                    state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
                    return true;
                }
                if(state->soft_area_candidate_count > 0) {
                    EditorSoftAreaId area_id = state->soft_area_candidates[0];
                    Uint64 now = SDL_GetTicks();
                    bool double_clicked = state->last_viewport_click_selection ==
                        EDITOR_SELECTION_SOFT_AREA &&
                        state->last_viewport_click_object == object->id &&
                        state->last_viewport_click_index == area_id &&
                        now - state->last_viewport_click_at <= 400;
                    if(double_clicked) {
                        state->selection = EDITOR_SELECTION_SOFT_AREA;
                        state->selected_soft_area = area_id;
                        state->mode = EDITOR_VIEWPORT_SOFT_AREA;
                        state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
                    } else {
                        state->last_viewport_click_selection = EDITOR_SELECTION_SOFT_AREA;
                        state->last_viewport_click_object = object->id;
                        state->last_viewport_click_index = area_id;
                        state->last_viewport_click_at = now;
                    }
                    return true;
                }
            }
            if(editor_soft_body_area_contains(object, soft_body, pointer)) {
                state->selection = EDITOR_SELECTION_SOFT_BODY;
                state->selected_soft_body = soft_body->id;
                state->mode = EDITOR_VIEWPORT_SOFT_BODY;
                state->dragged_soft_body = true;
                state->drag_offset = (Vec2D){
                    pointer.x - object->position.x - soft_body->position.x,
                    pointer.y - object->position.y - soft_body->position.y
                };
                state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
                return true;
            }
        }
    }

    if(hitbox != NULL && hitbox->visible && body != NULL && body->visible) {
        for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
            Position vertex = editor_hitbox_vertex_world_get(object, body, hitbox, i);
            Vec2D delta = {pointer.x - vertex.x, pointer.y - vertex.y};
            if(delta.x * delta.x + delta.y * delta.y > 100.0f) continue;
            state->selection = EDITOR_SELECTION_VERTEX;
            state->selected_vertex = i;
            editor_viewport_vertex_editor_enter(state, i);
            if(!hitbox->vertices[i].position_locked) state->dragged_vertex = (int)i;
            return true;
        }
        for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
            Position start = editor_hitbox_vertex_world_get(object, body, hitbox, i);
            Position end = editor_hitbox_vertex_world_get(
                object, body, hitbox, (i + 1) % hitbox->vertex_count);
            Vec2D edge = {end.x - start.x, end.y - start.y};
            float length_squared = edge.x * edge.x + edge.y * edge.y;
            float amount;
            Position nearest;
            Vec2D distance;
            if(length_squared <= 0.001f) continue;
            amount = ((pointer.x - start.x) * edge.x +
                (pointer.y - start.y) * edge.y) / length_squared;
            if(amount < 0.0f) amount = 0.0f;
            if(amount > 1.0f) amount = 1.0f;
            nearest = (Position){start.x + edge.x * amount, start.y + edge.y * amount};
            distance = (Vec2D){pointer.x - nearest.x, pointer.y - nearest.y};
            if(distance.x * distance.x + distance.y * distance.y > 36.0f) continue;
            state->selection = EDITOR_SELECTION_LINE;
            state->selected_line = i;
            editor_viewport_line_editor_enter(state, i);
            return true;
        }
    }

    for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
        EditorRigidBody *candidate_body = &object->rigid_bodies[body_index];
        if(!object->visible || !candidate_body->visible) continue;
        for(size_t box_index = 0; box_index < candidate_body->hitbox_count; box_index += 1) {
            EditorHitbox *candidate = &candidate_body->hitboxes[box_index];
            if(!candidate->visible ||
                    !editor_hitbox_point_contains(object, candidate_body, candidate, pointer)) {
                continue;
            }
            if(state->mode == EDITOR_VIEWPORT_HIERARCHY) {
                state->selection = EDITOR_SELECTION_OBJECT;
                editor_viewport_object_editor_enter(state);
            } else if(state->mode == EDITOR_VIEWPORT_OBJECT) {
                state->selection = EDITOR_SELECTION_RIGID_BODY;
                state->selected_rigid_body = candidate_body->id;
                state->mode = EDITOR_VIEWPORT_RIGID_BODY;
            } else if(state->selected_rigid_body != candidate_body->id ||
                    (state->mode != EDITOR_VIEWPORT_RIGID_BODY &&
                    state->mode != EDITOR_VIEWPORT_HITBOX &&
                    state->mode != EDITOR_VIEWPORT_LINE &&
                    state->mode != EDITOR_VIEWPORT_VERTEX)) {
                state->selection = EDITOR_SELECTION_RIGID_BODY;
                state->selected_rigid_body = candidate_body->id;
                state->mode = EDITOR_VIEWPORT_RIGID_BODY;
                state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
            } else {
                state->selection = EDITOR_SELECTION_HITBOX;
                state->selected_rigid_body = candidate_body->id;
                state->selected_hitbox = candidate->id;
                editor_viewport_hitbox_editor_enter(state);
            }
            return true;
        }
    }

    return false;
}

static void editor_viewport_object_draw(const EditorObject *object,
    const EditorViewportState *state, bool object_selected) {
    if(object == NULL || state == NULL || !object->visible) return;

    for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
        const EditorRigidBody *body = &object->rigid_bodies[body_index];
        if(!body->visible) continue;
        for(size_t box_index = 0; box_index < body->hitbox_count; box_index += 1) {
            const EditorHitbox *hitbox = &body->hitboxes[box_index];
            bool selected_body = state->selected_rigid_body == body->id;
            bool selected_hitbox = selected_body && state->selected_hitbox == hitbox->id;
            Color base = (state->selection == EDITOR_SELECTION_OBJECT && object_selected) ||
                    state->preview_rigid_body == body->id ||
                    (state->selection == EDITOR_SELECTION_RIGID_BODY && selected_body) ||
                    (state->selection == EDITOR_SELECTION_HITBOX && selected_hitbox) ?
                (Color){255, 215, 70, 255} : graphics_color_hex_create(body->border_color);
            if(!hitbox->visible) continue;
            for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
                Position start = editor_hitbox_vertex_world_get(object, body, hitbox, i);
                Position end = editor_hitbox_vertex_world_get(
                    object, body, hitbox, (i + 1) % hitbox->vertex_count);
                Color edge = selected_hitbox && state->selection == EDITOR_SELECTION_LINE &&
                        state->selected_line == i ? (Color){255, 215, 70, 255} : base;
                editor_line_draw(start, end, edge);
                if(selected_hitbox && (state->selection == EDITOR_SELECTION_HITBOX ||
                        state->selection == EDITOR_SELECTION_VERTEX ||
                        state->selection == EDITOR_SELECTION_LINE)) {
                    Color point = state->selection == EDITOR_SELECTION_VERTEX &&
                            state->selected_vertex == i ? (Color){255, 215, 70, 255} :
                        (hitbox->vertices[i].position_locked ?
                            (Color){245, 165, 70, 255} : (Color){235, 240, 248, 255});
                    editor_quad_draw(start, 10.0f, 10.0f, 0.0f, point);
                }
            }
        }
    }

    {
        const EditorRigidBody *selected = NULL;
        for(size_t i = 0; i < object->rigid_body_count; i += 1) {
            if(object->rigid_bodies[i].id == state->selected_rigid_body) {
                selected = &object->rigid_bodies[i];
            }
        }
        if(selected != NULL && selected->visible &&
                (state->mode == EDITOR_VIEWPORT_RIGID_BODY ||
                    state->mode == EDITOR_VIEWPORT_HITBOX ||
                    state->mode == EDITOR_VIEWPORT_LINE ||
                    state->mode == EDITOR_VIEWPORT_VERTEX ||
                    (state->mode == EDITOR_VIEWPORT_ORIGIN &&
                        state->selected_origin_kind == EDITOR_ORIGIN_RIGID_BODY) ||
                    state->selection == EDITOR_SELECTION_RIGID_BODY)) {
            Position center = {object->position.x + selected->position.x,
                object->position.y + selected->position.y};
            editor_body_origin_draw(object, selected);
            if(state->selection == EDITOR_SELECTION_ORIGIN &&
                    state->selected_origin_kind == EDITOR_ORIGIN_RIGID_BODY)
                editor_circle_draw(center, 7.0f, (Color){255, 215, 70, 255});
            if(state->mode == EDITOR_VIEWPORT_RIGID_BODY) {
                Position handle = editor_body_rotation_handle_get(object, selected);
                editor_line_draw(center, handle, (Color){255, 215, 70, 255});
                editor_circle_draw(handle, 10.0f, (Color){255, 215, 70, 255});
            }
        }
    }

    for(size_t soft_index = 0; soft_index < object->soft_body_count; soft_index += 1) {
        const EditorSoftBody *body = &object->soft_body_items[soft_index];
        bool selected_body = state->selection == EDITOR_SELECTION_SOFT_BODY &&
            state->selected_soft_body == body->id;
        const EditorSoftArea *selected_area = NULL;
        if(state->selection == EDITOR_SELECTION_SOFT_AREA &&
                state->selected_soft_body == body->id) {
            for(size_t i = 0; i < body->area_count; i += 1) {
                if(body->areas[i].id == state->selected_soft_area) {
                    selected_area = &body->areas[i];
                }
            }
        }
        if(!body->visible) continue;
        if(selected_area != NULL && selected_area->visible) {
            const EditorSoftNode *a = editor_soft_node_get(body, selected_area->node_a);
            const EditorSoftNode *b = editor_soft_node_get(body, selected_area->node_b);
            const EditorSoftNode *c = editor_soft_node_get(body, selected_area->node_c);
            if(a != NULL && b != NULL && c != NULL) {
                editor_triangle_filled_draw(
                    editor_soft_node_world_get(object, body, a),
                    editor_soft_node_world_get(object, body, b),
                    editor_soft_node_world_get(object, body, c),
                    (Color){255, 215, 70, 72});
            }
        }
        for(size_t beam_index = 0; beam_index < body->beam_count; beam_index += 1) {
            const EditorSoftBeam *beam = &body->beams[beam_index];
            const EditorSoftNode *a = NULL;
            const EditorSoftNode *b = NULL;
            if(!beam->visible) continue;
            for(size_t i = 0; i < body->node_count; i += 1) {
                if(body->nodes[i].id == beam->node_a) a = &body->nodes[i];
                if(body->nodes[i].id == beam->node_b) b = &body->nodes[i];
            }
            {
                bool selected_area_edge = selected_area != NULL &&
                    (((beam->node_a == selected_area->node_a ||
                        beam->node_a == selected_area->node_b ||
                        beam->node_a == selected_area->node_c) &&
                      (beam->node_b == selected_area->node_a ||
                        beam->node_b == selected_area->node_b ||
                        beam->node_b == selected_area->node_c)));
                if(a != NULL && b != NULL) editor_line_draw(
                editor_soft_node_world_get(object, body, a),
                editor_soft_node_world_get(object, body, b),
                selected_body || (state->selection == EDITOR_SELECTION_SOFT_BEAM &&
                    state->selected_soft_body == body->id &&
                    state->selected_soft_beam == beam->id) || selected_area_edge ?
                    (Color){255, 215, 70, 255} : graphics_color_hex_create(
                        beam->color_overridden ? beam->color : body->beam_color));
            }
        }
        for(size_t i = 0; i < body->node_count; i += 1) {
            const EditorSoftNode *node = &body->nodes[i];
            if(!node->visible) continue;
            editor_quad_draw(editor_soft_node_world_get(object, body, node),
                8.0f, 8.0f, 0.0f,
                selected_body || (state->selection == EDITOR_SELECTION_SOFT_NODE &&
                    state->selected_soft_body == body->id &&
                    state->selected_soft_node == node->id) ||
                    state->preview_soft_node == node->id ?
                    (Color){255, 215, 70, 255} : graphics_color_hex_create(
                        node->color_overridden ? node->color : body->node_color));
        }
    }

    if(state->mode == EDITOR_VIEWPORT_SOFT_BODY ||
            state->mode == EDITOR_VIEWPORT_SOFT_NODE ||
            (state->mode == EDITOR_VIEWPORT_ORIGIN &&
                state->selected_origin_kind == EDITOR_ORIGIN_SOFT_BODY)) {
        for(size_t i = 0; i < object->soft_body_count; i += 1) {
            const EditorSoftBody *body = &object->soft_body_items[i];
            Position center;
            Position handle;
            if(body->id != state->selected_soft_body || !body->visible) continue;
            center = (Position){object->position.x + body->position.x,
                object->position.y + body->position.y};
            handle = editor_soft_body_rotation_handle_get(object, body);
            editor_circle_draw(center, 5.0f, (Color){245, 245, 250, 255});
            editor_quad_draw(center, 3.0f, 3.0f, 0.0f,
                (Color){245, 245, 250, 255});
            if(state->selection == EDITOR_SELECTION_ORIGIN)
                editor_circle_draw(center, 7.0f, (Color){255, 215, 70, 255});
            if(state->mode == EDITOR_VIEWPORT_SOFT_BODY) {
                editor_line_draw(center, handle, (Color){255, 215, 70, 255});
                editor_circle_draw(handle, 10.0f, (Color){255, 215, 70, 255});
            }
            break;
        }
    }

    for(size_t joint_index = 0; joint_index < object->joint_count; joint_index += 1) {
        const EditorJoint *joint = &object->joint_items[joint_index];
        const EditorAnchor *a = NULL;
        const EditorAnchor *b = NULL;
        if(!joint->visible) continue;
        for(size_t i = 0; i < object->anchor_count; i += 1) {
            if(object->anchors[i].id == joint->anchor_a) a = &object->anchors[i];
            if(object->anchors[i].id == joint->anchor_b) b = &object->anchors[i];
        }
        if(a != NULL && b != NULL) editor_joint_symbol_draw(joint->kind,
            editor_anchor_world_get(object, a), editor_anchor_world_get(object, b),
            fmaxf(0.1f, joint->visual_size),
            state->selection == EDITOR_SELECTION_JOINT &&
                state->selected_joint == joint->id ?
                (Color){255, 215, 70, 255} : (Color){220, 120, 210, 255});
    }
    for(size_t i = 0; i < object->anchor_count; i += 1) {
        const EditorAnchor *anchor = &object->anchors[i];
        const EditorRigidBody *anchor_body = NULL;
        float rotation = anchor->rotation;
        if(!anchor->visible) continue;
        for(size_t j = 0; j < object->rigid_body_count; j += 1) {
            if(object->rigid_bodies[j].id == anchor->rigid_body) {
                anchor_body = &object->rigid_bodies[j];
            }
        }
        if(anchor_body != NULL && anchor->rotation_follows_body) {
            rotation += anchor_body->rotation;
        }
        editor_quad_draw(editor_anchor_world_get(object, anchor),
            9.0f, 9.0f, rotation + 0.78539816339f,
            state->selected_anchor == anchor->id || state->preview_anchor == anchor->id ?
                (Color){255, 215, 70, 255} : (Color){235, 150, 215, 255});
    }
}

void editor_viewport_draw(const EditorProject *project, const EditorViewportState *state) {
    const EditorObject *selected;

    if(project == NULL || state == NULL) return;
    selected = NULL;
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(project->objects[i].id == project->selected) selected = &project->objects[i];
    }
    editor_view_transform_set(state, selected);
    if(state->mode == EDITOR_VIEWPORT_HIERARCHY) {
        for(size_t i = 0; i < project->object_count; i += 1) {
            editor_viewport_object_draw(&project->objects[i], state,
                project->objects[i].id == project->selected);
        }
        return;
    }
    editor_viewport_object_draw(selected, state, true);
}

bool editor_viewport_selection_nudge(EditorViewportState *state,
    EditorProject *project, Vec2D screen_delta) {
    EditorObject *object;
    EditorRigidBody *body;

    if(state == NULL || project == NULL) return false;
    screen_delta.y = -screen_delta.y;
    object = editor_project_selected_get(project);
    if(object == NULL) return false;
    body = editor_selected_body_get(object, state);
    if(state->selection == EDITOR_SELECTION_RIGID_BODY && body != NULL) {
        body->position.x += screen_delta.x;
        body->position.y += screen_delta.y;
        editor_project_rigid_body_constraints_apply(object, body->id);
        return true;
    }
    if(state->selection == EDITOR_SELECTION_VERTEX && body != NULL) {
        EditorHitbox *hitbox = editor_selected_hitbox_get(object, state);
        float cosine = cosf(-body->rotation);
        float sine = sinf(-body->rotation);
        if(hitbox == NULL || state->selected_vertex >= hitbox->vertex_count ||
                hitbox->vertices[state->selected_vertex].position_locked) return false;
        hitbox->vertices[state->selected_vertex].position.x +=
            screen_delta.x * cosine - screen_delta.y * sine;
        hitbox->vertices[state->selected_vertex].position.y +=
            screen_delta.x * sine + screen_delta.y * cosine;
        return true;
    }
    if(state->selection == EDITOR_SELECTION_ANCHOR) {
        EditorAnchor *anchor = editor_project_anchor_get(object, state->selected_anchor);
        EditorRigidBody *anchor_body;
        Position world;
        if(anchor == NULL) return false;
        anchor_body = editor_project_rigid_body_get(object, anchor->rigid_body);
        world = editor_anchor_world_get(object, anchor);
        world.x += screen_delta.x;
        world.y += screen_delta.y;
        editor_anchor_world_set(object, anchor, anchor_body, world);
        editor_project_anchor_constraints_apply(object, anchor->id);
        return true;
    }
    if(state->selection == EDITOR_SELECTION_SOFT_NODE) {
        for(size_t body_index = 0; body_index < object->soft_body_count; body_index += 1) {
            EditorSoftBody *soft_body = &object->soft_body_items[body_index];
            if(soft_body->id != state->selected_soft_body) continue;
            for(size_t node_index = 0; node_index < soft_body->node_count; node_index += 1) {
                EditorSoftNode *node = &soft_body->nodes[node_index];
                if(node->id != state->selected_soft_node) continue;
                node->position.x += screen_delta.x;
                node->position.y += screen_delta.y;
                return true;
            }
        }
    }
    return false;
}
