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
    EntityContact contacts[1];
    Entity first = entity_handle(1, 1);
    Entity second = entity_handle(1, 2);
    OverlapInfo overlap = {
        .detected = true,
        .normal = {0.5f, -0.75f},
        .depth = 3.0f
    };
    ContactInfo contact = {
        .detected = true,
        .normal = {0.5f, -0.75f},
        .depth = 3.0f,
        .points = {{4.0f, 5.0f}},
        .point_count = 1,
        .relative_velocity = {6.0f, 7.0f},
        .normal_impulse = {8.0f, 9.0f},
        .friction_impulse = {10.0f, 11.0f}
    };
    EngineResult result;
    Vec2D total_impulse;
    size_t i;

    total_impulse = physics_contact_total_impulse_get(contact);
    if(fabsf(total_impulse.x - 18.0f) > 0.0001f ||
            fabsf(total_impulse.y - 20.0f) > 0.0001f) return 1;

    if(error_check(physics_interaction_set_init(&set, 2))) return 1;
    result = physics_interaction_set_record(
        &set, second, first, overlap, (ContactInfo){0},
        PHYSICS_INTERACTION_OVERLAP
    );
    if(error_check(result) || !result.result.value || set.count != 1 ||
            !physics_interaction_set_check(
                &set, first, second, PHYSICS_INTERACTION_OVERLAP) ||
            physics_interaction_set_check(
                &set, first, second, PHYSICS_INTERACTION_CONTACT)) goto fail;

    result = physics_interaction_set_record(
        &set, second, first, overlap, contact, PHYSICS_INTERACTION_CONTACT
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
            interaction.overlap.depth != 3.0f ||
            !interaction.contact.detected ||
            fabsf(interaction.contact.relative_velocity.x - 6.0f) > 0.0001f ||
            fabsf(interaction.contact.normal_impulse.y - 9.0f) > 0.0001f ||
            fabsf(interaction.contact.friction_impulse.x - 10.0f) > 0.0001f) goto fail;

    for(i = 3; i < 200; i += 1) {
        result = physics_interaction_set_record(
            &set,
            first,
            entity_handle(1, (uint16_t)i),
            overlap,
            (ContactInfo){0},
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
    if(physics_interaction_set_contacts_get(
                &set, first, contacts, 1) != 1 ||
            contacts[0].target != second ||
            !contacts[0].contact.detected ||
            fabsf(contacts[0].contact.normal.x - -0.5f) > 0.0001f ||
            fabsf(contacts[0].contact.relative_velocity.x - -6.0f) > 0.0001f ||
            fabsf(contacts[0].contact.normal_impulse.y - -9.0f) > 0.0001f ||
            fabsf(contacts[0].contact.friction_impulse.x - -10.0f) > 0.0001f ||
            contacts[0].contact.point_count != 1 ||
            contacts[0].contact.points[0].x != 4.0f) goto fail;
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
