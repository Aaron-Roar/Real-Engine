#ifndef ENTITY_COMPONENTS_H
#define ENTITY_COMPONENTS_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "engine.h"
#include "memory.h"


/** Stable entity id used by public APIs. */
typedef uint32_t Entity; //An id for an entity

/** Index into component tables used by engine internals and compute loops. */
typedef uint32_t EntityIndex; //An index into component tables

/** Result type for functions that return an EntityIndex. */
ERROR_DECLARE_RESULT_TYPE(EntityIndexResult, EntityIndex);

/** Invalid entity id sentinel. */
#define ENTITY_INVALID 0

/** Maximum number of live or reusable entity slots. */
#define MAX_ENTITIES 10000

/** Maximum number of reusable entity groups. */
#define MAX_GROUPS 10000

/** Maximum number of component bits reserved by the engine. */
#define MAX_COMPONENTS 100

/** Maximum entity name length, including the terminating null byte. */
#define ENTITY_NAME_MAX 64
/** Maximum group name length, including the terminating null byte. */
#define GROUP_NAME_MAX 64

/** Result type for functions that return an Entity. */
ERROR_DECLARE_RESULT_TYPE(EntityResult, Entity);

/** Stable group id used to refer to reusable entity groups. */
typedef uint32_t GroupId;

/** Invalid group id sentinel. */
#define GROUP_INVALID 0

/** Result type for functions that return a GroupId. */
ERROR_DECLARE_RESULT_TYPE(GroupIdResult, GroupId);

/** Fixed-size, engine-owned generic group name. */
typedef struct GroupName {
    char value[GROUP_NAME_MAX];
} GroupName;

/** Result type for functions that return a GroupName. */
ERROR_DECLARE_RESULT_TYPE(GroupNameResult, GroupName);

/** Component mask value for an entity. */
typedef uint32_t RohrComponentMask; //The bit mask for an entity's engine components

/** Object pool storing per-index entity component masks. */
MEMORY_DECLARE_OBJECT_POOL(EntityMaskPool, RohrComponentMask);

/** Pool backing the entity_mask table. */
extern EntityMaskPool entity_mask_pool;

/** Direct component mask table indexed by EntityIndex. */
#define entity_mask entity_mask_pool.objects

/** Component bits stored in RohrComponentMask values. */
typedef enum {
    /** No components. */
    NONE                    = 0,
    /** Entity is static in the physics system. */
    STATIC                  = 1 << 0,
    /** Entity is dynamic in the physics system. */
    DYNAMIC                 = 1 << 1,
    /** Entity participates in collision checks. */
    COLLISION               = 1 << 2,
    /** Entity has a hit box shape. */
    HIT_BOX                 = 1 << 3,
    /** Entity has an axis lock constraint. */
    AXIS_LOCK               = 1 << 4,
    /** Entity has an angle lock constraint. */
    ANGLE_LOCK              = 1 << 5,

    /** Entity represents a force source. */
    FORCE                   = 1 << 6,
    /** Entity represents a torque source. */
    TORQUE                  = 1 << 7,
    /** Entity targets another entity. */
    TARGETABLE              = 1 << 8,

    /** Entity has mass. */
    MASS                    = 1 << 9,
    /** Reserved time-window component bit. */
    TIMEWINDOW              = 1 << 10,
    /** Entity has a parent entity. */
    PARENT                      = 1 << 11,
    /** Entity has one or more child entities. */
    CHILD                       = 1 << 12,
    /** Entity has a transform lock constraint. */
    TRANSFORM_LOCK              = 1 << 13,
    /** Entity represents a joint constraint. */
    JOINT                       = 1 << 14,
    /** Entity has a lifetime expiry. */
    LIFETIME                    = 1 << 15,
    /** Entity is treated as a particle by collision/render helpers. */
    PARTICLE                    = 1 << 16,
    /** Entity has an animated sprite. */
    ANIMATED_SPRITE             = 1 << 17,
    /** Entity keeps physics values but skips physics updates. */
    HOLD                        = 1 << 18,
    /** Entity belongs to one or more groups. */
    GROUP                       = 1 << 19,
    /** Entity has a human-readable name used for state-file references. */
    ENTITY_NAME                 = 1 << 20,
} Component;

/** Fixed-size, engine-owned entity name. */
typedef struct EntityName {
    char value[ENTITY_NAME_MAX];
} EntityName;

/** Result type for functions that return an EntityName. */
ERROR_DECLARE_RESULT_TYPE(EntityNameResult, EntityName);

/** Parent entity id alias. */
typedef Entity Parent;

/** Child entity id alias. */
typedef Entity Child;

/** Dynamic pool storing entity ids in a group. */
MEMORY_DECLARE_OBJECT_POOL(GroupEntityPool, Entity);

/** Group ownership kind used to protect specialized group invariants. */
typedef enum GroupKind {
    /** Generic application-owned group. */
    GROUP_KIND_GENERIC,
    /** Parent-owned group containing child entities. */
    GROUP_KIND_CHILDREN,
} GroupKind;

