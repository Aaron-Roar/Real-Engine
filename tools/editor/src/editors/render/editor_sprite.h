#ifndef EDITOR_SPRITE_H
#define EDITOR_SPRITE_H

#include "editors/editor_mode_context.h"

typedef void (*EditorBodyPreviewFunction)(void *context, EditorObject *object,
    UIDropdownResult result, EditorRigidBodyId current);

typedef struct EditorSpriteEditor {
    FontAsset *font;
    TextAsset name_label, path_label, body_label, x_label, y_label;
    TextAsset rotation_label, width_label, height_label;
    TextAsset visible_label, follow_label, none_label, delete_label;
    TextAsset name_values[64], body_names[EDITOR_RIGID_BODY_MAX];
    TextAsset path_field, x_field, y_field, rotation_field, width_field, height_field;
    char name_cache[64][EDITOR_OBJECT_NAME_MAX];
    char body_cache[EDITOR_RIGID_BODY_MAX][EDITOR_OBJECT_NAME_MAX];
} EditorSpriteEditor;

bool editor_sprite_editor_create(EditorSpriteEditor *editor, FontAsset *font);
void editor_sprite_editor_destroy(EditorSpriteEditor *editor);
bool editor_sprite_editor_draw(EditorSpriteEditor *editor,
    const EditorModeContext *context, EditorBodyPreviewFunction preview,
    void *preview_context);

#endif
