#include "editor_viewport.h"
#include "editor_layout.h"

#include <math.h>

static Position editor_hitbox_vertex_world_get(const EditorObject *object,
    const EditorRigidBody *body, const EditorHitbox *hitbox, uint32_t vertex) {
    return (Position){
        object->position.x + body->position.x + hitbox->vertices[vertex].position.x,
        object->position.y + body->position.y + hitbox->vertices[vertex].position.y
    };
}

static Position editor_soft_node_world_get(const EditorObject *object,
    const EditorSoftBody *body, const EditorSoftNode *node) {
    return (Position){object->position.x + body->position.x + node->position.x,
        object->position.y + body->position.y + node->position.y};
}

static Position editor_anchor_world_get(const EditorObject *object,
    const EditorAnchor *anchor) {
    const EditorRigidBody *body = NULL;
    if(object == NULL || anchor == NULL) return (Position){0};
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        if(object->rigid_bodies[i].id == anchor->rigid_body) body = &object->rigid_bodies[i];
    }
    return (Position){object->position.x + (body == NULL ? 0.0f : body->position.x) +
            anchor->position.x,
        object->position.y + (body == NULL ? 0.0f : body->position.y) + anchor->position.y};
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

static void editor_line_draw(Position start, Position end, Color color) {
    Vec2D delta = {end.x - start.x, end.y - start.y};
    float length = sqrtf(delta.x * delta.x + delta.y * delta.y);

    if(length <= 0.0f) return;
    (void)rohr_graphics_screen_quad_draw(
        (Position){(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f},
        length, 2.0f, -atan2f(delta.y, delta.x), color);
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

static bool editor_viewport_double_click_get(EditorViewportState *state,
    EditorHierarchySelection selection, EditorObjectId object, uint32_t index) {
    Uint64 now;
    bool double_clicked;

    if(state == NULL) return false;
    now = SDL_GetTicks();
    double_clicked = state->last_viewport_click_selection == selection &&
        state->last_viewport_click_object == object &&
        state->last_viewport_click_index == index &&
        now - state->last_viewport_click_at <= 400;
    state->last_viewport_click_selection = selection;
    state->last_viewport_click_object = object;
    state->last_viewport_click_index = index;
    state->last_viewport_click_at = now;
    return double_clicked;
}

void editor_viewport_state_init(EditorViewportState *state) {
    if(state == NULL) return;
    *state = (EditorViewportState){.dragged_vertex = -1};
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
    } else if(state->mode == EDITOR_VIEWPORT_RIGID_BODY ||
            state->mode == EDITOR_VIEWPORT_JOINT ||
            state->mode == EDITOR_VIEWPORT_SOFT_BODY) {
        state->mode = EDITOR_VIEWPORT_OBJECT;
        state->selection = EDITOR_SELECTION_OBJECT;
    } else if(state->mode == EDITOR_VIEWPORT_SOFT_NODE ||
            state->mode == EDITOR_VIEWPORT_SOFT_BEAM) {
        state->mode = EDITOR_VIEWPORT_SOFT_BODY;
        state->selection = EDITOR_SELECTION_SOFT_BODY;
    } else if(state->mode == EDITOR_VIEWPORT_OBJECT) {
        state->mode = EDITOR_VIEWPORT_HIERARCHY;
        state->selection = EDITOR_SELECTION_OBJECT;
    }
    state->dragged_vertex = -1;
}

bool editor_viewport_update(EditorViewportState *state, EditorProject *project,
    Position pointer, MouseButtonState primary_button, bool pointer_consumed) {
    EditorObject *object;
    EditorRigidBody *body;
    EditorHitbox *hitbox;

    if(state == NULL || project == NULL) return false;
    object = editor_project_selected_get(project);
    if(object == NULL || pointer_consumed || pointer.x < 0.0f ||
            pointer.x >= EDITOR_VIEWPORT_WIDTH) return false;
    if(primary_button == MOUSE_BUTTON_STATE_RELEASED) {
        state->dragged_vertex = -1;
        return false;
    }
    body = editor_selected_body_get(object, state);
    hitbox = editor_selected_hitbox_get(object, state);
    if(hitbox != NULL && state->dragged_vertex >= 0 &&
            (primary_button == MOUSE_BUTTON_STATE_DOWN ||
                primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        if(hitbox->vertices[state->dragged_vertex].position_locked) return true;
        hitbox->vertices[state->dragged_vertex].position = (Position){
            pointer.x - object->position.x - body->position.x,
            pointer.y - object->position.y - body->position.y};
        return true;
    }
    if(primary_button != MOUSE_BUTTON_STATE_PRESSED) return false;

    if(object->visible) {
        for(size_t soft_index = 0; soft_index < object->soft_body_count; soft_index += 1) {
            EditorSoftBody *soft_body = &object->soft_body_items[soft_index];
            if(!soft_body->visible) continue;
            for(size_t i = 0; i < soft_body->node_count; i += 1) {
                EditorSoftNode *node = &soft_body->nodes[i];
                Position world = editor_soft_node_world_get(object, soft_body, node);
                if(!node->visible || (pointer.x - world.x) * (pointer.x - world.x) +
                        (pointer.y - world.y) * (pointer.y - world.y) > 100.0f) continue;
                state->selection = EDITOR_SELECTION_SOFT_NODE;
                state->selected_soft_body = soft_body->id;
                state->selected_soft_node = node->id;
                state->mode = EDITOR_VIEWPORT_SOFT_BODY;
                if(editor_viewport_double_click_get(state, EDITOR_SELECTION_SOFT_NODE,
                        object->id, node->id)) state->mode = EDITOR_VIEWPORT_SOFT_NODE;
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
                state->selection = EDITOR_SELECTION_SOFT_BEAM;
                state->selected_soft_body = soft_body->id;
                state->selected_soft_beam = beam->id;
                state->mode = EDITOR_VIEWPORT_SOFT_BODY;
                if(editor_viewport_double_click_get(state, EDITOR_SELECTION_SOFT_BEAM,
                        object->id, beam->id)) state->mode = EDITOR_VIEWPORT_SOFT_BEAM;
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
            if(editor_viewport_double_click_get(state, EDITOR_SELECTION_VERTEX,
                    object->id, hitbox->vertices[i].id)) {
                editor_viewport_vertex_editor_enter(state, i);
            } else {
                state->mode = EDITOR_VIEWPORT_HITBOX;
            }
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
            if(editor_viewport_double_click_get(state, EDITOR_SELECTION_LINE,
                    object->id, hitbox->vertices[i].id)) {
                editor_viewport_line_editor_enter(state, i);
            } else {
                state->mode = EDITOR_VIEWPORT_HITBOX;
            }
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
                if(editor_viewport_double_click_get(state, EDITOR_SELECTION_OBJECT,
                        object->id, 0)) editor_viewport_object_editor_enter(state);
            } else if(state->mode == EDITOR_VIEWPORT_OBJECT) {
                state->selection = EDITOR_SELECTION_RIGID_BODY;
                state->selected_rigid_body = candidate_body->id;
                if(editor_viewport_double_click_get(state, EDITOR_SELECTION_RIGID_BODY,
                        object->id, candidate_body->id)) state->mode = EDITOR_VIEWPORT_RIGID_BODY;
            } else {
                state->selection = EDITOR_SELECTION_HITBOX;
                state->selected_rigid_body = candidate_body->id;
                state->selected_hitbox = candidate->id;
                if(editor_viewport_double_click_get(state, EDITOR_SELECTION_HITBOX,
                        object->id, candidate->id)) editor_viewport_hitbox_editor_enter(state);
            }
            return true;
        }
    }
    return false;
}

void editor_viewport_draw(const EditorProject *project, const EditorViewportState *state) {
    const EditorObject *object;

    if(project == NULL || state == NULL || state->selection == EDITOR_SELECTION_NONE) return;
    object = NULL;
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(project->objects[i].id == project->selected) object = &project->objects[i];
    }
    if(object == NULL || !object->visible) return;

    for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
        const EditorRigidBody *body = &object->rigid_bodies[body_index];
        if(!body->visible) continue;
        for(size_t box_index = 0; box_index < body->hitbox_count; box_index += 1) {
            const EditorHitbox *hitbox = &body->hitboxes[box_index];
            bool selected_body = state->selected_rigid_body == body->id;
            bool selected_hitbox = selected_body && state->selected_hitbox == hitbox->id;
            Color base = state->selection == EDITOR_SELECTION_OBJECT ||
                    (state->selection == EDITOR_SELECTION_RIGID_BODY && selected_body) ||
                    (state->selection == EDITOR_SELECTION_HITBOX && selected_hitbox) ?
                (Color){255, 215, 70, 255} : (Color){90, 105, 125, 255};
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
                    (void)rohr_graphics_screen_quad_draw(start, 10.0f, 10.0f, 0.0f, point);
                }
            }
        }
    }

    for(size_t soft_index = 0; soft_index < object->soft_body_count; soft_index += 1) {
        const EditorSoftBody *body = &object->soft_body_items[soft_index];
        if(!body->visible) continue;
        for(size_t beam_index = 0; beam_index < body->beam_count; beam_index += 1) {
            const EditorSoftBeam *beam = &body->beams[beam_index];
            const EditorSoftNode *a = NULL;
            const EditorSoftNode *b = NULL;
            if(!beam->visible) continue;
            for(size_t i = 0; i < body->node_count; i += 1) {
                if(body->nodes[i].id == beam->node_a) a = &body->nodes[i];
                if(body->nodes[i].id == beam->node_b) b = &body->nodes[i];
            }
            if(a != NULL && b != NULL) editor_line_draw(
                editor_soft_node_world_get(object, body, a),
                editor_soft_node_world_get(object, body, b),
                state->selection == EDITOR_SELECTION_SOFT_BEAM &&
                    state->selected_soft_beam == beam->id ?
                    (Color){255, 215, 70, 255} : (Color){90, 170, 220, 255});
        }
        for(size_t i = 0; i < body->node_count; i += 1) {
            const EditorSoftNode *node = &body->nodes[i];
            if(!node->visible) continue;
            (void)rohr_graphics_screen_quad_draw(
                editor_soft_node_world_get(object, body, node), 8.0f, 8.0f, 0.0f,
                state->selection == EDITOR_SELECTION_SOFT_NODE &&
                    state->selected_soft_node == node->id ?
                    (Color){255, 215, 70, 255} : (Color){150, 220, 255, 255});
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
        if(a != NULL && b != NULL) editor_line_draw(
            editor_anchor_world_get(object, a), editor_anchor_world_get(object, b),
            state->selection == EDITOR_SELECTION_JOINT &&
                state->selected_joint == joint->id ?
                (Color){255, 215, 70, 255} : (Color){220, 120, 210, 255});
    }
    for(size_t i = 0; i < object->anchor_count; i += 1) {
        const EditorAnchor *anchor = &object->anchors[i];
        if(!anchor->visible) continue;
        (void)rohr_graphics_screen_quad_draw(editor_anchor_world_get(object, anchor),
            9.0f, 9.0f, 0.78539816339f,
            state->selected_anchor == anchor->id ?
                (Color){255, 215, 70, 255} : (Color){235, 150, 215, 255});
    }
}
