/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_animation_frame.h"

#include "editors/editor_mode_controls.h"

#include <stdio.h>

bool editor_animation_frame_editor_create(EditorAnimationFrameEditor *editor,
        FontAsset *font) {
    if(editor == NULL || font == NULL) return false;
    *editor = (EditorAnimationFrameEditor){.font = font};
#define CREATE(value, member) \
    if(!editor_mode_text_create(font, value, &editor->member)) goto fail
    CREATE("Name", name_label); CREATE("Path", path_label);
    CREATE("Width", width_label); CREATE("Height", height_label);
    CREATE("Delete Frame", delete_label); CREATE("frame", name_field);
    CREATE("", path_field); CREATE("", width_field); CREATE("", height_field);
#undef CREATE
    return true;
fail:
    editor_animation_frame_editor_destroy(editor);
    return false;
}

void editor_animation_frame_editor_destroy(EditorAnimationFrameEditor *editor) {
    if(editor == NULL) return;
    rohr_graphics_text_destroy(&editor->name_label);
    rohr_graphics_text_destroy(&editor->path_label);
    rohr_graphics_text_destroy(&editor->width_label);
    rohr_graphics_text_destroy(&editor->height_label);
    rohr_graphics_text_destroy(&editor->delete_label);
    rohr_graphics_text_destroy(&editor->name_field);
    rohr_graphics_text_destroy(&editor->path_field);
    rohr_graphics_text_destroy(&editor->width_field);
    rohr_graphics_text_destroy(&editor->height_field);
    *editor = (EditorAnimationFrameEditor){0};
}

bool editor_animation_frame_editor_draw(EditorAnimationFrameEditor *editor,
        const EditorModeContext *context) {
    EditorObject *object;
    EditorAnimatedSprite *animation;
    EditorAnimationFrame *frame = NULL;
    size_t frame_index = 0;
    char name[EDITOR_OBJECT_NAME_MAX];
    char path[EDITOR_ASSET_PATH_MAX];
    float width, height;
    UIFieldResult name_result, path_result, width_result, height_result;
    if(editor == NULL || context == NULL || context->project == NULL ||
            context->viewport == NULL) return false;
    object = editor_project_selected_get(context->project);
    animation = editor_project_animated_sprite_get(object,
        context->viewport->selected_animated_sprite);
    if(animation != NULL) for(size_t i = 0; i < animation->frame_count; i += 1) {
        if(animation->frames[i].id != context->viewport->selected_animation_frame)
            continue;
        frame = &animation->frames[i]; frame_index = i; break;
    }
    if(frame == NULL) return false;
    snprintf(name, sizeof(name), "%s", frame->name);
    snprintf(path, sizeof(path), "%s", frame->path);
    width = frame->size.x; height = frame->size.y;
    if(!editor_mode_named_text_sync(editor->font, frame->name,
            &editor->name_field, editor->name_cache,
            EDITOR_OBJECT_NAME_MAX)) return false;
    rohr_ui_label(&editor->name_label,
        (UIRect){context->x + 8.0f, 42.0f, 70.0f, 28.0f});
    name_result = rohr_ui_field("editor.animation_frame.name",
        (UIFieldBinding){.kind = UI_FIELD_STRING, .string = name,
            .string_capacity = sizeof(name)}, &editor->name_field,
        (UIRect){context->x + 82.0f, 42.0f,
            context->width - 92.0f, 28.0f}, NULL);
    rohr_ui_label(&editor->path_label,
        (UIRect){context->x + 8.0f, 80.0f, 70.0f, 28.0f});
    path_result = rohr_ui_field("editor.animation_frame.path",
        (UIFieldBinding){.kind = UI_FIELD_STRING, .string = path,
            .string_capacity = sizeof(path)}, &editor->path_field,
        (UIRect){context->x + 82.0f, 80.0f,
            context->width - 92.0f, 28.0f}, NULL);
    rohr_ui_label(&editor->width_label,
        (UIRect){context->x + 8.0f, 118.0f, 70.0f, 28.0f});
    width_result = rohr_ui_field("editor.animation_frame.width",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &width},
        &editor->width_field, (UIRect){context->x + 82.0f, 118.0f,
            context->width - 92.0f, 28.0f}, NULL);
    rohr_ui_label(&editor->height_label,
        (UIRect){context->x + 8.0f, 156.0f, 70.0f, 28.0f});
    height_result = rohr_ui_field("editor.animation_frame.height",
        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &height},
        &editor->height_field, (UIRect){context->x + 82.0f, 156.0f,
            context->width - 92.0f, 28.0f}, NULL);
    if(name_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_ANIMATION_FRAME_RENAME,
            .data.animation_frame_rename = {.object = object->id,
                .sprite = animation->id, .index = frame_index}};
        snprintf(command.data.animation_frame_rename.name,
            sizeof(command.data.animation_frame_rename.name), "%s", name);
        (void)editor_command_execute(context->project, &command);
    }
    if(path_result.changed && path[0] != '\0') {
        EditorCommand command = {.type = EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET,
            .data.animation_frame_path_set = {.object = object->id,
                .sprite = animation->id, .index = frame_index}};
        snprintf(command.data.animation_frame_path_set.path,
            sizeof(command.data.animation_frame_path_set.path), "%s", path);
        (void)editor_command_execute(context->project, &command);
    }
    if(width_result.changed || height_result.changed) {
        EditorCommand command = {.type = EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET,
            .data.animation_frame_size_set = {.object = object->id,
                .sprite = animation->id, .index = frame_index,
                .size = {width, height}}};
        (void)editor_command_execute(context->project, &command);
    }
    if(context->delete_y_get != NULL) {
        UIButtonStyle style = editor_mode_delete_style_get();
        if(rohr_ui_button("editor.animation_frame.delete", &editor->delete_label,
                (UIRect){context->x + 10.0f,
                    context->delete_y_get(context->delete_context),
                    context->width - 20.0f, 34.0f}, &style).clicked) {
            EditorCommand command = {.type = EDITOR_COMMAND_ANIMATION_FRAME_REMOVE,
                .data.animation_frame_remove = {.object = object->id,
                    .sprite = animation->id, .index = frame_index}};
            if(editor_command_execute(context->project, &command).kind ==
                    ERROR_RESULT_VALUE) editor_viewport_back(context->viewport);
        }
    }
    return name_result.active || path_result.active || width_result.active ||
        height_result.active;
}
