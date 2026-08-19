#include "editor_anchor.h"

#include "editors/editor_mode_controls.h"

#include <stdio.h>

static void boolean_set(EditorProject *project, EditorObjectId object,
        EditorAnchorId anchor, EditorPropertyKind property, bool value) {
    EditorCommand command = {.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = {EDITOR_ITEM_ANCHOR, object, 0, anchor, 0,
            property, EDITOR_PROPERTY_VALUE_BOOL, {.boolean = value}}};
    (void)editor_command_execute(project, &command);
}

bool editor_anchor_editor_create(EditorAnchorEditor *editor, FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorAnchorEditor){.font = font};
#define CREATE(value, member) \
    if(!editor_mode_text_create(font, value, &editor->member)) goto fail
    CREATE("Name", name_label); CREATE("X", x_label); CREATE("Y", y_label);
    CREATE("Rigid Body", rigid_body_label); CREATE("Rotation", rotation_label);
    CREATE("None", none_label); CREATE("Global Position", position_global_label);
    CREATE("Body Position", position_body_label);
    CREATE("Global Rotation", rotation_global_label);
    CREATE("Body Rotation", rotation_body_label);
    CREATE("[X]", visible_label); CREATE("[ ]", hidden_label);
    CREATE("Delete Anchor", delete_label); CREATE("", x_field);
    CREATE("", y_field); CREATE("", rotation_field);
#undef CREATE
    for(size_t i = 0; i < EDITOR_ANCHOR_MAX; i += 1) {
        char name[32]; snprintf(name, sizeof(name), "anchor_%zu", i + 1);
        if(!editor_mode_text_create(font, name, &editor->anchor_names[i])) goto fail;
    }
    for(size_t i = 0; i < EDITOR_RIGID_BODY_MAX; i += 1) {
        char name[32]; snprintf(name, sizeof(name), "body_%zu", i + 1);
        if(!editor_mode_text_create(font, name, &editor->body_names[i])) goto fail;
    }
    return true;
fail:
    editor_anchor_editor_destroy(editor);
    return false;
}

void editor_anchor_editor_destroy(EditorAnchorEditor *editor) {
    if(editor == NULL) return;
#define DESTROY(member) rohr_graphics_text_destroy(&editor->member)
    DESTROY(name_label); DESTROY(x_label); DESTROY(y_label);
    DESTROY(rigid_body_label); DESTROY(rotation_label); DESTROY(none_label);
    DESTROY(position_global_label); DESTROY(position_body_label);
    DESTROY(rotation_global_label); DESTROY(rotation_body_label);
    DESTROY(visible_label); DESTROY(hidden_label); DESTROY(delete_label);
    DESTROY(x_field); DESTROY(y_field); DESTROY(rotation_field);
#undef DESTROY
    for(size_t i = 0; i < EDITOR_ANCHOR_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->anchor_names[i]);
    for(size_t i = 0; i < EDITOR_RIGID_BODY_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->body_names[i]);
    *editor = (EditorAnchorEditor){0};
}

