#include "game_components.h"

#include <stddef.h>

static unsigned int inventory_destroy_count = 0;

static void inventory_destroy(Entity entity, Inventory *inventory) {
    (void)entity;
    inventory->count = 0;
    inventory_destroy_count += 1;
}

int main(void) {
    EntityResult first_result;
    EntityResult second_result;
    EntityResult third_result;
    Entity first;
    Entity second;
    Entity third;
    GameHealthResult health_result;
    Health *health_addr;
    Inventory inventory = {0};

    if(rohr_error_check(rohr_engine_init()) || !game_components_init()) {
        return 1;
    }

    first_result = rohr_entity_add();
    second_result = rohr_entity_add();
    third_result = rohr_entity_add();
    if(rohr_error_check(first_result) || rohr_error_check(second_result) ||
            rohr_error_check(third_result)) {
        game_components_shutdown();
        rohr_engine_shutdown();
        return 1;
    }
    first = first_result.result.value;
    second = second_result.result.value;
    third = third_result.result.value;

    if(!game_dead_set(first) || !game_dead_has(first)) {
        return 1;
    }
    game_dead_remove(first);
    if(game_dead_has(first)) {
        return 1;
    }

    if(!game_health_set(first, (Health){10.0f, 100.0f}) ||
            !game_health_set(second, (Health){20.0f, 100.0f}) ||
            !game_health_set(third, (Health){30.0f, 100.0f})) {
        return 1;
    }
    health_result = game_health_get(second);
    if(rohr_error_check(health_result) || health_result.result.value.current != 20.0f) {
        return 1;
    }
    health_addr = game_health_get_addr(first);
    if(health_addr == NULL) {
        return 1;
    }
    health_addr->current = 5.0f;
    health_result = game_health_get(first);
    if(rohr_error_check(health_result) || health_result.result.value.current != 5.0f) {
        return 1;
    }

    game_health_remove(second);
    if(game_health_has(second) || !game_health_has(third)) {
        return 1;
    }

    inventory.count = 1;
    game_inventory_set_destroy_hook(inventory_destroy);
    if(!game_inventory_set(first, inventory) || !game_inventory_has(first)) {
        return 1;
    }
    inventory.count = 2;
    if(!game_inventory_set(first, inventory) || inventory_destroy_count != 1) {
        return 1;
    }

    game_components_clear(first);
    if(game_health_has(first) || game_inventory_has(first) || game_dead_has(first) ||
            inventory_destroy_count != 2) {
        return 1;
    }

    game_components_clear(third);
    game_components_shutdown();
    rohr_engine_shutdown();
    return 0;
}
