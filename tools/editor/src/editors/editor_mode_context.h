#ifndef ROHR_EDITOR_MODE_CONTEXT_H
#define ROHR_EDITOR_MODE_CONTEXT_H

#include "editor_project.h"
#include "editor_viewport.h"
#include "editor_command.h"

typedef void (*EditorModeColorOpenFunction)(void *context, uint32_t *color,
    EditorItemKind kind, EditorObjectId object, uint32_t parent,
    uint32_t item, EditorPropertyKind property);
typedef void (*EditorModeLocalColorOpenFunction)(void *context, uint32_t *color);
typedef float (*EditorModeDeleteYGetFunction)(void *context);
typedef bool (*EditorModeDeleteFunction)(void *context);
typedef void (*EditorModeHierarchyRowFunction)(void *context,
    EditorViewportState *viewport, EditorSelectionRef selection,
    UIRect bounds, UIButtonResult interaction, bool last);

typedef struct EditorModeContext {
    EditorProject *project;
    EditorViewportState *viewport;
    float x;
    float width;
    EditorModeColorOpenFunction color_open;
    EditorModeLocalColorOpenFunction local_color_open;
    void *color_context;
    EditorModeDeleteYGetFunction delete_y_get;
    EditorModeDeleteFunction delete_open_item;
    void *delete_context;
    EditorModeHierarchyRowFunction hierarchy_row;
    void *hierarchy_context;
    MouseButtonState primary_button;
} EditorModeContext;

#endif
