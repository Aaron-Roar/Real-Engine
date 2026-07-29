#ifndef ERROR_H
#define ERROR_H

#include <stdbool.h>

/** Identifies whether a result object contains a value or an error. */
typedef enum {
    /** The result contains a valid value in result.value. */
    ERROR_RESULT_VALUE,
    /** The result contains an EngineError in result.error. */
    ERROR_RESULT_ERROR,
} ErrorResultKind;

/** Engine-wide error codes returned by fallible APIs. */
typedef enum EngineError {
    /** No error occurred. */
    ERROR_NONE = 0,

    /** A memory pool function received a null pointer. */
    ERROR_MEMORY_POOL_NULL_POINTER,
    /** A requested memory pool capacity would overflow. */
    ERROR_MEMORY_POOL_CAPACITY_OVERFLOW,
    /** A memory pool allocation failed. */
    ERROR_MEMORY_POOL_ALLOCATION_FAILED,
    /** A memory pool has no free slots. */
    ERROR_MEMORY_POOL_FULL,
    /** A pointer does not belong to the target memory pool. */
    ERROR_MEMORY_POOL_INVALID_OBJECT,
    /** A requested memory pool slot is not currently used. */
    ERROR_MEMORY_POOL_OBJECT_NOT_USED,
    /** A shrink request would remove a used memory pool slot. */
    ERROR_MEMORY_POOL_SHRINK_WOULD_REMOVE_USED_OBJECT,

    /** The engine is already initialized and running. */
    ERROR_ENGINE_ALREADY_RUNNING,
    /** SDL initialization failed. */
    ERROR_ENGINE_SDL_INIT_FAILED,
    /** Entity tables failed to initialize. */
    ERROR_ENGINE_ENTITY_TABLES_INIT_FAILED,
    /** Physics tables failed to initialize. */
    ERROR_ENGINE_PHYSICS_TABLES_INIT_FAILED,
    /** Graphics tables failed to initialize. */
    ERROR_ENGINE_GRAPHICS_TABLES_INIT_FAILED,
    /** Grid tables failed to initialize. */
    ERROR_ENGINE_GRID_TABLES_INIT_FAILED,
    /** Graphics window or renderer initialization failed. */
    ERROR_ENGINE_GRAPHICS_INIT_FAILED,
    /** An operation would exceed MAX_ENTITIES. */
    ERROR_ENGINE_MAX_ENTITIES_EXCEEDED,
    /** Entity-indexed subsystem tables could not grow. */
    ERROR_ENGINE_TABLE_EXPANSION_FAILED,
    /** An entity id is invalid or stale. */
    ERROR_ENGINE_INVALID_ENTITY,
    /** No live entity exists for the requested id or index. */
    ERROR_ENGINE_ENTITY_NOT_FOUND,
    /** A required component is missing. */
    ERROR_ENGINE_COMPONENT_MISSING,
    /** A texture asset could not be loaded. */
    ERROR_ENGINE_TEXTURE_LOAD_FAILED,
    /** One or more animation texture frames could not be loaded. */
    ERROR_ENGINE_ANIMATION_LOAD_FAILED,
    /** An entity name is null or empty. */
    ERROR_ENGINE_INVALID_ENTITY_NAME,
    /** An entity name exceeds ENTITY_NAME_MAX. */
    ERROR_ENGINE_ENTITY_NAME_TOO_LONG,
    /** An entity name is already assigned to another live entity. */
    ERROR_ENGINE_DUPLICATE_ENTITY_NAME,
    /** A game-state file could not be read or written. */
    ERROR_ENGINE_STATE_IO_FAILED,
    /** A game-state document does not match the supported schema. */
    ERROR_ENGINE_STATE_INVALID,
    /** A named entity relationship could not be resolved. */
    ERROR_ENGINE_STATE_REFERENCE_NOT_FOUND,
    /** A group name is null or empty. */
    ERROR_ENGINE_INVALID_GROUP_NAME,
    /** A group name exceeds GROUP_NAME_MAX. */
    ERROR_ENGINE_GROUP_NAME_TOO_LONG,
    /** A group name is already assigned to another generic group. */
    ERROR_ENGINE_DUPLICATE_GROUP_NAME,
    /** No live generic group has the requested name. */
    ERROR_ENGINE_GROUP_NOT_FOUND,
    /** Loading state files would exceed the retained template document limit. */
    ERROR_ENGINE_STATE_TEMPLATE_DOCUMENT_LIMIT_EXCEEDED,
    /** A named asset is defined more than once in retained state. */
    ERROR_ENGINE_STATE_DUPLICATE_ASSET_DEFINITION,
    /** A state component references an unknown named asset. */
    ERROR_ENGINE_STATE_ASSET_REFERENCE_NOT_FOUND,
} EngineError;

/**
 * A result type needs an explicit result name in C.
 *
 * C macros cannot reliably build a type name from an arbitrary C type because
 * valid types can include spaces, pointers, and other tokens that cannot be
 * pasted into an identifier. Pass the PascalCase ResultType explicitly.
 */
#define ERROR_DECLARE_RESULT_TYPE(ResultType, ValueType) \
    typedef union Result##ResultType { \
        ValueType value; \
        EngineError error; \
    } Result##ResultType; \
    \
    typedef struct ResultType { \
        ErrorResultKind kind; \
        Result##ResultType result; \
    } ResultType

/** Build a successful result value for a generated result type. */
#define ERROR_RESULT_MAKE_VALUE(ResultType, Value) \
    ((ResultType){ \
        .kind = ERROR_RESULT_VALUE, \
        .result.value = (Value) \
    })

/** Build an error result value for a generated result type. */
#define ERROR_RESULT_MAKE_ERROR(ResultType, ErrorValue) \
    ((ResultType){ \
        .kind = ERROR_RESULT_ERROR, \
        .result.error = (ErrorValue) \
    })

/** Return true when a result value contains an error. */
#define error_check(ResultValue) \
    ((ResultValue).kind == ERROR_RESULT_ERROR)

/** Result type for fallible APIs that return only success or failure. */
ERROR_DECLARE_RESULT_TYPE(EngineResult, bool);

/**
 * Create a successful EngineResult.
 *
 * @param value Success value to store.
 * @return EngineResult containing value.
 */
EngineResult error_result_value(bool value);

/**
 * Create a failed EngineResult.
 *
 * @param error Engine error code to store.
 * @return EngineResult containing error.
 */
EngineResult error_result_error(EngineError error);

/**
 * Get the default human-readable message for an EngineError.
 *
 * @param error Error code to describe.
 * @return Static string owned by the error module.
 */
const char *error_default_message(EngineError error);

/**
 * Get the human-readable message for an EngineError.
 *
 * This currently aliases error_default_message().
 *
 * @param error Error code to describe.
 * @return Static string owned by the error module.
 */
const char *error_string(EngineError error);

#endif
