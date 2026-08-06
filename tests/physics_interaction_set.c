#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "physics_interaction_set.h"

static Entity entity_handle(uint16_t generation, uint16_t slot) {
    return ((Entity)generation << 16) | slot;
}

int main(void) {
    PhysicsInteractionSet set;
    PhysicsInteraction interaction;
    EntityInteraction interactions[200];
    Entity first = entity_handle(1, 1);
    Entity second = entity_handle(1, 2);
    OverlapInfo overlap = {
        .detected = true,
        .normal = {0.5f, -0.75f},
        .depth = 3.0f
    };
    EngineResult result;
    size_t i;

    if(error_check(physics_interaction_set_init(&set, 2))) return 1;
    result = physics_interaction_set_record(
        &set, second, first, overlap, PHYSICS_INTERACTION_OVERLAP
    );
    if(error_check(result) || !result.result.value || set.count != 1 ||
            !physics_interaction_set_check(
                &set, first, second, PHYSICS_INTERACTION_OVERLAP) ||
            physics_interaction_set_check(
                &set, first, second, PHYSICS_INTERACTION_CONTACT)) goto fail;

    result = physics_interaction_set_record(
        &set, second, first, overlap, PHYSICS_INTERACTION_CONTACT
    );
    if(error_check(result) || result.result.value || set.count != 1 ||
            !physics_interaction_set_check(
                &set,
                first,
                second,
                PHYSICS_INTERACTION_OVERLAP | PHYSICS_INTERACTION_CONTACT
            )) goto fail;
    if(!physics_interaction_set_get(&set, second, first, &interaction) ||
            interaction.pair.first != second || interaction.pair.second != first ||
            fabsf(interaction.overlap.normal.x - 0.5f) > 0.0001f ||
            fabsf(interaction.overlap.normal.y - -0.75f) > 0.0001f ||
            interaction.overlap.depth != 3.0f) goto fail;

    for(i = 3; i < 200; i += 1) {
        result = physics_interaction_set_record(
            &set,
            first,
            entity_handle(1, (uint16_t)i),
            overlap,
            PHYSICS_INTERACTION_OVERLAP
        );
        if(error_check(result)) goto fail;
    }
    if(physics_interaction_set_entity_count_get(
                &set, first, PHYSICS_INTERACTION_OVERLAP) != 198 ||
            physics_interaction_set_entity_count_get(
                &set, first, PHYSICS_INTERACTION_CONTACT) != 1 ||
            physics_interaction_set_entities_get(
                &set,
                first,
                PHYSICS_INTERACTION_OVERLAP,
                interactions,
                200
            ) != 198) goto fail;
    {
        bool found = false;

        for(i = 0; i < 198; i += 1) {
            if(interactions[i].target != second) continue;
            if(fabsf(interactions[i].overlap.normal.x - -0.5f) > 0.0001f ||
                    fabsf(interactions[i].overlap.normal.y - 0.75f) > 0.0001f) {
                goto fail;
            }
            found = true;
            break;
        }
        if(!found) goto fail;
    }
    if(physics_interaction_set_entities_get(
                &set,
                second,
                PHYSICS_INTERACTION_CONTACT,
                interactions,
                1
            ) != 1 ||
            interactions[0].target != first ||
            fabsf(interactions[0].overlap.normal.x - 0.5f) > 0.0001f ||
            fabsf(interactions[0].overlap.normal.y - -0.75f) > 0.0001f ||
            physics_interaction_set_entities_get(
                &set,
                first,
                PHYSICS_INTERACTION_OVERLAP,
                interactions,
                4
            ) != 4) goto fail;
    if(!physics_interaction_set_get(&set, first, second, &interaction) ||
            !physics_interaction_set_check(
                &set, first, second, PHYSICS_INTERACTION_CONTACT)) goto fail;
    result = physics_interaction_set_remove(&set, first, second);
    if(error_check(result) || !result.result.value ||
            physics_interaction_set_check(
                &set, first, second, PHYSICS_INTERACTION_OVERLAP)) goto fail;

    physics_interaction_set_clear(&set);
    if(set.count != 0) goto fail;
    physics_interaction_set_destroy(&set);
    return 0;

fail:
    physics_interaction_set_destroy(&set);
    return 1;
}
