#include "rohr_editor.h"

int main(int argc, char **argv) {
    RE_ComponentRegistry registry;

    if(argc != 3) {
        return 1;
    }

    RE_component_registry_init(&registry);
    if(!RE_component_registry_tag_add(&registry, "DEAD")) {
        return 1;
    }
    if(!RE_component_registry_component_add(
            &registry,
            (RE_ComponentDefinition){
                .name = "HEALTH",
                .type_name = "Health",
                .type_declaration =
                    "typedef struct Health { float current; float maximum; } Health;",
                .default_value =
                    "(Health){ .current = 100.0f, .maximum = 100.0f }",
            })) {
        return 1;
    }
    if(!RE_component_registry_component_add(
            &registry,
            (RE_ComponentDefinition){
                .name = "INVENTORY",
                .type_name = "Inventory",
                .type_declaration =
                    "typedef struct Inventory { unsigned char payload[4096]; unsigned int count; } Inventory;",
                .default_value = "(Inventory){0}",
            })) {
        return 1;
    }

    return RE_component_registry_generate(&registry, argv[1], argv[2]) ? 0 : 1;
}
