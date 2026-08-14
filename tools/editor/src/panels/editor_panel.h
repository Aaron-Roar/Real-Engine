#ifndef ROHR_EDITOR_PANEL_H
#define ROHR_EDITOR_PANEL_H

#include "editor_viewport.h"

typedef struct EditorPanelContext {
    EditorProject *project;
    EditorViewportState *navigation;
    float x;
    float width;
} EditorPanelContext;

#endif
