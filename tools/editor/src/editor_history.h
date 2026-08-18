#ifndef ROHR_EDITOR_HISTORY_H
#define ROHR_EDITOR_HISTORY_H

#include "editor_command.h"

#define EDITOR_HISTORY_CAPACITY 32

typedef struct EditorHistoryEntry EditorHistoryEntry;
typedef struct EditorHistoryObjectChange EditorHistoryObjectChange;
typedef struct EditorHistoryAggregateChange EditorHistoryAggregateChange;
typedef struct EditorHistoryCollisionChange EditorHistoryCollisionChange;

typedef struct EditorHistory {
    EditorProject *project;
    EditorProject *pending;
    EditorProject *transaction_before;
    EditorHistoryEntry *transaction_commands;
    EditorHistoryEntry *undo[EDITOR_HISTORY_CAPACITY];
    EditorHistoryEntry *redo[EDITOR_HISTORY_CAPACITY];
    size_t undo_count;
    size_t redo_count;
    size_t snapshot_fallback_count;
    bool continuous;
    bool continuous_recorded;
    bool recorded_since_continuous_update;
    bool transaction_active;
    bool transaction_untyped;
    bool pending_command_valid;
    bool restoring;
    EditorCommand pending_forward;
    EditorCommand pending_inverse;
    EditorHistoryObjectChange *pending_object;
    EditorHistoryAggregateChange *pending_aggregate;
    EditorHistoryCollisionChange *pending_collision;
} EditorHistory;

bool editor_history_init(EditorHistory *history, EditorProject *project);
void editor_history_destroy(EditorHistory *history);
void editor_history_reset(EditorHistory *history);
void editor_history_command_begin(EditorHistory *history,
    const EditorProject *project, const EditorCommand *command);
void editor_history_command_finish(EditorHistory *history,
    const EditorCommand *command, const EditorCommandResult *result);
void editor_history_continuous_set(EditorHistory *history, bool continuous);
bool editor_history_transaction_begin(EditorHistory *history);
bool editor_history_transaction_end(EditorHistory *history);
void editor_history_transaction_cancel(EditorHistory *history);
bool editor_history_undo(EditorHistory *history);
bool editor_history_redo(EditorHistory *history);
bool editor_history_undo_check(const EditorHistory *history);
bool editor_history_redo_check(const EditorHistory *history);
size_t editor_history_memory_get(const EditorHistory *history);

#endif
