#include "editor_sprite.h"

#include "editors/editor_mode_controls.h"

#include <stdio.h>

bool editor_sprite_editor_create(EditorSpriteEditor *editor, FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorSpriteEditor){.font = font};
#define CREATE(value, member) \
    if(!editor_mode_text_create(font, value, &editor->member)) goto fail
    CREATE("Name", name_label); CREATE("Path", path_label);
    CREATE("Rigid Body", body_label); CREATE("X", x_label); CREATE("Y", y_label);
    CREATE("Rotation", rotation_label); CREATE("Width", width_label);
    CREATE("Height", height_label); CREATE("Visible", visible_label);
    CREATE("Follow Rotation", follow_label); CREATE("None", none_label);
    CREATE("Delete Sprite", delete_label); CREATE("", path_field);
    CREATE("", x_field); CREATE("", y_field); CREATE("", rotation_field);
    CREATE("", width_field); CREATE("", height_field);
#undef CREATE
    return true;
fail:
    editor_sprite_editor_destroy(editor);
    return false;
}

void editor_sprite_editor_destroy(EditorSpriteEditor *editor) {
    if(editor == NULL) return;
#define DESTROY(member) rohr_graphics_text_destroy(&editor->member)
    DESTROY(name_label); DESTROY(path_label); DESTROY(body_label); DESTROY(x_label);
    DESTROY(y_label); DESTROY(rotation_label); DESTROY(width_label);
    DESTROY(height_label); DESTROY(visible_label); DESTROY(follow_label);
    DESTROY(none_label); DESTROY(delete_label); DESTROY(path_field);
    DESTROY(x_field); DESTROY(y_field); DESTROY(rotation_field);
    DESTROY(width_field); DESTROY(height_field);
#undef DESTROY
    for(size_t i = 0; i < 64; i += 1)
        rohr_graphics_text_destroy(&editor->name_values[i]);
    for(size_t i = 0; i < EDITOR_RIGID_BODY_MAX; i += 1)
        rohr_graphics_text_destroy(&editor->body_names[i]);
    *editor = (EditorSpriteEditor){0};
}

