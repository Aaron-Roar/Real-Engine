#include "editor_vertex.h"

#include "editors/editor_mode_controls.h"

#include <stdio.h>

bool editor_vertex_editor_create(EditorVertexEditor *editor, FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorVertexEditor){.font = font};
    if(!editor_mode_text_create(font, "Name", &editor->name_label) ||
            !editor_mode_text_create(font, "Lock Position", &editor->lock_label) ||
            !editor_mode_text_create(font, "Unlock Position",
                &editor->unlock_label) ||
            !editor_mode_text_create(font, "X", &editor->x_label) ||
            !editor_mode_text_create(font, "Y", &editor->y_label) ||
            !editor_mode_text_create(font, "", &editor->x_field) ||
            !editor_mode_text_create(font, "", &editor->y_field) ||
            !editor_mode_text_create(font, "Delete Vertex",
                &editor->delete_label)) {
        editor_vertex_editor_destroy(editor);
        return false;
    }
    for(size_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
        char name[32];
        snprintf(name, sizeof(name), "vertex_%zu", i + 1);
        if(!editor_mode_text_create(font, name, &editor->vertex_labels[i])) {
            editor_vertex_editor_destroy(editor);
            return false;
        }
    }
    return true;
}

void editor_vertex_editor_destroy(EditorVertexEditor *editor) {
    if(editor == NULL) return;
    rohr_graphics_text_destroy(&editor->name_label);
    rohr_graphics_text_destroy(&editor->lock_label);
    rohr_graphics_text_destroy(&editor->unlock_label);
    rohr_graphics_text_destroy(&editor->x_label);
    rohr_graphics_text_destroy(&editor->y_label);
    rohr_graphics_text_destroy(&editor->x_field);
    rohr_graphics_text_destroy(&editor->y_field);
    rohr_graphics_text_destroy(&editor->delete_label);
    for(size_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->vertex_labels[i]);
    *editor = (EditorVertexEditor){0};
}

bool editor_vertex_editor_draw(EditorVertexEditor *editor,
        const EditorModeContext *context) {
    EditorObject *object;
    EditorRigidBody *body;
    EditorHitbox *hitbox;
    EditorVertex *vertex;
    uint32_t index;
    UIFieldResult name_result;
    bool field_active;
    char edited_name[EDITOR_OBJECT_NAME_MAX];
    if(editor == NULL || context == NULL || context->project == NULL ||
            context->viewport == NULL) return false;
    object = editor_project_selected_get(context->project);
    body = object == NULL ? NULL : editor_project_rigid_body_get(object,
        context->viewport->selected_rigid_body);
    hitbox = body == NULL ? NULL : editor_project_hitbox_get(body,
        context->viewport->selected_hitbox);
    index = context->viewport->selected_vertex;
    if(hitbox == NULL || index >= hitbox->vertex_count ||
            index >= EDITOR_HITBOX_VERTEX_MAX) return false;
    vertex = &hitbox->vertices[index];
    snprintf(edited_name, sizeof(edited_name), "%s", vertex->name);
    if(!editor_mode_named_text_sync(editor->font, vertex->name,
            &editor->vertex_labels[index], editor->vertex_name_cache[index],
            EDITOR_OBJECT_NAME_MAX)) return false;
    rohr_ui_label(&editor->name_label,
        (UIRect){context->x + 8.0f, 48.0f, 48.0f, 24.0f});
    name_result = editor_mode_name_field("editor.vertex.name", edited_name,
        sizeof(edited_name), &editor->vertex_labels[index],
        (UIRect){context->x + 56.0f, 48.0f, context->width - 64.0f, 24.0f});
    field_active = name_result.active;
    if(name_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
            .data.item_rename = {.kind = EDITOR_ITEM_VERTEX,
                .object = object->id, .parent = body->id,
                .item = hitbox->id, .index = vertex->id}};
        snprintf(command.data.item_rename.name,
            sizeof(command.data.item_rename.name), "%s", edited_name);
        (void)editor_command_execute(context->project, &command);
    }
    if(rohr_ui_button("editor.vertex.lock",
            vertex->position_locked ? &editor->unlock_label : &editor->lock_label,
            (UIRect){context->x + 10.0f, 82.0f,
                context->width - 20.0f, 34.0f}, NULL).clicked) {
        EditorCommand command = {.type = EDITOR_COMMAND_PROPERTY_SET,
            .data.property_set = {EDITOR_ITEM_VERTEX, object->id, body->id,
                hitbox->id, vertex->id, EDITOR_PROPERTY_POSITION_LOCKED,
                EDITOR_PROPERTY_VALUE_BOOL,
                {.boolean = !vertex->position_locked}}};
        (void)editor_command_execute(context->project, &command);
    }
    rohr_ui_label(&editor->x_label,
        (UIRect){context->x + 8.0f, 122.0f, 20.0f, 22.0f});
    rohr_ui_label(&editor->y_label,
        (UIRect){context->x + 8.0f, 192.0f, 20.0f, 22.0f});
    if(vertex->position_locked) {
        editor_mode_numeric_disabled_draw(&editor->x_field, vertex->position.x,
            (UIRect){context->x + 28.0f, 122.0f,
                context->width - 38.0f, 24.0f});
        editor_mode_numeric_disabled_draw(&editor->y_field, vertex->position.y,
            (UIRect){context->x + 28.0f, 192.0f,
                context->width - 38.0f, 24.0f});
    } else {
        Position edited = vertex->position;
        UISliderConfig slider = rohr_ui_slider_config_default_get();
        UIFieldResult x_result = rohr_ui_field("editor.vertex.x.field",
            (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &edited.x},
            &editor->x_field, (UIRect){context->x + 28.0f, 122.0f,
                context->width - 38.0f, 24.0f}, NULL);
        UIFieldResult y_result = rohr_ui_field("editor.vertex.y.field",
            (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &edited.y},
            &editor->y_field, (UIRect){context->x + 28.0f, 192.0f,
                context->width - 38.0f, 24.0f}, NULL);
        UISliderResult x_slider;
        UISliderResult y_slider;
        field_active = field_active || x_result.active || y_result.active;
        slider.length = context->width - 42.0f;
        slider.min_value = -context->x * 0.5f;
        slider.max_value = context->x * 0.5f;
        slider.center = (Position){context->x + 72.0f, 157.0f};
        x_slider = rohr_ui_slider("editor.vertex.x", edited.x, &slider);
        edited.x = x_slider.value;
        slider.center.y = 227.0f;
        y_slider = rohr_ui_slider("editor.vertex.y", edited.y, &slider);
        edited.y = y_slider.value;
        if(x_result.changed || y_result.changed || x_slider.changed ||
                y_slider.changed) {
            EditorCommand command = {.type = EDITOR_COMMAND_VERTEX_POSITION,
                .data.vertex_position = {object->id, body->id, hitbox->id,
                    vertex->id, edited}};
            (void)editor_command_execute(context->project, &command);
        }
    }
    if(context->delete_y_get != NULL) {
        UIButtonStyle style = editor_mode_delete_style_get();
        UIRect bounds = {context->x + 10.0f,
            context->delete_y_get(context->delete_context),
            context->width - 20.0f, 30.0f};
        if(hitbox->vertex_count <= EDITOR_HITBOX_VERTEX_MIN) {
            rohr_ui_button_disabled(bounds, &style);
            rohr_ui_label(&editor->delete_label, bounds);
        } else if(rohr_ui_button("editor.vertex.delete", &editor->delete_label,
                bounds, &style).clicked && context->delete_open_item != NULL) {
            (void)context->delete_open_item(context->delete_context);
        }
    }
    return field_active;
}
