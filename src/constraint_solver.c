#include "constraint_solver.h"

void constraint_solver_run(
    ContactConstraintList *contacts,
    uint32_t iterations,
    ConstraintContactsSolveFn contacts_solve,
    ConstraintJointsSolveFn joints_solve,
    ConstraintContactsFinalizeFn contacts_finalize,
    void *context
) {
    float position_fraction;

    if(contacts == NULL || iterations == 0 || contacts_solve == NULL) return;
    for(uint32_t iteration = 0; iteration < iterations; iteration += 1) {
        position_fraction = 1.0f / (float)(iterations - iteration);
        contacts_solve(contacts, position_fraction, context);
        if(joints_solve != NULL) joints_solve(context);
    }
    if(contacts_finalize != NULL) contacts_finalize(contacts, context);
}
