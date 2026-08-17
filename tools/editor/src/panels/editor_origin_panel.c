#include "editor_origin_panel.h"
#include "editor_command.h"

#include <stdio.h>

static bool editor_origin_panel_text_create(FontAsset *font, const char *value,
        TextAsset *output) {
    TextAssetResult result;

    if(font == NULL || value == NULL || output == NULL) return false;
    result = rohr_graphics_text_create(font, value, (Color){230, 234, 242, 255});
    if(rohr_error_check(result)) {
        fprintf(stderr, "%s\n", rohr_error_message_get(result));
        return false;
    }
    *output = result.result.value;
    return true;
}

bool editor_origin_panel_create(EditorOriginPanel *panel, FontAsset *font) {
    if(panel == NULL || font == NULL) return false;
    *panel = (EditorOriginPanel){0};
    if(!editor_origin_panel_text_create(font, "Origin", &panel->title) ||
            !editor_origin_panel_text_create(font, "X", &panel->x_label) ||
            !editor_origin_panel_text_create(font, "Y", &panel->y_label) ||
            !editor_origin_panel_text_create(font, "", &panel->x_field) ||
            !editor_origin_panel_text_create(font, "", &panel->y_field)) {
        editor_origin_panel_destroy(panel);
        return false;
    }
    return true;
}

void editor_origin_panel_destroy(EditorOriginPanel *panel) {
    if(panel == NULL) return;
    rohr_graphics_text_destroy(&panel->title);
    rohr_graphics_text_destroy(&panel->x_label);
    rohr_graphics_text_destroy(&panel->y_label);
    rohr_graphics_text_destroy(&panel->x_field);
    rohr_graphics_text_destroy(&panel->y_field);
    *panel = (EditorOriginPanel){0};
}

bool editor_origin_panel_draw(EditorOriginPanel *panel,
        const EditorPanelContext *context) {
    EditorObject *object;
    EditorRigidBody *rigid_body = NULL;
    EditorSoftBody *soft_body = NULL;
    Position position;
    UIFieldResult x_result;
    UIFieldResult y_result;

    if(panel == NULL || context == NULL || context->project == NULL ||
            context->navigation == NULL) return false;
    object = editor_project_selected_get(context->project);
    if(object == NULL) return false;
    if(context->navigation->selected_origin_kind == EDITOR_ORIGIN_RIGID_BODY) {
        rigid_body = editor_project_rigid_body_get(
            object, context->navigation->selected_rigid_body);
        if(rigid_body == NULL) return false;
        position = rigid_body->position;
    } else if(context->navigation->selected_origin_kind == EDITOR_ORIGIN_SOFT_BODY) {
        for(size_t i = 0; i < object->soft_body_count; i += 1) {
            if(object->soft_body_items[i].id == context->navigation->selected_soft_body)
                soft_body = &object->soft_body_items[i];
        }
        if(soft_body == NULL) return false;
        position = soft_body->position;
    } else {
        return false;
    }

    rohr_ui_label(&panel->title,
        (UIRect){context->x + 10.0f, 42.0f, context->width - 20.0f, 30.0f});
    rohr_ui_label(&panel->x_label,
        (UIRect){context->x + 8.0f, 122.0f, 50.0f, 26.0f});
    x_result = rohr_ui_field("editor.origin.x",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &position.x},
        &panel->x_field,
        (UIRect){context->x + 60.0f, 122.0f, context->width - 70.0f, 26.0f}, NULL);
    rohr_ui_label(&panel->y_label,
        (UIRect){context->x + 8.0f, 158.0f, 50.0f, 26.0f});
    y_result = rohr_ui_field("editor.origin.y",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &position.y},
        &panel->y_field,
        (UIRect){context->x + 60.0f, 158.0f, context->width - 70.0f, 26.0f}, NULL);
    if(x_result.changed || y_result.changed) {
        EditorCommand command = {.type = rigid_body != NULL ?
                EDITOR_COMMAND_RIGID_BODY_ORIGIN : EDITOR_COMMAND_SOFT_BODY_ORIGIN,
            .data.origin = {object->id,
                rigid_body != NULL ? rigid_body->id : soft_body->id, position}};
        (void)editor_command_execute(context->project, &command);
    }
    return x_result.active || y_result.active;
}
