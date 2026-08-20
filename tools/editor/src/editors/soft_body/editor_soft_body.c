/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_soft_body.h"

#include "editor_navigation.h"
#include "editors/editor_mode_controls.h"

#include <stdio.h>

static UIButtonStyle selected_style_get(void) {
    UIButtonStyle style = rohr_ui_button_style_default_get();
    style.idle = (Color){118, 96, 35, 255};
    style.hovered = (Color){145, 119, 45, 255};
    return style;
}

static UIButtonStyle delete_style_get(void) {
    UIButtonStyle style = rohr_ui_button_style_default_get();
    style.idle = (Color){145, 42, 48, 255};
    style.hovered = (Color){181, 53, 60, 255};
    style.pressed = (Color){112, 31, 37, 255};
    style.disabled = (Color){75, 35, 38, 210};
    return style;
}

static EditorSoftBody *body_get(EditorObject *object, EditorSoftBodyId id) {
    if(object == NULL) return NULL;
    for(size_t i = 0; i < object->soft_body_count; i += 1)
        if(object->soft_body_items[i].id == id) return &object->soft_body_items[i];
    return NULL;
}

bool editor_soft_body_editor_create(EditorSoftBodyEditor *editor,
        FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorSoftBodyEditor){.font = font};
#define CREATE(value, member) \
    if(!editor_mode_text_create(font, value, &editor->member)) goto fail
    CREATE("Name", name_label); CREATE("X", x_label); CREATE("Y", y_label);
    CREATE("Rotation", rotation_label); CREATE("Node Color", node_color_label);
    CREATE("Beam Color", beam_color_label); CREATE("Area Color", area_color_label);
    CREATE("Origin", origin_label); CREATE("Auto Shape", auto_shape_label);
    CREATE("Add Node", add_node_label); CREATE("Add Beam", add_beam_label);
    CREATE("[X]", visible_label); CREATE("[ ]", hidden_label);
    CREATE("Delete Soft Body", delete_label); CREATE("", x_field);
    CREATE("", y_field); CREATE("", rotation_field);
#undef CREATE
    return true;
fail:
    editor_soft_body_editor_destroy(editor);
    return false;
}

void editor_soft_body_editor_destroy(EditorSoftBodyEditor *editor) {
    if(editor == NULL) return;
#define DESTROY(member) rohr_graphics_text_destroy(&editor->member)
    DESTROY(name_label); DESTROY(x_label); DESTROY(y_label);
    DESTROY(rotation_label); DESTROY(node_color_label); DESTROY(beam_color_label);
    DESTROY(area_color_label); DESTROY(origin_label); DESTROY(auto_shape_label);
    DESTROY(add_node_label); DESTROY(add_beam_label); DESTROY(visible_label);
    DESTROY(hidden_label); DESTROY(delete_label); DESTROY(x_field);
    DESTROY(y_field); DESTROY(rotation_field);
#undef DESTROY
    for(size_t i = 0; i < EDITOR_SOFT_BODY_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->body_names[i]);
    for(size_t i = 0; i < EDITOR_SOFT_NODE_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->node_names[i]);
    for(size_t i = 0; i < EDITOR_SOFT_BEAM_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->beam_names[i]);
    for(size_t i = 0; i < EDITOR_SOFT_AREA_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->area_names[i]);
    *editor = (EditorSoftBodyEditor){0};
}

