/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "states/editor_app_state.h"

int main(void) {
    EditorAppStateMachine machine;

    editor_app_state_init(&machine);
    if(editor_app_state_get(&machine) != EDITOR_APP_STATE_PROJECT_LAUNCHER)
        return 1;
    editor_app_state_transition(&machine, EDITOR_APP_STATE_WORKSPACE);
    if(editor_app_state_get(&machine) != EDITOR_APP_STATE_WORKSPACE) return 1;
    editor_app_state_transition(&machine, EDITOR_APP_STATE_PROJECT_LAUNCHER);
    return editor_app_state_get(&machine) == EDITOR_APP_STATE_PROJECT_LAUNCHER ?
        0 : 1;
}
