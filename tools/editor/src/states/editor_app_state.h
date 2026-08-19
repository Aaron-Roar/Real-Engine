#ifndef ROHR_EDITOR_APP_STATE_H
#define ROHR_EDITOR_APP_STATE_H

typedef enum EditorAppStateKind {
    EDITOR_APP_STATE_PROJECT_LAUNCHER,
    EDITOR_APP_STATE_WORKSPACE
} EditorAppStateKind;

typedef struct EditorAppStateMachine {
    EditorAppStateKind current;
} EditorAppStateMachine;

void editor_app_state_init(EditorAppStateMachine *machine);
EditorAppStateKind editor_app_state_get(const EditorAppStateMachine *machine);
void editor_app_state_transition(EditorAppStateMachine *machine,
    EditorAppStateKind next);

#endif
