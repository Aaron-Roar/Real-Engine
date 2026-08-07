#include "physics.h"

#include "physics/physics_step_internal.h"

#include <SDL3/SDL_timer.h>

void physics_pipeline_substep(double dt) {
    physics_pipeline_substep_begin();
    physics_pipeline_accelerations_clear();
    physics_pipeline_forces_apply();
    physics_pipeline_integrate(dt);
    physics_pipeline_contacts_gather();
    physics_pipeline_joints_gather();
    physics_pipeline_constraints_solve(physics_solver_iterations_get());
}

void physics_pipeline_update(double dt) {
    uint64_t started =
        physics_step_debug_stats_enabled ? SDL_GetPerformanceCounter() : 0;
    uint32_t substeps = physics_substeps_get();
    double substep_dt = dt / (double)substeps;

    physics_pipeline_step_begin();
    for(uint32_t substep = 0; substep < substeps; substep += 1) {
        physics_pipeline_substep(substep_dt);
    }
    if(physics_step_debug_stats_enabled) {
        physics_step_debug_stats.total_ms = physics_step_elapsed_ms(started);
    }
}
