#include "rohr_tools.h"

int main(int argc, char **argv) {
    RohrToolsComponentRegistry registry;

    if(argc != 3) {
        return 1;
    }

    rohr_tools_component_registry_init(&registry);
    if(!rohr_tools_component_registry_component_add(
            &registry,
            (RohrToolsComponentDefinition){
                .name = "BALL_ON_FIRE",
                .type_name = "BallOnFire",
                .type_declaration = "typedef bool BallOnFire;",
                .default_value = "false",
            })) {
        return 1;
    }

    return rohr_tools_component_registry_generate(&registry, argv[1], argv[2]) ? 0 : 1;
}
