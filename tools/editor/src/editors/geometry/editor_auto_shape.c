/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_auto_shape_editor.h"

#include "editors/editor_mode_controls.h"
#include "editor_navigation.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static EditorSoftBody *soft_body_get(EditorObject *object, EditorSoftBodyId id) {
    if(object == NULL) return NULL;
    for(size_t i = 0; i < object->soft_body_count; i += 1)
        if(object->soft_body_items[i].id == id) return &object->soft_body_items[i];
    return NULL;
}

static void icon_line_draw(Position start, Position end, Color color) {
    Vec2D delta = {end.x - start.x, end.y - start.y};
    float length = sqrtf(delta.x * delta.x + delta.y * delta.y);
    if(length <= 0.0f) return;
    (void)rohr_graphics_screen_quad_draw(
        (Position){(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f},
        length, 1.5f, -atan2f(delta.y, delta.x), color);
}

static void icon_draw(UIRect bounds, EditorAutoShapeKind kind, Color color) {
    Position center = {bounds.x + bounds.width * 0.5f, bounds.y + 18.0f};
    if(kind == EDITOR_AUTO_SHAPE_TRIANGLE) {
        Position points[] = {{center.x, center.y - 9.0f},
            {center.x + 11.0f, center.y + 8.0f},
            {center.x - 11.0f, center.y + 8.0f}};
        for(size_t i = 0; i < 3; i += 1)
            icon_line_draw(points[i], points[(i + 1) % 3], color);
    } else if(kind == EDITOR_AUTO_SHAPE_RECTANGLE) {
        Position points[] = {{center.x - 11.0f, center.y - 8.0f},
            {center.x + 11.0f, center.y - 8.0f},
            {center.x + 11.0f, center.y + 8.0f},
            {center.x - 11.0f, center.y + 8.0f}};
        for(size_t i = 0; i < 4; i += 1)
            icon_line_draw(points[i], points[(i + 1) % 4], color);
    } else {
        Position previous = {center.x, center.y - 10.0f};
        for(size_t i = 1; i <= 16; i += 1) {
            float angle = -1.57079632679f + 6.28318530718f * (float)i / 16.0f;
            Position current = {center.x + cosf(angle) * 10.0f,
                center.y + sinf(angle) * 10.0f};
            icon_line_draw(previous, current, color);
            previous = current;
        }
    }
}

int editor_auto_shape_picker_draw(EditorAutoShapeEditor *editor,
        const char *id_prefix, UIRect bounds, size_t point_count) {
    const TextAsset *labels[3];
    float gap = 4.0f;
    float width = (bounds.width - gap * 2.0f) / 3.0f;
    if(editor == NULL || id_prefix == NULL) return -1;
    labels[0] = &editor->triangle_label;
    labels[1] = &editor->rectangle_label;
    labels[2] = &editor->circle_label;
    rohr_ui_surface(bounds, (Color){28, 31, 38, 255});
    rohr_ui_border(bounds, 2.0f, (Color){5, 6, 8, 255});
    for(size_t i = 0; i < 3; i += 1) {
        char id[96];
        UIRect button = {bounds.x + (width + gap) * (float)i, bounds.y,
            width, bounds.height};
        bool enabled = point_count >= (i == EDITOR_AUTO_SHAPE_RECTANGLE ? 4u : 3u);
        snprintf(id, sizeof(id), "%s.%zu", id_prefix, i);
        if(enabled) {
            UIButtonResult result = rohr_ui_button(id, labels[i], button, NULL);
            icon_draw(button, (EditorAutoShapeKind)i, (Color){230, 234, 242, 255});
            if(result.clicked) return (int)i;
        } else {
            rohr_ui_button_disabled(button, NULL);
            rohr_ui_label(labels[i], button);
            icon_draw(button, (EditorAutoShapeKind)i, (Color){105, 108, 116, 255});
        }
    }
    return -1;
}

bool editor_auto_shape_editor_create(EditorAutoShapeEditor *editor,
        FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorAutoShapeEditor){.font = font,
        .config = {.kind = EDITOR_AUTO_SHAPE_CIRCLE,
            .triangle_kind = EDITOR_AUTO_TRIANGLE_ISOSCELES,
            .width = 100.0f, .height = 100.0f, .radius = 50.0f}};
#define CREATE(value, member) \
    if(!editor_mode_text_create(font, value, &editor->member)) goto fail
    CREATE("Triangle", triangle_label);
    CREATE("Rectangle", rectangle_label);
    CREATE("Circle", circle_label);
    CREATE("Equilateral", equilateral_label);
    CREATE("Isosceles", isosceles_label);
    CREATE("Scalene", scalene_label);
    CREATE("Width", width_label);
    CREATE("Height", height_label);
    CREATE("Length", length_label);
    CREATE("Radius", radius_label);
    CREATE("Apex X", apex_offset_label);
    CREATE("", first_field);
    CREATE("", second_field);
    CREATE("", third_field);
#undef CREATE
    return true;
fail:
    editor_auto_shape_editor_destroy(editor);
    return false;
}

