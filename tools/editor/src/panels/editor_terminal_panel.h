#ifndef EDITOR_TERMINAL_PANEL_H
#define EDITOR_TERMINAL_PANEL_H

#include "rohr.h"
#include "rohr_terminal.h"

#define EDITOR_TERMINAL_VISIBLE_LINE_MAX 40

typedef struct EditorTerminalPanel {
    RohrTerminal *terminal;
    TextAsset lines[EDITOR_TERMINAL_VISIBLE_LINE_MAX];
    bool visible;
    bool focused;
    bool resizing;
    float height;
    float cell_width;
    size_t scroll_offset;
} EditorTerminalPanel;

bool editor_terminal_panel_create(EditorTerminalPanel *panel, FontAsset *font);
bool editor_terminal_panel_project_open(EditorTerminalPanel *panel,
    const char *project_directory);
void editor_terminal_panel_project_close(EditorTerminalPanel *panel);
void editor_terminal_panel_visible_toggle(EditorTerminalPanel *panel);
bool editor_terminal_panel_focused_check(const EditorTerminalPanel *panel);
bool editor_terminal_panel_event_add(EditorTerminalPanel *panel,
    const SDL_Event *event, float viewport_width, float viewport_bottom);
void editor_terminal_panel_update(EditorTerminalPanel *panel);
float editor_terminal_panel_viewport_bottom_get(const EditorTerminalPanel *panel);
void editor_terminal_panel_draw(EditorTerminalPanel *panel, float viewport_width,
    float viewport_bottom);
void editor_terminal_panel_destroy(EditorTerminalPanel *panel);

#endif
