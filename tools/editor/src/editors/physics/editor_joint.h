#ifndef EDITOR_JOINT_H
#define EDITOR_JOINT_H

#include "editors/editor_mode_context.h"

typedef struct EditorJointEditor {
    FontAsset *font;
    TextAsset name_label, visual_size_label;
    TextAsset revolute_label, weld_label, spring_label;
    TextAsset anchor_a_label, anchor_b_label, none_label, add_anchor_label;
    TextAsset damping_label, rest_length_label, stiffness_label;
    TextAsset visible_label, hidden_label, delete_label;
    TextAsset damping_field, rest_length_field, stiffness_field;
    TextAsset joint_names[EDITOR_JOINT_MAX];
    TextAsset anchor_names[EDITOR_ANCHOR_MAX];
    char joint_cache[EDITOR_JOINT_MAX][EDITOR_OBJECT_NAME_MAX];
    char anchor_cache[EDITOR_ANCHOR_MAX][EDITOR_OBJECT_NAME_MAX];
} EditorJointEditor;

bool editor_joint_editor_create(EditorJointEditor *editor, FontAsset *font);
void editor_joint_editor_destroy(EditorJointEditor *editor);
bool editor_joint_editor_draw(EditorJointEditor *editor,
    const EditorModeContext *context);

#endif
