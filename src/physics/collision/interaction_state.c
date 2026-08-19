#include "physics.h"

#include "core/engine_internal.h"
#include "physics/collision/interaction_set.h"

static PhysicsInteractionSet current_interactions = {0};
static PhysicsInteractionSet previous_interactions = {0};

EngineResult physics_interaction_state_init(void) {
    if(error_check(physics_interaction_set_init(&current_interactions, 64)))
        goto fail;
    if(error_check(physics_interaction_set_init(&previous_interactions, 64)))
        goto fail;
    return error_result_value(true);

fail:
    physics_interaction_set_destroy(&current_interactions);
    physics_interaction_set_destroy(&previous_interactions);
    return error_result_error(ERROR_ENGINE_PHYSICS_TABLES_INIT_FAILED);
}

void physics_interaction_state_destroy(void) {
    physics_interaction_set_destroy(&current_interactions);
    physics_interaction_set_destroy(&previous_interactions);
}

void physics_interactions_step_begin(void) {
    PhysicsInteractionSet interactions = previous_interactions;

    previous_interactions = current_interactions;
    current_interactions = interactions;
    physics_interaction_set_clear(&current_interactions);
}

EngineResult physics_interaction_record(
    Entity entity,
    Entity target,
    OverlapInfo overlap,
    ContactInfo contact,
    PhysicsInteractionFlags flags
) {
    return physics_interaction_set_record(
        &current_interactions, entity, target, overlap, contact, flags
    );
}

bool physics_interaction_current_check(
    Entity entity,
    Entity target,
    PhysicsInteractionFlags flags
) {
    return physics_interaction_set_check(
        &current_interactions, entity, target, flags
    );
}

bool physics_interaction_previous_check(
    Entity entity,
    Entity target,
    PhysicsInteractionFlags flags
) {
    return physics_interaction_set_check(
        &previous_interactions, entity, target, flags
    );
}

bool physics_interaction_current_get(
    Entity entity,
    Entity target,
    PhysicsInteraction *interaction
) {
    return physics_interaction_set_get(
        &current_interactions, entity, target, interaction
    );
}

bool physics_interaction_previous_get(
    Entity entity,
    Entity target,
    PhysicsInteraction *interaction
) {
    return physics_interaction_set_get(
        &previous_interactions, entity, target, interaction
    );
}

size_t physics_interaction_current_count_get(
    Entity entity,
    PhysicsInteractionFlags flags
) {
    return physics_interaction_set_entity_count_get(
        &current_interactions, entity, flags
    );
}

size_t physics_interaction_current_entities_get(
    Entity entity,
    PhysicsInteractionFlags flags,
    EntityInteraction *results,
    size_t capacity
) {
    return physics_interaction_set_entities_get(
        &current_interactions, entity, flags, results, capacity
    );
}

size_t physics_interaction_current_contacts_get(
    Entity entity,
    EntityContact *results,
    size_t capacity
) {
    return physics_interaction_set_contacts_get(
        &current_interactions, entity, results, capacity
    );
}

void physics_interaction_current_visit(
    PhysicsInteractionFlags flags,
    PhysicsInteractionVisitFunction function,
    void *context
) {
    if(function == NULL) return;
    for(size_t index = 0; index < current_interactions.capacity; index += 1) {
        const PhysicsInteraction *interaction;
        if(current_interactions.occupied[index] == 0) continue;
        interaction = &current_interactions.entries[index];
        if((interaction->flags & flags) != flags) continue;
        if(!function(interaction, context)) return;
    }
}
