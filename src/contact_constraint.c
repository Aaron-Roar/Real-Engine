#include "contact_constraint.h"

#include <stdlib.h>

static bool contact_constraint_list_reserve(
    ContactConstraintList *list,
    size_t requested
) {
    SystemContactConstraint *values;
    size_t capacity;

    if(list == NULL) return false;
    if(requested <= list->capacity) return true;
    capacity = list->capacity == 0 ? 64 : list->capacity;
    while(capacity < requested) capacity *= 2;
    values = realloc(list->values, capacity * sizeof(*values));
    if(values == NULL) return false;
    list->values = values;
    list->capacity = capacity;
    return true;
}

EngineResult contact_constraint_list_init(
    ContactConstraintList *list,
    size_t initial_capacity
) {
    if(list == NULL) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    *list = (ContactConstraintList){0};
    if(initial_capacity > 0 &&
            !contact_constraint_list_reserve(list, initial_capacity)) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    return error_result_value(true);
}

void contact_constraint_list_destroy(ContactConstraintList *list) {
    if(list == NULL) return;
    free(list->values);
    *list = (ContactConstraintList){0};
}

void contact_constraint_list_clear(ContactConstraintList *list) {
    if(list != NULL) list->count = 0;
}

bool contact_constraint_list_append(
    ContactConstraintList *list,
    SystemContactConstraint constraint
) {
    if(list == NULL || !contact_constraint_list_reserve(list, list->count + 1)) {
        return false;
    }
    list->values[list->count++] = constraint;
    return true;
}

SystemContactConstraint *contact_constraint_list_at(
    ContactConstraintList *list,
    size_t index
) {
    if(list == NULL || index >= list->count) return NULL;
    return &list->values[index];
}
