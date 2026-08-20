/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "physics.h"

#include "core/engine_internal.h"
#include "physics/collision/interaction_query.h"

bool physics_contact_check(Entity entity, Entity target) {
    return physics_interaction_query_check(
        entity, target, PHYSICS_INTERACTION_CONTACT);
}

ContactInfo physics_contact_get(Entity entity, Entity target) {
    PhysicsInteraction interaction;

    if(!physics_interaction_query_get(entity, target,
            PHYSICS_INTERACTION_CONTACT, &interaction)) {
        return (ContactInfo){.detected = false};
    }
    return interaction.contact;
}

Vec2D physics_contact_total_impulse_get(ContactInfo contact) {
    Vec2D total = {0};

    for(uint8_t i = 0; i < contact.point_count; i += 1) {
        total.x += contact.points[i].normal_impulse.x +
            contact.points[i].friction_impulse.x;
        total.y += contact.points[i].normal_impulse.y +
            contact.points[i].friction_impulse.y;
    }
    return total;
}

bool physics_contact_entered_check(Entity entity, Entity target) {
    return physics_interaction_query_entered_check(
        entity, target, PHYSICS_INTERACTION_CONTACT);
}

bool physics_contact_stayed_check(Entity entity, Entity target) {
    return physics_interaction_query_stayed_check(
        entity, target, PHYSICS_INTERACTION_CONTACT);
}

bool physics_contact_exited_check(Entity entity, Entity target) {
    return physics_interaction_query_exited_check(
        entity, target, PHYSICS_INTERACTION_CONTACT);
}

size_t physics_contact_count_get(Entity entity) {
    if(!physics_interaction_query_entity_valid(entity)) return 0;
    return physics_interaction_current_count_get(
        entity, PHYSICS_INTERACTION_CONTACT);
}

size_t physics_contacts_get(Entity entity, EntityContact *results,
        size_t capacity) {
    if(!physics_interaction_query_entity_valid(entity)) return 0;
    return physics_interaction_current_contacts_get(entity, results, capacity);
}
