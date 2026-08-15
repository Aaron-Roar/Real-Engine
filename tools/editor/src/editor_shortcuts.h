#ifndef ROHR_EDITOR_SHORTCUTS_H
#define ROHR_EDITOR_SHORTCUTS_H

#include "editor_history.h"

#include <SDL3/SDL.h>

typedef struct EditorHistoryShortcutResult {
    bool consumed;
    bool restored;
} EditorHistoryShortcutResult;

EditorHistoryShortcutResult editor_history_shortcut_handle(
    const SDL_Event *event, bool project_open, EditorHistory *history);

#endif