void editor_auto_shape_editor_destroy(EditorAutoShapeEditor *editor) {
    if(editor == NULL) return;
#define DESTROY(member) rohr_graphics_text_destroy(&editor->member)
    DESTROY(triangle_label); DESTROY(rectangle_label); DESTROY(circle_label);
    DESTROY(equilateral_label); DESTROY(isosceles_label); DESTROY(scalene_label);
    DESTROY(width_label); DESTROY(height_label); DESTROY(length_label);
    DESTROY(radius_label); DESTROY(apex_offset_label);
    DESTROY(first_field); DESTROY(second_field); DESTROY(third_field);
#undef DESTROY
    *editor = (EditorAutoShapeEditor){0};
}

bool editor_auto_shape_editor_apply(EditorAutoShapeEditor *editor,
        EditorProject *project, EditorViewportState *viewport,
        EditorViewportMode parent_mode) {
    EditorObject *object;
    EditorCommand command = {.type = EDITOR_COMMAND_AUTO_SHAPE};
    EditorCommandResult result;
    if(editor == NULL || project == NULL || viewport == NULL) return false;
    object = editor_project_selected_get(project);
    if(object == NULL) return false;
    command.data.auto_shape.object = object->id;
    command.data.auto_shape.config = editor->config;
    command.data.auto_shape.point_count = viewport->auto_shape_point_count;
    memcpy(command.data.auto_shape.points, viewport->auto_shape_points,
        viewport->auto_shape_point_count * sizeof(*viewport->auto_shape_points));
    if(parent_mode == EDITOR_VIEWPORT_HITBOX) {
        EditorRigidBody *body = editor_project_rigid_body_get(object,
            viewport->selected_rigid_body);
        EditorHitbox *hitbox = body == NULL ? NULL : editor_project_hitbox_get(body,
            viewport->selected_hitbox);
        if(hitbox == NULL) return false;
        command.data.auto_shape.kind = EDITOR_ITEM_HITBOX;
        command.data.auto_shape.parent = body->id;
        command.data.auto_shape.item = hitbox->id;
    } else if(parent_mode == EDITOR_VIEWPORT_SOFT_BODY) {
        EditorSoftBody *body = soft_body_get(object, viewport->selected_soft_body);
        if(body == NULL) return false;
        command.data.auto_shape.kind = EDITOR_ITEM_SOFT_BODY;
        command.data.auto_shape.item = body->id;
    } else return false;
    result = editor_command_execute(project, &command);
    if(result.kind == ERROR_RESULT_ERROR) {
        fprintf(stderr, "%s\n", result.result.error.message);
        return false;
    }
    return true;
}

