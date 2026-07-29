#include "entity_components.h"
#include "engine_internal.h"
#include <stdbool.h>
#include <string.h>

MEMORY_DECLARE_OBJECT_POOL(EntityAlivePool, bool);
MEMORY_DEFINE_OBJECT_POOL(EntityAlivePool, bool)
MEMORY_DEFINE_OBJECT_POOL(EntityMaskPool, CMask)
MEMORY_DEFINE_OBJECT_POOL(TargetPool, Entity)
MEMORY_DEFINE_OBJECT_POOL(ParentPool, Parent)
MEMORY_DEFINE_OBJECT_POOL(GroupEntityPool, Entity)
MEMORY_DEFINE_OBJECT_POOL(GroupIdPool, GroupId)
MEMORY_DEFINE_OBJECT_POOL(EntityGroupMembershipPool, EntityGroupMembership)
MEMORY_DEFINE_OBJECT_POOL(EntityGroupPool, EntityGroup)
MEMORY_DEFINE_OBJECT_POOL(ChildrenPool, Children)
MEMORY_DEFINE_OBJECT_POOL(LifeTimePool, LifeTime)
MEMORY_DEFINE_OBJECT_POOL(EntityNamePool, EntityName)

typedef struct EntityIdPool {
    Entity free_ids[MAX_ENTITIES];
    EntityIndex free_indices[MAX_ENTITIES];
    size_t free_count;
    size_t free_index_count;
    size_t live_count;
    Entity next_id;
    EntityIndex next_index;
} EntityIdPool;

EntityAlivePool entity_alive_pool = {0};
EntityMaskPool entity_mask_pool = {0};
static EntityIdPool entity_id_pool = {0};
static EntityIndex entity_to_index[MAX_ENTITIES + 1] = {0};
static Entity index_to_entity[MAX_ENTITIES] = {0};
static uint16_t entity_generations[MAX_ENTITIES + 1] = {0};
static Entity entity_alive_entities[MAX_ENTITIES] = {0};
static EntityIndex entity_alive_positions[MAX_ENTITIES + 1] = {0};

//Moveable Objects
TargetPool targets_pool = {0};
ParentPool parents_pool = {0};
ChildrenPool children_pool = {0};
EntityGroupMembershipPool entity_group_memberships_pool = {0};
EntityGroupPool entity_groups_pool = {0};
LifeTimePool life_times_pool = {0};
EntityNamePool entity_names_pool = {0};

static GroupId free_group_ids[MAX_GROUPS] = {0};
static size_t free_group_count = 0;
static GroupId next_group_id = 1;

static void entity_group_destroy_storage(EntityGroup *group);
static EngineResult entity_group_ensure_capacity(EntityGroup *group, size_t capacity);
static bool entity_group_storage_contains(const EntityGroup *group, Entity entity);
static EngineResult entity_group_storage_add(EntityGroup *group, Entity entity);
static bool entity_group_storage_remove(EntityGroup *group, Entity entity);
static bool entity_group_storage_last(const EntityGroup *group, Entity *entity);
static GroupIdResult entity_group_create_kind(GroupKind kind);
static EngineResult entity_group_destroy_internal(GroupId group);
static EngineResult entity_group_add_internal(GroupId group, Entity entity);
static EngineResult entity_group_remove_internal(GroupId group, Entity entity);
static void entity_group_membership_destroy(EntityGroupMembership *membership);
static EngineResult entity_group_membership_ensure_capacity(EntityGroupMembership *membership, size_t capacity);
static bool entity_group_membership_contains(const EntityGroupMembership *membership, GroupId group);
static EngineResult entity_group_membership_add(EntityGroupMembership *membership, GroupId group);
static bool entity_group_membership_remove(EntityGroupMembership *membership, GroupId group);

static void entity_id_pool_init(void) {
    memset(entity_id_pool.free_ids, 0, sizeof(entity_id_pool.free_ids));
    memset(entity_id_pool.free_indices, 0, sizeof(entity_id_pool.free_indices));
    memset(entity_to_index, 0, sizeof(entity_to_index));
    memset(index_to_entity, 0, sizeof(index_to_entity));
    memset(entity_generations, 0, sizeof(entity_generations));
    memset(entity_alive_entities, 0, sizeof(entity_alive_entities));
    memset(entity_alive_positions, 0, sizeof(entity_alive_positions));
    memset(free_group_ids, 0, sizeof(free_group_ids));
    entity_id_pool.free_count = 0;
    entity_id_pool.free_index_count = 0;
    entity_id_pool.live_count = 0;
    entity_id_pool.next_id = 1;
    entity_id_pool.next_index = 0;
    free_group_count = 0;
    next_group_id = 1;
}

EngineResult entity_tables_init(void) {
    if(EntityAlivePool_init(&entity_alive_pool, 0).kind == ERROR_RESULT_ERROR) {
        goto fail;
    }
    if(EntityMaskPool_init(&entity_mask_pool, 0).kind == ERROR_RESULT_ERROR) {
        goto fail;
    }
    if(TargetPool_init(&targets_pool, 0).kind == ERROR_RESULT_ERROR) {
        goto fail;
    }
    if(ParentPool_init(&parents_pool, 0).kind == ERROR_RESULT_ERROR) {
        goto fail;
    }
    if(ChildrenPool_init(&children_pool, 0).kind == ERROR_RESULT_ERROR) {
        goto fail;
    }
    if(EntityGroupMembershipPool_init(&entity_group_memberships_pool, 0).kind == ERROR_RESULT_ERROR) {
        goto fail;
    }
    if(EntityGroupPool_init(&entity_groups_pool, 0).kind == ERROR_RESULT_ERROR) {
        goto fail;
    }
    if(LifeTimePool_init(&life_times_pool, 0).kind == ERROR_RESULT_ERROR) {
        goto fail;
    }
    if(EntityNamePool_init(&entity_names_pool, 0).kind == ERROR_RESULT_ERROR) {
        goto fail;
    }
    entity_id_pool_init();
    return error_result_value(true);

fail:
    entity_tables_destroy();
    return error_result_error(ERROR_ENGINE_ENTITY_TABLES_INIT_FAILED);
}

