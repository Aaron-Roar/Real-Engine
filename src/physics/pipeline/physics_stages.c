#include "physics.h"

#include "physics/constraints/constraint_solver.h"
#include "physics/physics_step_internal.h"

#include <SDL3/SDL_timer.h>

void physics_pipeline_step_begin(void) {
    physics_step_debug_stats = (PhysicsDebugStats){0};
    physics_interactions_step_begin();
}

void physics_pipeline_substep_begin(void) {
    contact_constraint_list_clear(&physics_step_contact_constraints);
    joint_constraint_list_clear(&physics_step_joint_constraints);
}

void physics_pipeline_accelerations_clear(void) {
    physics_rigid_accelerations_clear();
}

void physics_pipeline_gravity_apply(void) {
    physics_rigid_gravity_apply(physics_gravity_get());
}

void physics_pipeline_forces_apply(void) {
    physics_joint_spring_forces_apply();
    physics_soft_body_beams_apply();
}

void physics_pipeline_integrate(double dt) {
    physics_rigid_integrate(dt);
}

void physics_pipeline_contacts_gather(void) {
    uint64_t started = 0;
    double broadphase_build_before = physics_step_debug_stats.broadphase_build_ms;
    double narrowphase_before = physics_step_debug_stats.narrowphase_ms;

    if(physics_step_debug_stats_enabled) started = SDL_GetPerformanceCounter();
    physics_rigid_constraints_gather();
    physics_soft_body_constraints_gather();
    if(physics_step_debug_stats_enabled) {
        double broadphase_query_ms =
            physics_step_elapsed_ms(started) -
            (physics_step_debug_stats.broadphase_build_ms -
                broadphase_build_before) -
            (physics_step_debug_stats.narrowphase_ms - narrowphase_before);

        if(broadphase_query_ms < 0.0) broadphase_query_ms = 0.0;
        physics_step_debug_stats.broadphase_query_ms += broadphase_query_ms;
    }
}

void physics_pipeline_joints_gather(void) {
    physics_joint_constraints_gather();
}

void physics_pipeline_constraints_solve(uint32_t iterations) {
    uint64_t started = 0;

    if(iterations == 0) return;
    if(physics_step_debug_stats_enabled) started = SDL_GetPerformanceCounter();
    constraint_solver_run(
        &physics_step_contact_constraints,
        iterations,
        physics_pipeline_contact_constraints_solve,
        physics_pipeline_joint_constraints_solve,
        physics_pipeline_contact_constraints_finalize,
        NULL);
    if(physics_step_debug_stats_enabled) {
        physics_step_debug_stats.response_ms += physics_step_elapsed_ms(started);
    }
}
