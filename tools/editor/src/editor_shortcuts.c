#include "editor_shortcuts.h"

EditorHistoryShortcutResult editor_history_shortcut_handle(
        const SDL_Event *event, bool project_open, EditorHistory *history) {
    EditorHistoryShortcutResult result = {0};
    bool redo;
    if(event == NULL || !project_open || event->type != SDL_EVENT_KEY_DOWN ||
            (event->key.mod & SDL_KMOD_CTRL) == 0 ||
            (event->key.key != SDLK_Z && event->key.key != SDLK_Y)) return result;
    result.consumed = true;
    redo = event->key.key == SDLK_Y || (event->key.mod & SDL_KMOD_SHIFT) != 0;
    result.restored = redo ? editor_history_redo(history) :
        editor_history_undo(history);
    return result;
}
