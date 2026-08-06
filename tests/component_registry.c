#include "rohr_tools.h"

#include <string.h>

int main(void) {
    RohrToolsComponentRegistry registry;
    RohrToolsComponentDefinition health = {
        .name = "HEALTH",
        .type_name = "Health",
        .type_declaration = "typedef struct Health { float current; float maximum; } Health;",
        .default_value = "(Health){ .current = 100.0f, .maximum = 100.0f }",
    };

    rohr_tools_component_registry_init(&registry);
    if(!rohr_tools_component_registry_tag_add(&registry, "DEAD")) {
        return 1;
    }
    if(!rohr_tools_component_registry_tag_add(&registry, "DEAD") || registry.tag_count != 1) {
        return 1;
    }
    if(!rohr_tools_component_registry_component_add(&registry, health)) {
        return 1;
    }
    if(registry.component_count != 1 ||
            strcmp(registry.components[0].type_name, "Health") != 0) {
        return 1;
    }
    if(rohr_tools_component_registry_tag_add(&registry, "HEALTH")) {
        return 1;
    }
    if(rohr_tools_component_registry_tag_add(&registry, "not-valid!")) {
        return 1;
    }
    return 0;
}