EngineResult entity_tables_ensure_capacity(size_t capacity) {
    size_t new_capacity;

    if(capacity > MAX_ENTITIES) {
        return error_result_error(ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
    }
    if(capacity <= entity_alive_pool.capacity) {
        return error_result_value(true);
    }
    new_capacity = entity_alive_pool.capacity == 0 ? 16 : entity_alive_pool.capacity;
    while(new_capacity < capacity) {
        new_capacity *= 2;
    }
    if(new_capacity > MAX_ENTITIES) {
        new_capacity = MAX_ENTITIES;
    }
    if(new_capacity > entity_alive_pool.capacity && EntityAlivePool_expand(&entity_alive_pool, new_capacity - entity_alive_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > entity_mask_pool.capacity && EntityMaskPool_expand(&entity_mask_pool, new_capacity - entity_mask_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > targets_pool.capacity && TargetPool_expand(&targets_pool, new_capacity - targets_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > parents_pool.capacity && ParentPool_expand(&parents_pool, new_capacity - parents_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > children_pool.capacity && ChildrenPool_expand(&children_pool, new_capacity - children_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > entity_group_memberships_pool.capacity && EntityGroupMembershipPool_expand(&entity_group_memberships_pool, new_capacity - entity_group_memberships_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > life_times_pool.capacity && LifeTimePool_expand(&life_times_pool, new_capacity - life_times_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    if(new_capacity > entity_names_pool.capacity && EntityNamePool_expand(&entity_names_pool, new_capacity - entity_names_pool.capacity).kind == ERROR_RESULT_ERROR) { return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED); }
    return error_result_value(true);
}

void entity_tables_destroy(void) {
    size_t i;

    if(entity_groups_pool.objects != NULL && entity_groups_pool.used != NULL) {
        for(i = 0; i < entity_groups_pool.capacity; i += 1) {
            if(entity_groups_pool.used[i] != 0) {
                entity_group_destroy_storage(&entity_groups_pool.objects[i]);
            }
        }
    }
    if(entity_group_memberships_pool.objects != NULL && entity_group_memberships_pool.used != NULL) {
        for(i = 0; i < entity_group_memberships_pool.capacity; i += 1) {
            if(entity_group_memberships_pool.used[i] != 0) {
                entity_group_membership_destroy(&entity_group_memberships_pool.objects[i]);
            }
        }
    }
    (void)EntityAlivePool_destroy(&entity_alive_pool);
    (void)EntityMaskPool_destroy(&entity_mask_pool);
    (void)TargetPool_destroy(&targets_pool);
    (void)ParentPool_destroy(&parents_pool);
    (void)ChildrenPool_destroy(&children_pool);
    (void)EntityGroupMembershipPool_destroy(&entity_group_memberships_pool);
    (void)EntityGroupPool_destroy(&entity_groups_pool);
    (void)LifeTimePool_destroy(&life_times_pool);
    (void)EntityNamePool_destroy(&entity_names_pool);
    entity_id_pool = (EntityIdPool){0};
    free_group_count = 0;
    next_group_id = 1;
}

static bool entity_id_valid(Entity entity) {
    Entity slot = entity & 0xFFFFu;
    uint16_t generation = (uint16_t)(entity >> 16);

    return slot > ENTITY_INVALID && slot <= MAX_ENTITIES && generation > 0;
}

static bool entity_table_index_valid(EntityIndex index) {
    return index < MAX_ENTITIES;
}

static Entity entity_make(uint32_t slot, uint16_t generation) {
    return ((Entity)generation << 16) | slot;
}

static uint32_t entity_slot(Entity entity) {
    return entity & 0xFFFFu;
}

static uint16_t entity_generation(Entity entity) {
    return (uint16_t)(entity >> 16);
}

static bool entity_group_id_valid(GroupId group) {
    return group > GROUP_INVALID && group <= MAX_GROUPS;
}

static EntityIndex entity_group_index(GroupId group) {
    return group - 1;
}

static bool entity_group_is_alive(GroupId group) {
    EntityIndex index;

    if(!entity_group_id_valid(group)) {
        return false;
    }
    index = entity_group_index(group);
    return index < entity_groups_pool.capacity
        && entity_groups_pool.used != NULL
        && entity_groups_pool.used[index] != 0;
}

static void entity_group_destroy_storage(EntityGroup *group) {
    if(group == NULL) {
        return;
    }
    (void)GroupEntityPool_destroy(&group->entities);
    *group = (EntityGroup){0};
}

static EngineResult entity_group_ensure_capacity(EntityGroup *group, size_t capacity) {
    GroupEntityPoolResult pool_result;
    size_t new_capacity;

    if(group == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    if(capacity <= group->entities.capacity) {
        return error_result_value(true);
    }
    if(group->entities.objects == NULL) {
        pool_result = GroupEntityPool_init(&group->entities, 0);
        if(pool_result.kind == ERROR_RESULT_ERROR) {
            return error_result_error(pool_result.result.error);
        }
    }
    new_capacity = group->entities.capacity == 0 ? 4 : group->entities.capacity;
    while(new_capacity < capacity) {
        if(new_capacity > SIZE_MAX / 2) {
            return error_result_error(ERROR_MEMORY_POOL_CAPACITY_OVERFLOW);
        }
        new_capacity *= 2;
    }
    pool_result = GroupEntityPool_expand(&group->entities, new_capacity - group->entities.capacity);
    if(pool_result.kind == ERROR_RESULT_ERROR) {
        return error_result_error(pool_result.result.error);
    }
    return error_result_value(true);
}

static bool entity_group_storage_contains(const EntityGroup *group, Entity entity) {
    size_t i;

    if(group == NULL || group->entities.objects == NULL || group->entities.used == NULL) {
        return false;
    }
    for(i = 0; i < group->entities.capacity; i += 1) {
        if(group->entities.used[i] != 0 && group->entities.objects[i] == entity) {
            return true;
        }
    }
    return false;
}

static EngineResult entity_group_storage_add(EntityGroup *group, Entity entity) {
    EngineResult result;

    if(group == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    if(entity_group_storage_contains(group, entity)) {
        return error_result_value(true);
    }
    result = entity_group_ensure_capacity(group, group->entities.count + 1);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(GroupEntityPool_store(&group->entities, entity).kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    return error_result_value(true);
}

static bool entity_group_storage_remove(EntityGroup *group, Entity entity) {
    size_t i;

    if(group == NULL || group->entities.objects == NULL || group->entities.used == NULL) {
        return false;
    }
    for(i = 0; i < group->entities.capacity; i += 1) {
        if(group->entities.used[i] != 0 && group->entities.objects[i] == entity) {
            (void)GroupEntityPool_release_at(&group->entities, i);
            return true;
        }
    }
    return false;
}

static bool entity_group_storage_last(const EntityGroup *group, Entity *entity) {
    size_t i;

    if(group == NULL || entity == NULL || group->entities.objects == NULL || group->entities.used == NULL) {
        return false;
    }
    for(i = group->entities.capacity; i > 0; i -= 1) {
        if(group->entities.used[i - 1] != 0) {
            *entity = group->entities.objects[i - 1];
            return true;
        }
    }
    return false;
}

static void entity_group_membership_destroy(EntityGroupMembership *membership) {
    if(membership == NULL) {
        return;
    }
    (void)GroupIdPool_destroy(&membership->groups);
    *membership = (EntityGroupMembership){0};
}

static EngineResult entity_group_membership_ensure_capacity(EntityGroupMembership *membership, size_t capacity) {
    GroupIdPoolResult pool_result;
    size_t new_capacity;

    if(membership == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    if(capacity <= membership->groups.capacity) {
        return error_result_value(true);
    }
    if(membership->groups.objects == NULL) {
        pool_result = GroupIdPool_init(&membership->groups, 0);
        if(pool_result.kind == ERROR_RESULT_ERROR) {
            return error_result_error(pool_result.result.error);
        }
    }
    new_capacity = membership->groups.capacity == 0 ? 4 : membership->groups.capacity;
    while(new_capacity < capacity) {
        if(new_capacity > SIZE_MAX / 2) {
            return error_result_error(ERROR_MEMORY_POOL_CAPACITY_OVERFLOW);
        }
        new_capacity *= 2;
    }
    pool_result = GroupIdPool_expand(&membership->groups, new_capacity - membership->groups.capacity);
    if(pool_result.kind == ERROR_RESULT_ERROR) {
        return error_result_error(pool_result.result.error);
    }
    return error_result_value(true);
}

static bool entity_group_membership_contains(const EntityGroupMembership *membership, GroupId group) {
    size_t i;

    if(membership == NULL || membership->groups.objects == NULL || membership->groups.used == NULL) {
        return false;
    }
    for(i = 0; i < membership->groups.capacity; i += 1) {
        if(membership->groups.used[i] != 0 && membership->groups.objects[i] == group) {
            return true;
        }
    }
    return false;
}

static EngineResult entity_group_membership_add(EntityGroupMembership *membership, GroupId group) {
    EngineResult result;

    if(membership == NULL) {
        return error_result_error(ERROR_MEMORY_POOL_NULL_POINTER);
    }
    if(entity_group_membership_contains(membership, group)) {
        return error_result_value(true);
    }
    result = entity_group_membership_ensure_capacity(membership, membership->groups.count + 1);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(GroupIdPool_store(&membership->groups, group).kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    return error_result_value(true);
}

static bool entity_group_membership_remove(EntityGroupMembership *membership, GroupId group) {
    size_t i;

    if(membership == NULL || membership->groups.objects == NULL || membership->groups.used == NULL) {
        return false;
    }
    for(i = 0; i < membership->groups.capacity; i += 1) {
        if(membership->groups.used[i] != 0 && membership->groups.objects[i] == group) {
            (void)GroupIdPool_release_at(&membership->groups, i);
            return true;
        }
    }
    return false;
}

bool entity_is_alive(Entity entity) {
    EntityIndex index;

    if(!entity_get_index(entity, &index)) {
        return false;
    }
    return entity_index_is_alive(index);
}

bool entity_index_is_alive(EntityIndex index) {
    if(!entity_table_index_valid(index)) {
        return false;
    }
    if(index >= entity_alive_pool.capacity) {
        return false;
    }
    if(entity_alive_pool.objects == NULL || entity_alive_pool.used == NULL) {
        return false;
    }
    if(entity_alive_pool.used[index] == 0) {
        return false;
    }
    return entity_alive_pool.objects[index];
}

uint32_t entity_alive_count(void) {
    return (uint32_t)entity_id_pool.live_count;
}

EntityResult entity_alive_at(uint32_t position) {
    if(position >= entity_id_pool.live_count) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    return ERROR_RESULT_MAKE_VALUE(EntityResult, entity_alive_entities[position]);
}

bool entity_get_index(Entity entity, EntityIndex *index) {
    uint32_t slot;

    if(index == NULL || !entity_id_valid(entity)) {
        return false;
    }
    slot = entity_slot(entity);
    if(entity_generations[slot] != entity_generation(entity)) {
        return false;
    }
    if(entity_to_index[slot] >= MAX_ENTITIES) {
        return false;
    }
    if(index_to_entity[entity_to_index[slot]] != entity) {
        return false;
    }
    *index = entity_to_index[slot];
    return true;
}

EntityResult entity_from_index(EntityIndex index) {
    if(!entity_table_index_valid(index)) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_INVALID_ENTITY);
    }
    if(!entity_index_is_alive(index)) {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    return ERROR_RESULT_MAKE_VALUE(EntityResult, index_to_entity[index]);
}

EntityResult entity_add(void) {
    Entity entity;
    uint32_t slot;
    EntityIndex index;
    size_t required_capacity;
    EngineResult result;
    bool reused_slot = false;
    bool reused_index = false;

    if(entity_id_pool.free_count > 0) {
        entity_id_pool.free_count -= 1;
        slot = entity_id_pool.free_ids[entity_id_pool.free_count];
        reused_slot = true;
    } else {
        if(entity_id_pool.next_id > MAX_ENTITIES) {
            return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
        }
        slot = entity_id_pool.next_id;
        entity_id_pool.next_id += 1;
    }

    if(entity_id_pool.free_index_count > 0) {
        entity_id_pool.free_index_count -= 1;
        index = entity_id_pool.free_indices[entity_id_pool.free_index_count];
        reused_index = true;
    } else {
        if(entity_id_pool.next_index >= MAX_ENTITIES) {
            return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
        }
        index = entity_id_pool.next_index;
        entity_id_pool.next_index += 1;
    }

    required_capacity = (size_t)index + 1;
    result = engine_tables_ensure_capacity(required_capacity);
    if(result.kind == ERROR_RESULT_ERROR) {
        if(reused_slot && entity_id_pool.free_count < MAX_ENTITIES) {
            entity_id_pool.free_ids[entity_id_pool.free_count] = slot;
            entity_id_pool.free_count += 1;
        }
        else if(!reused_slot && entity_id_pool.next_id > 1) {
            entity_id_pool.next_id -= 1;
        }
        if(reused_index && entity_id_pool.free_index_count < MAX_ENTITIES) {
            entity_id_pool.free_indices[entity_id_pool.free_index_count] = index;
            entity_id_pool.free_index_count += 1;
        }
        else if(!reused_index && entity_id_pool.next_index > 0) {
            entity_id_pool.next_index -= 1;
        }
        return ERROR_RESULT_MAKE_ERROR(EntityResult, result.result.error);
    }

    if(entity_generations[slot] == 0) {
        entity_generations[slot] = 1;
    }
    entity = entity_make(slot, entity_generations[slot]);
    entity_id_pool.live_count += 1;
    entity_to_index[slot] = index;
    index_to_entity[index] = entity;
    entity_alive_positions[slot] = (EntityIndex)(entity_id_pool.live_count - 1);
    entity_alive_entities[entity_alive_positions[slot]] = entity;

    (void)EntityAlivePool_store_at(&entity_alive_pool, index, true);
    (void)EntityMaskPool_store_at(&entity_mask_pool, index, 0);
    return ERROR_RESULT_MAKE_VALUE(EntityResult, entity);
}

static void entity_clear_index(EntityIndex index) {
    if(!entity_table_index_valid(index)) {
        return;
    }
    if(index < entity_alive_pool.capacity && entity_alive_pool.used[index]) {
        (void)EntityAlivePool_release_at(&entity_alive_pool, index);
    }
    if(index < entity_mask_pool.capacity && entity_mask_pool.used[index]) {
        (void)EntityMaskPool_release_at(&entity_mask_pool, index);
    }
    if(index < targets_pool.capacity && targets_pool.used[index]) {
        (void)TargetPool_release_at(&targets_pool, index);
    }
    if(index < parents_pool.capacity && parents_pool.used[index]) {
        (void)ParentPool_release_at(&parents_pool, index);
    }
    if(index < children_pool.capacity && children_pool.used[index]) {
        if(entity_group_is_alive(children_pool.objects[index])) {
            (void)entity_group_destroy_internal(children_pool.objects[index]);
        }
        (void)ChildrenPool_release_at(&children_pool, index);
    }
    if(index < entity_group_memberships_pool.capacity && entity_group_memberships_pool.used[index]) {
        entity_group_membership_destroy(&entity_group_memberships_pool.objects[index]);
        (void)EntityGroupMembershipPool_release_at(&entity_group_memberships_pool, index);
    }
    if(index < life_times_pool.capacity && life_times_pool.used[index]) {
        (void)LifeTimePool_release_at(&life_times_pool, index);
    }
    if(index < entity_names_pool.capacity && entity_names_pool.used[index]) {
        (void)EntityNamePool_release_at(&entity_names_pool, index);
    }
}

EntityResult entity_find_by_name(const char *name) {
    uint32_t position;

    if(name == NULL || name[0] == '\0') {
        return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_INVALID_ENTITY_NAME);
    }
    for(position = 0; position < entity_alive_count(); position += 1) {
        EntityResult entity_result = entity_alive_at(position);
        EntityIndex index;

        if(entity_result.kind == ERROR_RESULT_ERROR
                || !entity_get_index(entity_result.result.value, &index)) {
            continue;
        }
        if(index < entity_names_pool.capacity
                && entity_names_pool.used[index] != 0
                && (entity_mask[index] & ENTITY_NAME) != 0
                && strcmp(entity_names[index].value, name) == 0) {
            return entity_result;
        }
    }
    return ERROR_RESULT_MAKE_ERROR(EntityResult, ERROR_ENGINE_ENTITY_NOT_FOUND);
}

EngineResult entity_set_name(Entity entity, const char *name) {
    EntityIndex index;
    EntityResult existing;
    size_t length;
    EntityName entity_name = {0};

    if(!entity_get_index(entity, &index)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(name == NULL || name[0] == '\0') {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY_NAME);
    }
    length = strlen(name);
    if(length >= ENTITY_NAME_MAX) {
        return error_result_error(ERROR_ENGINE_ENTITY_NAME_TOO_LONG);
    }
    existing = entity_find_by_name(name);
    if(existing.kind == ERROR_RESULT_VALUE && existing.result.value != entity) {
        return error_result_error(ERROR_ENGINE_DUPLICATE_ENTITY_NAME);
    }
    memcpy(entity_name.value, name, length + 1);
    if(EntityNamePool_store_at(&entity_names_pool, index, entity_name).kind == ERROR_RESULT_ERROR) {
        return error_result_error(ERROR_MEMORY_POOL_ALLOCATION_FAILED);
    }
    entity_mask[index] |= ENTITY_NAME;
    return error_result_value(true);
}

EntityNameResult entity_get_name(Entity entity) {
    EntityIndex index;

    if(!entity_get_index(entity, &index)
            || index >= entity_names_pool.capacity
            || entity_names_pool.used[index] == 0
            || (entity_mask[index] & ENTITY_NAME) == 0) {
        return ERROR_RESULT_MAKE_ERROR(EntityNameResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(EntityNameResult, entity_names[index]);
}

static EngineResult entity_detach_group_memberships(Entity entity, EntityIndex index) {
    EngineResult result;

    while(
        index < entity_group_memberships_pool.capacity
        && entity_group_memberships_pool.used[index] != 0
        && entity_group_memberships[index].groups.count > 0
    ) {
        GroupId group = GROUP_INVALID;
        size_t i;

        for(i = entity_group_memberships[index].groups.capacity; i > 0; i -= 1) {
            if(entity_group_memberships[index].groups.used[i - 1] != 0) {
                group = entity_group_memberships[index].groups.objects[i - 1];
                break;
            }
        }
        if(group == GROUP_INVALID) {
            break;
        }
        result = entity_group_remove_internal(group, entity);
        if(result.kind == ERROR_RESULT_ERROR) {
            return result;
        }
    }
    return error_result_value(true);
}

static EngineResult entity_detach_relationships(Entity entity, EntityIndex index) {
    EngineResult result;

    if(index < parents_pool.capacity && parents_pool.used[index] != 0) {
        result = entity_remove_parent(entity);
        if(result.kind == ERROR_RESULT_ERROR) {
            return result;
        }
    }
    while(index < children_pool.capacity && children_pool.used[index] != 0 && entity_group_is_alive(children[index])) {
        Entity child;
        EntityGroup *group = &entity_groups[entity_group_index(children[index])];

        if(!entity_group_storage_last(group, &child)) {
            break;
        }

        result = entity_remove_child(entity, child);
        if(result.kind == ERROR_RESULT_ERROR) {
            return result;
        }
    }
    return error_result_value(true);
}

EngineResult entity_delete(Entity entity) {
    EntityIndex index;
    EntityIndex removed_alive_position;
    EntityIndex last_alive_position;
    Entity moved_entity;
    uint32_t moved_slot;
    uint32_t slot;
    EngineResult result;

    if(!entity_id_valid(entity)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(!entity_get_index(entity, &index) || !entity_index_is_alive(index)) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    result = entity_detach_relationships(entity, index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_detach_group_memberships(entity, index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    entity_clear_index(index);
    slot = entity_slot(entity);
    removed_alive_position = entity_alive_positions[slot];
    last_alive_position = (EntityIndex)(entity_id_pool.live_count - 1);
    moved_entity = entity_alive_entities[last_alive_position];
    moved_slot = entity_slot(moved_entity);

    entity_alive_entities[removed_alive_position] = moved_entity;
    entity_alive_positions[moved_slot] = removed_alive_position;
    entity_alive_entities[last_alive_position] = ENTITY_INVALID;
    entity_alive_positions[slot] = 0;
    entity_to_index[slot] = 0;
    index_to_entity[index] = ENTITY_INVALID;
    entity_generations[slot] += 1;
    if(entity_generations[slot] == 0) {
        entity_generations[slot] = 1;
    }

    if(entity_id_pool.free_count < MAX_ENTITIES) {
        entity_id_pool.free_ids[entity_id_pool.free_count] = slot;
        entity_id_pool.free_count += 1;
    }
    if(entity_id_pool.free_index_count < MAX_ENTITIES) {
        entity_id_pool.free_indices[entity_id_pool.free_index_count] = index;
        entity_id_pool.free_index_count += 1;
    }
    if(entity_id_pool.live_count > 0) {
        entity_id_pool.live_count -= 1;
    }
    return error_result_value(true);
}

EngineResult entity_add_components(Entity entity, CMask mask) {
    EntityIndex index;

    if(!entity_id_valid(entity)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(!entity_get_index(entity, &index) || !entity_index_is_alive(index)) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    (void)EntityMaskPool_store_at(&entity_mask_pool, index, entity_mask[index] | mask);
    return error_result_value(true);
}

EngineResult entity_delete_components(Entity entity, CMask mask) {
    EntityIndex index;

    if(!entity_id_valid(entity)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(!entity_get_index(entity, &index) || !entity_index_is_alive(index)) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }
    (void)EntityMaskPool_store_at(&entity_mask_pool, index, entity_mask[index] & ~mask);
    return error_result_value(true);
}

bool entity_has_components(Entity entity, CMask components) {
  EntityIndex index;

  if(!entity_get_index(entity, &index)) {
    return false;
  }
  if(!entity_index_is_alive(index)) {
    return false;
  }
  return entity_index_has_components(index, components);
}

bool entity_index_has_components(EntityIndex index, CMask components) {
  if(!entity_index_is_alive(index)) {
    return false;
  }
  if( (entity_mask[index] & components) == components) {
    return true;
  }
  return false;
}

static GroupIdResult entity_group_create_kind(GroupKind kind) {
    GroupId group;
    EntityIndex index;
    EngineResult result;
    bool reused_group = false;

    if(free_group_count > 0) {
        free_group_count -= 1;
        group = free_group_ids[free_group_count];
        reused_group = true;
    } else {
        if(next_group_id > MAX_GROUPS) {
            return ERROR_RESULT_MAKE_ERROR(GroupIdResult, ERROR_ENGINE_MAX_ENTITIES_EXCEEDED);
        }
        group = next_group_id;
        next_group_id += 1;
    }

    index = entity_group_index(group);
    if(index >= entity_groups_pool.capacity) {
        size_t new_capacity = entity_groups_pool.capacity == 0 ? 16 : entity_groups_pool.capacity;

        while(new_capacity <= index) {
            new_capacity *= 2;
        }
        if(new_capacity > MAX_GROUPS) {
            new_capacity = MAX_GROUPS;
        }
        if(EntityGroupPool_expand(&entity_groups_pool, new_capacity - entity_groups_pool.capacity).kind == ERROR_RESULT_ERROR) {
            if(reused_group && free_group_count < MAX_GROUPS) {
                free_group_ids[free_group_count] = group;
                free_group_count += 1;
            } else if(!reused_group && next_group_id > 1) {
                next_group_id -= 1;
            }
            return ERROR_RESULT_MAKE_ERROR(GroupIdResult, ERROR_ENGINE_TABLE_EXPANSION_FAILED);
        }
    }

    result = error_result_value(true);
    if(EntityGroupPool_store_at(&entity_groups_pool, index, (EntityGroup){.kind = kind}).kind == ERROR_RESULT_ERROR) {
        result = error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED);
    }
    if(result.kind == ERROR_RESULT_ERROR) {
        if(reused_group && free_group_count < MAX_GROUPS) {
            free_group_ids[free_group_count] = group;
            free_group_count += 1;
        } else if(!reused_group && next_group_id > 1) {
            next_group_id -= 1;
        }
        return ERROR_RESULT_MAKE_ERROR(GroupIdResult, result.result.error);
    }
    return ERROR_RESULT_MAKE_VALUE(GroupIdResult, group);
}

GroupIdResult entity_group_create(void) {
    return entity_group_create_kind(GROUP_KIND_GENERIC);
}

static EngineResult entity_group_destroy_internal(GroupId group) {
    EntityIndex group_index;
    EntityGroup *group_storage;
    Entity entity;
    EngineResult result;

    if(!entity_group_is_alive(group)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    group_index = entity_group_index(group);
    group_storage = &entity_groups[group_index];
    while(group_storage->entities.count > 0) {
        if(!entity_group_storage_last(group_storage, &entity)) {
            break;
        }
        result = entity_group_remove_internal(group, entity);
        if(result.kind == ERROR_RESULT_ERROR) {
            return result;
        }
    }
    entity_group_destroy_storage(group_storage);
    (void)EntityGroupPool_release_at(&entity_groups_pool, group_index);
    if(free_group_count < MAX_GROUPS) {
        free_group_ids[free_group_count] = group;
        free_group_count += 1;
    }
    return error_result_value(true);
}

EngineResult entity_group_destroy(GroupId group) {
    if(!entity_group_is_alive(group)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(entity_groups[entity_group_index(group)].kind != GROUP_KIND_GENERIC) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    return entity_group_destroy_internal(group);
}

static EngineResult entity_group_add_internal(GroupId group, Entity entity) {
    EntityIndex group_index;
    EntityIndex entity_index;
    EngineResult result;

    if(!entity_group_is_alive(group)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(!entity_get_index(entity, &entity_index) || !entity_index_is_alive(entity_index)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    group_index = entity_group_index(group);
    result = entity_group_storage_add(&entity_groups[group_index], entity);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(entity_group_memberships_pool.used[entity_index] == 0) {
        if(EntityGroupMembershipPool_store_at(
            &entity_group_memberships_pool,
            entity_index,
            (EntityGroupMembership){0}
        ).kind == ERROR_RESULT_ERROR) {
            (void)entity_group_storage_remove(&entity_groups[group_index], entity);
            return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED);
        }
    }
    result = entity_group_membership_add(&entity_group_memberships[entity_index], group);
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)entity_group_storage_remove(&entity_groups[group_index], entity);
        return result;
    }
    return entity_add_components(entity, GROUP);
}

EngineResult entity_group_add(GroupId group, Entity entity) {
    if(!entity_group_is_alive(group)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(entity_groups[entity_group_index(group)].kind != GROUP_KIND_GENERIC) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    return entity_group_add_internal(group, entity);
}

static EngineResult entity_group_remove_internal(GroupId group, Entity entity) {
    EntityIndex group_index;
    EntityIndex entity_index;
    EngineResult result;

    if(!entity_group_is_alive(group)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    group_index = entity_group_index(group);
    (void)entity_group_storage_remove(&entity_groups[group_index], entity);
    if(!entity_get_index(entity, &entity_index) || !entity_index_is_alive(entity_index)) {
        return error_result_value(true);
    }
    if(entity_index < entity_group_memberships_pool.capacity && entity_group_memberships_pool.used[entity_index] != 0) {
        (void)entity_group_membership_remove(&entity_group_memberships[entity_index], group);
        if(entity_group_memberships[entity_index].groups.count == 0) {
            entity_group_membership_destroy(&entity_group_memberships[entity_index]);
            (void)EntityGroupMembershipPool_release_at(&entity_group_memberships_pool, entity_index);
            result = entity_delete_components(entity, GROUP);
            if(result.kind == ERROR_RESULT_ERROR) {
                return result;
            }
        }
    }
    return error_result_value(true);
}

EngineResult entity_group_remove(GroupId group, Entity entity) {
    if(!entity_group_is_alive(group)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(entity_groups[entity_group_index(group)].kind != GROUP_KIND_GENERIC) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    return entity_group_remove_internal(group, entity);
}

bool entity_group_contains(GroupId group, Entity entity) {
    if(!entity_group_is_alive(group)) {
        return false;
    }
    return entity_group_storage_contains(&entity_groups[entity_group_index(group)], entity);
}

EntityGroupResult entity_group_get(GroupId group) {
    if(!entity_group_is_alive(group)) {
        return ERROR_RESULT_MAKE_ERROR(EntityGroupResult, ERROR_ENGINE_INVALID_ENTITY);
    }
    return ERROR_RESULT_MAKE_VALUE(EntityGroupResult, entity_groups[entity_group_index(group)]);
}

EntityGroupMembershipResult entity_get_groups(Entity entity) {
    EntityIndex index;

    if(!entity_get_index(entity, &index) || !entity_index_is_alive(index)) {
        return ERROR_RESULT_MAKE_ERROR(EntityGroupMembershipResult, ERROR_ENGINE_INVALID_ENTITY);
    }
    if(index >= entity_group_memberships_pool.capacity || entity_group_memberships_pool.used[index] == 0) {
        return ERROR_RESULT_MAKE_ERROR(EntityGroupMembershipResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(EntityGroupMembershipResult, entity_group_memberships[index]);
}

static EngineResult entity_ensure_children_component(Entity parent, EntityIndex parent_index) {
    GroupIdResult group_result;
    EngineResult result;

    if(parent_index >= children_pool.capacity) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(children_pool.used[parent_index] == 0) {
        group_result = entity_group_create_kind(GROUP_KIND_CHILDREN);
        if(group_result.kind == ERROR_RESULT_ERROR) {
            return error_result_error(group_result.result.error);
        }
        if(ChildrenPool_store_at(&children_pool, parent_index, group_result.result.value).kind == ERROR_RESULT_ERROR) {
            (void)entity_group_destroy_internal(group_result.result.value);
            return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED);
        }
    }
    result = entity_add_components(parent, CHILD);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    return error_result_value(true);
}

EngineResult entity_set_child(Entity parent, Entity child) {
    return entity_set_parent(child, parent);
}

EngineResult entity_set_parent(Entity child, Entity parent) {
    EntityIndex parent_index;
    EntityIndex child_index;
    EngineResult result;

    if(parent == child) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(!entity_get_index(parent, &parent_index) || !entity_get_index(child, &child_index)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(!entity_index_is_alive(parent_index) || !entity_index_is_alive(child_index)) {
        return error_result_error(ERROR_ENGINE_ENTITY_NOT_FOUND);
    }

    if(child_index < parents_pool.capacity && parents_pool.used[child_index] != 0) {
        if(parents[child_index] == parent) {
            result = entity_ensure_children_component(parent, parent_index);
            if(result.kind == ERROR_RESULT_ERROR) {
                return result;
            }
            result = entity_group_add_internal(children[parent_index], child);
            if(result.kind == ERROR_RESULT_ERROR) {
                return result;
            }
            return entity_add_components(child, PARENT);
        }
        result = entity_remove_parent(child);
        if(result.kind == ERROR_RESULT_ERROR) {
            return result;
        }
    }

    result = entity_ensure_children_component(parent, parent_index);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    result = entity_group_add_internal(children[parent_index], child);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(ParentPool_store_at(&parents_pool, child_index, parent).kind == ERROR_RESULT_ERROR) {
        (void)entity_group_remove_internal(children[parent_index], child);
        return error_result_error(ERROR_ENGINE_TABLE_EXPANSION_FAILED);
    }
    result = entity_add_components(child, PARENT);
    if(result.kind == ERROR_RESULT_ERROR) {
        (void)entity_group_remove_internal(children[parent_index], child);
        (void)ParentPool_release_at(&parents_pool, child_index);
        return result;
    }
    return error_result_value(true);
}

EngineResult entity_remove_parent(Entity child) {
    EntityIndex child_index;
    Entity parent;
    EntityIndex parent_index;
    EngineResult result;

    if(!entity_get_index(child, &child_index) || !entity_index_is_alive(child_index)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(child_index >= parents_pool.capacity || parents_pool.used[child_index] == 0) {
        return entity_delete_components(child, PARENT);
    }
    parent = parents[child_index];
    if(!entity_get_index(parent, &parent_index) || !entity_index_is_alive(parent_index)) {
        (void)ParentPool_release_at(&parents_pool, child_index);
        return entity_delete_components(child, PARENT);
    }

    if(parent_index < children_pool.capacity && children_pool.used[parent_index] != 0) {
        GroupId group = children[parent_index];

        (void)entity_group_remove_internal(group, child);
        if(entity_group_is_alive(group) && entity_groups[entity_group_index(group)].entities.count == 0) {
            (void)entity_group_destroy_internal(group);
            (void)ChildrenPool_release_at(&children_pool, parent_index);
            result = entity_delete_components(parent, CHILD);
            if(result.kind == ERROR_RESULT_ERROR) {
                return result;
            }
        }
    }

    //Remove the parent from the child
    if(child_index < parents_pool.capacity && parents_pool.used[child_index]) {
        (void)ParentPool_release_at(&parents_pool, child_index);
    }
    result = entity_delete_components(child, PARENT);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    return error_result_value(true);
}

EngineResult entity_remove_child(Entity parent, Entity child) {
    EntityIndex parent_index;
    EntityIndex child_index;
    EngineResult result;

    if(!entity_get_index(parent, &parent_index) || !entity_index_is_alive(parent_index)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    if(!entity_get_index(child, &child_index) || !entity_index_is_alive(child_index)) {
        //Warning this child is currently not alive
        return error_result_value(true);
    }
    if(parent_index < children_pool.capacity && children_pool.used[parent_index] != 0) {
        GroupId group = children[parent_index];

        (void)entity_group_remove_internal(group, child);
        if(entity_group_is_alive(group) && entity_groups[entity_group_index(group)].entities.count == 0) {
            (void)entity_group_destroy_internal(group);
            (void)ChildrenPool_release_at(&children_pool, parent_index);
            result = entity_delete_components(parent, CHILD);
            if(result.kind == ERROR_RESULT_ERROR) {
                return result;
            }
        }
    }

    //Removing the parent from the child
    if(child_index < parents_pool.capacity && parents_pool.used[child_index] && parents[child_index] == parent) {
        (void)ParentPool_release_at(&parents_pool, child_index);
        result = entity_delete_components(child, PARENT);
        if(result.kind == ERROR_RESULT_ERROR) {
            return result;
        }
    }

    return error_result_value(true);
}

ChildrenResult entity_get_children(Entity entity) {
    EntityIndex index;

    if(!entity_get_index(entity, &index) || !entity_index_is_alive(index)) {
        return ERROR_RESULT_MAKE_ERROR(ChildrenResult, ERROR_ENGINE_INVALID_ENTITY);
    }
    if(index >= children_pool.capacity || children_pool.used[index] == 0) {
        return ERROR_RESULT_MAKE_ERROR(ChildrenResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(ChildrenResult, children[index]);
}
ParentResult entity_get_parent(Entity entity) {
    EntityIndex index;

    if(!entity_get_index(entity, &index) || !entity_index_is_alive(index)) {
        return ERROR_RESULT_MAKE_ERROR(ParentResult, ERROR_ENGINE_INVALID_ENTITY);
    }
    if(index >= parents_pool.capacity || parents_pool.used[index] == 0) {
        return ERROR_RESULT_MAKE_ERROR(ParentResult, ERROR_ENGINE_COMPONENT_MISSING);
    }
    return ERROR_RESULT_MAKE_VALUE(ParentResult, parents[index]);
}





EngineResult entity_set_life_time(Entity entity, Time expirey_time, Tick expirey_tick) {
    EntityIndex index;
    EngineResult result;

    if(!entity_get_index(entity, &index) || !entity_index_is_alive(index)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }

    result = entity_add_components(entity, LIFETIME);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    (void)LifeTimePool_store_at(&life_times_pool, index, (LifeTime){
        .expirey_time = expirey_time,
        .expirey_tick = expirey_tick
    });
    return error_result_value(true);
}

EngineResult entity_remove_life_time(Entity entity) {
    EntityIndex index;
    EngineResult result;

    if(!entity_get_index(entity, &index)) {
        return error_result_error(ERROR_ENGINE_INVALID_ENTITY);
    }
    result = entity_delete_components(entity, LIFETIME);
    if(result.kind == ERROR_RESULT_ERROR) {
        return result;
    }
    if(index < life_times_pool.capacity && life_times_pool.used[index]) {
        (void)LifeTimePool_release_at(&life_times_pool, index);
    }
    return error_result_value(true);
}