bool editor_sprite_editor_draw(EditorSpriteEditor *editor,
        const EditorModeContext *context, EditorBodyPreviewFunction preview,
        void *preview_context) {
    EditorObject *object;
    EditorSprite *sprite;
    size_t index, body_selected = 0;
    char name[EDITOR_OBJECT_NAME_MAX], path[EDITOR_ASSET_PATH_MAX];
    Position position;
    float rotation, width, height;
    bool visible, follow;
    const TextAsset *body_options[EDITOR_RIGID_BODY_MAX + 1];
    UIFieldResult name_result, path_result, x_result, y_result;
    UIFieldResult rotation_result, width_result, height_result;
    UIDropdownResult body_result;
    if(editor == NULL || context == NULL || context->project == NULL ||
            context->viewport == NULL) return false;
    object = editor_project_selected_get(context->project);
    sprite = editor_project_sprite_get(object, context->viewport->selected_sprite);
    if(sprite == NULL || object == NULL) return false;
    index = (size_t)(sprite - object->sprites);
    if(index >= 64) return false;
    snprintf(name, sizeof(name), "%s", sprite->name);
    snprintf(path, sizeof(path), "%s", sprite->path);
    if(!editor_mode_named_text_sync(editor->font, sprite->name,
            &editor->name_values[index], editor->name_cache[index],
            EDITOR_OBJECT_NAME_MAX)) return false;
    rohr_ui_label(&editor->name_label,
        (UIRect){context->x + 8.0f, 42.0f, 70.0f, 28.0f});
    name_result = rohr_ui_field("editor.sprite.name",
        (UIFieldBinding){.kind = UI_FIELD_STRING, .string = name,
            .string_capacity = sizeof(name)}, &editor->name_values[index],
        (UIRect){context->x + 82.0f, 42.0f, context->width - 92.0f, 28.0f}, NULL);
    rohr_ui_label(&editor->path_label,
        (UIRect){context->x + 8.0f, 80.0f, 70.0f, 28.0f});
    path_result = rohr_ui_field("editor.sprite.path",
        (UIFieldBinding){.kind = UI_FIELD_STRING, .string = path,
            .string_capacity = sizeof(path)}, &editor->path_field,
        (UIRect){context->x + 82.0f, 80.0f, context->width - 92.0f, 28.0f}, NULL);
    body_options[0] = &editor->none_label;
    for(size_t i = 0; i < object->rigid_body_count &&
            i < EDITOR_RIGID_BODY_MAX; i += 1) {
        if(!editor_mode_named_text_sync(editor->font, object->rigid_bodies[i].name,
                &editor->body_names[i], editor->body_cache[i],
                EDITOR_OBJECT_NAME_MAX)) return false;
        body_options[i + 1] = &editor->body_names[i];
        if(object->rigid_bodies[i].id == sprite->rigid_body) body_selected = i + 1;
    }
    rohr_ui_label(&editor->body_label,
        (UIRect){context->x + 8.0f, 118.0f, 90.0f, 28.0f});
    body_result = rohr_ui_dropdown("editor.sprite.body", body_options,
        object->rigid_body_count + 1, body_selected,
        (UIRect){context->x + 100.0f, 118.0f, context->width - 110.0f, 28.0f}, NULL);
    if(preview != NULL) preview(preview_context, object, body_result, sprite->rigid_body);
    position = sprite->position; rotation = sprite->rotation;
    width = sprite->size.x; height = sprite->size.y;
    rohr_ui_label(&editor->x_label,
        (UIRect){context->x + 8.0f, 156.0f, 70.0f, 28.0f});
    x_result = rohr_ui_field("editor.sprite.x",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &position.x},
        &editor->x_field, (UIRect){context->x + 82.0f, 156.0f,
            context->width - 92.0f, 28.0f}, NULL);
    rohr_ui_label(&editor->y_label,
        (UIRect){context->x + 8.0f, 194.0f, 70.0f, 28.0f});
    y_result = rohr_ui_field("editor.sprite.y",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &position.y},
        &editor->y_field, (UIRect){context->x + 82.0f, 194.0f,
            context->width - 92.0f, 28.0f}, NULL);
    rohr_ui_label(&editor->rotation_label,
        (UIRect){context->x + 8.0f, 232.0f, 70.0f, 28.0f});
    rotation_result = rohr_ui_field("editor.sprite.rotation",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &rotation},
        &editor->rotation_field, (UIRect){context->x + 82.0f, 232.0f,
            context->width - 92.0f, 28.0f}, NULL);
    rohr_ui_label(&editor->width_label,
        (UIRect){context->x + 8.0f, 270.0f, 70.0f, 28.0f});
    width_result = rohr_ui_field("editor.sprite.width",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &width},
        &editor->width_field, (UIRect){context->x + 82.0f, 270.0f,
            context->width - 92.0f, 28.0f}, NULL);
    rohr_ui_label(&editor->height_label,
        (UIRect){context->x + 8.0f, 308.0f, 70.0f, 28.0f});
    height_result = rohr_ui_field("editor.sprite.height",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &height},
        &editor->height_field, (UIRect){context->x + 82.0f, 308.0f,
            context->width - 92.0f, 28.0f}, NULL);
    visible = sprite->visible; follow = sprite->follow_body_rotation;
    bool follow_changed = editor_mode_checkbox_left("editor.sprite.follow",
        &editor->follow_label, (UIRect){context->x + 10.0f, 346.0f,
            context->width - 20.0f, 28.0f}, &follow);
    bool visible_changed = editor_mode_checkbox_left("editor.sprite.visible",
        &editor->visible_label, (UIRect){context->x + 10.0f, 384.0f,
            context->width - 20.0f, 28.0f}, &visible);
    if(name_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_RENAME,
            .data.sprite_rename = {object->id, sprite->id}};
        snprintf(command.data.sprite_rename.name,
            sizeof(command.data.sprite_rename.name), "%s", name);
        (void)editor_command_execute(context->project, &command);
    }
    if(path_result.changed && path[0] != '\0') {
        EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_PATH_SET,
            .data.sprite_path_set = {object->id, sprite->id}};
        snprintf(command.data.sprite_path_set.path,
            sizeof(command.data.sprite_path_set.path), "%s", path);
        (void)editor_command_execute(context->project, &command);
    }
    if(body_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_BODY_SET,
            .data.sprite_body_set = {object->id, sprite->id,
                body_result.selected_index == 0 ? 0 :
                    object->rigid_bodies[body_result.selected_index - 1].id}};
        (void)editor_command_execute(context->project, &command);
    }
    if(x_result.changed || y_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_POSITION_SET,
            .data.sprite_position_set = {object->id, sprite->id, position}};
        (void)editor_command_execute(context->project, &command);
    }
    if(rotation_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_ROTATION_SET,
            .data.sprite_rotation_set = {object->id, sprite->id, rotation}};
        (void)editor_command_execute(context->project, &command);
    }
    if(width_result.changed || height_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_SIZE_SET,
            .data.sprite_size_set = {object->id, sprite->id, {width, height}}};
        (void)editor_command_execute(context->project, &command);
    }
    if(visible_changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_VISIBILITY_SET,
            .data.sprite_visibility_set = {object->id, sprite->id, visible}};
        (void)editor_command_execute(context->project, &command);
    }
    if(follow_changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_FOLLOW_ROTATION_SET,
            .data.sprite_boolean_set = {object->id, sprite->id, follow}};
        (void)editor_command_execute(context->project, &command);
    }
    if(context->delete_open_item != NULL) {
        UIButtonStyle style = editor_mode_delete_style_get();
        if(rohr_ui_button("editor.sprite.delete", &editor->delete_label,
                (UIRect){context->x + 10.0f,
                    context->delete_y_get != NULL ?
                        context->delete_y_get(context->delete_context) : 650.0f,
                    context->width - 20.0f, 34.0f}, &style).clicked)
            (void)context->delete_open_item(context->delete_context);
    }
    return name_result.active || path_result.active || x_result.active ||
        y_result.active || rotation_result.active || width_result.active ||
        height_result.active;
}
