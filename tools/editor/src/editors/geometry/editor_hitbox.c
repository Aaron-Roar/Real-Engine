/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_hitbox.h"

#include "editor_navigation.h"
#include "editors/editor_mode_controls.h"

#include <stdio.h>
#include <string.h>

static UIButtonStyle selected_style_get(void) {
    UIButtonStyle style = rohr_ui_button_style_default_get();
    style.idle = (Color){118, 96, 35, 255};
    style.hovered = (Color){145, 119, 45, 255};
    return style;
}

static bool selected_check(const EditorViewportState *viewport,
        EditorSelectionRef selection) {
    return editor_viewport_selection_contains(viewport, selection);
}

bool editor_hitbox_editor_create(EditorHitboxEditor *editor, FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorHitboxEditor){.font = font};
#define CREATE(value, member) \
    if(!editor_mode_text_create(font, value, &editor->member)) goto fail
    CREATE("Name", name_label);
    CREATE("Auto Shape", auto_shape_label);
    CREATE("Vertices", vertices_label);
    CREATE("Lines", lines_label);
    CREATE("[X]", visible_label);
    CREATE("[ ]", hidden_label);
    CREATE("Delete Hitbox Variant", delete_label);
#undef CREATE
    for(size_t i = 0; i < EDITOR_BODY_HITBOX_MAX; i += 1) {
        char name[32];
        snprintf(name, sizeof(name), "hitbox_%zu", i + 1);
        if(!editor_mode_text_create(font, name, &editor->hitbox_names[i])) goto fail;
    }
    for(size_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
        char name[32];
        snprintf(name, sizeof(name), "vertex_%zu", i + 1);
        if(!editor_mode_text_create(font, name, &editor->vertex_names[i])) goto fail;
        snprintf(name, sizeof(name), "line_%zu", i + 1);
        if(!editor_mode_text_create(font, name, &editor->line_names[i])) goto fail;
    }
    return true;
fail:
    editor_hitbox_editor_destroy(editor);
    return false;
}

void editor_hitbox_editor_destroy(EditorHitboxEditor *editor) {
    if(editor == NULL) return;
    rohr_graphics_text_destroy(&editor->name_label);
    rohr_graphics_text_destroy(&editor->auto_shape_label);
    rohr_graphics_text_destroy(&editor->vertices_label);
    rohr_graphics_text_destroy(&editor->lines_label);
    rohr_graphics_text_destroy(&editor->visible_label);
    rohr_graphics_text_destroy(&editor->hidden_label);
    rohr_graphics_text_destroy(&editor->delete_label);
    for(size_t i = 0; i < EDITOR_BODY_HITBOX_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->hitbox_names[i]);
    for(size_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
        rohr_graphics_text_destroy(&editor->vertex_names[i]);
        rohr_graphics_text_destroy(&editor->line_names[i]);
    }
    *editor = (EditorHitboxEditor){0};
}