/** Dynamic group of entity ids. */
typedef struct EntityGroup {
    /** Group member entity ids. */
    GroupEntityPool entities;
    /** Ownership kind for mutation rules. */
    GroupKind kind;
} EntityGroup;

/** Result type for functions returning EntityGroup. */
ERROR_DECLARE_RESULT_TYPE(EntityGroupResult, EntityGroup);

/** Dynamic pool storing group ids on one entity. */
MEMORY_DECLARE_OBJECT_POOL(GroupIdPool, GroupId);

/** Dynamic group id list owned by one entity component. */
typedef struct EntityGroupMembership {
    /** Groups this entity belongs to. */
    GroupIdPool groups;
} EntityGroupMembership;

/** Result type for functions returning EntityGroupMembership. */
ERROR_DECLARE_RESULT_TYPE(EntityGroupMembershipResult, EntityGroupMembership);

/** Children component stores the group id containing a parent's children. */
typedef GroupId Children;

/** Result type for functions returning a children group id. */
ERROR_DECLARE_RESULT_TYPE(ChildrenResult, Children);

/** Result type for functions returning Parent. */
ERROR_DECLARE_RESULT_TYPE(ParentResult, Parent);

/** Expiry settings for an entity lifetime. */
typedef struct LifeTime {
    /** Engine time at which the entity expires, or zero when unused. */
    Time expirey_time;
    /** Engine tick at which the entity expires, or zero when unused. */
    Tick expirey_tick;
} LifeTime;

/** Pool storing parent entity ids by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(ParentPool, Parent);
/** Pool storing children group ids by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(ChildrenPool, Children);
/** Pool storing entity group memberships by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(EntityGroupMembershipPool, EntityGroupMembership);
/** Pool storing entity groups by group index. */
MEMORY_DECLARE_OBJECT_POOL(EntityGroupPool, EntityGroup);
/** Pool storing lifetime data by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(LifeTimePool, LifeTime);
/** Pool storing target entity ids by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(TargetPool, Entity);
/** Pool storing fixed-size entity names by EntityIndex. */
MEMORY_DECLARE_OBJECT_POOL(EntityNamePool, EntityName);

/** Parent pool backing the parents table. */
extern ParentPool parents_pool;
/** Children pool backing the children table. */
extern ChildrenPool children_pool;
/** Entity group membership pool backing the entity_group_memberships table. */
extern EntityGroupMembershipPool entity_group_memberships_pool;
/** Global entity group pool. */
extern EntityGroupPool entity_groups_pool;
/** Lifetime pool backing the life_times table. */
extern LifeTimePool life_times_pool;
/** Target pool backing the targets table. */
extern TargetPool targets_pool;
/** Entity name pool backing the entity_names table. */
extern EntityNamePool entity_names_pool;

/** Parent table indexed by EntityIndex. */
#define parents parents_pool.objects
/** Children table indexed by EntityIndex. */
#define children children_pool.objects
/** Group membership table indexed by EntityIndex. */
#define entity_group_memberships entity_group_memberships_pool.objects
/** Entity group table indexed by GroupId - 1. */
#define entity_groups entity_groups_pool.objects
/** Lifetime table indexed by EntityIndex. */
#define life_times life_times_pool.objects
/** Target table indexed by EntityIndex. */
#define targets targets_pool.objects
/** Entity name table indexed by EntityIndex. */
#define entity_names entity_names_pool.objects

/**
 * Assign a unique, non-empty name to an entity.
 *
 * @param entity Entity id to name.
 * @param name Null-terminated name shorter than ENTITY_NAME_MAX.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_name_set(Entity entity, const char *name);

/**
 * Find a live entity by its unique name.
 *
 * @param name Null-terminated entity name.
 * @return EntityResult containing the entity id, or an error.
 */
EntityResult entity_by_name_get(const char *name);

/**
 * Return a copy of an entity's fixed-size name component.
 *
 * @param entity Entity id to inspect.
 * @return EntityNameResult containing the name, or an error.
 */
EntityNameResult entity_name_get(Entity entity);

/**
 * Check whether an entity id currently refers to a live entity.
 *
 * @param entity Entity id to check.
 * @return true when the entity id is valid and live.
 */
bool entity_alive_is(Entity entity);

/**
 * Check whether an entity table index is currently live.
 *
 * @param index Entity table index to check.
 * @return true when the index is live.
 */
bool entity_index_alive_is(EntityIndex index);

/**
 * Return the number of currently alive entities.
 *
 * Systems can use this with entity_alive_at_get() to iterate only live entities
 * instead of scanning every possible entity slot.
 *
 * @return Number of alive entities.
 */
uint32_t entity_alive_count_get(void);

/**
 * Return the entity id stored at a dense alive-list position.
 *
 * The position is not a component table index and can change when entities are
 * deleted. Resolve the returned id with entity_index_get() before accessing
 * component tables.
 *
 * @param position Dense alive-list position.
 * @return EntityResult containing the entity id, or an error.
 */
EntityResult entity_alive_at_get(uint32_t position);

