#ifndef ROHR_EDITOR_AUTO_SHAPE_H
#define ROHR_EDITOR_AUTO_SHAPE_H

#include "editor_project.h"

typedef enum EditorAutoShapeKind {
    EDITOR_AUTO_SHAPE_TRIANGLE,
    EDITOR_AUTO_SHAPE_RECTANGLE,
    EDITOR_AUTO_SHAPE_CIRCLE
} EditorAutoShapeKind;

typedef enum EditorAutoTriangleKind {
    EDITOR_AUTO_TRIANGLE_EQUILATERAL,
    EDITOR_AUTO_TRIANGLE_ISOSCELES,
    EDITOR_AUTO_TRIANGLE_SCALENE
} EditorAutoTriangleKind;

typedef struct EditorAutoShapeConfig {
    EditorAutoShapeKind kind;
    EditorAutoTriangleKind triangle_kind;
    float width;
    float height;
    float radius;
    float apex_offset;
} EditorAutoShapeConfig;

EditorResult editor_auto_shape_positions_get(const EditorAutoShapeConfig *config,
    Position *output_positions, size_t position_count);
EditorResult editor_auto_shape_hitbox_apply(EditorHitbox *hitbox,
    const EditorAutoShapeConfig *config);
EditorResult editor_auto_shape_soft_body_apply(EditorSoftBody *body,
    const EditorAutoShapeConfig *config);
bool editor_auto_shape_control_check(const EditorAutoShapeConfig *config,
    size_t point_count, size_t point_index);
EditorResult editor_auto_shape_control_set(EditorAutoShapeConfig *config,
    size_t point_count, size_t point_index, Position position);

#endif
