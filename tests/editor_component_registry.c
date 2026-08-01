#include "rohr_editor.h"

#include <string.h>

int main(void) {
    RE_ComponentRegistry registry;
    RE_ComponentDefinition health = {
        .name = "HEALTH",
        .type_name = "Health",
        .type_declaration = "typedef struct Health { float current; float maximum; } Health;",
        .default_value = "(Health){ .current = 100.0f, .maximum = 100.0f }",
    };

    RE_component_registry_init(&registry);
    if(!RE_component_registry_add_tag(&registry, "DEAD")) {
        return 1;
    }
    if(!RE_component_registry_add_tag(&registry, "DEAD") || registry.tag_count != 1) {
        return 1;
    }
    if(!RE_component_registry_add_component(&registry, health)) {
        return 1;
    }
    if(registry.component_count != 1 ||
            strcmp(registry.components[0].type_name, "Health") != 0) {
        return 1;
    }
    if(RE_component_registry_add_tag(&registry, "HEALTH")) {
        return 1;
    }
    if(RE_component_registry_add_tag(&registry, "not-valid!")) {
        return 1;
    }
    return 0;
}
