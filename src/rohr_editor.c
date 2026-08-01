#include "rohr_editor.h"
#include "level_editor.h"

EntityResult RE_entity_find_by_name(const char *name) {
    return rohr_entity_find_by_name(name);
}

EngineResult RE_init(void) {
    return level_editor_init();
}

EngineResult RE_update(void) {
    return level_editor_update();
}
