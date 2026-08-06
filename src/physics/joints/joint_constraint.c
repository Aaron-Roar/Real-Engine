#include "joint_constraint.h"

#include <stdlib.h>

static bool joint_constraint_list_reserve(
    JointConstraintList *list,
    size_t requested
) {
    EntityIndex *values;
    size_t capacity;

    if(list == NULL) return false;
    if(requested <= list->capacity) return true;
    capacity = list->capacity == 0 ? 16 : list->capacity;
    while(capacity < requested) capacity *= 2;
    values = realloc(list->values, capacity * sizeof(*values));
    if(values == NULL) return false;
    list->values = values;
    list->capacity = capacity;
    return true;
}

EngineResult joint_constraint_list_init(
    JointConstraintList *list,
    size_t initial_capacity
) {
    if(list == NULL) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    *list = (JointConstraintList){0};
    if(initial_capacity > 0 &&
            !joint_constraint_list_reserve(list, initial_capacity)) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    return error_result_value(true);
}

void joint_constraint_list_destroy(JointConstraintList *list) {
    if(list == NULL) return;
    free(list->values);
    *list = (JointConstraintList){0};
}

void joint_constraint_list_clear(JointConstraintList *list) {
    if(list != NULL) list->count = 0;
}

bool joint_constraint_list_append(
    JointConstraintList *list,
    EntityIndex joint
) {
    if(list == NULL || !joint_constraint_list_reserve(list, list->count + 1)) {
        return false;
    }
    list->values[list->count++] = joint;
    return true;
}
