/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef EDITOR_SOFT_NODE_H
#define EDITOR_SOFT_NODE_H

#include "editors/editor_mode_context.h"

typedef bool (*EditorSoftNodeCollisionMenuFunction)(void *context,
    const char *id_prefix, EditorProject *project, uint64_t *active_masks,
    EditorObjectId object, EditorSoftBodyId body, EditorSoftNodeId node,
    EditorCollisionFilterKind filter, float x, float y, float width,
    bool *field_active, size_t *row_count);

typedef struct EditorSoftNodeEditor {
    FontAsset *font;
    TextAsset name_label, x_label, y_label, mass_label, radius_label;
    TextAsset friction_label, restitution_label, gravity_label, collision_label;
    TextAsset collision_category_label, collide_with_label;
    TextAsset color_label, inherit_label, visible_label, hidden_label, delete_label;
    TextAsset x_field, y_field, mass_field, radius_field;
    TextAsset friction_field, restitution_field;
    TextAsset node_names[EDITOR_SOFT_NODE_MAX];
    char node_cache[EDITOR_SOFT_NODE_MAX][EDITOR_OBJECT_NAME_MAX];
    bool collision_category_open;
    bool collide_with_open;
} EditorSoftNodeEditor;

bool editor_soft_node_editor_create(EditorSoftNodeEditor *editor,
    FontAsset *font);
void editor_soft_node_editor_destroy(EditorSoftNodeEditor *editor);
bool editor_soft_node_editor_draw(EditorSoftNodeEditor *editor,
    const EditorModeContext *context,
    EditorSoftNodeCollisionMenuFunction collision_menu,
    void *collision_menu_context);

#endif
