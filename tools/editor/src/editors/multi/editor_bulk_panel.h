/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_EDITOR_BULK_PANEL_H
#define ROHR_EDITOR_BULK_PANEL_H

#include "editor_history.h"
#include "editor_viewport.h"

#define EDITOR_BULK_PROPERTY_MAX 24

typedef void (*EditorBulkColorOpen)(void *context, uint32_t *color,
    EditorPropertyKind property);

typedef struct EditorBulkPanel {
    EditorHierarchySelection kind;
    TextAsset title;
    TextAsset delete_label;
    TextAsset labels[EDITOR_BULK_PROPERTY_MAX];
    TextAsset fields[EDITOR_BULK_PROPERTY_MAX];
    TextAsset unset_label;
    TextAsset dynamic_label;
    TextAsset static_label;
    TextAsset rotation_unlocked_label;
    TextAsset rotation_locked_label;
    TextAsset revolute_label;
    TextAsset weld_label;
    TextAsset spring_label;
    char values[EDITOR_BULK_PROPERTY_MAX][64];
    bool assigned[EDITOR_BULK_PROPERTY_MAX];
    bool booleans[EDITOR_BULK_PROPERTY_MAX];
    size_t dropdown_indices[EDITOR_BULK_PROPERTY_MAX];
    float slider_values[EDITOR_BULK_PROPERTY_MAX];
    uint32_t colors[EDITOR_BULK_PROPERTY_MAX];
    size_t property_count;
} EditorBulkPanel;

bool editor_bulk_panel_create(EditorBulkPanel *panel, FontAsset *font);
void editor_bulk_panel_destroy(EditorBulkPanel *panel);
bool editor_bulk_panel_draw(EditorBulkPanel *panel, EditorProject *project,
    EditorViewportState *state, EditorHistory *history, float x, float width,
    float delete_y, EditorBulkColorOpen color_open, void *color_context);
float editor_bulk_panel_content_height_get(const EditorViewportState *state);
bool editor_bulk_property_set(EditorProject *project, EditorViewportState *state,
    EditorHistory *history, const EditorPropertySetCommand *property);

#endif
