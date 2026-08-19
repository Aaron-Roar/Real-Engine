#include "editor_app_state.h"

#include <stddef.h>

void editor_app_state_init(EditorAppStateMachine *machine) {
    if(machine == NULL) return;
    machine->current = EDITOR_APP_STATE_PROJECT_LAUNCHER;
}

EditorAppStateKind editor_app_state_get(const EditorAppStateMachine *machine) {
    if(machine == NULL) return EDITOR_APP_STATE_PROJECT_LAUNCHER;
    return machine->current;
}

void editor_app_state_transition(EditorAppStateMachine *machine,
        EditorAppStateKind next) {
    if(machine == NULL) return;
    machine->current = next;
}
