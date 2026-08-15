#include "editor_auto_shape.h"

#include <math.h>

#define EDITOR_AUTO_SHAPE_PI 3.14159265358979323846f

static bool editor_auto_shape_positive_finite_check(float value) {
    return isfinite(value) && value > 0.0f;
}

static Position editor_auto_shape_lerp(Position first, Position second, float t) {
    return (Position){
        first.x + (second.x - first.x) * t,
        first.y + (second.y - first.y) * t
    };
}

static void editor_auto_shape_polygon_get(const Position *corners,
        size_t corner_count, Position *output_positions, size_t position_count) {
    size_t edge_segments[4] = {1, 1, 1, 1};
    size_t output = 0;

    for(size_t i = corner_count; i < position_count; i += 1)
        edge_segments[(i - corner_count) % corner_count] += 1;
    for(size_t edge = 0; edge < corner_count; edge += 1) {
        Position first = corners[edge];
        Position second = corners[(edge + 1) % corner_count];
        for(size_t segment = 0; segment < edge_segments[edge]; segment += 1) {
            output_positions[output] = editor_auto_shape_lerp(first, second,
                (float)segment / (float)edge_segments[edge]);
            output += 1;
        }
    }
}

static bool editor_auto_shape_corner_get(size_t corner_count, size_t point_count,
        size_t point_index, size_t *corner_index) {
    size_t edge_segments[4] = {1, 1, 1, 1};
    size_t at = 0;
    if(point_count < corner_count || point_index >= point_count) return false;
    for(size_t i = corner_count; i < point_count; i += 1)
        edge_segments[(i - corner_count) % corner_count] += 1;
    for(size_t corner = 0; corner < corner_count; corner += 1) {
        if(point_index == at) {
            if(corner_index != NULL) *corner_index = corner;
            return true;
        }
        at += edge_segments[corner];
    }
    return false;
}

EditorResult editor_auto_shape_positions_get(const EditorAutoShapeConfig *config,
        Position *output_positions, size_t position_count) {
    Position corners[4];

    if(config == NULL || output_positions == NULL)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "auto shape requires configuration and output positions");
    if(position_count < 3)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "auto shape requires at least 3 points");
    switch(config->kind) {
        case EDITOR_AUTO_SHAPE_TRIANGLE: {
            float width = config->width;
            float height = config->height;
            float apex_x = 0.0f;
            if(!editor_auto_shape_positive_finite_check(width))
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "triangle width must be greater than zero");
            if(config->triangle_kind == EDITOR_AUTO_TRIANGLE_EQUILATERAL)
                height = width * sqrtf(3.0f) * 0.5f;
            if(!editor_auto_shape_positive_finite_check(height))
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "triangle height must be greater than zero");
            if(config->triangle_kind == EDITOR_AUTO_TRIANGLE_SCALENE) {
                if(!isfinite(config->apex_offset))
                    return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                        "scalene apex offset must be finite");
                apex_x = config->apex_offset;
            }
            corners[0] = (Position){apex_x, -height * 0.5f};
            corners[1] = (Position){width * 0.5f, height * 0.5f};
            corners[2] = (Position){-width * 0.5f, height * 0.5f};
            editor_auto_shape_polygon_get(corners, 3, output_positions, position_count);
            return editor_result_value(true);
        }
        case EDITOR_AUTO_SHAPE_RECTANGLE:
            if(!editor_auto_shape_positive_finite_check(config->width) ||
                    !editor_auto_shape_positive_finite_check(config->height))
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "rectangle width and height must be greater than zero");
            if(position_count < 4)
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "rectangle auto shape requires at least 4 points");
            corners[0] = (Position){-config->width * 0.5f, -config->height * 0.5f};
            corners[1] = (Position){config->width * 0.5f, -config->height * 0.5f};
            corners[2] = (Position){config->width * 0.5f, config->height * 0.5f};
            corners[3] = (Position){-config->width * 0.5f, config->height * 0.5f};
            editor_auto_shape_polygon_get(corners, 4, output_positions, position_count);
            return editor_result_value(true);
        case EDITOR_AUTO_SHAPE_CIRCLE:
            if(!editor_auto_shape_positive_finite_check(config->radius))
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "circle radius must be greater than zero");
            for(size_t i = 0; i < position_count; i += 1) {
                float angle = -EDITOR_AUTO_SHAPE_PI * 0.5f +
                    2.0f * EDITOR_AUTO_SHAPE_PI * (float)i / (float)position_count;
                output_positions[i] = (Position){
                    cosf(angle) * config->radius,
                    sinf(angle) * config->radius
                };
            }
            return editor_result_value(true);
        default:
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "unknown auto shape kind");
    }
}