static bool hierarchy_item_draw(EditorSoftBodyEditor *editor,
        const EditorModeContext *context, EditorObject *object,
        EditorSoftBody *body, EditorSoftHierarchyItem item,
        size_t hierarchy_index) {
    EditorHierarchySelection selection;
    EditorVisibilityKind visibility;
    TextAsset *label = NULL;
    char *cache = NULL;
    const char *name = NULL;
    bool shown = false, selected = false;
    char id[64], visibility_id[72];
    float y = 476.0f + (float)hierarchy_index * 28.0f;
    if(item.kind == EDITOR_SOFT_HIERARCHY_NODE) {
        for(size_t i = 0; i < body->node_count; i += 1)
            if(body->nodes[i].id == item.id) {
                name = body->nodes[i].name; shown = body->nodes[i].visible;
                label = &editor->node_names[i]; cache = editor->node_cache[i];
            }
        selection = EDITOR_SELECTION_SOFT_NODE;
        visibility = EDITOR_VISIBILITY_SOFT_NODE;
        selected = context->viewport->selection == selection &&
            context->viewport->selected_soft_node == item.id;
        snprintf(id, sizeof(id), "editor.soft_node.%u", item.id);
        snprintf(visibility_id, sizeof(visibility_id),
            "editor.soft_node.%u.visibility", item.id);
    } else if(item.kind == EDITOR_SOFT_HIERARCHY_BEAM) {
        for(size_t i = 0; i < body->beam_count; i += 1)
            if(body->beams[i].id == item.id) {
                name = body->beams[i].name; shown = body->beams[i].visible;
                label = &editor->beam_names[i]; cache = editor->beam_cache[i];
            }
        selection = EDITOR_SELECTION_SOFT_BEAM;
        visibility = EDITOR_VISIBILITY_SOFT_BEAM;
        selected = context->viewport->selection == selection &&
            context->viewport->selected_soft_beam == item.id;
        snprintf(id, sizeof(id), "editor.soft_beam.%u", item.id);
        snprintf(visibility_id, sizeof(visibility_id),
            "editor.soft_beam.%u.visibility", item.id);
    } else {
        for(size_t i = 0; i < body->area_count; i += 1)
            if(body->areas[i].id == item.id) {
                name = body->areas[i].name; shown = body->areas[i].visible;
                label = &editor->area_names[i]; cache = editor->area_cache[i];
            }
        selection = EDITOR_SELECTION_SOFT_AREA;
        visibility = EDITOR_VISIBILITY_SOFT_AREA;
        selected = context->viewport->selection == selection &&
            context->viewport->selected_soft_area == item.id;
        snprintf(id, sizeof(id), "editor.soft_area.%u", item.id);
        snprintf(visibility_id, sizeof(visibility_id),
            "editor.soft_area.%u.visibility", item.id);
    }
    if(name == NULL || label == NULL || cache == NULL) return true;
    if(!editor_mode_named_text_sync(editor->font, name, label, cache,
            EDITOR_OBJECT_NAME_MAX)) return false;
    if(rohr_ui_button(visibility_id, shown ? &editor->visible_label :
            &editor->hidden_label,
            (UIRect){context->x + 10.0f, y, 24.0f, 24.0f}, NULL).clicked) {
        EditorCommand command = {.type = EDITOR_COMMAND_VISIBILITY,
            .data.visibility = {visibility, object->id, body->id, item.id, !shown}};
        (void)editor_command_execute(context->project, &command);
    }
    {
        UIButtonStyle style = selected_style_get();
        UIRect bounds = {context->x + 40.0f, y, context->width - 48.0f, 24.0f};
        UIButtonResult result = rohr_ui_button(id, label, bounds,
            selected || editor_viewport_selection_contains(context->viewport,
                (EditorSelectionRef){selection, object->id, body->id, 0, item.id}) ?
                &style : NULL);
        if(context->hierarchy_row != NULL)
            context->hierarchy_row(context->hierarchy_context, context->viewport,
                (EditorSelectionRef){selection, object->id, body->id, 0, item.id},
                bounds, result, hierarchy_index + 1 == body->hierarchy_count);
        if(result.clicked || result.focus_changed) {
            context->viewport->selection = selection;
            if(selection == EDITOR_SELECTION_SOFT_NODE)
                context->viewport->selected_soft_node = item.id;
            else if(selection == EDITOR_SELECTION_SOFT_BEAM)
                context->viewport->selected_soft_beam = item.id;
            else {
                context->viewport->selected_soft_area = item.id;
                context->viewport->soft_area_candidates[0] = item.id;
                context->viewport->soft_area_candidate_count = 1;
            }
            if(result.double_clicked)
                (void)editor_navigation_selected_open(context->project,
                    context->viewport);
        }
    }
    return true;
}

