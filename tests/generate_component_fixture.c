/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "rohr_tools.h"

int main(int argc, char **argv) {
    RohrToolsComponentRegistry registry;

    if(argc != 3) {
        return 1;
    }

    rohr_tools_component_registry_init(&registry);
    if(!rohr_tools_component_registry_tag_add(&registry, "DEAD")) {
        return 1;
    }
    if(!rohr_tools_component_registry_component_add(
            &registry,
            (RohrToolsComponentDefinition){
                .name = "HEALTH",
                .type_name = "Health",
                .type_declaration =
                    "typedef struct Health { float current; float maximum; } Health;",
                .default_value =
                    "(Health){ .current = 100.0f, .maximum = 100.0f }",
            })) {
        return 1;
    }
    if(!rohr_tools_component_registry_component_add(
            &registry,
            (RohrToolsComponentDefinition){
                .name = "INVENTORY",
                .type_name = "Inventory",
                .type_declaration =
                    "typedef struct Inventory { unsigned char payload[4096]; unsigned int count; } Inventory;",
                .default_value = "(Inventory){0}",
            })) {
        return 1;
    }

    return rohr_tools_component_registry_generate(&registry, argv[1], argv[2]) ? 0 : 1;
}
