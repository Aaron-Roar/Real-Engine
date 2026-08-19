#ifndef ROHR_EDITOR_LINE_H
#define ROHR_EDITOR_LINE_H

#include "editors/editor_mode_context.h"

typedef struct EditorLineEditor {
    FontAsset *font;
    TextAsset name_label;
    TextAsset add_vertex_label;
    TextAsset length_label;
    TextAsset constrained_label;
    TextAsset length_field;
    TextAsset delete_label;
    TextAsset line_labels[EDITOR_HITBOX_VERTEX_MAX];
    char line_name_cache[EDITOR_HITBOX_VERTEX_MAX][EDITOR_OBJECT_NAME_MAX];
} EditorLineEditor;

bool editor_line_editor_create(EditorLineEditor *editor, FontAsset *font);
void editor_line_editor_destroy(EditorLineEditor *editor);
bool editor_line_editor_draw(EditorLineEditor *editor,
    const EditorModeContext *context);

#endif
