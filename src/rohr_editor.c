#include "rohr_editor.h"
#include "level_editor.h"

EngineResult RE_entity_find_by_name(const char *name, Entity *entity) {
    EntityResult result;

    if(name == NULL || entity == NULL) {
        return rohr_error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    result = rohr_entity_find_by_name(name);
    if(rohr_error_check(result)) {
        return rohr_error_result_error(result.result.error);
    }
    *entity = result.result.value;
    return rohr_error_result_value(true);
}

EngineResult RE_init(void) {
    return level_editor_init();
}

EngineResult RE_update(void) {
    return level_editor_update();
}
