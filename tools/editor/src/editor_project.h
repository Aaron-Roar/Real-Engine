#ifndef ROHR_EDITOR_PROJECT_H
#define ROHR_EDITOR_PROJECT_H

#include "rohr.h"

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

typedef uint32_t EditorObjectId;
typedef uint32_t EditorVertexId;
typedef uint32_t EditorRigidBodyId;
typedef uint32_t EditorHitboxId;
typedef uint32_t EditorJointId;
typedef uint32_t EditorAnchorId;
typedef uint32_t EditorSoftBodyId;
typedef uint32_t EditorSoftNodeId;
typedef uint32_t EditorSoftBeamId;

#define EDITOR_OBJECT_INVALID 0

typedef struct EditorVertex {
    EditorVertexId id;
    Position position;
    bool position_locked;
} EditorVertex;

typedef struct EditorHitbox {
    EditorHitboxId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    bool visible;
    EditorVertex vertices[EDITOR_HITBOX_VERTEX_MAX];
    uint32_t vertex_count;
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
    bool visible;
    EditorHitbox hitboxes[EDITOR_BODY_HITBOX_MAX];
    size_t hitbox_count;
} EditorRigidBody;

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
    bool generated;
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
    bool visible;
} EditorSoftNode;

typedef struct EditorSoftBeam {
    EditorSoftBeamId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    EditorSoftNodeId node_a;
    EditorSoftNodeId node_b;
    float stiffness;
    bool visible;
} EditorSoftBeam;

typedef struct EditorSoftBody {
    EditorSoftBodyId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    Position position;
    bool visible;
    EditorSoftNode nodes[EDITOR_SOFT_NODE_MAX];
    size_t node_count;
    EditorSoftBeam beams[EDITOR_SOFT_BEAM_MAX];
    size_t beam_count;
} EditorSoftBody;

typedef struct EditorObject {
    EditorObjectId id;
    char name[EDITOR_OBJECT_NAME_MAX];
    Position position;
    bool visible;
    EditorRigidBody rigid_bodies[EDITOR_RIGID_BODY_MAX];
    size_t rigid_body_count;
    EditorJoint joint_items[EDITOR_JOINT_MAX];
    size_t joint_count;
    EditorAnchor anchors[EDITOR_ANCHOR_MAX];
    size_t anchor_count;
    EditorSoftBody soft_body_items[EDITOR_SOFT_BODY_MAX];
    size_t soft_body_count;
} EditorObject;

typedef struct EditorProject {
    EditorObject objects[EDITOR_OBJECT_MAX];
    size_t object_count;
    EditorObjectId next_id;
    EditorVertexId next_vertex_id;
    EditorRigidBodyId next_rigid_body_id;
    EditorHitboxId next_hitbox_id;
    EditorJointId next_joint_id;
    EditorAnchorId next_anchor_id;
    EditorSoftBodyId next_soft_body_id;
    EditorSoftNodeId next_soft_node_id;
    EditorSoftBeamId next_soft_beam_id;
    EditorObjectId selected;
} EditorProject;

void editor_project_init(EditorProject *project);
EditorObject *editor_project_object_add(EditorProject *project, Position position);
bool editor_project_object_remove(EditorProject *project, EditorObjectId id);
EditorObject *editor_project_selected_get(EditorProject *project);
bool editor_project_object_select(EditorProject *project, EditorObjectId id);
void editor_project_selection_clear(EditorProject *project);
EditorRigidBody *editor_project_rigid_body_add(EditorProject *project,
    EditorObject *object);
EditorRigidBody *editor_project_rigid_body_get(EditorObject *object,
    EditorRigidBodyId id);
bool editor_project_rigid_body_remove(EditorObject *object, EditorRigidBodyId id);
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
bool editor_project_joint_remove(EditorObject *object, EditorJointId id);
EditorAnchor *editor_project_anchor_add(EditorProject *project, EditorObject *object,
    Position position, EditorRigidBodyId rigid_body, bool generated);
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
EditorSoftNode *editor_project_soft_node_add(EditorProject *project, EditorSoftBody *body,
    Position position);
bool editor_project_soft_node_remove(EditorSoftBody *body, EditorSoftNodeId id);
EditorSoftBeam *editor_project_soft_beam_add(EditorProject *project, EditorSoftBody *body,
    EditorSoftNodeId node_a, EditorSoftNodeId node_b);
bool editor_project_soft_beam_remove(EditorSoftBody *body, EditorSoftBeamId id);

#endif
