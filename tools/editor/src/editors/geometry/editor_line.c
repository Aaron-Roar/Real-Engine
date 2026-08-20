/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_line.h"

#include "editors/editor_mode_controls.h"

#include <stdio.h>

static void editor_line_length_set(EditorProject *project,
        EditorObjectId object,
        EditorRigidBodyId body,
        EditorHitboxId hitbox,
        uint32_t line,
        float value) {
    EditorCommand command = {.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = {EDITOR_ITEM_LINE, object, body, hitbox, line,
            EDITOR_PROPERTY_LINE_LENGTH, EDITOR_PROPERTY_VALUE_FLOAT,
            {.number = value}}};
    (void)editor_command_execute(project, &command);
}

bool editor_line_editor_create(EditorLineEditor *editor, FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorLineEditor){.font = font};
    if(!editor_mode_text_create(font, "Name", &editor->name_label) ||
            !editor_mode_text_create(font, "Add Vertex",
                &editor->add_vertex_label) ||
            !editor_mode_text_create(font, "Length", &editor->length_label) ||
            !editor_mode_text_create(font, "Line distance fully constrained",
                &editor->constrained_label) ||
            !editor_mode_text_create(font, "", &editor->length_field) ||
            !editor_mode_text_create(font, "Delete Line", &editor->delete_label)) {
        editor_line_editor_destroy(editor);
        return false;
    }
    for(size_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
        char name[32];
        snprintf(name, sizeof(name), "line_%zu", i + 1);
        if(!editor_mode_text_create(font, name, &editor->line_labels[i])) {
            editor_line_editor_destroy(editor);
            return false;
        }
    }
    return true;
}

void editor_line_editor_destroy(EditorLineEditor *editor) {
    if(editor == NULL) return;
    rohr_graphics_text_destroy(&editor->name_label);
    rohr_graphics_text_destroy(&editor->add_vertex_label);
    rohr_graphics_text_destroy(&editor->length_label);
    rohr_graphics_text_destroy(&editor->constrained_label);
    rohr_graphics_text_destroy(&editor->length_field);
    rohr_graphics_text_destroy(&editor->delete_label);
    for(size_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->line_labels[i]);
    *editor = (EditorLineEditor){0};
}

bool editor_line_editor_draw(EditorLineEditor *editor,
        const EditorModeContext *context) {
    EditorObject *object;
    EditorRigidBody *body;
    EditorHitbox *hitbox;
    EditorVertex *a;
    EditorVertex *b;
    uint32_t line;
    bool constrained;
    bool vertex_inserted = false;
    bool field_active;
    float length;
    UIFieldResult name_result;
    char edited_name[EDITOR_OBJECT_NAME_MAX];
    if(editor == NULL || context == NULL || context->project == NULL ||
            context->viewport == NULL) return false;
    object = editor_project_selected_get(context->project);
    body = object == NULL ? NULL : editor_project_rigid_body_get(object,
        context->viewport->selected_rigid_body);
    hitbox = body == NULL ? NULL : editor_project_hitbox_get(body,
        context->viewport->selected_hitbox);
    line = context->viewport->selected_line;
    if(hitbox == NULL || line >= hitbox->vertex_count ||
            line >= EDITOR_HITBOX_VERTEX_MAX) return false;
    a = &hitbox->vertices[line];
    b = &hitbox->vertices[(line + 1) % hitbox->vertex_count];
    constrained = a->position_locked && b->position_locked;
    length = editor_project_hitbox_line_length_get(hitbox, line);
    snprintf(edited_name, sizeof(edited_name), "%s", hitbox->line_names[line]);
    if(!editor_mode_named_text_sync(editor->font, hitbox->line_names[line],
            &editor->line_labels[line], editor->line_name_cache[line],
            EDITOR_OBJECT_NAME_MAX)) return false;
    rohr_ui_label(&editor->name_label,
        (UIRect){context->x + 8.0f, 48.0f, 48.0f, 24.0f});
    name_result = editor_mode_name_field("editor.line.name", edited_name,
        sizeof(edited_name), &editor->line_labels[line],
        (UIRect){context->x + 56.0f, 48.0f, context->width - 64.0f, 24.0f});
    field_active = name_result.active;
    if(name_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
            .data.item_rename = {.kind = EDITOR_ITEM_LINE,
                .object = object->id, .parent = body->id,
                .item = hitbox->id, .index = line}};
        snprintf(command.data.item_rename.name,
            sizeof(command.data.item_rename.name), "%s", edited_name);
        (void)editor_command_execute(context->project, &command);
    }
    if(rohr_ui_button("editor.line.add_vertex", &editor->add_vertex_label,
            (UIRect){context->x + 10.0f, 82.0f,
                context->width - 20.0f, 34.0f}, NULL).clicked) {
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
            .data.item_add = {.kind = EDITOR_ITEM_VERTEX,
                .object = object->id, .parent = body->id,
                .first = hitbox->id, .index = line}};
        if(editor_command_execute(context->project, &command).kind ==
                ERROR_RESULT_VALUE) {
            editor_viewport_hitbox_editor_enter(context->viewport);
            vertex_inserted = true;
        }
    }
    if(vertex_inserted) return field_active;
    rohr_ui_label(&editor->length_label,
        (UIRect){context->x + 8.0f, 150.0f, 52.0f, 26.0f});
    if(constrained) {
        editor_mode_numeric_disabled_draw(&editor->length_field, length,
            (UIRect){context->x + 60.0f, 150.0f,
                context->width - 70.0f, 26.0f});
        rohr_ui_label(&editor->constrained_label,
            (UIRect){context->x + 5.0f, 190.0f,
                context->width - 10.0f, 38.0f});
    } else {
        UISliderConfig slider = rohr_ui_slider_config_default_get();
        UIFieldResult result = rohr_ui_field("editor.line.length.field",
            (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &length},
            &editor->length_field, (UIRect){context->x + 60.0f, 150.0f,
                context->width - 70.0f, 26.0f}, NULL);
        UISliderResult slider_result;
        field_active = field_active || result.active;
        if(result.changed) editor_line_length_set(context->project,
            object->id, body->id, hitbox->id, line, length);
        slider.center = (Position){context->x + context->width * 0.5f, 202.0f};
        slider.length = context->width - 36.0f;
        slider.min_value = 5.0f;
        slider.max_value = context->x;
        slider_result = rohr_ui_slider("editor.line.length", length, &slider);
        if(slider_result.changed) editor_line_length_set(context->project,
            object->id, body->id, hitbox->id, line, slider_result.value);
    }
    if(context->delete_y_get != NULL) {
        UIButtonStyle style = editor_mode_delete_style_get();
        UIRect bounds = {context->x + 10.0f,
            context->delete_y_get(context->delete_context),
            context->width - 20.0f, 30.0f};
        if(hitbox->vertex_count <= EDITOR_HITBOX_VERTEX_MIN) {
            rohr_ui_button_disabled(bounds, &style);
            rohr_ui_label(&editor->delete_label, bounds);
        } else if(rohr_ui_button("editor.line.delete", &editor->delete_label,
                bounds, &style).clicked && context->delete_open_item != NULL) {
            (void)context->delete_open_item(context->delete_context);
        }
    }
    return field_active;
}
