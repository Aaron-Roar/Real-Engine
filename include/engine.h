#ifndef ENGINE_H
#define ENGINE_H
#include <SDL3/SDL.h>
#include <stdint.h>
#include "error.h"

/** SDL performance counter value used for engine timing. */
typedef Uint64 SDLTime;

/** Engine time value, measured in seconds. */
typedef double Time;

/** Monotonic engine tick counter. */
typedef uint64_t Tick;

/**
 * Initialize SDL and all engine-owned subsystem tables.
 *
 * @return EngineResult containing true on success, or an error describing the
 * failing subsystem.
 */
EngineResult engine_init(void);

/**
 * Shut down all engine subsystems and SDL.
 */
void engine_shutdown(void);

/**
 * Update engine time and delta time from SDL's performance counter.
 *
 * If the engine is paused, delta time is set to zero.
 */
void engine_update_time(void);

/**
 * Get accumulated engine time in seconds.
 *
 * @return Current simulated engine time.
 */
Time engine_time_get(void);

/**
 * Get the current engine tick count.
 *
 * @return Number of ticks advanced since initialization or reset.
 */
Tick engine_tick_get(void);

/**
 * Pause engine time and tick advancement.
 */
void engine_pause(void);

/**
 * Resume engine time and tick advancement.
 */
void engine_resume(void);

/** Update elapsed time and consume every complete fixed tick. */
Tick engine_update_tick(void);

/** Set the real-time duration required for one engine tick. */
EngineResult engine_time_per_tick_set(Time time_per_tick);
/** Return the real-time duration required for one engine tick. */
Time engine_time_per_tick_get(void);

/**
 * Poll one SDL event.
 *
 * @return The next SDL event, or a zeroed SDL_Event when no event is pending.
 */
SDL_Event engine_poll_event(void);

/**
 * Check whether the engine is paused.
 *
 * @return true when paused, false otherwise.
 */
bool engine_is_paused(void);

/**
 * Reset the internal timing baseline without advancing time.
 */
void engine_reset_clock(void);
#endif
