#ifndef ROHR_EDITOR_HISTORY_H
#define ROHR_EDITOR_HISTORY_H

#include "editor_command.h"

#define EDITOR_HISTORY_CAPACITY 32

typedef struct EditorHistory {
    EditorProject *project;
    EditorProject *shadow;
    EditorProject *undo[EDITOR_HISTORY_CAPACITY];
    EditorProject *redo[EDITOR_HISTORY_CAPACITY];
    size_t undo_count;
    size_t redo_count;
    bool continuous;
    bool continuous_recorded;
} EditorHistory;

bool editor_history_init(EditorHistory *history, EditorProject *project);
void editor_history_destroy(EditorHistory *history);
void editor_history_reset(EditorHistory *history);
void editor_history_command_record(EditorHistory *history,
    const EditorCommand *command);
void editor_history_continuous_set(EditorHistory *history, bool continuous);
bool editor_history_undo(EditorHistory *history);
bool editor_history_redo(EditorHistory *history);
bool editor_history_undo_check(const EditorHistory *history);
bool editor_history_redo_check(const EditorHistory *history);

#endif
