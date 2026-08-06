#ifndef JOINT_CONSTRAINT_H
#define JOINT_CONSTRAINT_H

#include <stdbool.h>
#include <stddef.h>

#include "entity_components.h"

typedef struct JointConstraintList {
    EntityIndex *values;
    size_t count;
    size_t capacity;
} JointConstraintList;

EngineResult joint_constraint_list_init(
    JointConstraintList *list,
    size_t initial_capacity
);
void joint_constraint_list_destroy(JointConstraintList *list);
void joint_constraint_list_clear(JointConstraintList *list);
bool joint_constraint_list_append(
    JointConstraintList *list,
    EntityIndex joint
);

#endif
