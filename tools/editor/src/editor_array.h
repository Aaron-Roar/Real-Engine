#ifndef ROHR_EDITOR_ARRAY_H
#define ROHR_EDITOR_ARRAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static inline bool editor_array_reserve(void **items, size_t *capacity,
        size_t required, size_t item_size) {
    void *resized;
    size_t next;
    if(items == NULL || capacity == NULL || item_size == 0) return false;
    if(required <= *capacity) return true;
    next = *capacity == 0 ? 4 : *capacity;
    while(next < required) {
        if(next > SIZE_MAX / 2) {
            next = required;
            break;
        }
        next *= 2;
    }
    if(next > SIZE_MAX / item_size) return false;
    resized = realloc(*items, next * item_size);
    if(resized == NULL) return false;
    *items = resized;
    *capacity = next;
    return true;
}

#define EDITOR_ARRAY_RESERVE(items, capacity, required) \
    editor_array_reserve((void **)&(items), &(capacity), (required), \
        sizeof(*(items)))

#endif
