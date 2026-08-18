#ifndef ROHR_EDITOR_PROJECT_H
#define ROHR_EDITOR_PROJECT_H

#include "rohr.h"
#include "editor_error.h"

#define EDITOR_OBJECT_MAX 64
#define EDITOR_HITBOX_VERTEX_MIN 3
#define EDITOR_HITBOX_VERTEX_MAX 8
#define EDITOR_OBJECT_NAME_MAX 64
#define EDITOR_RIGID_BODY_MAX 16
#define EDITOR_BODY_HITBOX_MAX 8
#define EDITOR_JOINT_MAX 32
#define EDITOR_ANCHOR_MAX 64
#define EDITOR_SOFT_BODY_MAX 8
#define EDITOR_SOFT_NODE_MAX 64
#define EDITOR_SOFT_BEAM_MAX 128
#define EDITOR_SOFT_AREA_MAX 128
#define EDITOR_SOFT_AREA_NODE_MAX EDITOR_SOFT_NODE_MAX
#define EDITOR_OBJECT_HIERARCHY_MAX \
    (EDITOR_RIGID_BODY_MAX + EDITOR_JOINT_MAX + EDITOR_SOFT_BODY_MAX)
#define EDITOR_COLLISION_MASK_MAX 64
/* Pre-release project schemas remain version 1 until the editor format is stable. */
#define EDITOR_PROJECT_FORMAT_VERSION 1

typedef uint32_t EditorObjectId;
typedef uint32_t EditorVertexId;
typedef uint32_t EditorRigidBodyId;
typedef uint32_t EditorHitboxId;
typedef uint32_t EditorJointId;
typedef uint32_t EditorAnchorId;
typedef uint32_t EditorSoftBodyId;
typedef uint32_t EditorSoftNodeId;
typedef uint32_t EditorSoftBeamId;
typedef uint32_t EditorSoftAreaId;

typedef enum EditorHierarchyItemKind {
    EDITOR_HIERARCHY_RIGID_BODY,
    EDITOR_HIERARCHY_JOINT,
    EDITOR_HIERARCHY_SOFT_BODY
} EditorHierarchyItemKind;

typedef struct EditorHierarchyItem {
    EditorHierarchyItemKind kind;
    uint32_t id;
} EditorHierarchyItem;

#define EDITOR_OBJECT_INVALID 0

typedef struct EditorVertex {
    EditorVertexId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    Position position;
    bool position_locked;
} EditorVertex;

typedef struct EditorHitbox {
    EditorHitboxId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    bool visible;
    EditorVertex *vertices;
    char (*line_names)[EDITOR_OBJECT_NAME_MAX];
    uint32_t vertex_count;
    size_t vertex_capacity;
} EditorHitbox;

typedef struct EditorRigidBody {
    EditorRigidBodyId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    Position position;
    float rotation;
    float mass_value;
    float friction;
    float restitution;
    bool static_body;
    bool rotation_locked;
    bool gravity_enabled;
    bool collision_enabled;
    bool particle;
    bool particle_auto_fit;
    bool visible;
    float particle_radius;
    uint32_t particle_ring_color;
    uint32_t particle_fill_color;
    uint32_t border_color;
    uint32_t surface_color;
    RohrCollisionCategoryMask collision_category;
    RohrCollisionCategoryMask collision_with;
    EditorHitbox *hitboxes;
    size_t hitbox_count;
    size_t hitbox_capacity;
} EditorRigidBody;

typedef struct EditorCollisionMask {
    char name[EDITOR_OBJECT_NAME_MAX];
} EditorCollisionMask;

typedef enum EditorJointKind {
    EDITOR_JOINT_REVOLUTE,
    EDITOR_JOINT_WELD,
    EDITOR_JOINT_SPRING
} EditorJointKind;

typedef struct EditorAnchor {
    EditorAnchorId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    Position position;
    float rotation;
    EditorRigidBodyId rigid_body;
    bool position_follows_body;
    bool rotation_follows_body;
    bool visible;
} EditorAnchor;

typedef struct EditorJoint {
    EditorJointId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    EditorJointKind kind;
    EditorAnchorId anchor_a;
    EditorAnchorId anchor_b;
    float rest_length;
    float stiffness;
    float damping;
    float rest_angle;
    float visual_size;
    bool visible;
} EditorJoint;

EditorRigidBody editor_project_rigid_body_default_get(void);
EditorJoint editor_project_joint_default_get(EditorJointKind kind);

typedef struct EditorSoftNode {
    EditorSoftNodeId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    Position position;
    float node_mass;
    float radius;
    float friction;
    float restitution;
    bool gravity_enabled;
    bool collision_enabled;
    RohrCollisionCategoryMask collision_category;
    RohrCollisionCategoryMask collision_with;
    bool visible;
    uint32_t color;
    bool color_overridden;
} EditorSoftNode;

