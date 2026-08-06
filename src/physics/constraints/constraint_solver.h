#ifndef CONSTRAINT_SOLVER_H
#define CONSTRAINT_SOLVER_H

#include <stdint.h>

#include "physics/collision/contact_constraint.h"

typedef void (*ConstraintContactsSolveFn)(
    ContactConstraintList *contacts,
    float position_fraction,
    void *context
);
typedef void (*ConstraintJointsSolveFn)(void *context);
typedef void (*ConstraintContactsFinalizeFn)(
    ContactConstraintList *contacts,
    void *context
);

void constraint_solver_run(
    ContactConstraintList *contacts,
    uint32_t iterations,
    ConstraintContactsSolveFn contacts_solve,
    ConstraintJointsSolveFn joints_solve,
    ConstraintContactsFinalizeFn contacts_finalize,
    void *context
);

#endif
