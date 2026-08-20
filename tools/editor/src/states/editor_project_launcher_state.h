/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_EDITOR_PROJECT_LAUNCHER_STATE_H
#define ROHR_EDITOR_PROJECT_LAUNCHER_STATE_H

#include "rohr.h"

typedef enum EditorProjectLauncherRequest {
    EDITOR_PROJECT_LAUNCHER_REQUEST_NONE,
    EDITOR_PROJECT_LAUNCHER_REQUEST_NEW_PROJECT,
    EDITOR_PROJECT_LAUNCHER_REQUEST_LOAD_PROJECT
} EditorProjectLauncherRequest;

typedef struct EditorProjectLauncherState {
    const TextAsset *new_project_label;
    const TextAsset *load_project_label;
} EditorProjectLauncherState;

void editor_project_launcher_state_init(EditorProjectLauncherState *state,
    const TextAsset *new_project_label, const TextAsset *load_project_label);
EditorProjectLauncherRequest editor_project_launcher_state_draw(
    const EditorProjectLauncherState *state, float window_width,
    float viewport_bottom);

#endif
