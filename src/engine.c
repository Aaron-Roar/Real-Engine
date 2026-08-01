#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>
#include "engine.h"
#include "engine_internal.h"
#include "entity_components.h"
#include "physics.h"
#include "graphics.h"
#include "grid.h"

SDL_Event sdl_event;

bool engine_running = false;
bool engine_paused = false;

Tick engine_tick_count = 0;

Time engine_time = 0.0;   // simulated engine time in seconds
Time engine_time_per_tick = 1.0 / 60.0;
Time engine_tick_accumulator = 0.0;

SDLTime sdl_prev_counter = 0;
SDLTime sdl_frequency = 0;

EngineResult engine_tables_ensure_capacity(size_t capacity) {
    EngineResult result;

    result = entity_tables_ensure_capacity(capacity);
    if(result.kind != ERROR_RESULT_ERROR) {
        result = physics_tables_ensure_capacity(capacity);
    }
    if(result.kind != ERROR_RESULT_ERROR) {
        result = graphics_tables_ensure_capacity(capacity);
    }
    if(result.kind != ERROR_RESULT_ERROR) {
        result = grid_tables_ensure_capacity(capacity);
    }
    return result;
}

EngineResult engine_init(void) {
    EngineResult result;

    if(engine_running) {
        return error_result_error(ERROR_ENGINE_ALREADY_RUNNING);
    }

    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return error_result_error(ERROR_ENGINE_SDL_INIT_FAILED);
    }
    result = entity_tables_init();
    if(result.kind == ERROR_RESULT_ERROR) {
        SDL_Quit();
        return result;
    }
    result = physics_tables_init();
    if(result.kind == ERROR_RESULT_ERROR) {
        entity_tables_destroy();
        SDL_Quit();
        return result;
    }
    result = graphics_tables_init();
    if(result.kind == ERROR_RESULT_ERROR) {
        physics_tables_destroy();
        entity_tables_destroy();
        SDL_Quit();
        return result;
    }
    result = grid_tables_init();
    if(result.kind == ERROR_RESULT_ERROR) {
        graphics_tables_destroy();
        physics_tables_destroy();
        entity_tables_destroy();
        SDL_Quit();
        return result;
    }
    game_state_runtime_reset();

    sdl_frequency = SDL_GetPerformanceFrequency();
    sdl_prev_counter = SDL_GetPerformanceCounter();

    engine_time = 0.0;
    engine_time_per_tick = 1.0 / 60.0;
    engine_tick_accumulator = 0.0;
    engine_tick_count = 0;

    engine_paused = false;
    engine_running = true;
    return error_result_value(true);
}

void engine_pause(void) {
    engine_paused = true;
}
bool engine_is_paused(void) {
    if(engine_paused) {
        return true;
    }
    return false;
}

void engine_resume(void) {
    sdl_prev_counter = SDL_GetPerformanceCounter();
    engine_paused = false;
}

void engine_update_time(void) {
    SDLTime current_counter = SDL_GetPerformanceCounter();

    Time real_dt =
        (Time)(current_counter - sdl_prev_counter) /
        (Time)sdl_frequency;

    sdl_prev_counter = current_counter;

    if(engine_paused || !engine_running) {
        return;
    }
    engine_time += real_dt;
    engine_tick_accumulator += real_dt;
}

Tick engine_update_tick(void) {
    Tick ticks_advanced;
    engine_update_time();
    if(engine_paused || !engine_running) {
        return 0;
    }
    ticks_advanced = (Tick)(engine_tick_accumulator / engine_time_per_tick);
    if(ticks_advanced == 0) {
        return 0;
    }
    engine_tick_accumulator -= (Time)ticks_advanced * engine_time_per_tick;
    engine_tick_count += ticks_advanced;
    return ticks_advanced;
}

Tick engine_tick_get(void) {
    return engine_tick_count;
}

Time engine_time_get(void) {
    return engine_time;
}
void engine_reset_clock(void) {
    sdl_prev_counter = SDL_GetPerformanceCounter();
    engine_tick_accumulator = 0.0;
}

EngineResult engine_time_per_tick_set(Time value) {
    if(value <= 0.0) return error_result_error(ERROR_ENGINE_STATE_INVALID);
    engine_time_per_tick = value;
    return error_result_value(true);
}

Time engine_time_per_tick_get(void) { return engine_time_per_tick; }

void engine_shutdown(void) {
    game_state_runtime_reset();
    grid_tables_destroy();
    graphics_tables_destroy();
    physics_tables_destroy();
    entity_tables_destroy();
    engine_running = false;
    SDL_Quit();
}

SDL_Event engine_poll_event(void) {
    while (SDL_PollEvent(&sdl_event)) {
        return sdl_event;
    }
    return (SDL_Event) {0};
}
