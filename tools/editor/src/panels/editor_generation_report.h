/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef EDITOR_GENERATION_REPORT_H
#define EDITOR_GENERATION_REPORT_H

#include "editor_project.h"
#include "panels/editor_terminal_panel.h"

bool editor_generation_report_write(EditorTerminalPanel *terminal,
    const EditorProject *project);

#endif
