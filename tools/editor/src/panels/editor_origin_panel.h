#ifndef ROHR_EDITOR_ORIGIN_PANEL_H
#define ROHR_EDITOR_ORIGIN_PANEL_H

#include "editor_panel.h"

typedef struct EditorOriginPanel {
    TextAsset title;
    TextAsset x_label;
    TextAsset y_label;
    TextAsset x_field;
    TextAsset y_field;
} EditorOriginPanel;

bool editor_origin_panel_create(EditorOriginPanel *panel, FontAsset *font);
void editor_origin_panel_destroy(EditorOriginPanel *panel);
bool editor_origin_panel_draw(EditorOriginPanel *panel,
    const EditorPanelContext *context);

#endif