bool editor_soft_body_editor_draw(EditorSoftBodyEditor *editor,
        EditorAutoShapeEditor *auto_shape, const EditorModeContext *context) {
    EditorObject *object;
    EditorSoftBody *body;
    size_t body_index;
    char name[EDITOR_OBJECT_NAME_MAX];
    Position position;
    float rotation;
    UIFieldResult name_result, x_result, y_result, rotation_result;
    bool field_active;
    if(editor == NULL || auto_shape == NULL || context == NULL ||
            context->project == NULL || context->viewport == NULL) return false;
    object = editor_project_selected_get(context->project);
    body = body_get(object, context->viewport->selected_soft_body);
    if(body == NULL) return false;
    body_index = (size_t)(body - object->soft_body_items);
    if(body_index >= EDITOR_SOFT_BODY_MAX) return false;
    snprintf(name, sizeof(name), "%s", body->name);
    if(!editor_mode_named_text_sync(editor->font, body->name,
            &editor->body_names[body_index], editor->body_cache[body_index],
            EDITOR_OBJECT_NAME_MAX)) return false;
    rohr_ui_label(&editor->name_label,
        (UIRect){context->x + 40.0f, 42.0f, 48.0f, 30.0f});
    name_result = editor_mode_name_field("editor.soft_body.name", name,
        sizeof(name), &editor->body_names[body_index],
        (UIRect){context->x + 88.0f, 42.0f, context->width - 96.0f, 30.0f});
    if(name_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
            .data.item_rename = {.kind = EDITOR_ITEM_SOFT_BODY,
                .object = object->id, .item = body->id}};
        snprintf(command.data.item_rename.name,
            sizeof(command.data.item_rename.name), "%s", name);
        (void)editor_command_execute(context->project, &command);
    }
    if(rohr_ui_button("editor.soft_body.visibility", body->visible ?
            &editor->visible_label : &editor->hidden_label,
            (UIRect){context->x + 8.0f, 44.0f, 26.0f, 26.0f}, NULL).clicked) {
        EditorCommand command = {.type = EDITOR_COMMAND_VISIBILITY,
            .data.visibility = {EDITOR_VISIBILITY_SOFT_BODY, object->id,
                0, body->id, !body->visible}};
        (void)editor_command_execute(context->project, &command);
    }
    position = body->position; rotation = body->rotation;
    rohr_ui_label(&editor->x_label,
        (UIRect){context->x + 8.0f, 118.0f, 50.0f, 26.0f});
    x_result = rohr_ui_field("editor.soft_body.x",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &position.x},
        &editor->x_field, (UIRect){context->x + 60.0f, 118.0f,
            context->width - 70.0f, 26.0f}, NULL);
    rohr_ui_label(&editor->y_label,
        (UIRect){context->x + 8.0f, 154.0f, 50.0f, 26.0f});
    y_result = rohr_ui_field("editor.soft_body.y",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &position.y},
        &editor->y_field, (UIRect){context->x + 60.0f, 154.0f,
            context->width - 70.0f, 26.0f}, NULL);
    rohr_ui_label(&editor->rotation_label,
        (UIRect){context->x + 8.0f, 190.0f, 82.0f, 26.0f});
    rotation_result = rohr_ui_field("editor.soft_body.rotation",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &rotation},
        &editor->rotation_field, (UIRect){context->x + 92.0f, 190.0f,
            context->width - 102.0f, 26.0f}, NULL);
    if(x_result.changed || y_result.changed || rotation_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_SOFT_BODY_TRANSFORM,
            .data.soft_body_transform = {object->id, body->id, position, rotation}};
        (void)editor_command_execute(context->project, &command);
    }
    (void)editor_mode_color_swatch("editor.soft_body.node_color",
        &body->node_color, false, (UIRect){context->x + 100.0f, 226.0f,
            context->width - 110.0f, 26.0f}, context, EDITOR_ITEM_SOFT_BODY,
        object->id, 0, body->id, EDITOR_PROPERTY_NODE_COLOR);
    rohr_ui_label(&editor->node_color_label,
        (UIRect){context->x + 8.0f, 226.0f, 90.0f, 26.0f});
    rohr_ui_label(&editor->beam_color_label,
        (UIRect){context->x + 8.0f, 258.0f, 90.0f, 26.0f});
    (void)editor_mode_color_swatch("editor.soft_body.beam_color",
        &body->beam_color, false, (UIRect){context->x + 100.0f, 258.0f,
            context->width - 110.0f, 26.0f}, context, EDITOR_ITEM_SOFT_BODY,
        object->id, 0, body->id, EDITOR_PROPERTY_BEAM_COLOR);
    rohr_ui_label(&editor->area_color_label,
        (UIRect){context->x + 8.0f, 290.0f, 90.0f, 26.0f});
    (void)editor_mode_color_swatch("editor.soft_body.area_color",
        &body->area_color, false, (UIRect){context->x + 100.0f, 290.0f,
            context->width - 110.0f, 26.0f}, context, EDITOR_ITEM_SOFT_BODY,
        object->id, 0, body->id, EDITOR_PROPERTY_AREA_COLOR);
    {
        UIButtonStyle style = selected_style_get();
        UIButtonResult result = rohr_ui_button("editor.soft_body.origin",
            &editor->origin_label, (UIRect){context->x + 10.0f, 326.0f,
                context->width - 20.0f, 28.0f},
            context->viewport->selection == EDITOR_SELECTION_ORIGIN &&
                context->viewport->selected_origin_kind == EDITOR_ORIGIN_SOFT_BODY ?
                &style : NULL);
        if(result.clicked || result.focus_changed) {
            context->viewport->selection = EDITOR_SELECTION_ORIGIN;
            context->viewport->selected_origin_kind = EDITOR_ORIGIN_SOFT_BODY;
            if(result.double_clicked) context->viewport->mode = EDITOR_VIEWPORT_ORIGIN;
        }
    }
    if(rohr_ui_button("editor.soft_body.auto_shape", &editor->auto_shape_label,
            (UIRect){context->x + 10.0f, 360.0f,
                context->width - 20.0f, 30.0f}, NULL).clicked)
        editor->auto_shape_picker_open = !editor->auto_shape_picker_open;
    if(!editor->auto_shape_picker_open) {
        if(rohr_ui_button("editor.soft_body.add_node", &editor->add_node_label,
                (UIRect){context->x + 10.0f, 396.0f,
                    context->width - 20.0f, 30.0f}, NULL).clicked) {
            EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
                .data.item_add = {.kind = EDITOR_ITEM_SOFT_NODE,
                    .object = object->id, .parent = body->id,
                    .position = {(float)body->node_count * 24.0f, 0.0f}}};
            EditorCommandResult result = editor_command_execute(context->project, &command);
            if(result.kind == ERROR_RESULT_VALUE) {
                context->viewport->selection = EDITOR_SELECTION_SOFT_NODE;
                context->viewport->selected_soft_node = result.result.object;
            }
        }
        if(rohr_ui_button("editor.soft_body.add_beam", &editor->add_beam_label,
                (UIRect){context->x + 10.0f, 432.0f,
                    context->width - 20.0f, 30.0f}, NULL).clicked) {
            EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
                .data.item_add = {.kind = EDITOR_ITEM_SOFT_BEAM,
                    .object = object->id, .parent = body->id}};
            EditorCommandResult result = editor_command_execute(context->project, &command);
            if(result.kind == ERROR_RESULT_VALUE) {
                context->viewport->selection = EDITOR_SELECTION_SOFT_BEAM;
                context->viewport->selected_soft_beam = result.result.object;
            }
        }
        editor_project_soft_body_hierarchy_sync(body);
        for(size_t i = 0; i < body->hierarchy_count; i += 1)
            if(!hierarchy_item_draw(editor, context, object, body,
                    body->hierarchy[i], i)) return false;
    } else {
        size_t count = editor_auto_shape_soft_body_points_capture(
            context->viewport, object, body);
        int shape = editor_auto_shape_picker_draw(auto_shape,
            "editor.soft_body.auto_shape.option",
            (UIRect){context->x + 10.0f, 394.0f,
                context->width - 20.0f, 62.0f}, count > 0 ? count : body->node_count);
        if(shape >= 0) {
            auto_shape->config.kind = (EditorAutoShapeKind)shape;
            context->viewport->auto_shape_parent_mode = EDITOR_VIEWPORT_SOFT_BODY;
            (void)editor_auto_shape_editor_apply(auto_shape, context->project,
                context->viewport, EDITOR_VIEWPORT_SOFT_BODY);
            context->viewport->mode = EDITOR_VIEWPORT_AUTO_SHAPE;
            context->viewport->selection = EDITOR_SELECTION_SOFT_BODY;
            auto_shape->first_was_active = false;
            auto_shape->second_was_active = false;
            auto_shape->third_was_active = false;
            editor->auto_shape_picker_open = false;
        }
    }
    if(context->delete_open_item != NULL) {
        UIButtonStyle delete_style = delete_style_get();
        if(rohr_ui_button(
            "editor.soft_body.delete", &editor->delete_label,
            (UIRect){context->x + 10.0f,
                context->delete_y_get != NULL ?
                    context->delete_y_get(context->delete_context) : 650.0f,
                context->width - 20.0f, 34.0f}, &delete_style).clicked)
            (void)context->delete_open_item(context->delete_context);
    }
    field_active = name_result.active || x_result.active || y_result.active ||
        rotation_result.active;
    return field_active;
}
