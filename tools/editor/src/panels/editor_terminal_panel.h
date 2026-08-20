/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

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
    size_t tracked_scan_line;
    int tracked_exit_code;
    bool tracked_pending;
    bool tracked_completed;
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
void editor_terminal_panel_operation_write(EditorTerminalPanel *panel,
    const char *command);
bool editor_terminal_panel_command_execute(EditorTerminalPanel *panel,
    const char *command);
bool editor_terminal_panel_command_execute_tracked(EditorTerminalPanel *panel,
    const char *command);
bool editor_terminal_panel_command_completion_take(EditorTerminalPanel *panel,
    int *exit_code);
bool editor_terminal_panel_output_write(EditorTerminalPanel *panel,
    const char *output);
float editor_terminal_panel_viewport_bottom_get(const EditorTerminalPanel *panel);
void editor_terminal_panel_draw(EditorTerminalPanel *panel, float viewport_width,
    float viewport_bottom);
void editor_terminal_panel_destroy(EditorTerminalPanel *panel);

#endif
