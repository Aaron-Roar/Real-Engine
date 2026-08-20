/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

/**
 * Declare a fixed-size object pool type and its generated function prototypes.
 *
 * PoolType is the concrete pool struct name to create. ObjectType is the
 * object stored by value in the pool. Each pool owns two heap arrays after
 * initialization: objects for the stored values and used for slot state.
 *
 * The generated functions all return explicit result types:
 * - PoolTypeResult contains a bool success value or an EngineError.
 * - PoolTypeObjectResult contains an ObjectType pointer or an EngineError.
 *
 * PoolType_store copies an ObjectType value into the first free slot
 * and returns a borrowed pointer to the stored object.
 * PoolType_store_at copies an ObjectType value into a requested slot.
 *
 * Expanding or shrinking a pool can move the backing objects array. Object
 * pointers returned before a resize must be treated as stale after that call.
 */
#define MEMORY_DECLARE_OBJECT_POOL(PoolType, ObjectType) \
    typedef struct PoolType { \
        ObjectType *objects; \
        uint8_t *used; \
        size_t capacity; \
        size_t count; \
    } PoolType; \
    \
    ERROR_DECLARE_RESULT_TYPE(PoolType##Result, bool); \
    ERROR_DECLARE_RESULT_TYPE(PoolType##ObjectResult, ObjectType *); \
    \
    PoolType##Result PoolType##_init(PoolType *pool, size_t capacity); \
    PoolType##Result PoolType##_expand(PoolType *pool, size_t additional_capacity); \
    PoolType##Result PoolType##_defrag(PoolType *pool); \
    PoolType##Result PoolType##_shrink(PoolType *pool, size_t capacity); \
    PoolType##Result PoolType##_clear(PoolType *pool); \
    PoolType##Result PoolType##_destroy(PoolType *pool); \
    PoolType##ObjectResult PoolType##_acquire(PoolType *pool); \
    PoolType##ObjectResult PoolType##_store(PoolType *pool, ObjectType object); \
    PoolType##ObjectResult PoolType##_store_at(PoolType *pool, size_t index, ObjectType object); \
    PoolType##Result PoolType##_release_at(PoolType *pool, size_t index); \
    PoolType##Result PoolType##_release(PoolType *pool, ObjectType *object)

/**
 * Build a successful result value for one of the memory result types.
 *
 * ResultType must be a result type declared by ERROR_DECLARE_RESULT_TYPE, and Value
 * must match that result type's value field.
 */
#define MEMORY_POOL_VALUE(ResultType, Value) \
    (ResultType) { \
        .kind = ERROR_RESULT_VALUE, \
        .result.value = (Value), \
    }

/**
 * Build an error result for one of the memory result types.
 *
 * ResultType must be a result type declared by ERROR_DECLARE_RESULT_TYPE, and
 * ErrorValue must be one of the EngineError enum values.
 */
#define MEMORY_POOL_ERROR(ResultType, ErrorValue) \
    (ResultType) { \
        .kind = ERROR_RESULT_ERROR, \
        .result.error = (ErrorValue), \
    }

/**
 * Define the object pool functions declared by MEMORY_DECLARE_OBJECT_POOL.
 *
 * Include this macro in exactly one translation unit for each PoolType.
 *
 * Generated function behavior:
 * - init zeroes the pool fields, then allocates capacity slots when capacity
 *   is greater than zero.
 * - expand allocates larger arrays, copies the current slots, and frees the
 *   old arrays.
 * - defrag compacts used slots toward the front of the pool and zeroes the
 *   trailing unused slots.
 * - shrink refuses to remove any slot that is still marked used.
 * - clear zeroes every allocated object and marks every slot unused, while
 *   keeping the allocated capacity.
 * - destroy frees owned arrays and resets the pool to an empty state.
 * - acquire finds the first unused slot, marks it used, zeroes the object, and
 *   returns a pointer to it.
 * - store finds the first unused slot, copies the supplied object value into
 *   it, and returns a pointer to the stored object.
 * - store_at copies the supplied object value into the requested slot, marking
 *   it used if it was not already active.
 * - release_at marks the requested slot unused and zeroes its object.
 * - release validates that the pointer belongs to an active pool slot, marks
 *   the slot unused, and zeroes the object.
 *
 * store copies ObjectType by value. The pool does not call object-specific
 * destructors. Callers own any cleanup required by fields inside ObjectType
 * before clear, destroy, or release.
 */
#define MEMORY_DEFINE_OBJECT_POOL(PoolType, ObjectType) \
    PoolType##Result PoolType##_init(PoolType *pool, size_t capacity) { \
        if(pool == NULL) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_NULL_POINTER); \
        } \
        \
        pool->objects = NULL; \
        pool->used = NULL; \
        pool->capacity = 0; \
        pool->count = 0; \
        if(capacity == 0) { \
            return MEMORY_POOL_VALUE(PoolType##Result, true); \
        } \
        if(capacity > SIZE_MAX / sizeof(ObjectType)) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_CAPACITY_OVERFLOW); \
        } \
        \
        pool->objects = calloc(capacity, sizeof(ObjectType)); \
        pool->used = calloc(capacity, sizeof(uint8_t)); \
        if(pool->objects == NULL || pool->used == NULL) { \
            free(pool->objects); \
            free(pool->used); \
            pool->objects = NULL; \
            pool->used = NULL; \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_ALLOCATION_FAILED); \
        } \
        \
        pool->capacity = capacity; \
        return MEMORY_POOL_VALUE(PoolType##Result, true); \
    } \
    \
    PoolType##Result PoolType##_expand(PoolType *pool, size_t additional_capacity) { \
        size_t capacity; \
        ObjectType *objects; \
        uint8_t *used; \
        \
        if(pool == NULL) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_NULL_POINTER); \
        } \
        if(additional_capacity == 0) { \
            return MEMORY_POOL_VALUE(PoolType##Result, true); \
        } \
        if(pool->capacity > SIZE_MAX - additional_capacity) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_CAPACITY_OVERFLOW); \
        } \
        \
        capacity = pool->capacity + additional_capacity; \
        if(capacity > SIZE_MAX / sizeof(ObjectType)) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_CAPACITY_OVERFLOW); \
        } \
        \
        objects = calloc(capacity, sizeof(ObjectType)); \
        used = calloc(capacity, sizeof(uint8_t)); \
        if(objects == NULL || used == NULL) { \
            free(objects); \
            free(used); \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_ALLOCATION_FAILED); \
        } \
        if(pool->capacity > 0) { \
            memcpy(objects, pool->objects, pool->capacity * sizeof(ObjectType)); \
            memcpy(used, pool->used, pool->capacity * sizeof(uint8_t)); \
        } \
        free(pool->objects); \
        free(pool->used); \
        pool->objects = objects; \
        pool->used = used; \
        pool->capacity = capacity; \
        return MEMORY_POOL_VALUE(PoolType##Result, true); \
    } \
    \
    PoolType##Result PoolType##_defrag(PoolType *pool) { \
        size_t read_index; \
        size_t write_index; \
        \
        if(pool == NULL) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_NULL_POINTER); \
        } \
        if(pool->capacity == 0) { \
            return MEMORY_POOL_VALUE(PoolType##Result, true); \
        } \
        if(pool->objects == NULL || pool->used == NULL) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_NULL_POINTER); \
        } \
        write_index = 0; \
        for(read_index = 0; read_index < pool->capacity; read_index += 1) { \
            if(pool->used[read_index] != 0) { \
                if(write_index != read_index) { \
                    pool->objects[write_index] = pool->objects[read_index]; \
                    pool->used[write_index] = 1; \
                    pool->used[read_index] = 0; \
                    memset(&pool->objects[read_index], 0, sizeof(ObjectType)); \
                } \
                write_index += 1; \
            } \
        } \
        while(write_index < pool->capacity) { \
            pool->used[write_index] = 0; \
            memset(&pool->objects[write_index], 0, sizeof(ObjectType)); \
            write_index += 1; \
        } \
        return MEMORY_POOL_VALUE(PoolType##Result, true); \
    } \
    \
    PoolType##Result PoolType##_shrink(PoolType *pool, size_t capacity) { \
        ObjectType *objects; \
        uint8_t *used; \
        size_t i; \
        \
        if(pool == NULL) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_NULL_POINTER); \
        } \
        if(capacity >= pool->capacity) { \
            return MEMORY_POOL_VALUE(PoolType##Result, true); \
        } \
        if(capacity > SIZE_MAX / sizeof(ObjectType)) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_CAPACITY_OVERFLOW); \
        } \
        for(i = capacity; i < pool->capacity; i += 1) { \
            if(pool->used[i] != 0) { \
                return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_SHRINK_WOULD_REMOVE_USED_OBJECT); \
            } \
        } \
        if(capacity == 0) { \
            free(pool->objects); \
            free(pool->used); \
            pool->objects = NULL; \
            pool->used = NULL; \
            pool->capacity = 0; \
            pool->count = 0; \
            return MEMORY_POOL_VALUE(PoolType##Result, true); \
        } \
        \
        objects = calloc(capacity, sizeof(ObjectType)); \
        used = calloc(capacity, sizeof(uint8_t)); \
        if(objects == NULL || used == NULL) { \
            free(objects); \
            free(used); \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_ALLOCATION_FAILED); \
        } \
        memcpy(objects, pool->objects, capacity * sizeof(ObjectType)); \
        memcpy(used, pool->used, capacity * sizeof(uint8_t)); \
        free(pool->objects); \
        free(pool->used); \
        pool->objects = objects; \
        pool->used = used; \
        pool->capacity = capacity; \
        return MEMORY_POOL_VALUE(PoolType##Result, true); \
    } \
    \
    PoolType##Result PoolType##_clear(PoolType *pool) { \
        if(pool == NULL) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_NULL_POINTER); \
        } \
        if(pool->capacity > 0) { \
            memset(pool->objects, 0, pool->capacity * sizeof(ObjectType)); \
            memset(pool->used, 0, pool->capacity * sizeof(uint8_t)); \
        } \
        pool->count = 0; \
        return MEMORY_POOL_VALUE(PoolType##Result, true); \
    } \
    \
    PoolType##Result PoolType##_destroy(PoolType *pool) { \
        if(pool == NULL) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_NULL_POINTER); \
        } \
        free(pool->objects); \
        free(pool->used); \
        pool->objects = NULL; \
        pool->used = NULL; \
        pool->capacity = 0; \
        pool->count = 0; \
        return MEMORY_POOL_VALUE(PoolType##Result, true); \
    } \
    \
    PoolType##ObjectResult PoolType##_acquire(PoolType *pool) { \
        size_t i; \
        \
        if(pool == NULL) { \
            return MEMORY_POOL_ERROR(PoolType##ObjectResult, ERROR_MEMORY_POOL_NULL_POINTER); \
        } \
        for(i = 0; i < pool->capacity; i += 1) { \
            if(pool->used[i] == 0) { \
                pool->used[i] = 1; \
                pool->count += 1; \
                memset(&pool->objects[i], 0, sizeof(ObjectType)); \
                return MEMORY_POOL_VALUE(PoolType##ObjectResult, &pool->objects[i]); \
            } \
        } \
        return MEMORY_POOL_ERROR(PoolType##ObjectResult, ERROR_MEMORY_POOL_FULL); \
    } \
    \
    PoolType##ObjectResult PoolType##_store(PoolType *pool, ObjectType object) { \
        PoolType##ObjectResult result; \
        \
        result = PoolType##_acquire(pool); \
        if(result.kind == ERROR_RESULT_ERROR) { \
            return result; \
        } \
        *result.result.value = object; \
        return result; \
    } \
    \
    PoolType##ObjectResult PoolType##_store_at(PoolType *pool, size_t index, ObjectType object) { \
        if(pool == NULL || pool->objects == NULL || pool->used == NULL) { \
            return MEMORY_POOL_ERROR(PoolType##ObjectResult, ERROR_MEMORY_POOL_NULL_POINTER); \
        } \
        if(index >= pool->capacity) { \
            return MEMORY_POOL_ERROR(PoolType##ObjectResult, ERROR_MEMORY_POOL_CAPACITY_OVERFLOW); \
        } \
        if(pool->used[index] == 0) { \
            pool->used[index] = 1; \
            pool->count += 1; \
        } \
        pool->objects[index] = object; \
        return MEMORY_POOL_VALUE(PoolType##ObjectResult, &pool->objects[index]); \
    } \
    \
    PoolType##Result PoolType##_release_at(PoolType *pool, size_t index) { \
        if(pool == NULL || pool->objects == NULL || pool->used == NULL) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_NULL_POINTER); \
        } \
        if(index >= pool->capacity) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_CAPACITY_OVERFLOW); \
        } \
        if(pool->used[index] == 0) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_OBJECT_NOT_USED); \
        } \
        pool->used[index] = 0; \
        pool->count -= 1; \
        memset(&pool->objects[index], 0, sizeof(ObjectType)); \
        return MEMORY_POOL_VALUE(PoolType##Result, true); \
    } \
    \
    PoolType##Result PoolType##_release(PoolType *pool, ObjectType *object) { \
        uintptr_t object_address; \
        uintptr_t object_start; \
        uintptr_t object_end; \
        size_t offset; \
        size_t index; \
        \
        if(pool == NULL || object == NULL || pool->objects == NULL) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_NULL_POINTER); \
        } \
        object_address = (uintptr_t)object; \
        object_start = (uintptr_t)pool->objects; \
        object_end = object_start + pool->capacity * sizeof(ObjectType); \
        if(object_address < object_start || object_address >= object_end) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_INVALID_OBJECT); \
        } \
        offset = (size_t)(object_address - object_start); \
        if(offset % sizeof(ObjectType) != 0) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_INVALID_OBJECT); \
        } \
        index = offset / sizeof(ObjectType); \
        if(pool->used[index] == 0) { \
            return MEMORY_POOL_ERROR(PoolType##Result, ERROR_MEMORY_POOL_OBJECT_NOT_USED); \
        } \
        pool->used[index] = 0; \
        pool->count -= 1; \
        memset(object, 0, sizeof(ObjectType)); \
        return MEMORY_POOL_VALUE(PoolType##Result, true); \
    }

#endif
