#include "editor_viewport.h"
#include "editor_layout.h"

#include <math.h>

static Position editor_vertex_world_get(
    const EditorObject *object,
    uint32_t vertex
) {
    return (Position){
        object->position.x + object->hitbox.vertices[vertex].x,
        object->position.y + object->hitbox.vertices[vertex].y
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

void editor_viewport_state_init(EditorViewportState *state) {
    if(state == NULL) return;
    *state = (EditorViewportState){
        .dragged_vertex = -1
    };
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
        object->hitbox.vertices[state->dragged_vertex] = (Position){
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
            state->dragged_vertex = (int)i;
            return;
        }
    }
}

void editor_viewport_draw(const EditorProject *project) {
    if(project == NULL) return;
    for(size_t object_index = 0;
            object_index < project->object_count;
            object_index += 1) {
        const EditorObject *object = &project->objects[object_index];
        Color line_color = object->id == project->selected
            ? (Color){90, 190, 255, 255}
            : (Color){90, 105, 125, 255};

        if(!object->has_hitbox) continue;
        for(uint32_t i = 0; i < object->hitbox.vertex_count; i += 1) {
            Position start = editor_vertex_world_get(object, i);
            Position end = editor_vertex_world_get(
                object, (i + 1) % object->hitbox.vertex_count);

            editor_line_draw(start, end, line_color);
            if(object->id == project->selected) {
                (void)rohr_graphics_screen_quad_draw(
                    start, 10.0f, 10.0f, 0.0f,
                    (Color){235, 240, 248, 255});
            }
        }
    }
}
