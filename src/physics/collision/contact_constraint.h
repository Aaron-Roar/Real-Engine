/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef CONTACT_CONSTRAINT_H
#define CONTACT_CONSTRAINT_H

#include <stdbool.h>
#include <stddef.h>

#include "entity_components.h"
#include "physics.h"
#include "physics/collision/contact_manifold.h"

typedef struct SystemSoftBoundaryQuery {
    Entity node_a;
    Entity node_b;
    EntityIndex a;
    EntityIndex b;
    Shape shape;
    Position start;
    Position end;
    Entity rigid_entity;
    EntityIndex rigid;
    OverlapInfo overlap;
    float t;
    bool solving;
    bool solved;
    float position_fraction;
    ContactInfo contact;
} SystemSoftBoundaryQuery;

typedef enum SystemContactConstraintType {
    SYSTEM_CONTACT_CONSTRAINT_RIGID_PAIR,
    SYSTEM_CONTACT_CONSTRAINT_SOFT_BOUNDARY
} SystemContactConstraintType;

typedef struct SystemContactConstraint {
    SystemContactConstraintType type;
    union {
        struct {
            Entity first;
            Entity second;
            EntityIndex first_index;
            EntityIndex second_index;
            OverlapInfo overlap;
            bool responds;
            bool solved;
            ContactInfo contact;
            ContactInfo manifold_contacts[CONTACT_MANIFOLD_MAX];
            uint8_t manifold_contact_count;
        } rigid;
        SystemSoftBoundaryQuery soft;
    } value;
} SystemContactConstraint;

typedef struct ContactConstraintList {
    SystemContactConstraint *values;
    size_t count;
    size_t capacity;
} ContactConstraintList;

EngineResult contact_constraint_list_init(
    ContactConstraintList *list,
    size_t initial_capacity
);
void contact_constraint_list_destroy(ContactConstraintList *list);
void contact_constraint_list_clear(ContactConstraintList *list);
bool contact_constraint_list_append(
    ContactConstraintList *list,
    SystemContactConstraint constraint
);
SystemContactConstraint *contact_constraint_list_at(
    ContactConstraintList *list,
    size_t index
);

#endif