typedef struct EditorSoftBeam {
    EditorSoftBeamId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    EditorSoftNodeId node_a;
    EditorSoftNodeId node_b;
    float stiffness;
    float damping;
    bool visible;
    uint32_t color;
    bool color_overridden;
} EditorSoftBeam;

typedef struct EditorSoftArea {
    EditorSoftAreaId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    EditorSoftNodeId *nodes;
    size_t node_count;
    size_t node_capacity;
    uint32_t color;
    bool color_overridden;
    bool visible;
} EditorSoftArea;

#define EDITOR_SOFT_BODY_HIERARCHY_MAX \
    (EDITOR_SOFT_NODE_MAX + EDITOR_SOFT_BEAM_MAX + EDITOR_SOFT_AREA_MAX)

typedef enum EditorSoftHierarchyItemKind {
    EDITOR_SOFT_HIERARCHY_NODE,
    EDITOR_SOFT_HIERARCHY_BEAM,
    EDITOR_SOFT_HIERARCHY_AREA
} EditorSoftHierarchyItemKind;

typedef struct EditorSoftHierarchyItem {
    EditorSoftHierarchyItemKind kind;
    uint32_t id;
} EditorSoftHierarchyItem;

typedef struct EditorSoftBody {
    EditorSoftBodyId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    Position position;
    float rotation;
    bool visible;
    uint32_t node_color;
    uint32_t beam_color;
    uint32_t area_color;
    EditorSoftNode *nodes;
    size_t node_count;
    size_t node_capacity;
    EditorSoftBeam *beams;
    size_t beam_count;
    size_t beam_capacity;
    EditorSoftArea *areas;
    size_t area_count;
    size_t area_capacity;
    EditorSoftHierarchyItem *hierarchy;
    size_t hierarchy_count;
    size_t hierarchy_capacity;
} EditorSoftBody;

typedef struct EditorObject {
    EditorObjectId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    Position position;
    bool visible;
    EditorRigidBody *rigid_bodies;
    size_t rigid_body_count;
    size_t rigid_body_capacity;
    EditorJoint *joint_items;
    size_t joint_count;
    size_t joint_capacity;
    EditorAnchor *anchors;
    size_t anchor_count;
    size_t anchor_capacity;
    EditorSoftBody *soft_body_items;
    size_t soft_body_count;
    size_t soft_body_capacity;
    EditorHierarchyItem *hierarchy;
    size_t hierarchy_count;
    size_t hierarchy_capacity;
} EditorObject;

typedef struct EditorNavigationState {
    uint32_t mode;
    uint32_t selection;
    EditorObjectId object;
    uint32_t selected_line;
    uint32_t selected_vertex;
    EditorRigidBodyId rigid_body;
    EditorHitboxId hitbox;
    EditorJointId joint;
    EditorAnchorId anchor;
    EditorSoftBodyId soft_body;
    EditorSoftNodeId soft_node;
    EditorSoftBeamId soft_beam;
    uint32_t origin_kind;
} EditorNavigationState;

typedef struct EditorProject {
    Vec2D viewport_camera_offset;
    float viewport_camera_zoom;
    bool viewport_local_view;
    EditorNavigationState navigation;
    EditorCollisionMask *collision_masks;
    size_t collision_mask_count;
    size_t collision_mask_capacity;
    EditorObject *objects;
    size_t object_count;
    size_t object_capacity;
    EditorObjectId next_id;
    EditorVertexId next_vertex_id;
    EditorRigidBodyId next_rigid_body_id;
    EditorHitboxId next_hitbox_id;
    EditorJointId next_joint_id;
    EditorAnchorId next_anchor_id;
    EditorSoftBodyId next_soft_body_id;
    EditorSoftNodeId next_soft_node_id;
    EditorSoftBeamId next_soft_beam_id;
    EditorSoftAreaId next_soft_area_id;
    EditorObjectId selected;
} EditorProject;

void editor_project_init(EditorProject *project);
void editor_project_destroy(EditorProject *project);
bool editor_project_clone(EditorProject *destination, const EditorProject *source);
void editor_project_object_destroy(EditorObject *object);
bool editor_project_object_clone(EditorObject *destination,
    const EditorObject *source);
bool editor_project_object_copy_set(EditorObject *destination,
    const EditorObject *source);
void editor_project_rigid_body_destroy(EditorRigidBody *body);
bool editor_project_rigid_body_clone(EditorRigidBody *destination,
    const EditorRigidBody *source);
bool editor_project_rigid_body_copy_set(EditorRigidBody *destination,
    const EditorRigidBody *source);
void editor_project_soft_body_destroy(EditorSoftBody *body);
bool editor_project_soft_body_clone(EditorSoftBody *destination,
    const EditorSoftBody *source);
bool editor_project_soft_body_copy_set(EditorSoftBody *destination,
    const EditorSoftBody *source);
void editor_project_object_name_format(char *output, size_t capacity,
    const char *input);
void editor_project_property_name_format(char *output, size_t capacity,
    const char *input);
bool editor_project_collision_mask_add(EditorProject *project, const char *name,
    size_t *index);
