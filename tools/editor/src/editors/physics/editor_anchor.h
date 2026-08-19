#ifndef EDITOR_ANCHOR_H
#define EDITOR_ANCHOR_H

#include "editors/editor_mode_context.h"

typedef struct EditorAnchorEditor {
    FontAsset *font;
    TextAsset name_label;
    TextAsset x_label;
    TextAsset y_label;
    TextAsset rigid_body_label;
    TextAsset rotation_label;
    TextAsset none_label;
    TextAsset position_global_label;
    TextAsset position_body_label;
    TextAsset rotation_global_label;
    TextAsset rotation_body_label;
    TextAsset visible_label;
    TextAsset hidden_label;
    TextAsset delete_label;
    TextAsset x_field;
    TextAsset y_field;
    TextAsset rotation_field;
    TextAsset anchor_names[EDITOR_ANCHOR_MAX];
    TextAsset body_names[EDITOR_RIGID_BODY_MAX];
    char anchor_cache[EDITOR_ANCHOR_MAX][EDITOR_OBJECT_NAME_MAX];
    char body_cache[EDITOR_RIGID_BODY_MAX][EDITOR_OBJECT_NAME_MAX];
} EditorAnchorEditor;

bool editor_anchor_editor_create(EditorAnchorEditor *editor, FontAsset *font);
void editor_anchor_editor_destroy(EditorAnchorEditor *editor);
bool editor_anchor_editor_draw(EditorAnchorEditor *editor,
    const EditorModeContext *context);

#endif