bool editor_anchor_editor_draw(EditorAnchorEditor *editor,
        const EditorModeContext *context) {
    EditorObject *object;
    EditorAnchor *anchor;
    size_t anchor_index;
    Position position;
    float rotation;
    char name[EDITOR_OBJECT_NAME_MAX];
    UIFieldResult name_result, x_result, y_result, rotation_result;
    if(editor == NULL || context == NULL || context->project == NULL ||
            context->viewport == NULL) return false;
    object = editor_project_selected_get(context->project);
    anchor = editor_project_anchor_get(object, context->viewport->selected_anchor);
    if(anchor == NULL) return false;
    anchor_index = (size_t)(anchor - object->anchors);
    if(anchor_index >= EDITOR_ANCHOR_MAX) return false;
    position = anchor->position;
    rotation = anchor->rotation;
    snprintf(name, sizeof(name), "%s", anchor->name);
    if(!editor_mode_named_text_sync(editor->font, anchor->name,
            &editor->anchor_names[anchor_index], editor->anchor_cache[anchor_index],
            EDITOR_OBJECT_NAME_MAX)) return false;
    rohr_ui_label(&editor->name_label,
        (UIRect){context->x + 40.0f, 42.0f, 48.0f, 30.0f});
    name_result = editor_mode_name_field("editor.anchor.name", name,
        sizeof(name), &editor->anchor_names[anchor_index],
        (UIRect){context->x + 88.0f, 42.0f, context->width - 96.0f, 30.0f});
    if(name_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
            .data.item_rename = {.kind = EDITOR_ITEM_ANCHOR,
                .object = object->id, .item = anchor->id}};
        snprintf(command.data.item_rename.name,
            sizeof(command.data.item_rename.name), "%s", name);
        (void)editor_command_execute(context->project, &command);
    }
    if(rohr_ui_button("editor.anchor.visibility", anchor->visible ?
            &editor->visible_label : &editor->hidden_label,
            (UIRect){context->x + 8.0f, 44.0f, 26.0f, 26.0f}, NULL).clicked) {
        EditorCommand command = {.type = EDITOR_COMMAND_VISIBILITY,
            .data.visibility = {EDITOR_VISIBILITY_ANCHOR, object->id, 0,
                anchor->id, !anchor->visible}};
        (void)editor_command_execute(context->project, &command);
    }
    rohr_ui_label(&editor->x_label,
        (UIRect){context->x + 8.0f, 90.0f, 24.0f, 26.0f});
    x_result = rohr_ui_field("editor.anchor.x",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &position.x},
        &editor->x_field, (UIRect){context->x + 34.0f, 90.0f,
            context->width - 44.0f, 26.0f}, NULL);
    rohr_ui_label(&editor->y_label,
        (UIRect){context->x + 8.0f, 124.0f, 24.0f, 26.0f});
    y_result = rohr_ui_field("editor.anchor.y",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &position.y},
        &editor->y_field, (UIRect){context->x + 34.0f, 124.0f,
            context->width - 44.0f, 26.0f}, NULL);
    if(x_result.changed || y_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_ANCHOR_TRANSFORM,
            .data.anchor_transform = {object->id, anchor->id, position, rotation}};
        (void)editor_command_execute(context->project, &command);
    }
    rohr_ui_label(&editor->rigid_body_label,
        (UIRect){context->x + 8.0f, 158.0f, 90.0f, 28.0f});
    {
        const TextAsset *options[EDITOR_RIGID_BODY_MAX + 1];
        size_t selected = 0;
        options[0] = &editor->none_label;
        for(size_t i = 0; i < object->rigid_body_count &&
                i < EDITOR_RIGID_BODY_MAX; i += 1) {
            if(!editor_mode_named_text_sync(editor->font, object->rigid_bodies[i].name,
                    &editor->body_names[i], editor->body_cache[i],
                    EDITOR_OBJECT_NAME_MAX)) return false;
            options[i + 1] = &editor->body_names[i];
            if(object->rigid_bodies[i].id == anchor->rigid_body) selected = i + 1;
        }
        UIDropdownResult result = rohr_ui_dropdown("editor.anchor.rigid_body",
            options, object->rigid_body_count + 1, selected,
            (UIRect){context->x + 100.0f, 158.0f,
                context->width - 110.0f, 28.0f}, NULL);
        if(result.hovered_index >= 0) {
            size_t option = (size_t)result.hovered_index;
            context->viewport->preview_rigid_body = option == 0 ||
                option > object->rigid_body_count ? 0 :
                object->rigid_bodies[option - 1].id;
        } else if(result.button_hovered)
            context->viewport->preview_rigid_body = anchor->rigid_body;
        if(result.changed) {
            EditorCommand command = {.type = EDITOR_COMMAND_RELATIONSHIP_SET,
                .data.relationship_set = {EDITOR_RELATIONSHIP_ANCHOR_RIGID_BODY,
                    object->id, 0, anchor->id, 0, result.selected_index == 0 ? 0 :
                        object->rigid_bodies[result.selected_index - 1].id}};
            (void)editor_command_execute(context->project, &command);
        }
    }
    rohr_ui_label(&editor->rotation_label,
        (UIRect){context->x + 8.0f, 192.0f, 76.0f, 26.0f});
    rotation_result = rohr_ui_field("editor.anchor.rotation",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &rotation},
        &editor->rotation_field, (UIRect){context->x + 86.0f, 192.0f,
            context->width - 96.0f, 26.0f}, NULL);
    if(rotation_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_ANCHOR_TRANSFORM,
            .data.anchor_transform = {object->id, anchor->id, position, rotation}};
        (void)editor_command_execute(context->project, &command);
    }
    {
        const TextAsset *position_options[] = {&editor->position_global_label,
            &editor->position_body_label};
        const TextAsset *rotation_options[] = {&editor->rotation_global_label,
            &editor->rotation_body_label};
        UIDropdownResult position_result = rohr_ui_dropdown(
            "editor.anchor.position_lock", position_options, 2,
            anchor->position_follows_body ? 1 : 0,
            (UIRect){context->x + 10.0f, 226.0f,
                context->width - 20.0f, 28.0f}, NULL);
        UIDropdownResult orientation_result = rohr_ui_dropdown(
            "editor.anchor.rotation_lock", rotation_options, 2,
            anchor->rotation_follows_body ? 1 : 0,
            (UIRect){context->x + 10.0f, 258.0f,
                context->width - 20.0f, 28.0f}, NULL);
        if(position_result.changed) boolean_set(context->project, object->id,
            anchor->id, EDITOR_PROPERTY_POSITION_FOLLOWS_BODY,
            position_result.selected_index == 1);
        if(orientation_result.changed) boolean_set(context->project, object->id,
            anchor->id, EDITOR_PROPERTY_ROTATION_FOLLOWS_BODY,
            orientation_result.selected_index == 1);
    }
    if(context->delete_y_get != NULL && context->delete_open_item != NULL) {
        UIButtonStyle style = editor_mode_delete_style_get();
        if(rohr_ui_button("editor.anchor.delete", &editor->delete_label,
                (UIRect){context->x + 10.0f,
                    context->delete_y_get(context->delete_context),
                    context->width - 20.0f, 34.0f}, &style).clicked)
            (void)context->delete_open_item(context->delete_context);
    }
    return name_result.active || x_result.active || y_result.active ||
        rotation_result.active;
}