bool editor_project_save(const EditorProject *project, const char *path);
EditorResult editor_project_load(EditorProject *project, const char *path);
EditorObject *editor_project_object_add(EditorProject *project, Position position);
bool editor_project_object_remove(EditorProject *project, EditorObjectId id);
EditorObject *editor_project_selected_get(EditorProject *project);
bool editor_project_object_select(EditorProject *project, EditorObjectId id);
void editor_project_selection_clear(EditorProject *project);
void editor_project_object_hierarchy_sync(EditorObject *object);
size_t editor_project_object_hierarchy_index_get(const EditorObject *object,
    EditorHierarchyItemKind kind, uint32_t id);
EditorRigidBody *editor_project_rigid_body_add(EditorProject *project,
    EditorObject *object);
EditorRigidBody *editor_project_rigid_body_get(EditorObject *object,
    EditorRigidBodyId id);
bool editor_project_rigid_body_remove(EditorObject *object, EditorRigidBodyId id);
bool editor_project_rigid_body_origin_set(EditorObject *object, EditorRigidBody *body,
    Position position);
Position editor_project_particle_center_get(const EditorRigidBody *body);
float editor_project_particle_auto_radius_get(const EditorRigidBody *body);
void editor_project_particle_auto_fit_update(EditorProject *project);
EditorHitbox *editor_project_hitbox_add(EditorProject *project, EditorRigidBody *body);
EditorHitbox *editor_project_hitbox_get(EditorRigidBody *body, EditorHitboxId id);
bool editor_project_hitbox_remove(EditorRigidBody *body, EditorHitboxId id);
bool editor_project_hitbox_vertex_remove(EditorHitbox *hitbox, uint32_t vertex_index);
bool editor_project_hitbox_line_remove(EditorHitbox *hitbox, uint32_t line_index);
bool editor_project_hitbox_vertex_insert(EditorProject *project, EditorHitbox *hitbox,
    uint32_t line_index);
float editor_project_hitbox_line_length_get(const EditorHitbox *hitbox, uint32_t line_index);
bool editor_project_hitbox_line_length_set(EditorHitbox *hitbox, uint32_t line_index,
    float length);
EditorJoint *editor_project_joint_add(EditorProject *project, EditorObject *object,
    EditorJointKind kind);
bool editor_project_joint_kind_set(EditorObject *object, EditorJoint *joint,
    EditorJointKind kind);
bool editor_project_joint_constraints_apply(EditorObject *object, EditorJoint *joint);
void editor_project_anchor_constraints_apply(EditorObject *object, EditorAnchorId anchor);
void editor_project_rigid_body_constraints_apply(EditorObject *object,
    EditorRigidBodyId rigid_body);
bool editor_project_joint_remove(EditorObject *object, EditorJointId id);
EditorAnchor *editor_project_anchor_add(EditorProject *project, EditorObject *object,
    Position position, EditorRigidBodyId rigid_body);
EditorAnchor *editor_project_anchor_get(EditorObject *object, EditorAnchorId id);
bool editor_project_anchor_remove(EditorObject *object, EditorAnchorId id);
bool editor_project_joint_anchor_set(EditorObject *object, EditorJoint *joint,
    uint32_t endpoint, EditorAnchorId anchor);
bool editor_project_anchor_position_lock_set(EditorObject *object, EditorAnchor *anchor,
    bool locked);
bool editor_project_anchor_rotation_lock_set(EditorObject *object, EditorAnchor *anchor,
    bool locked);
bool editor_project_anchor_rigid_body_set(EditorObject *object, EditorAnchor *anchor,
    EditorRigidBodyId rigid_body);
EditorSoftBody *editor_project_soft_body_add(EditorProject *project, EditorObject *object);
bool editor_project_soft_body_remove(EditorObject *object, EditorSoftBodyId id);
void editor_project_soft_body_hierarchy_sync(EditorSoftBody *body);
size_t editor_project_soft_body_hierarchy_index_get(const EditorSoftBody *body,
    EditorSoftHierarchyItemKind kind, uint32_t id);
bool editor_project_soft_body_origin_set(EditorSoftBody *body, Position position);
EditorSoftNode *editor_project_soft_node_add(EditorProject *project, EditorSoftBody *body,
    Position position);
bool editor_project_soft_node_remove(EditorProject *project, EditorSoftBody *body,
    EditorSoftNodeId id);
EditorSoftBeam *editor_project_soft_beam_add(EditorProject *project, EditorSoftBody *body,
    EditorSoftNodeId node_a, EditorSoftNodeId node_b);
bool editor_project_soft_beam_remove(EditorProject *project, EditorSoftBody *body,
    EditorSoftBeamId id);
void editor_project_soft_areas_sync(EditorProject *project, EditorSoftBody *body);
size_t editor_project_soft_area_triangulate(const EditorSoftBody *body,
    const EditorSoftArea *area, uint32_t triangles[][3], size_t capacity);

#endif
