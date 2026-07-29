#include "error.h"

EngineResult error_result_value(bool value) {
    return (EngineResult){
        .kind = ERROR_RESULT_VALUE,
        .result.value = value
    };
}

EngineResult error_result_error(EngineError error) {
    return (EngineResult){
        .kind = ERROR_RESULT_ERROR,
        .result.error = error
    };
}

const char *error_default_message(EngineError error) {
    switch(error) {
        case ERROR_NONE:
            return "no error";
        case ERROR_MEMORY_POOL_NULL_POINTER:
            return "memory pool null pointer";
        case ERROR_MEMORY_POOL_CAPACITY_OVERFLOW:
            return "memory pool capacity overflow";
        case ERROR_MEMORY_POOL_ALLOCATION_FAILED:
            return "memory pool allocation failed";
        case ERROR_MEMORY_POOL_FULL:
            return "memory pool full";
        case ERROR_MEMORY_POOL_INVALID_OBJECT:
            return "memory pool invalid object";
        case ERROR_MEMORY_POOL_OBJECT_NOT_USED:
            return "memory pool object not used";
        case ERROR_MEMORY_POOL_SHRINK_WOULD_REMOVE_USED_OBJECT:
            return "memory pool shrink would remove used object";
        case ERROR_ENGINE_ALREADY_RUNNING:
            return "engine already running";
        case ERROR_ENGINE_SDL_INIT_FAILED:
            return "SDL initialization failed";
        case ERROR_ENGINE_ENTITY_TABLES_INIT_FAILED:
            return "entity tables initialization failed";
        case ERROR_ENGINE_PHYSICS_TABLES_INIT_FAILED:
            return "physics tables initialization failed";
        case ERROR_ENGINE_GRAPHICS_TABLES_INIT_FAILED:
            return "graphics tables initialization failed";
        case ERROR_ENGINE_GRID_TABLES_INIT_FAILED:
            return "grid tables initialization failed";
        case ERROR_ENGINE_GRAPHICS_INIT_FAILED:
            return "graphics initialization failed";
        case ERROR_ENGINE_MAX_ENTITIES_EXCEEDED:
            return "maximum entity count exceeded";
        case ERROR_ENGINE_TABLE_EXPANSION_FAILED:
            return "engine table expansion failed";
        case ERROR_ENGINE_INVALID_ENTITY:
            return "invalid entity";
        case ERROR_ENGINE_ENTITY_NOT_FOUND:
            return "entity not found";
        case ERROR_ENGINE_COMPONENT_MISSING:
            return "component missing";
        case ERROR_ENGINE_TEXTURE_LOAD_FAILED:
            return "texture load failed";
        case ERROR_ENGINE_ANIMATION_LOAD_FAILED:
            return "animation load failed";
        case ERROR_ENGINE_INVALID_ENTITY_NAME:
            return "invalid entity name";
        case ERROR_ENGINE_ENTITY_NAME_TOO_LONG:
            return "entity name is too long";
        case ERROR_ENGINE_DUPLICATE_ENTITY_NAME:
            return "duplicate entity name";
        case ERROR_ENGINE_STATE_IO_FAILED:
            return "game state file I/O failed";
        case ERROR_ENGINE_STATE_INVALID:
            return "invalid game state";
        case ERROR_ENGINE_STATE_REFERENCE_NOT_FOUND:
            return "game state entity reference not found";
        default:
            return "unknown error";
    }
}

const char *error_string(EngineError error) {
    return error_default_message(error);
}