bool editor_hitbox_editor_draw(EditorHitboxEditor *editor,
        EditorAutoShapeEditor *auto_shape, const EditorModeContext *context) {
    EditorObject *object;
    EditorRigidBody *body;
    EditorHitbox *hitbox;
    size_t hitbox_index;
    UIFieldResult name_result;
    char edited_name[EDITOR_OBJECT_NAME_MAX];
    bool field_active;
    if(editor == NULL || auto_shape == NULL || context == NULL ||
            context->project == NULL || context->viewport == NULL) return false;
    object = editor_project_selected_get(context->project);
    body = object == NULL ? NULL : editor_project_rigid_body_get(object,
        context->viewport->selected_rigid_body);
    hitbox = body == NULL ? NULL : editor_project_hitbox_get(body,
        context->viewport->selected_hitbox);
    if(hitbox == NULL) return false;
    hitbox_index = (size_t)(hitbox - body->hitboxes);
    if(hitbox_index >= EDITOR_BODY_HITBOX_MAX) return false;
    snprintf(edited_name, sizeof(edited_name), "%s", hitbox->name);
    if(!editor_mode_named_text_sync(editor->font, hitbox->name,
            &editor->hitbox_names[hitbox_index], editor->hitbox_cache[hitbox_index],
            EDITOR_OBJECT_NAME_MAX)) return false;
    rohr_ui_label(&editor->name_label,
        (UIRect){context->x + 40.0f, 44.0f, 48.0f, 28.0f});
    name_result = editor_mode_name_field("editor.hitbox.name", edited_name,
        sizeof(edited_name), &editor->hitbox_names[hitbox_index],
        (UIRect){context->x + 88.0f, 44.0f, context->width - 96.0f, 28.0f});
    field_active = name_result.active;
    if(name_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
            .data.item_rename = {.kind = EDITOR_ITEM_HITBOX,
                .object = object->id, .parent = body->id, .item = hitbox->id}};
        snprintf(command.data.item_rename.name,
            sizeof(command.data.item_rename.name), "%s", edited_name);
        (void)editor_command_execute(context->project, &command);
    }
    if(rohr_ui_button("editor.hitbox.visibility", hitbox->visible ?
            &editor->visible_label : &editor->hidden_label,
            (UIRect){context->x + 8.0f, 47.0f, 26.0f, 26.0f}, NULL).clicked) {
        EditorCommand command = {.type = EDITOR_COMMAND_VISIBILITY,
            .data.visibility = {EDITOR_VISIBILITY_HITBOX, object->id,
                body->id, hitbox->id, !hitbox->visible}};
        (void)editor_command_execute(context->project, &command);
    }
    if(rohr_ui_button("editor.hitbox.auto_shape", &editor->auto_shape_label,
            (UIRect){context->x + 10.0f, 78.0f,
                context->width - 20.0f, 28.0f}, NULL).clicked)
        editor->auto_shape_picker_open = !editor->auto_shape_picker_open;
    if(editor->auto_shape_picker_open) {
        size_t selected_count = editor_auto_shape_hitbox_points_capture(
            context->viewport, object, body, hitbox);
        int shape = editor_auto_shape_picker_draw(auto_shape,
            "editor.hitbox.auto_shape.option",
            (UIRect){context->x + 10.0f, 110.0f,
                context->width - 20.0f, 62.0f},
            selected_count > 0 ? selected_count : hitbox->vertex_count);
        if(shape >= 0) {
            auto_shape->config.kind = (EditorAutoShapeKind)shape;
            context->viewport->auto_shape_parent_mode = EDITOR_VIEWPORT_HITBOX;
            (void)editor_auto_shape_editor_apply(auto_shape, context->project,
                context->viewport, EDITOR_VIEWPORT_HITBOX);
            context->viewport->mode = EDITOR_VIEWPORT_AUTO_SHAPE;
            context->viewport->selection = EDITOR_SELECTION_HITBOX;
            auto_shape->first_was_active = false;
            auto_shape->second_was_active = false;
            auto_shape->third_was_active = false;
            editor->auto_shape_picker_open = false;
        }
        return field_active;
    }
    rohr_ui_label(&editor->vertices_label,
        (UIRect){context->x + 10.0f, 110.0f, context->width - 20.0f, 24.0f});
    for(uint32_t i = 0; i < hitbox->vertex_count &&
            i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
        char id[64];
        UIButtonStyle style = selected_style_get();
        bool selected = (context->viewport->selection == EDITOR_SELECTION_VERTEX &&
            context->viewport->selected_vertex == i) || selected_check(context->viewport,
                (EditorSelectionRef){EDITOR_SELECTION_VERTEX, object->id,
                    body->id, hitbox->id, hitbox->vertices[i].id});
        UIButtonResult result;
        if(!editor_mode_named_text_sync(editor->font, hitbox->vertices[i].name,
                &editor->vertex_names[i], editor->vertex_cache[i],
                EDITOR_OBJECT_NAME_MAX)) return field_active;
        snprintf(id, sizeof(id), "editor.vertex.%u", hitbox->vertices[i].id);
        result = rohr_ui_button(id, &editor->vertex_names[i],
            (UIRect){context->x + 18.0f, 138.0f + (float)i * 27.0f,
                context->width - 26.0f, 23.0f}, selected ? &style : NULL);
        if(result.clicked || result.focus_changed) {
            context->viewport->selection = EDITOR_SELECTION_VERTEX;
            context->viewport->selected_vertex = i;
            if(result.double_clicked)
                (void)editor_navigation_selected_open(context->project,
                    context->viewport);
        }
    }
    {
        float base = 146.0f + (float)hitbox->vertex_count * 27.0f;
        rohr_ui_label(&editor->lines_label,
            (UIRect){context->x + 10.0f, base, context->width - 20.0f, 24.0f});
        for(uint32_t i = 0; i < hitbox->vertex_count &&
                i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
            char id[64];
            UIButtonStyle style = selected_style_get();
            bool selected = (context->viewport->selection == EDITOR_SELECTION_LINE &&
                context->viewport->selected_line == i) || selected_check(context->viewport,
                    (EditorSelectionRef){EDITOR_SELECTION_LINE, object->id,
                        body->id, hitbox->id, i});
            UIButtonResult result;
            if(!editor_mode_named_text_sync(editor->font, hitbox->line_names[i],
                    &editor->line_names[i], editor->line_cache[i],
                    EDITOR_OBJECT_NAME_MAX)) return field_active;
            snprintf(id, sizeof(id), "editor.line.%u", i);
            result = rohr_ui_button(id, &editor->line_names[i],
                (UIRect){context->x + 18.0f, base + 28.0f + (float)i * 27.0f,
                    context->width - 26.0f, 23.0f}, selected ? &style : NULL);
            if(result.clicked || result.focus_changed) {
                context->viewport->selection = EDITOR_SELECTION_LINE;
                context->viewport->selected_line = i;
                if(result.double_clicked)
                    (void)editor_navigation_selected_open(context->project,
                        context->viewport);
            }
        }
    }
    if(context->delete_y_get != NULL && context->delete_open_item != NULL) {
        UIButtonStyle style = editor_mode_delete_style_get();
        if(rohr_ui_button("editor.hitbox.delete", &editor->delete_label,
                (UIRect){context->x + 18.0f,
                    context->delete_y_get(context->delete_context),
                    context->width - 26.0f, 30.0f}, &style).clicked)
            (void)context->delete_open_item(context->delete_context);
    }
    return field_active;
}
