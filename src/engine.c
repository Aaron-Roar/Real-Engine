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
Time engine_dt = 0.0;     // simulated delta time in seconds

Time engine_override_dt = 0.0;
bool engine_dt_overwritten = false;

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
    engine_dt = 0.0;
    engine_override_dt = 0.0;
    engine_dt_overwritten = false;
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
    engine_paused = false;
}

void engine_update_time(void) {
    SDLTime current_counter = SDL_GetPerformanceCounter();

    Time real_dt =
        (Time)(current_counter - sdl_prev_counter) /
        (Time)sdl_frequency;

    sdl_prev_counter = current_counter;

    if(engine_paused || !engine_running) {
        engine_dt = 0.0;
        return;
    }

    if(engine_dt_overwritten) {
        engine_dt = engine_override_dt;
    } else {
        engine_dt = real_dt;
    }
    engine_time += engine_dt;
}

void engine_update_tick(void) {
    if(engine_paused || !engine_running) {
        return;
    }
    engine_tick_count += 1;

}

Tick engine_get_tick(void) {
    return engine_tick_count;
}

Time engine_get_time(void) {
    return engine_time;
}
void engine_reset_clock(void) {
    sdl_prev_counter = SDL_GetPerformanceCounter();
    engine_dt = engine_dt_overwritten ? engine_override_dt : 0.0;
}

void engine_set_dt(Time dt) {
    engine_override_dt = dt;
    engine_dt = dt;
    engine_dt_overwritten = true;
}

void engine_calculate_dt(void) {
    engine_override_dt = 0.0;
    engine_dt_overwritten = false;
}

Time engine_get_dt(void) {
    return engine_dt;
}

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
