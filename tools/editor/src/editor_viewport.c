#include "editor_viewport.h"
#include "editor_command.h"
#include "editor_layout.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static Position editor_view_origin;
static float editor_view_scale = 1.0f;

static void editor_view_transform_set(const EditorProject *project,
    const EditorViewportState *state, const EditorObject *object) {
    Position center = {EDITOR_VIEWPORT_WIDTH * 0.5f,
        EDITOR_MENU_HEIGHT +
            (EDITOR_VIEWPORT_BOTTOM - EDITOR_MENU_HEIGHT) * 0.5f};

    editor_view_origin = center;
    editor_view_scale = project != NULL && project->viewport_camera_zoom > 0.0f ?
        project->viewport_camera_zoom : 1.0f;
    if(project != NULL) {
        editor_view_origin.x += project->viewport_camera_offset.x;
        editor_view_origin.y += project->viewport_camera_offset.y;
    }
    if(project != NULL && state != NULL && project->viewport_local_view && object != NULL &&
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

static Position editor_particle_center_world_get(const EditorObject *object,
    const EditorRigidBody *body) {
    Position local = editor_project_particle_center_get(body);
    float cosine = cosf(body->rotation);
    float sine = sinf(body->rotation);
    return (Position){object->position.x + body->position.x +
            local.x * cosine - local.y * sine,
        object->position.y + body->position.y +
            local.x * sine + local.y * cosine};
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

static Position editor_anchor_world_local_get(const EditorObject *object,
    const EditorAnchor *anchor,
    const EditorRigidBody *body, Position world) {
    Position local = {world.x - object->position.x, world.y - object->position.y};
    if(body != NULL && anchor->position_follows_body) {
        float cosine = cosf(-body->rotation);
        float sine = sinf(-body->rotation);
        local.x -= body->position.x;
        local.y -= body->position.y;
        return (Position){local.x * cosine - local.y * sine,
            local.x * sine + local.y * cosine};
    }
    return local;
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

static void editor_soft_area_filled_draw(const EditorObject *object,
        const EditorSoftBody *body, const EditorSoftArea *area, Color color) {
    uint32_t triangles[EDITOR_SOFT_AREA_NODE_MAX - 2][3];
    size_t count = editor_project_soft_area_triangulate(body, area, triangles,
        EDITOR_SOFT_AREA_NODE_MAX - 2);
    for(size_t i = 0; i < count; i += 1) {
        const EditorSoftNode *a = NULL;
        const EditorSoftNode *b = NULL;
        const EditorSoftNode *c = NULL;
        for(size_t node_index = 0; node_index < body->node_count; node_index += 1) {
            if(body->nodes[node_index].id == area->nodes[triangles[i][0]])
                a = &body->nodes[node_index];
            if(body->nodes[node_index].id == area->nodes[triangles[i][1]])
                b = &body->nodes[node_index];
            if(body->nodes[node_index].id == area->nodes[triangles[i][2]])
                c = &body->nodes[node_index];
        }
        if(a != NULL && b != NULL && c != NULL) editor_triangle_filled_draw(
            editor_soft_node_world_get(object, body, a),
            editor_soft_node_world_get(object, body, b),
            editor_soft_node_world_get(object, body, c), color);
    }
}

static void editor_hitbox_filled_draw(const EditorObject *object,
        const EditorRigidBody *body, const EditorHitbox *hitbox, Color color) {
    Position points[EDITOR_HITBOX_VERTEX_MAX];
    float minimum_y;
    float maximum_y;

    if(object == NULL || body == NULL || hitbox == NULL || hitbox->vertex_count < 3)
        return;
    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
        points[i] = editor_view_world_to_screen(
            editor_hitbox_vertex_world_get(object, body, hitbox, i));
    }
    minimum_y = points[0].y;
    maximum_y = points[0].y;
    for(uint32_t i = 1; i < hitbox->vertex_count; i += 1) {
        minimum_y = fminf(minimum_y, points[i].y);
        maximum_y = fmaxf(maximum_y, points[i].y);
    }

    int first_row = (int)floorf(fmaxf(minimum_y, EDITOR_MENU_HEIGHT));
    int last_row = (int)ceilf(fminf(maximum_y, WINDOW_HEIGHT - 1.0f));
    for(int row = first_row; row <= last_row; row += 1) {
        float scan_y = (float)row + 0.5f;
        float intersections[EDITOR_HITBOX_VERTEX_MAX];
        uint32_t count = 0;
        for(uint32_t edge = 0; edge < hitbox->vertex_count; edge += 1) {
            Position start = points[edge];
            Position end = points[(edge + 1) % hitbox->vertex_count];
            float low = fminf(start.y, end.y);
            float high = fmaxf(start.y, end.y);
            if(scan_y < low || scan_y >= high || fabsf(end.y - start.y) <= 0.0001f)
                continue;
            intersections[count++] = start.x + (scan_y - start.y) *
                (end.x - start.x) / (end.y - start.y);
        }
        for(uint32_t i = 1; i < count; i += 1) {
            float value = intersections[i];
            uint32_t position = i;
            while(position > 0 && intersections[position - 1] > value) {
                intersections[position] = intersections[position - 1];
                position -= 1;
            }
            intersections[position] = value;
        }
        for(uint32_t i = 0; i + 1 < count; i += 2) {
            float left = fmaxf(intersections[i], 0.0f);
            float right = fminf(intersections[i + 1], EDITOR_VIEWPORT_WIDTH);
            if(right > left) (void)rohr_graphics_screen_rect_draw(
                left, (float)row, right - left, 1.0f, color);
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

static void editor_circle_filled_draw(Position center, float radius, Color color) {
    Position screen = editor_view_world_to_screen(center);
    float screen_radius = radius * editor_view_scale;
    int first_row = (int)floorf(fmaxf(screen.y - screen_radius, EDITOR_MENU_HEIGHT));
    int last_row = (int)ceilf(fminf(screen.y + screen_radius, WINDOW_HEIGHT - 1.0f));

    if(screen_radius <= 0.0f) return;
    for(int row = first_row; row <= last_row; row += 1) {
        float y = ((float)row + 0.5f) - screen.y;
        float half_width = sqrtf(fmaxf(0.0f, screen_radius * screen_radius - y * y));
        float left = fmaxf(0.0f, screen.x - half_width);
        float right = fminf(EDITOR_VIEWPORT_WIDTH, screen.x + half_width);
        if(right > left) (void)rohr_graphics_screen_rect_draw(
            left, (float)row, right - left, 1.0f, color);
    }
}

static void editor_circle_dotted_draw(Position center, float radius, Color color) {
    for(uint32_t i = 0; i < 32; i += 2) {
        float start_angle = 6.28318530718f * (float)i / 32.0f;
        float end_angle = 6.28318530718f * (float)(i + 1) / 32.0f;
        editor_line_draw((Position){center.x + cosf(start_angle) * radius,
                center.y + sinf(start_angle) * radius},
            (Position){center.x + cosf(end_angle) * radius,
                center.y + sinf(end_angle) * radius}, color);
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
    free(state->selected_items);
    *state = (EditorViewportState){.dragged_vertex = -1};
}

void editor_viewport_state_destroy(EditorViewportState *state) {
    if(state == NULL) return;
    free(state->selected_items);
    *state = (EditorViewportState){0};
}

void editor_viewport_selection_clear(EditorViewportState *state) {
    if(state == NULL) return;
    state->selected_item_count = 0;
}

static bool editor_selection_ref_equal(EditorSelectionRef first,
        EditorSelectionRef second) {
    return first.kind == second.kind && first.object == second.object &&
        first.parent == second.parent && first.container == second.container &&
        first.item == second.item;
}

bool editor_viewport_selection_contains(const EditorViewportState *state,
        EditorSelectionRef selection) {
    if(state == NULL) return false;
    for(size_t i = 0; i < state->selected_item_count; i += 1)
        if(editor_selection_ref_equal(state->selected_items[i], selection)) return true;
    return false;
}

bool editor_viewport_selection_homogeneous_check(const EditorViewportState *state) {
    if(state == NULL || state->selected_item_count == 0) return false;
    for(size_t i = 1; i < state->selected_item_count; i += 1)
        if(state->selected_items[i].kind != state->selected_items[0].kind)
            return false;
    return true;
}

bool editor_viewport_selection_ref_get(const EditorProject *project,
        const EditorViewportState *state, EditorSelectionRef *selection) {
    const EditorObject *object = NULL;
    const EditorRigidBody *body = NULL;
    const EditorHitbox *hitbox = NULL;
    if(project == NULL || state == NULL || selection == NULL ||
            state->selection == EDITOR_SELECTION_NONE) return false;
    for(size_t i = 0; i < project->object_count; i += 1)
        if(project->objects[i].id == project->selected) object = &project->objects[i];
    *selection = (EditorSelectionRef){.kind = state->selection,
        .object = project->selected};
    switch(state->selection) {
        case EDITOR_SELECTION_OBJECT:
            selection->item = project->selected;
            return selection->item != 0;
        case EDITOR_SELECTION_RIGID_BODY:
        case EDITOR_SELECTION_PARTICLE:
            selection->item = state->selected_rigid_body;
            return selection->item != 0;
        case EDITOR_SELECTION_HITBOX:
            selection->parent = state->selected_rigid_body;
            selection->item = state->selected_hitbox;
            return selection->item != 0;
        case EDITOR_SELECTION_VERTEX:
            if(object != NULL)
                for(size_t i = 0; i < object->rigid_body_count; i += 1)
                    if(object->rigid_bodies[i].id == state->selected_rigid_body)
                        body = &object->rigid_bodies[i];
            if(body != NULL)
                for(size_t i = 0; i < body->hitbox_count; i += 1)
                    if(body->hitboxes[i].id == state->selected_hitbox)
                        hitbox = &body->hitboxes[i];
            if(hitbox == NULL || state->selected_vertex >= hitbox->vertex_count)
                return false;
            selection->parent = state->selected_rigid_body;
            selection->container = state->selected_hitbox;
            selection->item = hitbox->vertices[state->selected_vertex].id;
            return true;
        case EDITOR_SELECTION_LINE:
            selection->parent = state->selected_rigid_body;
            selection->container = state->selected_hitbox;
            selection->item = state->selected_line;
            return true;
        case EDITOR_SELECTION_JOINT:
            selection->item = state->selected_joint;
            return selection->item != 0;
        case EDITOR_SELECTION_ANCHOR:
            selection->item = state->selected_anchor;
            return selection->item != 0;
        case EDITOR_SELECTION_SOFT_BODY:
            selection->item = state->selected_soft_body;
            return selection->item != 0;
        case EDITOR_SELECTION_SOFT_NODE:
            selection->parent = state->selected_soft_body;
            selection->item = state->selected_soft_node;
            return selection->item != 0;
        case EDITOR_SELECTION_SOFT_BEAM:
            selection->parent = state->selected_soft_body;
            selection->item = state->selected_soft_beam;
            return selection->item != 0;
        case EDITOR_SELECTION_SOFT_AREA:
            selection->parent = state->selected_soft_body;
            selection->item = state->selected_soft_area;
            return selection->item != 0;
        case EDITOR_SELECTION_ORIGIN:
            selection->parent = state->selected_origin_kind;
            selection->item = state->selected_origin_kind == EDITOR_ORIGIN_RIGID_BODY ?
                state->selected_rigid_body : state->selected_soft_body;
            return selection->item != 0;
        default:
            return false;
    }
}

static bool editor_viewport_selection_primary_apply(EditorProject *project,
        EditorViewportState *state, EditorSelectionRef selection) {
    EditorObject *object;
    if(project == NULL || state == NULL || selection.object == 0 ||
            !editor_project_object_select(project, selection.object)) return false;
    object = editor_project_selected_get(project);
    state->selection = selection.kind;
    switch(selection.kind) {
        case EDITOR_SELECTION_OBJECT: break;
        case EDITOR_SELECTION_RIGID_BODY:
        case EDITOR_SELECTION_PARTICLE:
            state->selected_rigid_body = selection.item;
            break;
        case EDITOR_SELECTION_HITBOX:
            state->selected_rigid_body = selection.parent;
            state->selected_hitbox = selection.item;
            break;
        case EDITOR_SELECTION_VERTEX: {
            EditorRigidBody *body = editor_project_rigid_body_get(object,
                selection.parent);
            EditorHitbox *hitbox = body == NULL ? NULL :
                editor_project_hitbox_get(body, selection.container);
            if(hitbox == NULL) return false;
            state->selected_rigid_body = selection.parent;
            state->selected_hitbox = selection.container;
            for(uint32_t i = 0; i < hitbox->vertex_count; i += 1)
                if(hitbox->vertices[i].id == selection.item) {
                    state->selected_vertex = i;
                    return true;
                }
            return false;
        }
        case EDITOR_SELECTION_LINE:
            state->selected_rigid_body = selection.parent;
            state->selected_hitbox = selection.container;
            state->selected_line = selection.item;
            break;
        case EDITOR_SELECTION_JOINT: state->selected_joint = selection.item; break;
        case EDITOR_SELECTION_ANCHOR: state->selected_anchor = selection.item; break;
        case EDITOR_SELECTION_SOFT_BODY:
            state->selected_soft_body = selection.item;
            break;
        case EDITOR_SELECTION_SOFT_NODE:
            state->selected_soft_body = selection.parent;
            state->selected_soft_node = selection.item;
            break;
        case EDITOR_SELECTION_SOFT_BEAM:
            state->selected_soft_body = selection.parent;
            state->selected_soft_beam = selection.item;
            break;
        case EDITOR_SELECTION_SOFT_AREA:
            state->selected_soft_body = selection.parent;
            state->selected_soft_area = selection.item;
            break;
        case EDITOR_SELECTION_ORIGIN:
            state->selected_origin_kind = (EditorOriginKind)selection.parent;
            if(state->selected_origin_kind == EDITOR_ORIGIN_RIGID_BODY)
                state->selected_rigid_body = selection.item;
            else state->selected_soft_body = selection.item;
            break;
        default: return false;
    }
    return true;
}

bool editor_viewport_selection_set(EditorProject *project,
        EditorViewportState *state, EditorSelectionRef selection, bool additive) {
    size_t existing = SIZE_MAX;
    if(project == NULL || state == NULL || selection.kind == EDITOR_SELECTION_NONE)
        return false;
    for(size_t i = 0; i < state->selected_item_count; i += 1)
        if(editor_selection_ref_equal(state->selected_items[i], selection)) existing = i;
    if(!additive || (state->selected_item_count > 0 &&
            state->selected_items[0].kind != selection.kind)) {
        state->selected_item_count = 0;
        existing = SIZE_MAX;
    } else if(existing != SIZE_MAX) {
        memmove(&state->selected_items[existing], &state->selected_items[existing + 1],
            (state->selected_item_count - existing - 1) * sizeof(*state->selected_items));
        state->selected_item_count -= 1;
        if(state->selected_item_count == 0) {
            state->selection = EDITOR_SELECTION_NONE;
            return true;
        }
        return editor_viewport_selection_primary_apply(project, state,
            state->selected_items[state->selected_item_count - 1]);
    }
    if(state->selected_item_count == state->selected_item_capacity) {
        size_t capacity = state->selected_item_capacity == 0 ? 8 :
            state->selected_item_capacity * 2;
        EditorSelectionRef *items = realloc(state->selected_items,
            capacity * sizeof(*items));
        if(items == NULL) return false;
        state->selected_items = items;
        state->selected_item_capacity = capacity;
    }
    state->selected_items[state->selected_item_count] = selection;
    state->selected_item_count += 1;
    return editor_viewport_selection_primary_apply(project, state, selection);
}

typedef struct EditorMarqueeBounds {
    float left;
    float right;
    float bottom;
    float top;
    bool valid;
} EditorMarqueeBounds;

static void editor_marquee_bounds_point_add(EditorMarqueeBounds *bounds,
        Position point) {
    if(!bounds->valid) {
        *bounds = (EditorMarqueeBounds){point.x, point.x, point.y, point.y, true};
        return;
    }
    bounds->left = fminf(bounds->left, point.x);
    bounds->right = fmaxf(bounds->right, point.x);
    bounds->bottom = fminf(bounds->bottom, point.y);
    bounds->top = fmaxf(bounds->top, point.y);
}

static bool editor_marquee_bounds_overlap(EditorMarqueeBounds first,
        EditorMarqueeBounds second) {
    return first.valid && second.valid && first.left <= second.right &&
        first.right >= second.left && first.bottom <= second.top &&
        first.top >= second.bottom;
}

static bool editor_marquee_selection_add(EditorViewportState *state,
        EditorSelectionRef selection) {
    EditorSelectionRef *items;
    size_t capacity;
    if(state->selected_item_count > 0 &&
            state->selected_items[0].kind != selection.kind) return true;
    if(editor_viewport_selection_contains(state, selection)) return true;
    if(state->selected_item_count < state->selected_item_capacity) {
        state->selected_items[state->selected_item_count++] = selection;
        return true;
    }
    capacity = state->selected_item_capacity == 0 ? 8 :
        state->selected_item_capacity * 2;
    items = realloc(state->selected_items, capacity * sizeof(*items));
    if(items == NULL) return false;
    state->selected_items = items;
    state->selected_item_capacity = capacity;
    state->selected_items[state->selected_item_count++] = selection;
    return true;
}

static EditorMarqueeBounds editor_marquee_rigid_body_bounds_get(
        const EditorObject *object, const EditorRigidBody *body) {
    EditorMarqueeBounds bounds = {0};
    for(size_t box_index = 0; box_index < body->hitbox_count; box_index += 1) {
        const EditorHitbox *hitbox = &body->hitboxes[box_index];
        if(!hitbox->visible) continue;
        for(uint32_t i = 0; i < hitbox->vertex_count; i += 1)
            editor_marquee_bounds_point_add(&bounds,
                editor_hitbox_vertex_world_get(object, body, hitbox, i));
    }
    if(body->particle && body->particle_radius > 0.0f) {
        Position center = editor_particle_center_world_get(object, body);
        editor_marquee_bounds_point_add(&bounds, (Position){
            center.x - body->particle_radius, center.y - body->particle_radius});
        editor_marquee_bounds_point_add(&bounds, (Position){
            center.x + body->particle_radius, center.y + body->particle_radius});
    }
    return bounds;
}

static EditorMarqueeBounds editor_marquee_soft_body_bounds_get(
        const EditorObject *object, const EditorSoftBody *body) {
    EditorMarqueeBounds bounds = {0};
    for(size_t i = 0; i < body->node_count; i += 1)
        if(body->nodes[i].visible) editor_marquee_bounds_point_add(&bounds,
            editor_soft_node_world_get(object, body, &body->nodes[i]));
    return bounds;
}

static EditorMarqueeBounds editor_marquee_object_bounds_get(
        const EditorObject *object) {
    EditorMarqueeBounds bounds = {0};
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        EditorMarqueeBounds child = editor_marquee_rigid_body_bounds_get(
            object, &object->rigid_bodies[i]);
        if(child.valid) {
            editor_marquee_bounds_point_add(&bounds,
                (Position){child.left, child.bottom});
            editor_marquee_bounds_point_add(&bounds,
                (Position){child.right, child.top});
        }
    }
    for(size_t i = 0; i < object->soft_body_count; i += 1) {
        EditorMarqueeBounds child = editor_marquee_soft_body_bounds_get(
            object, &object->soft_body_items[i]);
        if(child.valid) {
            editor_marquee_bounds_point_add(&bounds,
                (Position){child.left, child.bottom});
            editor_marquee_bounds_point_add(&bounds,
                (Position){child.right, child.top});
        }
    }
    for(size_t i = 0; i < object->anchor_count; i += 1)
        if(object->anchors[i].visible) editor_marquee_bounds_point_add(&bounds,
            editor_anchor_world_get(object, &object->anchors[i]));
    return bounds;
}

void editor_viewport_marquee_begin(EditorViewportState *state, Position pointer) {
    if(state == NULL) return;
    state->marquee_active = true;
    state->marquee_start = pointer;
    state->marquee_end = pointer;
}

void editor_viewport_marquee_update(EditorViewportState *state, Position pointer) {
    if(state == NULL || !state->marquee_active) return;
    state->marquee_end = pointer;
}

static void editor_marquee_body_children_add(EditorViewportState *state,
        const EditorObject *object, EditorMarqueeBounds marquee) {
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        const EditorRigidBody *body = &object->rigid_bodies[i];
        if(body->visible && editor_marquee_bounds_overlap(marquee,
                editor_marquee_rigid_body_bounds_get(object, body)))
            (void)editor_marquee_selection_add(state,
                (EditorSelectionRef){EDITOR_SELECTION_RIGID_BODY,
                    object->id, 0, 0, body->id});
    }
    for(size_t i = 0; i < object->soft_body_count; i += 1) {
        const EditorSoftBody *body = &object->soft_body_items[i];
        if(body->visible && editor_marquee_bounds_overlap(marquee,
                editor_marquee_soft_body_bounds_get(object, body)))
            (void)editor_marquee_selection_add(state,
                (EditorSelectionRef){EDITOR_SELECTION_SOFT_BODY,
                    object->id, 0, 0, body->id});
    }
    for(size_t i = 0; i < object->anchor_count; i += 1) {
        const EditorAnchor *anchor = &object->anchors[i];
        Position world = editor_anchor_world_get(object, anchor);
        EditorMarqueeBounds point = {world.x - 5.0f, world.x + 5.0f,
            world.y - 5.0f, world.y + 5.0f, true};
        if(anchor->visible && editor_marquee_bounds_overlap(marquee, point))
            (void)editor_marquee_selection_add(state,
                (EditorSelectionRef){EDITOR_SELECTION_ANCHOR,
                    object->id, 0, 0, anchor->id});
    }
    for(size_t i = 0; i < object->joint_count; i += 1) {
        const EditorJoint *joint = &object->joint_items[i];
        const EditorAnchor *a = NULL;
        const EditorAnchor *b = NULL;
        EditorMarqueeBounds bounds = {0};
        for(size_t anchor_index = 0; anchor_index < object->anchor_count;
                anchor_index += 1) {
            if(object->anchors[anchor_index].id == joint->anchor_a)
                a = &object->anchors[anchor_index];
            if(object->anchors[anchor_index].id == joint->anchor_b)
                b = &object->anchors[anchor_index];
        }
        if(!joint->visible || a == NULL || b == NULL) continue;
        editor_marquee_bounds_point_add(&bounds, editor_anchor_world_get(object, a));
        editor_marquee_bounds_point_add(&bounds, editor_anchor_world_get(object, b));
        if(editor_marquee_bounds_overlap(marquee, bounds))
            (void)editor_marquee_selection_add(state,
                (EditorSelectionRef){EDITOR_SELECTION_JOINT,
                    object->id, 0, 0, joint->id});
    }
}

static const EditorSoftNode *editor_marquee_soft_node_get(
        const EditorSoftBody *body, EditorSoftNodeId id) {
    for(size_t i = 0; i < body->node_count; i += 1)
        if(body->nodes[i].id == id) return &body->nodes[i];
    return NULL;
}

static void editor_marquee_soft_children_add(EditorViewportState *state,
        const EditorObject *object, const EditorSoftBody *body,
        EditorMarqueeBounds marquee) {
    for(size_t i = 0; i < body->node_count; i += 1) {
        const EditorSoftNode *node = &body->nodes[i];
        Position world = editor_soft_node_world_get(object, body, node);
        EditorMarqueeBounds bounds = {world.x - node->radius,
            world.x + node->radius, world.y - node->radius,
            world.y + node->radius, true};
        if(node->visible && editor_marquee_bounds_overlap(marquee, bounds))
            (void)editor_marquee_selection_add(state,
                (EditorSelectionRef){EDITOR_SELECTION_SOFT_NODE,
                    object->id, body->id, 0, node->id});
    }
    for(size_t i = 0; i < body->beam_count; i += 1) {
        const EditorSoftBeam *beam = &body->beams[i];
        const EditorSoftNode *a = editor_marquee_soft_node_get(body, beam->node_a);
        const EditorSoftNode *b = editor_marquee_soft_node_get(body, beam->node_b);
        EditorMarqueeBounds bounds = {0};
        if(!beam->visible || a == NULL || b == NULL) continue;
        editor_marquee_bounds_point_add(&bounds,
            editor_soft_node_world_get(object, body, a));
        editor_marquee_bounds_point_add(&bounds,
            editor_soft_node_world_get(object, body, b));
        if(editor_marquee_bounds_overlap(marquee, bounds))
            (void)editor_marquee_selection_add(state,
                (EditorSelectionRef){EDITOR_SELECTION_SOFT_BEAM,
                    object->id, body->id, 0, beam->id});
    }
    for(size_t i = 0; i < body->area_count; i += 1) {
        const EditorSoftArea *area = &body->areas[i];
        EditorMarqueeBounds bounds = {0};
        if(!area->visible) continue;
        for(size_t node_index = 0; node_index < area->node_count; node_index += 1) {
            const EditorSoftNode *node = editor_marquee_soft_node_get(
                body, area->nodes[node_index]);
            if(node != NULL) editor_marquee_bounds_point_add(&bounds,
                editor_soft_node_world_get(object, body, node));
        }
        if(editor_marquee_bounds_overlap(marquee, bounds))
            (void)editor_marquee_selection_add(state,
                (EditorSelectionRef){EDITOR_SELECTION_SOFT_AREA,
                    object->id, body->id, 0, area->id});
    }
}

bool editor_viewport_marquee_finish(EditorViewportState *state,
        EditorProject *project, Position pointer) {
    EditorObject *object;
    EditorViewportMode starting_mode;
    Position first;
    Position second;
    EditorMarqueeBounds marquee;
    if(state == NULL || project == NULL || !state->marquee_active) return false;
    state->marquee_end = pointer;
    state->marquee_active = false;
    starting_mode = state->mode;
    object = editor_project_selected_get(project);
    editor_view_transform_set(project, state, object);
    first = editor_view_screen_to_world(state->marquee_start);
    second = editor_view_screen_to_world(state->marquee_end);
    marquee = (EditorMarqueeBounds){fminf(first.x, second.x),
        fmaxf(first.x, second.x), fminf(first.y, second.y),
        fmaxf(first.y, second.y), true};
    editor_viewport_selection_clear(state);
    if(state->mode == EDITOR_VIEWPORT_HIERARCHY) {
        for(size_t i = 0; i < project->object_count; i += 1) {
            EditorObject *candidate = &project->objects[i];
            if(candidate->visible && editor_marquee_bounds_overlap(marquee,
                    editor_marquee_object_bounds_get(candidate)))
                (void)editor_marquee_selection_add(state,
                    (EditorSelectionRef){EDITOR_SELECTION_OBJECT,
                        candidate->id, 0, 0, candidate->id});
        }
    } else if(object != NULL && state->mode == EDITOR_VIEWPORT_OBJECT) {
        editor_marquee_body_children_add(state, object, marquee);
    } else if(object != NULL) {
        EditorHierarchySelection kind = state->selection;
        if(kind == EDITOR_SELECTION_RIGID_BODY ||
                state->mode == EDITOR_VIEWPORT_RIGID_BODY) {
            for(size_t i = 0; i < object->rigid_body_count; i += 1) {
                EditorRigidBody *body = &object->rigid_bodies[i];
                if(body->visible && editor_marquee_bounds_overlap(marquee,
                        editor_marquee_rigid_body_bounds_get(object, body)))
                    (void)editor_marquee_selection_add(state,
                        (EditorSelectionRef){EDITOR_SELECTION_RIGID_BODY,
                            object->id, 0, 0, body->id});
            }
        } else if(state->mode == EDITOR_VIEWPORT_HITBOX ||
                state->mode == EDITOR_VIEWPORT_VERTEX ||
                state->mode == EDITOR_VIEWPORT_LINE) {
            EditorRigidBody *body = editor_selected_body_get(object, state);
            EditorHitbox *hitbox = editor_selected_hitbox_get(object, state);
            if(body != NULL && hitbox != NULL) {
                for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
                    Position a = editor_hitbox_vertex_world_get(object, body,
                        hitbox, i);
                    Position b = editor_hitbox_vertex_world_get(object, body,
                        hitbox, (i + 1) % hitbox->vertex_count);
                    EditorMarqueeBounds point = {a.x - 5.0f, a.x + 5.0f,
                        a.y - 5.0f, a.y + 5.0f, true};
                    EditorMarqueeBounds line = {fminf(a.x, b.x), fmaxf(a.x, b.x),
                        fminf(a.y, b.y), fmaxf(a.y, b.y), true};
                    if(editor_marquee_bounds_overlap(marquee, point))
                        (void)editor_marquee_selection_add(state,
                            (EditorSelectionRef){EDITOR_SELECTION_VERTEX,
                                object->id, body->id, hitbox->id,
                                hitbox->vertices[i].id});
                    if(editor_marquee_bounds_overlap(marquee, line))
                        (void)editor_marquee_selection_add(state,
                            (EditorSelectionRef){EDITOR_SELECTION_LINE,
                                object->id, body->id, hitbox->id, i});
                }
            }
        } else if(kind == EDITOR_SELECTION_ANCHOR ||
                state->mode == EDITOR_VIEWPORT_ANCHOR) {
            for(size_t i = 0; i < object->anchor_count; i += 1) {
                Position world = editor_anchor_world_get(object, &object->anchors[i]);
                EditorMarqueeBounds point = {world.x - 5.0f, world.x + 5.0f,
                    world.y - 5.0f, world.y + 5.0f, true};
                if(object->anchors[i].visible &&
                        editor_marquee_bounds_overlap(marquee, point))
                    (void)editor_marquee_selection_add(state,
                        (EditorSelectionRef){EDITOR_SELECTION_ANCHOR,
                            object->id, 0, 0, object->anchors[i].id});
            }
        } else if(state->mode == EDITOR_VIEWPORT_JOINT) {
            for(size_t i = 0; i < object->joint_count; i += 1) {
                EditorJoint *joint = &object->joint_items[i];
                EditorAnchor *a = editor_project_anchor_get(object, joint->anchor_a);
                EditorAnchor *b = editor_project_anchor_get(object, joint->anchor_b);
                EditorMarqueeBounds bounds = {0};
                if(!joint->visible || a == NULL || b == NULL) continue;
                editor_marquee_bounds_point_add(&bounds,
                    editor_anchor_world_get(object, a));
                editor_marquee_bounds_point_add(&bounds,
                    editor_anchor_world_get(object, b));
                if(editor_marquee_bounds_overlap(marquee, bounds))
                    (void)editor_marquee_selection_add(state,
                        (EditorSelectionRef){EDITOR_SELECTION_JOINT,
                            object->id, 0, 0, joint->id});
            }
        } else if(state->selected_soft_body != 0) {
            EditorSoftBody *body = NULL;
            for(size_t i = 0; i < object->soft_body_count; i += 1)
                if(object->soft_body_items[i].id == state->selected_soft_body)
                    body = &object->soft_body_items[i];
            if(body != NULL)
                editor_marquee_soft_children_add(state, object, body, marquee);
        }
    }
    if(state->selected_item_count > 0) {
        EditorSelectionRef primary =
            state->selected_items[state->selected_item_count - 1];
        if(!editor_viewport_selection_primary_apply(project, state, primary))
            return false;
        if(starting_mode == EDITOR_VIEWPORT_OBJECT) {
            switch(primary.kind) {
                case EDITOR_SELECTION_RIGID_BODY:
                case EDITOR_SELECTION_PARTICLE:
                    state->mode = EDITOR_VIEWPORT_RIGID_BODY;
                    break;
                case EDITOR_SELECTION_JOINT:
                    state->mode = EDITOR_VIEWPORT_JOINT;
                    break;
                case EDITOR_SELECTION_ANCHOR:
                    state->mode = EDITOR_VIEWPORT_ANCHOR;
                    break;
                case EDITOR_SELECTION_SOFT_BODY:
                    state->mode = EDITOR_VIEWPORT_SOFT_BODY;
                    break;
                default: break;
            }
        }
        return true;
    }
    state->selection = EDITOR_SELECTION_NONE;
    return true;
}

void editor_viewport_hitbox_editor_enter(EditorViewportState *state) {
    if(state == NULL) return;
    state->mode = EDITOR_VIEWPORT_HITBOX;
    state->selection = EDITOR_SELECTION_HITBOX;
    state->dragged_vertex = -1;
}

void editor_viewport_object_editor_enter(EditorViewportState *state) {
    if(state == NULL) return;
    editor_viewport_selection_clear(state);
    state->mode = EDITOR_VIEWPORT_OBJECT;
    state->selection = EDITOR_SELECTION_NONE;
    state->selected_rigid_body = 0;
    state->selected_hitbox = 0;
    state->selected_joint = 0;
    state->selected_anchor = 0;
    state->selected_soft_body = 0;
    state->selected_soft_node = 0;
    state->selected_soft_beam = 0;
    state->selected_soft_area = 0;
    state->selected_origin_kind = EDITOR_ORIGIN_NONE;
    state->selected_line = 0;
    state->selected_vertex = 0;
    state->preview_rigid_body = 0;
    state->preview_anchor = 0;
    state->preview_soft_node = 0;
    state->soft_area_candidate_count = 0;
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
    editor_viewport_selection_clear(state);
    if(state->mode == EDITOR_VIEWPORT_AUTO_SHAPE) {
        state->mode = state->auto_shape_parent_mode;
        state->selection = state->mode == EDITOR_VIEWPORT_HITBOX ?
            EDITOR_SELECTION_HITBOX : EDITOR_SELECTION_SOFT_BODY;
    } else if(state->mode == EDITOR_VIEWPORT_LINE || state->mode == EDITOR_VIEWPORT_VERTEX) {
        state->mode = EDITOR_VIEWPORT_HITBOX;
        state->selection = EDITOR_SELECTION_HITBOX;
    } else if(state->mode == EDITOR_VIEWPORT_HITBOX ||
            state->mode == EDITOR_VIEWPORT_PARTICLE) {
        state->mode = EDITOR_VIEWPORT_RIGID_BODY;
        state->selection = EDITOR_SELECTION_RIGID_BODY;
    } else if(state->mode == EDITOR_VIEWPORT_ANCHOR) {
        state->mode = EDITOR_VIEWPORT_OBJECT;
        state->selection = EDITOR_SELECTION_NONE;
        state->selected_anchor = 0;
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
    bool inside = false;
    if(object == NULL || body == NULL || area == NULL || area->node_count < 3) return false;
    for(size_t i = 0, previous = area->node_count - 1;
            i < area->node_count; previous = i++) {
        const EditorSoftNode *a = editor_soft_node_get(body, area->nodes[i]);
        const EditorSoftNode *b = editor_soft_node_get(body, area->nodes[previous]);
        Position pa;
        Position pb;
        if(a == NULL || b == NULL) return false;
        pa = editor_soft_node_world_get(object, body, a);
        pb = editor_soft_node_world_get(object, body, b);
        if(((pa.y > point.y) != (pb.y > point.y)) &&
                point.x < (pb.x - pa.x) * (point.y - pa.y) /
                    (pb.y - pa.y) + pa.x) inside = !inside;
    }
    return inside;
}

static bool editor_soft_area_beam_check(const EditorSoftArea *area,
        EditorSoftNodeId a, EditorSoftNodeId b) {
    if(area == NULL) return false;
    for(size_t i = 0; i < area->node_count; i += 1) {
        EditorSoftNodeId first = area->nodes[i];
        EditorSoftNodeId second = area->nodes[(i + 1) % area->node_count];
        if((first == a && second == b) || (first == b && second == a)) return true;
    }
    return false;
}

static bool editor_object_visual_point_contains(const EditorObject *object,
        Position point) {
    if(object == NULL || !object->visible) return false;
    for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
        const EditorRigidBody *body = &object->rigid_bodies[body_index];
        if(!body->visible) continue;
        for(size_t hitbox_index = 0; hitbox_index < body->hitbox_count; hitbox_index += 1) {
            const EditorHitbox *hitbox = &body->hitboxes[hitbox_index];
            if(hitbox->visible && editor_hitbox_point_contains(
                    object, body, hitbox, point)) return true;
        }
    }
    for(size_t body_index = 0; body_index < object->soft_body_count; body_index += 1) {
        const EditorSoftBody *body = &object->soft_body_items[body_index];
        if(!body->visible) continue;
        for(size_t node_index = 0; node_index < body->node_count; node_index += 1) {
            const EditorSoftNode *node = &body->nodes[node_index];
            Position world;
            if(!node->visible) continue;
            world = editor_soft_node_world_get(object, body, node);
            if((point.x - world.x) * (point.x - world.x) +
                    (point.y - world.y) * (point.y - world.y) <= 100.0f) return true;
        }
        for(size_t beam_index = 0; beam_index < body->beam_count; beam_index += 1) {
            const EditorSoftBeam *beam = &body->beams[beam_index];
            const EditorSoftNode *a = editor_soft_node_get(body, beam->node_a);
            const EditorSoftNode *b = editor_soft_node_get(body, beam->node_b);
            if(beam->visible && a != NULL && b != NULL &&
                    editor_segment_distance_squared(point,
                        editor_soft_node_world_get(object, body, a),
                        editor_soft_node_world_get(object, body, b)) <= 36.0f) return true;
        }
        for(size_t area_index = 0; area_index < body->area_count; area_index += 1) {
            const EditorSoftArea *area = &body->areas[area_index];
            if(area->visible && editor_soft_area_point_contains(
                    object, body, area, point)) return true;
        }
    }
    for(size_t anchor_index = 0; anchor_index < object->anchor_count; anchor_index += 1) {
        const EditorAnchor *anchor = &object->anchors[anchor_index];
        Position world;
        if(!anchor->visible) continue;
        world = editor_anchor_world_get(object, anchor);
        if((point.x - world.x) * (point.x - world.x) +
                (point.y - world.y) * (point.y - world.y) <= 100.0f) return true;
    }
    for(size_t joint_index = 0; joint_index < object->joint_count; joint_index += 1) {
        const EditorJoint *joint = &object->joint_items[joint_index];
        const EditorAnchor *a = NULL;
        const EditorAnchor *b = NULL;
        if(!joint->visible) continue;
        for(size_t anchor_index = 0; anchor_index < object->anchor_count; anchor_index += 1) {
            if(object->anchors[anchor_index].id == joint->anchor_a) {
                a = &object->anchors[anchor_index];
            }
            if(object->anchors[anchor_index].id == joint->anchor_b) {
                b = &object->anchors[anchor_index];
            }
        }
        if(a != NULL && b != NULL && editor_segment_distance_squared(point,
                editor_anchor_world_get(object, a),
                editor_anchor_world_get(object, b)) <= 64.0f) return true;
    }
    return false;
}

static Position editor_auto_shape_rigid_local_get(const EditorObject *object,
        const EditorRigidBody *body, Position world) {
    Position local = {world.x - object->position.x - body->position.x,
        world.y - object->position.y - body->position.y};
    float cosine = cosf(-body->rotation);
    float sine = sinf(-body->rotation);
    return (Position){local.x * cosine - local.y * sine,
        local.x * sine + local.y * cosine};
}

static Position editor_auto_shape_soft_local_get(const EditorObject *object,
        const EditorSoftBody *body, Position world) {
    Position local = {world.x - object->position.x - body->position.x,
        world.y - object->position.y - body->position.y};
    float cosine = cosf(-body->rotation);
    float sine = sinf(-body->rotation);
    return (Position){local.x * cosine - local.y * sine,
        local.x * sine + local.y * cosine};
}

bool editor_viewport_auto_shape_update(EditorViewportState *state,
        EditorProject *project, EditorAutoShapeConfig *config, Position pointer,
        MouseButtonState primary_button, MouseButtonState pan_button,
        bool pan_modifier, float wheel_y, bool pointer_consumed) {
    EditorObject *object;
    Position world_pointer;

    if(state == NULL || project == NULL || config == NULL) return false;
    if(wheel_y != 0.0f || pan_button != MOUSE_BUTTON_STATE_UP ||
            state->camera_panning ||
            (pan_modifier && primary_button != MOUSE_BUTTON_STATE_UP))
        return editor_viewport_update(state, project, pointer, primary_button,
            pan_button, pan_modifier, wheel_y, pointer_consumed);
    if(primary_button == MOUSE_BUTTON_STATE_RELEASED) {
        state->dragged_vertex = -1;
        return false;
    }
    if(pointer_consumed || pointer.x < 0.0f ||
            pointer.x >= EDITOR_VIEWPORT_WIDTH) return false;
    object = editor_project_selected_get(project);
    if(object == NULL) return false;
    editor_view_transform_set(project, state, object);
    world_pointer = editor_view_screen_to_world(pointer);

    if(state->auto_shape_parent_mode == EDITOR_VIEWPORT_HITBOX) {
        EditorRigidBody *body = editor_selected_body_get(object, state);
        EditorHitbox *hitbox = editor_selected_hitbox_get(object, state);
        if(body == NULL || hitbox == NULL || !body->visible || !hitbox->visible)
            return false;
        if(state->dragged_vertex >= 0 &&
                primary_button == MOUSE_BUTTON_STATE_DOWN) {
            Position desired = {world_pointer.x - state->drag_offset.x,
                world_pointer.y - state->drag_offset.y};
            EditorResult adjusted = editor_auto_shape_control_set(config,
                hitbox->vertex_count, (size_t)state->dragged_vertex,
                editor_auto_shape_rigid_local_get(object, body, desired));
            EditorCommand command;
            if(editor_result_check(adjusted)) return true;
            command = (EditorCommand){.type = EDITOR_COMMAND_AUTO_SHAPE,
                .data.auto_shape = {.kind = EDITOR_ITEM_HITBOX,
                    .object = object->id, .parent = body->id, .item = hitbox->id,
                    .config = *config}};
            (void)editor_command_execute(project, &command);
            return true;
        }
        if(primary_button != MOUSE_BUTTON_STATE_PRESSED) return false;
        for(size_t i = 0; i < hitbox->vertex_count; i += 1) {
            Position control;
            Vec2D delta;
            if(hitbox->vertices[i].position_locked ||
                    !editor_auto_shape_control_check(config,
                        hitbox->vertex_count, i)) continue;
            control = editor_hitbox_vertex_world_get(object, body, hitbox, (uint32_t)i);
            delta = (Vec2D){world_pointer.x - control.x, world_pointer.y - control.y};
            if(delta.x * delta.x + delta.y * delta.y >
                    100.0f / (editor_view_scale * editor_view_scale)) continue;
            state->dragged_vertex = (int)i;
            state->drag_offset = (Vec2D){world_pointer.x - control.x,
                world_pointer.y - control.y};
            return true;
        }
        return false;
    }
    if(state->auto_shape_parent_mode == EDITOR_VIEWPORT_SOFT_BODY) {
        EditorSoftBody *body = NULL;
        for(size_t i = 0; i < object->soft_body_count; i += 1)
            if(object->soft_body_items[i].id == state->selected_soft_body)
                body = &object->soft_body_items[i];
        if(body == NULL || !body->visible) return false;
        if(state->dragged_vertex >= 0 && primary_button == MOUSE_BUTTON_STATE_DOWN) {
            Position desired = {world_pointer.x - state->drag_offset.x,
                world_pointer.y - state->drag_offset.y};
            EditorResult adjusted = editor_auto_shape_control_set(config,
                body->node_count, (size_t)state->dragged_vertex,
                editor_auto_shape_soft_local_get(object, body, desired));
            EditorCommand command;
            if(editor_result_check(adjusted)) return true;
            command = (EditorCommand){.type = EDITOR_COMMAND_AUTO_SHAPE,
                .data.auto_shape = {.kind = EDITOR_ITEM_SOFT_BODY,
                    .object = object->id, .item = body->id, .config = *config}};
            (void)editor_command_execute(project, &command);
            return true;
        }
        if(primary_button != MOUSE_BUTTON_STATE_PRESSED) return false;
        for(size_t i = 0; i < body->node_count; i += 1) {
            Position control;
            Vec2D delta;
            if(!body->nodes[i].visible || !editor_auto_shape_control_check(
                    config, body->node_count, i)) continue;
            control = editor_soft_node_world_get(object, body, &body->nodes[i]);
            delta = (Vec2D){world_pointer.x - control.x, world_pointer.y - control.y};
            if(delta.x * delta.x + delta.y * delta.y >
                    100.0f / (editor_view_scale * editor_view_scale)) continue;
            state->dragged_vertex = (int)i;
            state->selected_soft_node = body->nodes[i].id;
            state->drag_offset = (Vec2D){world_pointer.x - control.x,
                world_pointer.y - control.y};
            return true;
        }
    }
    return false;
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
    editor_view_transform_set(project, state, object);
    if(wheel_y != 0.0f) {
        Position world = editor_view_screen_to_world(pointer);
        Position desired_origin;
        float factor = powf(1.1f, wheel_y);
        float zoom = fminf(8.0f, fmaxf(0.1f,
            project->viewport_camera_zoom * factor));
        Position center = {EDITOR_VIEWPORT_WIDTH * 0.5f,
            EDITOR_MENU_HEIGHT +
                (EDITOR_VIEWPORT_BOTTOM - EDITOR_MENU_HEIGHT) * 0.5f};
        Vec2D offset;
        desired_origin = (Position){pointer.x - world.x * zoom,
            pointer.y + world.y * zoom};
        offset = (Vec2D){desired_origin.x - center.x,
            desired_origin.y - center.y};
        if(project->viewport_local_view && object != NULL &&
                state->mode != EDITOR_VIEWPORT_HIERARCHY) {
            offset.x += object->position.x * zoom;
            offset.y -= object->position.y * zoom;
        }
        {
            EditorCommand command = {.type = EDITOR_COMMAND_VIEWPORT_CAMERA,
                .data.viewport_camera = {offset, zoom}};
            (void)editor_command_execute(project, &command);
        }
        editor_view_transform_set(project, state, object);
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
        EditorCommand command = {.type = EDITOR_COMMAND_VIEWPORT_CAMERA,
            .data.viewport_camera = {
                {project->viewport_camera_offset.x + pointer.x - state->camera_pointer.x,
                    project->viewport_camera_offset.y + pointer.y - state->camera_pointer.y},
                project->viewport_camera_zoom}};
        (void)editor_command_execute(project, &command);
        state->camera_pointer = pointer;
        return true;
    }
    pointer = editor_view_screen_to_world(pointer);
    if(state->mode == EDITOR_VIEWPORT_HIERARCHY &&
            primary_button == MOUSE_BUTTON_STATE_PRESSED) {
        for(size_t object_index = project->object_count; object_index > 0; object_index -= 1) {
            EditorObject *candidate_object = &project->objects[object_index - 1];
            Uint64 now;
            bool double_clicked;
            if(!editor_object_visual_point_contains(candidate_object, pointer)) continue;
            now = SDL_GetTicks();
            double_clicked = state->last_viewport_click_selection ==
                    EDITOR_SELECTION_OBJECT &&
                state->last_viewport_click_object == candidate_object->id &&
                now - state->last_viewport_click_at <= 400;
            (void)editor_project_object_select(project, candidate_object->id);
            if(double_clicked) {
                editor_viewport_object_editor_enter(state);
                state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
            } else {
                state->selection = EDITOR_SELECTION_OBJECT;
                state->last_viewport_click_selection = EDITOR_SELECTION_OBJECT;
                state->last_viewport_click_object = candidate_object->id;
                state->last_viewport_click_index = candidate_object->id;
                state->last_viewport_click_at = now;
            }
            return true;
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
        if(anchor != NULL) {
            Position position = editor_anchor_world_local_get(object, anchor, anchor_body,
                (Position){pointer.x - state->drag_offset.x,
                    pointer.y - state->drag_offset.y});
            EditorCommand command = {.type = EDITOR_COMMAND_ANCHOR_TRANSFORM,
                .data.anchor_transform = {object->id, anchor->id,
                    position, anchor->rotation}};
            (void)editor_command_execute(project, &command);
        }
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
                Position position = {
                    local.x * cosine - local.y * sine,
                    local.x * sine + local.y * cosine
                };
                EditorCommand command = {.type = EDITOR_COMMAND_SOFT_NODE_POSITION,
                    .data.soft_node_position = {object->id, soft_body->id,
                        soft_body->nodes[i].id, position}};
                (void)editor_command_execute(project, &command);
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
            Position position = {
                pointer.x - object->position.x - state->drag_offset.x,
                pointer.y - object->position.y - state->drag_offset.y
            };
            EditorCommand command = {.type = EDITOR_COMMAND_SOFT_BODY_TRANSFORM,
                .data.soft_body_transform = {object->id, soft_body->id,
                    position, soft_body->rotation}};
            (void)editor_command_execute(project, &command);
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
            EditorCommand command = {.type = EDITOR_COMMAND_RIGID_BODY_ORIGIN,
                .data.origin = {object->id, origin_body == NULL ? 0 : origin_body->id,
                    position}};
            return editor_command_execute(project, &command).kind == ERROR_RESULT_VALUE;
        }
        if(state->selected_origin_kind == EDITOR_ORIGIN_SOFT_BODY) {
            for(size_t i = 0; i < object->soft_body_count; i += 1) {
                if(object->soft_body_items[i].id == state->selected_soft_body)
                    {
                        EditorCommand command = {.type = EDITOR_COMMAND_SOFT_BODY_ORIGIN,
                            .data.origin = {object->id, object->soft_body_items[i].id,
                                position}};
                        return editor_command_execute(project, &command).kind ==
                            ERROR_RESULT_VALUE;
                    }
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
            EditorCommand command = {.type = EDITOR_COMMAND_SOFT_BODY_TRANSFORM,
                .data.soft_body_transform = {object->id, soft_body->id,
                    soft_body->position,
                    atan2f(pointer.y - center.y, pointer.x - center.x) +
                        state->rotation_pointer_offset}};
            (void)editor_command_execute(project, &command);
            return true;
        }
    }
    if(body != NULL && state->dragged_body && (primary_button == MOUSE_BUTTON_STATE_DOWN ||
            primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        EditorCommand command = {.type = EDITOR_COMMAND_RIGID_BODY_TRANSFORM,
            .data.rigid_body_transform = {object->id, body->id,
                {pointer.x - object->position.x - state->drag_offset.x,
                    pointer.y - object->position.y - state->drag_offset.y},
                body->rotation}};
        (void)editor_command_execute(project, &command);
        return true;
    }
    if(body != NULL && state->rotated_body && (primary_button == MOUSE_BUTTON_STATE_DOWN ||
            primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        Position center = {object->position.x + body->position.x,
            object->position.y + body->position.y};
        EditorCommand command = {.type = EDITOR_COMMAND_RIGID_BODY_TRANSFORM,
            .data.rigid_body_transform = {object->id, body->id, body->position,
                atan2f(pointer.y - center.y, pointer.x - center.x) +
                    state->rotation_pointer_offset}};
        (void)editor_command_execute(project, &command);
        return true;
    }
    if(hitbox != NULL && state->dragged_vertex >= 0 &&
            (primary_button == MOUSE_BUTTON_STATE_DOWN ||
                primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        if(hitbox->vertices[state->dragged_vertex].position_locked) return true;
        {
            Position local = {
                pointer.x - state->drag_offset.x - object->position.x -
                    body->position.x,
                pointer.y - state->drag_offset.y - object->position.y -
                    body->position.y
            };
            float cosine = cosf(-body->rotation);
            float sine = sinf(-body->rotation);
            EditorVertex *vertex = &hitbox->vertices[state->dragged_vertex];
            EditorCommand command = {.type = EDITOR_COMMAND_VERTEX_POSITION,
                .data.vertex_position = {object->id, body->id, hitbox->id,
                    vertex->id, {local.x * cosine - local.y * sine,
                        local.x * sine + local.y * cosine}}};
            (void)editor_command_execute(project, &command);
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
            if(state->selection_modifier) {
                state->selection = EDITOR_SELECTION_ORIGIN;
                state->selected_origin_kind = EDITOR_ORIGIN_RIGID_BODY;
                state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
            } else if(editing || double_clicked) {
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
            if(state->selection_modifier) {
                state->selection = EDITOR_SELECTION_ORIGIN;
                state->selected_origin_kind = EDITOR_ORIGIN_SOFT_BODY;
                state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
            } else if(editing || double_clicked) {
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

    if(object->visible && primary_button == MOUSE_BUTTON_STATE_PRESSED) {
        for(size_t i = object->rigid_body_count; i > 0; i -= 1) {
            EditorRigidBody *particle_body = &object->rigid_bodies[i - 1];
            Position center = editor_particle_center_world_get(object, particle_body);
            float distance = hypotf(pointer.x - center.x, pointer.y - center.y);
            float tolerance = 7.0f / editor_view_scale;
            Uint64 now;
            bool double_clicked;
            if(!particle_body->visible || !particle_body->particle ||
                    fabsf(distance - particle_body->particle_radius) > tolerance) continue;
            if(state->mode == EDITOR_VIEWPORT_HITBOX &&
                    state->selected_rigid_body == particle_body->id) {
                continue;
            }
            now = SDL_GetTicks();
            double_clicked = state->last_viewport_click_selection ==
                    EDITOR_SELECTION_PARTICLE &&
                state->last_viewport_click_object == object->id &&
                state->last_viewport_click_index == particle_body->id &&
                now - state->last_viewport_click_at <= 400;
            state->selected_rigid_body = particle_body->id;
            state->selection = EDITOR_SELECTION_PARTICLE;
            if(double_clicked) {
                state->mode = EDITOR_VIEWPORT_PARTICLE;
                state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
            } else {
                if(state->mode != EDITOR_VIEWPORT_PARTICLE)
                    state->mode = EDITOR_VIEWPORT_RIGID_BODY;
                state->dragged_body = true;
                state->drag_offset = (Vec2D){
                    pointer.x - object->position.x - particle_body->position.x,
                    pointer.y - object->position.y - particle_body->position.y
                };
                state->last_viewport_click_selection = EDITOR_SELECTION_PARTICLE;
                state->last_viewport_click_object = object->id;
                state->last_viewport_click_index = particle_body->id;
                state->last_viewport_click_at = now;
            }
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
                if(state->selection_modifier) {
                    state->selection = EDITOR_SELECTION_SOFT_NODE;
                    state->selected_soft_node = node->id;
                    state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
                    return true;
                }
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
                if(state->selection_modifier) {
                    state->selection = EDITOR_SELECTION_SOFT_BEAM;
                    state->selected_soft_beam = beam->id;
                    state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
                    return true;
                }
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
                    if(state->selection_modifier) {
                        state->selection = EDITOR_SELECTION_SOFT_AREA;
                        state->selected_soft_body = soft_body->id;
                        state->selected_soft_area = area_id;
                        state->last_viewport_click_selection = EDITOR_SELECTION_NONE;
                        return true;
                    }
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
                        state->dragged_soft_body = true;
                        state->drag_offset = (Vec2D){
                            pointer.x - object->position.x - soft_body->position.x,
                            pointer.y - object->position.y - soft_body->position.y
                        };
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
            if(!hitbox->vertices[i].position_locked) {
                state->dragged_vertex = (int)i;
                state->drag_offset = (Vec2D){pointer.x - vertex.x,
                    pointer.y - vertex.y};
            }
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

static void editor_viewport_particle_fills_draw(const EditorObject *object) {
    if(object == NULL || !object->visible) return;
    for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
        const EditorRigidBody *body = &object->rigid_bodies[body_index];
        Position center;
        if(!body->visible || !body->particle || body->particle_radius <= 0.0f) continue;
        center = editor_particle_center_world_get(object, body);
        editor_circle_filled_draw(center, body->particle_radius,
            graphics_color_hex_create(body->particle_fill_color));
    }
}

static bool editor_viewport_path_selected(const EditorViewportState *state,
        EditorHierarchySelection kind, EditorObjectId object, uint32_t parent,
        uint32_t container, uint32_t item) {
    return editor_viewport_selection_contains(state,
        (EditorSelectionRef){kind, object, parent, container, item});
}

static void editor_viewport_object_draw(const EditorObject *object,
    const EditorViewportState *state, bool object_selected) {
    bool object_highlighted;
    if(object == NULL || state == NULL || !object->visible) return;
    object_highlighted = (state->mode == EDITOR_VIEWPORT_HIERARCHY &&
        state->selection == EDITOR_SELECTION_OBJECT && object_selected) ||
        editor_viewport_path_selected(state, EDITOR_SELECTION_OBJECT,
            object->id, 0, 0, object->id);

    for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
        const EditorRigidBody *body = &object->rigid_bodies[body_index];
        if(!body->visible) continue;
        for(size_t box_index = 0; box_index < body->hitbox_count; box_index += 1) {
            const EditorHitbox *hitbox = &body->hitboxes[box_index];
            bool selected_body = state->selected_rigid_body == body->id;
            bool selected_hitbox = selected_body && state->selected_hitbox == hitbox->id;
            Color base = object_highlighted || state->preview_rigid_body == body->id ||
                    (state->selection == EDITOR_SELECTION_RIGID_BODY && selected_body) ||
                    (state->selection == EDITOR_SELECTION_HITBOX && selected_hitbox) ||
                    editor_viewport_path_selected(state,
                        EDITOR_SELECTION_RIGID_BODY, object->id, 0, 0, body->id) ||
                    editor_viewport_path_selected(state, EDITOR_SELECTION_HITBOX,
                        object->id, body->id, 0, hitbox->id) ?
                (Color){255, 215, 70, 255} : graphics_color_hex_create(body->border_color);
            if(!hitbox->visible) continue;
            editor_hitbox_filled_draw(object, body, hitbox,
                graphics_color_hex_create(body->surface_color));
            for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
                Position start = editor_hitbox_vertex_world_get(object, body, hitbox, i);
                Position end = editor_hitbox_vertex_world_get(
                    object, body, hitbox, (i + 1) % hitbox->vertex_count);
                bool multi_line = editor_viewport_path_selected(state,
                    EDITOR_SELECTION_LINE, object->id, body->id, hitbox->id, i);
                bool multi_vertex = editor_viewport_path_selected(state,
                    EDITOR_SELECTION_VERTEX, object->id, body->id, hitbox->id,
                    hitbox->vertices[i].id);
                Color edge = (selected_hitbox &&
                        state->selection == EDITOR_SELECTION_LINE &&
                        state->selected_line == i) || multi_line ?
                    (Color){255, 215, 70, 255} : base;
                editor_line_draw(start, end, edge);
                if((selected_hitbox && (state->selection == EDITOR_SELECTION_HITBOX ||
                        state->selection == EDITOR_SELECTION_VERTEX ||
                        state->selection == EDITOR_SELECTION_LINE ||
                        (state->mode == EDITOR_VIEWPORT_AUTO_SHAPE &&
                            state->auto_shape_parent_mode ==
                                EDITOR_VIEWPORT_HITBOX))) || multi_vertex) {
                    Color point = (state->selection == EDITOR_SELECTION_VERTEX &&
                            state->selected_vertex == i) || multi_vertex ?
                            (Color){255, 215, 70, 255} :
                        (state->mode == EDITOR_VIEWPORT_AUTO_SHAPE &&
                            state->dragged_vertex == (int)i) ?
                            (Color){255, 215, 70, 255} :
                        (hitbox->vertices[i].position_locked ?
                            (Color){245, 165, 70, 255} : (Color){235, 240, 248, 255});
                    editor_quad_draw(start, 10.0f, 10.0f, 0.0f, point);
                }
            }
        }
    }

    for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
        const EditorRigidBody *body = &object->rigid_bodies[body_index];
        Position center;
        Color ring;
        if(!body->visible || !body->particle || body->particle_radius <= 0.0f) continue;
        center = editor_particle_center_world_get(object, body);
        ring = object_highlighted ||
                (state->selection == EDITOR_SELECTION_PARTICLE &&
                    state->selected_rigid_body == body->id) ||
                editor_viewport_path_selected(state, EDITOR_SELECTION_PARTICLE,
                    object->id, 0, 0, body->id) ?
            (Color){255, 215, 70, 255} :
            graphics_color_hex_create(body->particle_ring_color);
        editor_circle_dotted_draw(center, body->particle_radius, ring);
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
                    (state->mode == EDITOR_VIEWPORT_AUTO_SHAPE &&
                        state->auto_shape_parent_mode == EDITOR_VIEWPORT_HITBOX) ||
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
        selected_body = selected_body || editor_viewport_path_selected(state,
            EDITOR_SELECTION_SOFT_BODY, object->id, 0, 0, body->id);
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
        for(size_t area_index = 0; area_index < body->area_count; area_index += 1) {
            const EditorSoftArea *area = &body->areas[area_index];
            if(area->visible) editor_soft_area_filled_draw(object, body, area,
                graphics_color_hex_create(
                    area->color_overridden ? area->color : body->area_color));
        }
        if(object_highlighted) {
            for(size_t area_index = 0; area_index < body->area_count; area_index += 1) {
                const EditorSoftArea *area = &body->areas[area_index];
                if(area->visible) editor_soft_area_filled_draw(
                    object, body, area, (Color){255, 215, 70, 48});
            }
        }
        if(selected_area != NULL && selected_area->visible) {
            editor_soft_area_filled_draw(
                object, body, selected_area, (Color){255, 215, 70, 72});
        }
        for(size_t area_index = 0; area_index < body->area_count; area_index += 1) {
            const EditorSoftArea *area = &body->areas[area_index];
            if(area->visible && editor_viewport_path_selected(state,
                    EDITOR_SELECTION_SOFT_AREA, object->id, body->id, 0, area->id))
                editor_soft_area_filled_draw(
                    object, body, area, (Color){255, 215, 70, 72});
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
                bool selected_area_edge = editor_soft_area_beam_check(
                    selected_area, beam->node_a, beam->node_b);
                if(a != NULL && b != NULL) editor_line_draw(
                editor_soft_node_world_get(object, body, a),
                editor_soft_node_world_get(object, body, b),
                object_highlighted || selected_body ||
                    (state->selection == EDITOR_SELECTION_SOFT_BEAM &&
                    state->selected_soft_body == body->id &&
                    state->selected_soft_beam == beam->id) ||
                    editor_viewport_path_selected(state,
                        EDITOR_SELECTION_SOFT_BEAM, object->id, body->id, 0,
                        beam->id) || selected_area_edge ?
                    (Color){255, 215, 70, 255} : graphics_color_hex_create(
                        beam->color_overridden ? beam->color : body->beam_color));
            }
        }
        for(size_t i = 0; i < body->node_count; i += 1) {
            const EditorSoftNode *node = &body->nodes[i];
            if(!node->visible) continue;
            editor_quad_draw(editor_soft_node_world_get(object, body, node),
                8.0f, 8.0f, 0.0f,
                object_highlighted || selected_body ||
                    (state->selection == EDITOR_SELECTION_SOFT_NODE &&
                    state->selected_soft_body == body->id &&
                    state->selected_soft_node == node->id) ||
                    editor_viewport_path_selected(state,
                        EDITOR_SELECTION_SOFT_NODE, object->id, body->id, 0,
                        node->id) ||
                    state->preview_soft_node == node->id ?
                    (Color){255, 215, 70, 255} : graphics_color_hex_create(
                        node->color_overridden ? node->color : body->node_color));
        }
    }

    if(state->mode == EDITOR_VIEWPORT_SOFT_BODY ||
            state->mode == EDITOR_VIEWPORT_SOFT_NODE ||
            (state->mode == EDITOR_VIEWPORT_AUTO_SHAPE &&
                state->auto_shape_parent_mode == EDITOR_VIEWPORT_SOFT_BODY) ||
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
            object_highlighted || (state->selection == EDITOR_SELECTION_JOINT &&
                state->selected_joint == joint->id) ||
                editor_viewport_path_selected(state, EDITOR_SELECTION_JOINT,
                    object->id, 0, 0, joint->id) ?
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
            object_highlighted || (state->selection == EDITOR_SELECTION_ANCHOR &&
                state->selected_anchor == anchor->id) ||
                editor_viewport_path_selected(state, EDITOR_SELECTION_ANCHOR,
                    object->id, 0, 0, anchor->id) ||
                state->preview_anchor == anchor->id ?
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
    editor_view_transform_set(project, state, selected);
    if(state->mode == EDITOR_VIEWPORT_HIERARCHY) {
        for(size_t i = 0; i < project->object_count; i += 1)
            editor_viewport_particle_fills_draw(&project->objects[i]);
        for(size_t i = 0; i < project->object_count; i += 1) {
            editor_viewport_object_draw(&project->objects[i], state,
                project->objects[i].id == project->selected);
        }
    } else {
        editor_viewport_particle_fills_draw(selected);
        editor_viewport_object_draw(selected, state, true);
    }
    if(state->marquee_active) {
        float left = fminf(state->marquee_start.x, state->marquee_end.x);
        float right = fmaxf(state->marquee_start.x, state->marquee_end.x);
        float top = fminf(state->marquee_start.y, state->marquee_end.y);
        float bottom = fmaxf(state->marquee_start.y, state->marquee_end.y);
        Color fill = {90, 145, 225, 42};
        Color border = {145, 190, 255, 255};
        (void)rohr_graphics_screen_rect_draw(left, top, right - left,
            bottom - top, fill);
        (void)rohr_graphics_screen_rect_draw(left, top, right - left, 1.0f, border);
        (void)rohr_graphics_screen_rect_draw(left, bottom - 1.0f,
            right - left, 1.0f, border);
        (void)rohr_graphics_screen_rect_draw(left, top, 1.0f,
            bottom - top, border);
        (void)rohr_graphics_screen_rect_draw(right - 1.0f, top, 1.0f,
            bottom - top, border);
    }
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
    if((state->selection == EDITOR_SELECTION_RIGID_BODY ||
            state->selection == EDITOR_SELECTION_PARTICLE) && body != NULL) {
        EditorCommand command = {.type = EDITOR_COMMAND_RIGID_BODY_TRANSFORM,
            .data.rigid_body_transform = {object->id, body->id,
                {body->position.x + screen_delta.x,
                    body->position.y + screen_delta.y}, body->rotation}};
        return editor_command_execute(project, &command).kind == ERROR_RESULT_VALUE;
    }
    if(state->selection == EDITOR_SELECTION_VERTEX && body != NULL) {
        EditorHitbox *hitbox = editor_selected_hitbox_get(object, state);
        float cosine = cosf(-body->rotation);
        float sine = sinf(-body->rotation);
        if(hitbox == NULL || state->selected_vertex >= hitbox->vertex_count ||
                hitbox->vertices[state->selected_vertex].position_locked) return false;
        {
            EditorVertex *vertex = &hitbox->vertices[state->selected_vertex];
            EditorCommand command = {.type = EDITOR_COMMAND_VERTEX_POSITION,
                .data.vertex_position = {object->id, body->id, hitbox->id,
                    vertex->id,
                    {vertex->position.x + screen_delta.x * cosine - screen_delta.y * sine,
                        vertex->position.y + screen_delta.x * sine +
                            screen_delta.y * cosine}}};
            return editor_command_execute(project, &command).kind == ERROR_RESULT_VALUE;
        }
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
        {
            Position position = editor_anchor_world_local_get(
                object, anchor, anchor_body, world);
            EditorCommand command = {.type = EDITOR_COMMAND_ANCHOR_TRANSFORM,
                .data.anchor_transform = {object->id, anchor->id,
                    position, anchor->rotation}};
            return editor_command_execute(project, &command).kind == ERROR_RESULT_VALUE;
        }
    }
    if(state->selection == EDITOR_SELECTION_SOFT_NODE) {
        for(size_t body_index = 0; body_index < object->soft_body_count; body_index += 1) {
            EditorSoftBody *soft_body = &object->soft_body_items[body_index];
            if(soft_body->id != state->selected_soft_body) continue;
            for(size_t node_index = 0; node_index < soft_body->node_count; node_index += 1) {
                EditorSoftNode *node = &soft_body->nodes[node_index];
                if(node->id != state->selected_soft_node) continue;
                {
                    EditorCommand command = {.type = EDITOR_COMMAND_SOFT_NODE_POSITION,
                        .data.soft_node_position = {object->id, soft_body->id, node->id,
                            {node->position.x + screen_delta.x,
                                node->position.y + screen_delta.y}}};
                    return editor_command_execute(project, &command).kind ==
                        ERROR_RESULT_VALUE;
                }
            }
        }
    }
    return false;
}
