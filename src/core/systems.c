/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "systems.h"

#include "engine.h"

static void system_entity_by_index_delete(EntityIndex index) {
    EntityResult result = entity_from_index_get(index);

    if(result.kind == ERROR_RESULT_VALUE) {
        (void)entity_delete(result.result.value);
    }
}

void system_entities_past_lifetime_clean(void) {
    for(EntityIndex index = 0; index < MAX_ENTITIES; index += 1) {
        if(!entity_index_alive_check(index) ||
                !entity_index_components_check(index, ROHR_LIFETIME)) continue;
        if((life_times[index].expirey_time != 0 &&
                    life_times[index].expirey_time <= engine_time_get()) ||
                (life_times[index].expirey_tick != 0 &&
                    life_times[index].expirey_tick <= engine_tick_get())) {
            system_entity_by_index_delete(index);
        }
    }
}

Tick system_tick_update(void) {
    Tick ticks = engine_tick_update();

    system_entities_past_lifetime_clean();
    return ticks;
}
