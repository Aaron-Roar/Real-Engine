#include "rohr_editor.h"
#include "level_editor.h"

EntityResult RE_entity_by_name_get(const char *name) {
    return rohr_entity_by_name_get(name);
}

EngineResult RE_init(void) {
    return level_editor_init();
}

EngineResult RE_update(void) {
    return level_editor_update();
}
