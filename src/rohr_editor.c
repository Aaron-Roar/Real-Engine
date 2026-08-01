#include "rohr_editor.h"
#include "level_editor.h"

EngineResult RE_init(void) {
    return level_editor_init();
}

EngineResult RE_update(void) {
    return level_editor_update();
}
