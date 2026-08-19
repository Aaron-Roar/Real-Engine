#include "physics.h"

#include "systems.h"

#include <math.h>

static Time physics_dt_per_tick = 0.0;
static bool physics_dt_overwritten = false;
static uint32_t physics_solver_iterations = PHYSICS_SOLVER_ITERATIONS_DEFAULT;
static uint32_t physics_substeps = PHYSICS_SUBSTEPS_DEFAULT;

void physics_config_init(void) {
    physics_dt_per_tick = 0.0;
    physics_dt_overwritten = false;
    physics_solver_iterations = PHYSICS_SOLVER_ITERATIONS_DEFAULT;
    physics_substeps = PHYSICS_SUBSTEPS_DEFAULT;
}

EngineResult physics_dt_per_tick_set(Time dt) {
    if(dt <= 0.0) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    physics_dt_per_tick = dt;
    physics_dt_overwritten = true;
    return error_result_value(true);
}

Time physics_dt_per_tick_get(void) {
    return physics_dt_overwritten ? physics_dt_per_tick : engine_time_per_tick_get();
}

void physics_engine_time_per_tick_use(void) {
    physics_dt_per_tick = 0.0;
    physics_dt_overwritten = false;
}

EngineResult physics_solver_iterations_set(uint32_t iterations) {
    if(iterations == 0) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    physics_solver_iterations = iterations;
    return error_result_value(true);
}

uint32_t physics_solver_iterations_get(void) {
    return physics_solver_iterations;
}

EngineResult physics_substeps_set(uint32_t substeps) {
    if(substeps == 0) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    physics_substeps = substeps;
    return error_result_value(true);
}

uint32_t physics_substeps_get(void) {
    return physics_substeps;
}

void physics_update(Tick ticks) {
    if(ticks == 0) return;
    system_physics_update(physics_dt_per_tick_get() * (Time)ticks);
}

void physics_dt_update(Time dt) {
    if(dt > 0.0) system_physics_update(dt);
}