bool editor_auto_shape_editor_draw(EditorAutoShapeEditor *editor,
        const EditorModeContext *context) {
    UIFieldResult first = {0}, second = {0}, third = {0};
    bool changed = false;
    bool first_active, second_active, third_active;
    const TextAsset *title;
    if(editor == NULL || context == NULL || context->project == NULL ||
            context->viewport == NULL) return false;
    title = editor->config.kind == EDITOR_AUTO_SHAPE_TRIANGLE ?
        &editor->triangle_label : editor->config.kind == EDITOR_AUTO_SHAPE_RECTANGLE ?
        &editor->rectangle_label : &editor->circle_label;
    rohr_ui_label(title, (UIRect){context->x + 10.0f, 44.0f,
        context->width - 20.0f, 30.0f});
    if(editor->config.kind == EDITOR_AUTO_SHAPE_CIRCLE) {
        rohr_ui_label(&editor->radius_label,
            (UIRect){context->x + 10.0f, 88.0f, 80.0f, 28.0f});
        first = rohr_ui_field("editor.auto_shape.radius",
            (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                .number = &editor->config.radius}, &editor->first_field,
            (UIRect){context->x + 94.0f, 88.0f,
                context->width - 104.0f, 28.0f}, NULL);
    } else {
        rohr_ui_label(&editor->width_label,
            (UIRect){context->x + 10.0f, 88.0f, 80.0f, 28.0f});
        first = rohr_ui_field("editor.auto_shape.width",
            (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &editor->config.width},
            &editor->first_field, (UIRect){context->x + 94.0f, 88.0f,
                context->width - 104.0f, 28.0f}, NULL);
        rohr_ui_label(editor->config.kind == EDITOR_AUTO_SHAPE_RECTANGLE ?
                &editor->length_label : &editor->height_label,
            (UIRect){context->x + 10.0f, 124.0f, 80.0f, 28.0f});
        if(editor->config.kind == EDITOR_AUTO_SHAPE_TRIANGLE &&
                editor->config.triangle_kind == EDITOR_AUTO_TRIANGLE_EQUILATERAL) {
            editor_mode_numeric_disabled_draw(&editor->second_field,
                editor->config.width * sqrtf(3.0f) * 0.5f,
                (UIRect){context->x + 94.0f, 124.0f,
                    context->width - 104.0f, 28.0f});
        } else second = rohr_ui_field("editor.auto_shape.height",
            (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &editor->config.height},
            &editor->second_field, (UIRect){context->x + 94.0f, 124.0f,
                context->width - 104.0f, 28.0f}, NULL);
        if(editor->config.kind == EDITOR_AUTO_SHAPE_TRIANGLE) {
            const TextAsset *options[] = {&editor->equilateral_label,
                &editor->isosceles_label, &editor->scalene_label};
            UIDropdownResult result = rohr_ui_dropdown("editor.auto_shape.triangle_kind",
                options, 3, (size_t)editor->config.triangle_kind,
                (UIRect){context->x + 10.0f, 164.0f,
                    context->width - 20.0f, 28.0f}, NULL);
            if(result.changed) {
                editor->config.triangle_kind =
                    (EditorAutoTriangleKind)result.selected_index;
                changed = true;
            }
            if(editor->config.triangle_kind == EDITOR_AUTO_TRIANGLE_SCALENE) {
                rohr_ui_label(&editor->apex_offset_label,
                    (UIRect){context->x + 10.0f, 202.0f, 80.0f, 28.0f});
                third = rohr_ui_field("editor.auto_shape.apex_offset",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &editor->config.apex_offset}, &editor->third_field,
                    (UIRect){context->x + 94.0f, 202.0f,
                        context->width - 104.0f, 28.0f}, NULL);
            }
        }
    }
    first_active = first.active && !first.submitted;
    second_active = second.active && !second.submitted;
    third_active = third.active && !third.submitted;
    changed = changed || (editor->first_was_active && !first_active) ||
        (editor->second_was_active && !second_active) ||
        (editor->third_was_active && !third_active) || first.submitted ||
        second.submitted || third.submitted;
    if(changed) (void)editor_auto_shape_editor_apply(editor, context->project,
        context->viewport, context->viewport->auto_shape_parent_mode);
    editor->first_was_active = first_active;
    editor->second_was_active = second_active;
    editor->third_was_active = third_active;
    return first_active || second_active || third_active;
}

size_t editor_auto_shape_hitbox_points_capture(EditorViewportState *viewport,
        const EditorObject *object, const EditorRigidBody *body,
        const EditorHitbox *hitbox) {
    if(viewport == NULL || object == NULL || body == NULL || hitbox == NULL) return 0;
    viewport->auto_shape_point_count = 0;
    for(size_t i = 0; i < hitbox->vertex_count; i += 1)
        if(editor_viewport_selection_contains(viewport,
                (EditorSelectionRef){EDITOR_SELECTION_VERTEX, object->id,
                    body->id, hitbox->id, hitbox->vertices[i].id}))
            viewport->auto_shape_points[viewport->auto_shape_point_count++] =
                hitbox->vertices[i].id;
    return viewport->auto_shape_point_count;
}

size_t editor_auto_shape_soft_body_points_capture(EditorViewportState *viewport,
        const EditorObject *object, const EditorSoftBody *body) {
    if(viewport == NULL || object == NULL || body == NULL) return 0;
    viewport->auto_shape_point_count = 0;
    for(size_t i = 0; i < body->node_count; i += 1)
        if(editor_viewport_selection_contains(viewport,
                (EditorSelectionRef){EDITOR_SELECTION_SOFT_NODE, object->id,
                    body->id, 0, body->nodes[i].id}))
            viewport->auto_shape_points[viewport->auto_shape_point_count++] =
                body->nodes[i].id;
    return viewport->auto_shape_point_count;
}
