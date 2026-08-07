#include "editor_viewport.h"
#include "editor_layout.h"

#include <math.h>

static Position editor_vertex_world_get(
    const EditorObject *object,
    uint32_t vertex
) {
    return (Position){
        object->position.x + object->hitbox.vertices[vertex].position.x,
        object->position.y + object->hitbox.vertices[vertex].position.y
    };
}

static void editor_line_draw(Position start, Position end, Color color) {
    Vec2D delta = {end.x - start.x, end.y - start.y};
    float length = sqrtf(delta.x * delta.x + delta.y * delta.y);

    if(length <= 0.0f) return;
    (void)rohr_graphics_screen_quad_draw(
        (Position){(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f},
        length,
        2.0f,
        -atan2f(delta.y, delta.x),
        color);
}

static bool editor_object_point_contains(const EditorObject *object, Position point) {
    bool inside = false;
    uint32_t previous;

    if(object == NULL || !object->has_hitbox || object->hitbox.vertex_count < 3) {
        return false;
    }
    previous = object->hitbox.vertex_count - 1;
    for(uint32_t i = 0; i < object->hitbox.vertex_count; i += 1) {
        Position current = editor_vertex_world_get(object, i);
        Position prior = editor_vertex_world_get(object, previous);
        bool crosses = (current.y > point.y) != (prior.y > point.y) &&
            point.x < (prior.x - current.x) * (point.y - current.y) /
                (prior.y - current.y) + current.x;

        if(crosses) inside = !inside;
        previous = i;
    }
    return inside;
}

static bool editor_viewport_double_click_get(
    EditorViewportState *state,
    EditorHierarchySelection selection,
    EditorObjectId object,
    uint32_t index
) {
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
    *state = (EditorViewportState){
        .dragged_vertex = -1
    };
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
        state->mode = EDITOR_VIEWPORT_OBJECT;
        state->selection = EDITOR_SELECTION_OBJECT;
    } else if(state->mode == EDITOR_VIEWPORT_OBJECT) {
        state->mode = EDITOR_VIEWPORT_HIERARCHY;
        state->selection = EDITOR_SELECTION_OBJECT;
    }
    state->dragged_vertex = -1;
}

