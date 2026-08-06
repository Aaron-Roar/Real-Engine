#include "physics/constraints/constraint_solver.h"
#include "physics/joints/joint_constraint.h"

typedef struct SolverTestContext {
    size_t contact_solves;
    size_t joint_solves;
    size_t finalizes;
    bool finalized_early;
    size_t expected_contact_solves;
    float position_fractions[4];
    size_t solve_batches;
} SolverTestContext;

static void contacts_solve(
    ContactConstraintList *contacts,
    float position_fraction,
    void *context
) {
    SolverTestContext *test = context;

    if(contacts == NULL || test->solve_batches >= 4) return;
    test->position_fractions[test->solve_batches++] = position_fraction;
    for(size_t i = 0; i < contacts->count; i += 1) {
        test->contact_solves += 1;
    }
}

static void joints_solve(void *context) {
    SolverTestContext *test = context;
    test->joint_solves += 1;
}

static void contacts_finalize(
    ContactConstraintList *contacts,
    void *context
) {
    SolverTestContext *test = context;

    if(contacts == NULL) return;
    if(test->contact_solves != test->expected_contact_solves) {
        test->finalized_early = true;
    }
    test->finalizes += contacts->count;
}

int main(void) {
    ContactConstraintList contacts;
    JointConstraintList joint_constraints;
    SolverTestContext context = {.expected_contact_solves = 520};

    if(contact_constraint_list_init(&contacts, 1).kind == ERROR_RESULT_ERROR) return 1;
    if(joint_constraint_list_init(&joint_constraints, 1).kind == ERROR_RESULT_ERROR) {
        contact_constraint_list_destroy(&contacts);
        return 1;
    }
    for(EntityIndex i = 0; i < 130; i += 1) {
        if(!joint_constraint_list_append(&joint_constraints, i)) {
            joint_constraint_list_destroy(&joint_constraints);
            contact_constraint_list_destroy(&contacts);
            return 1;
        }
    }
    if(joint_constraints.count != 130 ||
            joint_constraints.capacity < joint_constraints.count) {
        joint_constraint_list_destroy(&joint_constraints);
        contact_constraint_list_destroy(&contacts);
        return 1;
    }
    joint_constraint_list_clear(&joint_constraints);
    if(joint_constraints.count != 0 || joint_constraints.capacity < 130) {
        joint_constraint_list_destroy(&joint_constraints);
        contact_constraint_list_destroy(&contacts);
        return 1;
    }
    for(size_t i = 0; i < 130; i += 1) {
        if(!contact_constraint_list_append(&contacts, (SystemContactConstraint){
                .type = SYSTEM_CONTACT_CONSTRAINT_RIGID_PAIR,
                .value.rigid = {.first_index = (EntityIndex)i}
            })) {
            joint_constraint_list_destroy(&joint_constraints);
            contact_constraint_list_destroy(&contacts);
            return 1;
        }
    }
    if(contacts.count != 130 || contacts.capacity < contacts.count ||
            contact_constraint_list_at(&contacts, 129) == NULL ||
            contact_constraint_list_at(&contacts, 130) != NULL) {
        joint_constraint_list_destroy(&joint_constraints);
        contact_constraint_list_destroy(&contacts);
        return 1;
    }
    constraint_solver_run(&contacts, 4, contacts_solve, joints_solve,
        contacts_finalize, &context);
    if(context.contact_solves != 520 || context.joint_solves != 4 ||
            context.finalizes != 130 || context.finalized_early ||
            context.position_fractions[0] != 0.25f ||
            context.position_fractions[1] != 1.0f / 3.0f ||
            context.position_fractions[2] != 0.5f ||
            context.position_fractions[3] != 1.0f) {
        joint_constraint_list_destroy(&joint_constraints);
        contact_constraint_list_destroy(&contacts);
        return 1;
    }
    contact_constraint_list_clear(&contacts);
    if(contacts.count != 0 || contacts.capacity < 130) {
        joint_constraint_list_destroy(&joint_constraints);
        contact_constraint_list_destroy(&contacts);
        return 1;
    }
    joint_constraint_list_destroy(&joint_constraints);
    contact_constraint_list_destroy(&contacts);
    return 0;
}