/**
 * Resolve an entity id to its current table index.
 *
 * @param entity Entity id to resolve.
 * @param index Out parameter receiving the current table index.
 * @return true when the entity resolves to a live index.
 */
bool entity_index_get(Entity entity, EntityIndex *index);

/**
 * Resolve a table index to its current entity id.
 *
 * @param index Entity table index to resolve.
 * @return EntityResult containing the entity id, or an error.
 */
EntityResult entity_from_index(EntityIndex index);

/**
 * Add a live entity and grow entity-indexed tables as needed.
 *
 * @return EntityResult containing a stable entity id, or an error.
 */
EntityResult entity_add(void);

/**
 * Delete an entity and make its id/index slots reusable.
 *
 * @param entity Entity id to delete.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_delete(Entity entity);

/**
 * Add one or more component bits to an entity.
 *
 * @param entity Entity id to update.
 * @param mask Component mask bits to add.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_add_components(Entity entity, RohrComponentMask mask);

/**
 * Check whether an entity has all requested components.
 *
 * @param entity Entity id to check.
 * @param components Component mask that must be present.
 * @return true when all requested bits are set.
 */
bool entity_components_has(Entity entity, RohrComponentMask components);

/**
 * Check whether an entity index has all requested components.
 *
 * @param index Entity table index to check.
 * @param components Component mask that must be present.
 * @return true when all requested bits are set.
 */
bool entity_index_components_has(EntityIndex index, RohrComponentMask components);

/**
 * Create a reusable group of entity ids.
 *
 * @return GroupIdResult containing a group id, or an error.
 */
GroupIdResult entity_group_create(void);

/** Assign a unique name to a generic entity group. */
EngineResult entity_group_name_set(GroupId group, const char *name);

/** Find a live generic group by name. */
GroupIdResult entity_group_by_name_get(const char *name);

/** Return a copy of a generic group's fixed-size name. */
GroupNameResult entity_group_name_get(GroupId group);

/**
 * Destroy a generic group and remove it from member entity group lists.
 *
 * Groups owned by parent-child relationships must be changed through the
 * parent/child APIs.
 *
 * @param group Group id to destroy.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_group_destroy(GroupId group);

/**
 * Add an entity to a generic group.
 *
 * Groups owned by parent-child relationships must be changed through the
 * parent/child APIs.
 *
 * @param group Group id to update.
 * @param entity Entity id to add.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_group_add(GroupId group, Entity entity);

/**
 * Remove an entity from a generic group.
 *
 * Groups owned by parent-child relationships must be changed through the
 * parent/child APIs.
 *
 * @param group Group id to update.
 * @param entity Entity id to remove.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_group_remove(GroupId group, Entity entity);

/**
 * Check whether an entity belongs to a group.
 *
 * @param group Group id to inspect.
 * @param entity Entity id to search for.
 * @return true when entity is a member of group.
 */
bool entity_group_entity_has(GroupId group, Entity entity);

/**
 * Get the group component for a group id.
 *
 * @param group Group id to inspect.
 * @return EntityGroupResult containing the group, or an error.
 */
EntityGroupResult entity_group_get(GroupId group);

/**
 * Get the groups assigned to an entity.
 *
 * @param entity Entity id to inspect.
 * @return EntityGroupMembershipResult containing group ids, or an error.
 */
EntityGroupMembershipResult entity_groups_get(Entity entity);

/**
 * Remove component bits from an entity.
 *
 * @param entity Entity id to update.
 * @param mask Component mask bits to remove.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_delete_components(Entity entity, RohrComponentMask mask);

/**
 * Set a parent-child relationship.
 *
 * @param parent Parent entity id.
 * @param child Child entity id.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_child_set(Entity parent, Entity child);

/**
 * Set a parent-child relationship from the child side.
 *
 * @param child Child entity id.
 * @param parent Parent entity id.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_parent_set(Entity child, Entity parent);

/**
 * Remove the parent from a child entity.
 *
 * @param child Child entity id.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_remove_parent(Entity child);

/**
 * Remove a child from a parent entity.
 *
 * @param parent Parent entity id.
 * @param child Child entity id.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_remove_child(Entity parent, Entity child);

/**
 * Get the children group id for an entity.
 *
 * @param entity Entity id to inspect.
 * @return ChildrenResult containing a group id, or an error.
 */
ChildrenResult entity_children_get(Entity entity);

/**
 * Get the parent entity for an entity.
 *
 * @param entity Entity id to inspect.
 * @return ParentResult containing parent id, or an error.
 */
ParentResult entity_parent_get(Entity entity);

/**
 * Set an entity lifetime expiry by time and/or tick.
 *
 * @param entity Entity id to update.
 * @param expirey_time Engine time expiry, or zero when unused.
 * @param expirey_tick Engine tick expiry, or zero when unused.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_life_time_set(Entity entity, Time expirey_time, Tick expirey_tick);

/**
 * Remove lifetime data from an entity.
 *
 * @param entity Entity id to update.
 * @return EngineResult describing success or failure.
 */
EngineResult entity_remove_life_time(Entity entity);
#endif
