#ifndef EDITOR_HITBOX_H
#define EDITOR_HITBOX_H

#include "editors/editor_mode_context.h"
#include "editors/geometry/editor_auto_shape.h"

typedef struct EditorHitboxEditor {
    FontAsset *font;
    TextAsset name_label;
    TextAsset auto_shape_label;
    TextAsset vertices_label;
    TextAsset lines_label;
    TextAsset visible_label;
    TextAsset hidden_label;
    TextAsset delete_label;
    TextAsset hitbox_names[EDITOR_BODY_HITBOX_MAX];
    TextAsset vertex_names[EDITOR_HITBOX_VERTEX_MAX];
    TextAsset line_names[EDITOR_HITBOX_VERTEX_MAX];
    char hitbox_cache[EDITOR_BODY_HITBOX_MAX][EDITOR_OBJECT_NAME_MAX];
    char vertex_cache[EDITOR_HITBOX_VERTEX_MAX][EDITOR_OBJECT_NAME_MAX];
    char line_cache[EDITOR_HITBOX_VERTEX_MAX][EDITOR_OBJECT_NAME_MAX];
    bool auto_shape_picker_open;
} EditorHitboxEditor;

bool editor_hitbox_editor_create(EditorHitboxEditor *editor, FontAsset *font);
void editor_hitbox_editor_destroy(EditorHitboxEditor *editor);
bool editor_hitbox_editor_draw(EditorHitboxEditor *editor,
    EditorAutoShapeEditor *auto_shape, const EditorModeContext *context);

#endif