void editor_viewport_update(
    EditorViewportState *state,
    EditorProject *project,
    Position pointer,
    MouseButtonState primary_button,
    bool pointer_consumed
) {
    EditorObject *object;

    if(state == NULL || project == NULL) return;
    if(!pointer_consumed && pointer.x >= 0.0f &&
            pointer.x < EDITOR_VIEWPORT_WIDTH &&
            primary_button == MOUSE_BUTTON_STATE_PRESSED &&
            state->mode == EDITOR_VIEWPORT_HIERARCHY) {
        for(size_t i = project->object_count; i > 0; i -= 1) {
            EditorObject *candidate = &project->objects[i - 1];
            if(!editor_object_point_contains(candidate, pointer)) continue;
            (void)editor_project_object_select(project, candidate->id);
            state->selection = EDITOR_SELECTION_OBJECT;
            if(editor_viewport_double_click_get(state, EDITOR_SELECTION_OBJECT,
                    candidate->id, 0)) {
                editor_viewport_object_editor_enter(state);
            }
            return;
        }
    }
    object = editor_project_selected_get(project);
    if(object == NULL || !object->has_hitbox) {
        state->dragged_vertex = -1;
        return;
    }
    if(primary_button == MOUSE_BUTTON_STATE_RELEASED) {
        state->dragged_vertex = -1;
        return;
    }
    if(state->dragged_vertex >= 0 &&
            (primary_button == MOUSE_BUTTON_STATE_DOWN ||
                primary_button == MOUSE_BUTTON_STATE_PRESSED)) {
        if(object->hitbox.vertices[state->dragged_vertex].position_locked) return;
        object->hitbox.vertices[state->dragged_vertex].position = (Position){
            pointer.x - object->position.x,
            pointer.y - object->position.y
        };
        return;
    }
    if(pointer_consumed || pointer.x < 0.0f ||
            pointer.x >= EDITOR_VIEWPORT_WIDTH ||
            primary_button != MOUSE_BUTTON_STATE_PRESSED) return;
    for(uint32_t i = 0; i < object->hitbox.vertex_count; i += 1) {
        Position vertex = editor_vertex_world_get(object, i);
        Vec2D delta = {pointer.x - vertex.x, pointer.y - vertex.y};

        if(delta.x * delta.x + delta.y * delta.y <= 100.0f) {
            state->selection = EDITOR_SELECTION_VERTEX;
            state->selected_vertex = i;
            if(editor_viewport_double_click_get(state, EDITOR_SELECTION_VERTEX,
                    object->id, i)) {
                editor_viewport_vertex_editor_enter(state, i);
            } else if(state->mode != EDITOR_VIEWPORT_VERTEX) {
                state->mode = EDITOR_VIEWPORT_HITBOX;
            }
            if(!object->hitbox.vertices[i].position_locked) {
                state->dragged_vertex = (int)i;
            }
            return;
        }
    }
    for(uint32_t i = 0; i < object->hitbox.vertex_count; i += 1) {
        Position start = editor_vertex_world_get(object, i);
        Position end = editor_vertex_world_get(object,
            (i + 1) % object->hitbox.vertex_count);
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
        if(distance.x * distance.x + distance.y * distance.y <= 36.0f) {
            state->selection = EDITOR_SELECTION_LINE;
            state->selected_line = i;
            if(editor_viewport_double_click_get(state, EDITOR_SELECTION_LINE,
                    object->id, i)) {
                editor_viewport_line_editor_enter(state, i);
            } else if(state->mode != EDITOR_VIEWPORT_LINE) {
                state->mode = EDITOR_VIEWPORT_HITBOX;
            }
            return;
        }
    }
    if(editor_object_point_contains(object, pointer)) {
        state->selection = EDITOR_SELECTION_HITBOX;
        if(editor_viewport_double_click_get(state, EDITOR_SELECTION_HITBOX,
                object->id, 0)) {
            editor_viewport_hitbox_editor_enter(state);
        } else if(state->mode != EDITOR_VIEWPORT_OBJECT) {
            state->mode = EDITOR_VIEWPORT_HITBOX;
        }
    }
}

void editor_viewport_draw(
    const EditorProject *project,
    const EditorViewportState *state
) {
    if(project == NULL || state == NULL) return;
    for(size_t object_index = 0;
            object_index < project->object_count;
            object_index += 1) {
        const EditorObject *object = &project->objects[object_index];
        bool selected_object = object->id == project->selected;
        Color line_color = selected_object
            ? (Color){90, 190, 255, 255}
            : (Color){90, 105, 125, 255};

        if(selected_object && (state->selection == EDITOR_SELECTION_OBJECT ||
                state->selection == EDITOR_SELECTION_HITBOX)) {
            line_color = (Color){255, 215, 70, 255};
        }

        if(!object->has_hitbox) continue;
        for(uint32_t i = 0; i < object->hitbox.vertex_count; i += 1) {
            Position start = editor_vertex_world_get(object, i);
            Position end = editor_vertex_world_get(
                object, (i + 1) % object->hitbox.vertex_count);
            Color edge_color = line_color;

            if(selected_object && state->selection == EDITOR_SELECTION_LINE &&
                    state->selected_line == i) {
                edge_color = (Color){255, 215, 70, 255};
            }

            editor_line_draw(start, end, edge_color);
            if(selected_object && state->selection != EDITOR_SELECTION_NONE &&
                    state->selection != EDITOR_SELECTION_OBJECT) {
                Color vertex_color = object->hitbox.vertices[i].position_locked
                    ? (Color){245, 165, 70, 255}
                    : (Color){235, 240, 248, 255};

                if(state->selection == EDITOR_SELECTION_VERTEX &&
                        state->selected_vertex == i) {
                    vertex_color = (Color){255, 215, 70, 255};
                }
                (void)rohr_graphics_screen_quad_draw(
                    start, 10.0f, 10.0f, 0.0f,
                    vertex_color);
            }
        }
    }
}
