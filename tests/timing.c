#include "rohr.h"

#include <math.h>

int main(void) {
    Tick ticks;

    if(rohr_error_check(rohr_engine_init()) ||
            rohr_error_check(rohr_engine_time_per_tick_set(0.01))) {
        return 1;
    }
    rohr_engine_reset_clock();
    if(rohr_engine_update_tick() != 0) {
        rohr_engine_shutdown();
        return 1;
    }
    SDL_Delay(30);
    ticks = rohr_engine_update_tick();
    if(ticks < 2 || rohr_physics_dt_per_tick_get() != 0.01) {
        rohr_engine_shutdown();
        return 1;
    }
    if(rohr_error_check(rohr_physics_dt_per_tick_set(0.5))) {
        rohr_engine_shutdown();
        return 1;
    }
    rohr_engine_reset_clock();
    SDL_Delay(25);
    ticks = rohr_engine_update_tick();
    if(ticks < 2 || rohr_physics_dt_per_tick_get() != 0.5) {
        rohr_engine_shutdown();
        return 1;
    }
    rohr_physics_use_engine_time_per_tick();
    if(rohr_physics_dt_per_tick_get() != rohr_engine_time_per_tick_get()) {
        rohr_engine_shutdown();
        return 1;
    }
    rohr_engine_shutdown();
    return 0;
}
