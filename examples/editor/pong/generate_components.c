#include "rohr_editor.h"

int main(int argc, char **argv) {
    RE_ComponentRegistry registry;

    if(argc != 3) {
        return 1;
    }

    RE_component_registry_init(&registry);
    if(!RE_component_registry_add_component(
            &registry,
            (RE_ComponentDefinition){
                .name = "BALL_ON_FIRE",
                .type_name = "BallOnFire",
                .type_declaration = "typedef bool BallOnFire;",
                .default_value = "false",
            })) {
        return 1;
    }

    return RE_component_registry_generate(&registry, argv[1], argv[2]) ? 0 : 1;
}