EditorResult editor_auto_shape_hitbox_apply(EditorHitbox *hitbox,
        const EditorAutoShapeConfig *config) {
    Position output_positions[EDITOR_HITBOX_VERTEX_MAX];
    EditorResult result;
    if(hitbox == NULL)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "auto shape requires a hitbox");
    result = editor_auto_shape_positions_get(config, output_positions,
        hitbox->vertex_count);
    if(editor_result_check(result)) return result;
    for(size_t i = 0; i < hitbox->vertex_count; i += 1)
        hitbox->vertices[i].position = output_positions[i];
    return editor_result_value(true);
}

EditorResult editor_auto_shape_soft_body_apply(EditorSoftBody *body,
        const EditorAutoShapeConfig *config) {
    Position output_positions[EDITOR_SOFT_NODE_MAX];
    EditorResult result;
    if(body == NULL)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "auto shape requires a soft body");
    result = editor_auto_shape_positions_get(config, output_positions, body->node_count);
    if(editor_result_check(result)) return result;
    for(size_t i = 0; i < body->node_count; i += 1)
        body->nodes[i].position = output_positions[i];
    return editor_result_value(true);
}

bool editor_auto_shape_control_check(const EditorAutoShapeConfig *config,
        size_t point_count, size_t point_index) {
    if(config == NULL || point_index >= point_count) return false;
    if(config->kind == EDITOR_AUTO_SHAPE_CIRCLE) return point_count >= 3;
    if(config->kind == EDITOR_AUTO_SHAPE_RECTANGLE)
        return editor_auto_shape_corner_get(4, point_count, point_index, NULL);
    if(config->kind == EDITOR_AUTO_SHAPE_TRIANGLE)
        return editor_auto_shape_corner_get(3, point_count, point_index, NULL);
    return false;
}

EditorResult editor_auto_shape_control_set(EditorAutoShapeConfig *config,
        size_t point_count, size_t point_index, Position position) {
    size_t corner = 0;
    const float minimum = 0.001f;
    if(config == NULL || !isfinite(position.x) || !isfinite(position.y))
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "auto shape control requires a finite position");
    if(config->kind == EDITOR_AUTO_SHAPE_CIRCLE) {
        if(!editor_auto_shape_control_check(config, point_count, point_index))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "circle point is not a shape control");
        config->radius = fmaxf(minimum, hypotf(position.x, position.y));
        return editor_result_value(true);
    }
    if(config->kind == EDITOR_AUTO_SHAPE_RECTANGLE) {
        if(!editor_auto_shape_corner_get(4, point_count, point_index, NULL))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "rectangle point is not a corner control");
        config->width = fmaxf(minimum, fabsf(position.x) * 2.0f);
        config->height = fmaxf(minimum, fabsf(position.y) * 2.0f);
        return editor_result_value(true);
    }
    if(config->kind == EDITOR_AUTO_SHAPE_TRIANGLE) {
        if(!editor_auto_shape_corner_get(3, point_count, point_index, &corner))
            return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "triangle point is not a corner control");
        if(config->triangle_kind == EDITOR_AUTO_TRIANGLE_EQUILATERAL) {
            if(corner == 0)
                config->width = fmaxf(minimum,
                    fabsf(position.y) * 4.0f / sqrtf(3.0f));
            else config->width = fmaxf(minimum, fabsf(position.x) * 2.0f);
            return editor_result_value(true);
        }
        config->height = fmaxf(minimum, fabsf(position.y) * 2.0f);
        if(corner == 0) {
            if(config->triangle_kind == EDITOR_AUTO_TRIANGLE_SCALENE)
                config->apex_offset = position.x;
        } else config->width = fmaxf(minimum, fabsf(position.x) * 2.0f);
        return editor_result_value(true);
    }
    return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "unknown auto shape kind");
}
