#include "editor_project.h"

#include <math.h>
#include <stdio.h>

static uint32_t editor_vertex_count_clamp(uint32_t vertex_count) {
    if(vertex_count < EDITOR_HITBOX_VERTEX_MIN) {
        return EDITOR_HITBOX_VERTEX_MIN;
    }
    if(vertex_count > EDITOR_HITBOX_VERTEX_MAX) {
        return EDITOR_HITBOX_VERTEX_MAX;
    }
    return vertex_count;
}

static void editor_hitbox_regular_set(EditorProject *project, EditorHitbox *hitbox,
    uint32_t vertex_count) {
    const float radius = 70.0f;

    if(project == NULL || hitbox == NULL) return;
    vertex_count = editor_vertex_count_clamp(vertex_count);
    hitbox->visible = true;
    hitbox->vertex_count = vertex_count;
    for(uint32_t i = 0; i < vertex_count; i += 1) {
        float angle = -1.57079632679f + 6.28318530718f *
            (float)i / (float)vertex_count;
        hitbox->vertices[i] = (EditorVertex){
            .id = project->next_vertex_id++,
            .position = {
                cosf(angle) * radius,
                sinf(angle) * radius
            }
        };
    }
}

void editor_project_init(EditorProject *project) {
    if(project == NULL) return;
    *project = (EditorProject){
        .next_id = 1,
        .next_vertex_id = 1,
        .next_rigid_body_id = 1,
        .next_hitbox_id = 1,
        .next_joint_id = 1,
        .next_anchor_id = 1,
        .next_soft_body_id = 1,
        .next_soft_node_id = 1,
        .next_soft_beam_id = 1
    };
}

EditorObject *editor_project_object_add(EditorProject *project, Position position) {
    EditorObject *object;

    if(project == NULL || project->object_count >= EDITOR_OBJECT_MAX) return NULL;
    object = &project->objects[project->object_count++];
    *object = (EditorObject){
        .id = project->next_id++,
        .position = position,
        .visible = true
    };
    snprintf(object->name, sizeof(object->name), "object_%u", object->id);
    project->selected = object->id;
    return object;
}

bool editor_project_object_remove(EditorProject *project, EditorObjectId id) {
    size_t index;

    if(project == NULL || id == EDITOR_OBJECT_INVALID) return false;
    for(index = 0; index < project->object_count; index += 1) {
        if(project->objects[index].id == id) break;
    }
    if(index == project->object_count) return false;
    for(size_t i = index + 1; i < project->object_count; i += 1) {
        project->objects[i - 1] = project->objects[i];
    }
    project->object_count -= 1;
    project->objects[project->object_count] = (EditorObject){0};
    if(project->selected == id) project->selected = EDITOR_OBJECT_INVALID;
    return true;
}

EditorObject *editor_project_selected_get(EditorProject *project) {
    if(project == NULL || project->selected == EDITOR_OBJECT_INVALID) return NULL;
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(project->objects[i].id == project->selected) return &project->objects[i];
    }
    return NULL;
}

bool editor_project_object_select(EditorProject *project, EditorObjectId id) {
    if(project == NULL) return false;
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(project->objects[i].id != id) continue;
        project->selected = id;
        return true;
    }
    return false;
}

void editor_project_selection_clear(EditorProject *project) {
    if(project == NULL) return;
    project->selected = EDITOR_OBJECT_INVALID;
}

EditorRigidBody editor_project_rigid_body_default_get(void) {
    return (EditorRigidBody){
        .mass_value = 1.0f,
        .friction = 0.5f,
        .restitution = 0.0f,
        .visible = true
    };
}

EditorJoint editor_project_joint_default_get(EditorJointKind kind) {
    return (EditorJoint){
        .kind = kind,
        .rest_length = 0.0f,
        .stiffness = 100.0f,
        .damping = kind == EDITOR_JOINT_SPRING ? 10.0f : 0.0f,
        .visual_size = 1.0f,
        .visible = true
    };
}

EditorRigidBody *editor_project_rigid_body_add(EditorProject *project,
    EditorObject *object) {
    EditorRigidBody *body;

    if(project == NULL || object == NULL ||
            object->rigid_body_count >= EDITOR_RIGID_BODY_MAX) return NULL;
    body = &object->rigid_bodies[object->rigid_body_count++];
    *body = editor_project_rigid_body_default_get();
    body->id = project->next_rigid_body_id++;
    snprintf(body->name, sizeof(body->name), "rigid_body_%u", body->id);
    if(editor_project_hitbox_add(project, body) == NULL) {
        object->rigid_body_count -= 1;
        object->rigid_bodies[object->rigid_body_count] = (EditorRigidBody){0};
        return NULL;
    }
    return body;
}

EditorRigidBody *editor_project_rigid_body_get(EditorObject *object,
    EditorRigidBodyId id) {
    if(object == NULL || id == 0) return NULL;
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        if(object->rigid_bodies[i].id == id) return &object->rigid_bodies[i];
    }
    return NULL;
}

bool editor_project_rigid_body_remove(EditorObject *object, EditorRigidBodyId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->rigid_body_count; i += 1) {
        if(object->rigid_bodies[i].id != id) continue;
        for(size_t j = 0; j < object->anchor_count; j += 1) {
            EditorAnchor *anchor = &object->anchors[j];
            if(anchor->rigid_body != id) continue;
            if(anchor->position_follows_body) {
                (void)editor_project_anchor_position_lock_set(object, anchor, false);
            }
            if(anchor->rotation_follows_body) {
                (void)editor_project_anchor_rotation_lock_set(object, anchor, false);
            }
            anchor->rigid_body = 0;
        }
        for(size_t j = i + 1; j < object->rigid_body_count; j += 1) {
            object->rigid_bodies[j - 1] = object->rigid_bodies[j];
        }
        object->rigid_body_count -= 1;
        object->rigid_bodies[object->rigid_body_count] = (EditorRigidBody){0};
        return true;
    }
    return false;
}

EditorAnchor *editor_project_anchor_get(EditorObject *object, EditorAnchorId id) {
    if(object == NULL || id == 0) return NULL;
    for(size_t i = 0; i < object->anchor_count; i += 1) {
        if(object->anchors[i].id == id) return &object->anchors[i];
    }
    return NULL;
}

EditorAnchor *editor_project_anchor_add(EditorProject *project, EditorObject *object,
    Position position, EditorRigidBodyId rigid_body) {
    EditorAnchor *anchor;

    if(project == NULL || object == NULL || object->anchor_count >= EDITOR_ANCHOR_MAX ||
            (rigid_body != 0 && editor_project_rigid_body_get(object, rigid_body) == NULL)) {
        return NULL;
    }
    anchor = &object->anchors[object->anchor_count++];
    *anchor = (EditorAnchor){.id = project->next_anchor_id++, .position = position,
        .rigid_body = rigid_body, .position_follows_body = rigid_body != 0,
        .rotation_follows_body = rigid_body != 0, .visible = true};
    snprintf(anchor->name, sizeof(anchor->name), "anchor_%u", anchor->id);
    return anchor;
}

static Position editor_position_rotate(Position position, float rotation) {
    float cosine = cosf(rotation);
    float sine = sinf(rotation);
    return (Position){position.x * cosine - position.y * sine,
        position.x * sine + position.y * cosine};
}

bool editor_project_anchor_position_lock_set(EditorObject *object, EditorAnchor *anchor,
    bool locked) {
    EditorRigidBody *body;
    Position position;

    if(object == NULL || anchor == NULL) return false;
    if(anchor->position_follows_body == locked) return true;
    body = editor_project_rigid_body_get(object, anchor->rigid_body);
    if(body == NULL) return false;
    if(locked) {
        position = (Position){anchor->position.x - body->position.x,
            anchor->position.y - body->position.y};
        anchor->position = editor_position_rotate(position, -body->rotation);
    } else {
        position = editor_position_rotate(anchor->position, body->rotation);
        anchor->position = (Position){body->position.x + position.x,
            body->position.y + position.y};
    }
    anchor->position_follows_body = locked;
    return true;
}

bool editor_project_anchor_rotation_lock_set(EditorObject *object, EditorAnchor *anchor,
    bool locked) {
    EditorRigidBody *body;

    if(object == NULL || anchor == NULL) return false;
    if(anchor->rotation_follows_body == locked) return true;
    body = editor_project_rigid_body_get(object, anchor->rigid_body);
    if(body == NULL) return false;
    anchor->rotation += locked ? -body->rotation : body->rotation;
    anchor->rotation_follows_body = locked;
    return true;
}

bool editor_project_anchor_rigid_body_set(EditorObject *object, EditorAnchor *anchor,
    EditorRigidBodyId rigid_body) {
    bool position_locked;
    bool rotation_locked;

    if(object == NULL || anchor == NULL || (rigid_body != 0 &&
            editor_project_rigid_body_get(object, rigid_body) == NULL)) return false;
    if(anchor->rigid_body == rigid_body) return true;
    position_locked = anchor->position_follows_body;
    rotation_locked = anchor->rotation_follows_body;
    if(position_locked && !editor_project_anchor_position_lock_set(object, anchor, false)) {
        return false;
    }
    if(rotation_locked && !editor_project_anchor_rotation_lock_set(object, anchor, false)) {
        return false;
    }
    anchor->rigid_body = rigid_body;
    if(rigid_body != 0) {
        if(position_locked) (void)editor_project_anchor_position_lock_set(object, anchor, true);
        if(rotation_locked) (void)editor_project_anchor_rotation_lock_set(object, anchor, true);
    }
    return true;
}

bool editor_project_anchor_remove(EditorObject *object, EditorAnchorId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->joint_count; i += 1) {
        EditorJoint *joint = &object->joint_items[i];
        if(joint->anchor_a == id) joint->anchor_a = 0;
        if(joint->anchor_b == id) joint->anchor_b = 0;
    }
    for(size_t i = 0; i < object->anchor_count; i += 1) {
        if(object->anchors[i].id != id) continue;
        for(size_t j = i + 1; j < object->anchor_count; j += 1) {
            object->anchors[j - 1] = object->anchors[j];
        }
        object->anchor_count -= 1;
        object->anchors[object->anchor_count] = (EditorAnchor){0};
        return true;
    }
    return false;
}

EditorHitbox *editor_project_hitbox_add(EditorProject *project, EditorRigidBody *body) {
    EditorHitbox *hitbox;

    if(project == NULL || body == NULL || body->hitbox_count >= EDITOR_BODY_HITBOX_MAX) {
        return NULL;
    }
    hitbox = &body->hitboxes[body->hitbox_count++];
    *hitbox = (EditorHitbox){.id = project->next_hitbox_id++, .visible = true};
    snprintf(hitbox->name, sizeof(hitbox->name), "hitbox_%u", hitbox->id);
    editor_hitbox_regular_set(project, hitbox, EDITOR_HITBOX_VERTEX_MIN);
    return hitbox;
}

EditorHitbox *editor_project_hitbox_get(EditorRigidBody *body, EditorHitboxId id) {
    if(body == NULL || id == 0) return NULL;
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        if(body->hitboxes[i].id == id) return &body->hitboxes[i];
    }
    return NULL;
}

bool editor_project_hitbox_remove(EditorRigidBody *body, EditorHitboxId id) {
    if(body == NULL || id == 0) return false;
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        if(body->hitboxes[i].id != id) continue;
        for(size_t j = i + 1; j < body->hitbox_count; j += 1) {
            body->hitboxes[j - 1] = body->hitboxes[j];
        }
        body->hitbox_count -= 1;
        body->hitboxes[body->hitbox_count] = (EditorHitbox){0};
        return true;
    }
    return false;
}

bool editor_project_hitbox_vertex_remove(EditorHitbox *hitbox, uint32_t vertex_index) {
    if(hitbox == NULL) return false;
    if(hitbox->vertex_count <= EDITOR_HITBOX_VERTEX_MIN ||
            vertex_index >= hitbox->vertex_count) return false;
    for(uint32_t i = vertex_index + 1; i < hitbox->vertex_count; i += 1) {
        hitbox->vertices[i - 1] = hitbox->vertices[i];
    }
    hitbox->vertex_count -= 1;
    hitbox->vertices[hitbox->vertex_count] = (EditorVertex){0};
    return true;
}

bool editor_project_hitbox_line_remove(EditorHitbox *hitbox, uint32_t line_index) {
    if(hitbox == NULL || line_index >= hitbox->vertex_count) return false;
    return editor_project_hitbox_vertex_remove(hitbox,
        (line_index + 1) % hitbox->vertex_count);
}

bool editor_project_hitbox_vertex_insert(EditorProject *project, EditorHitbox *hitbox,
    uint32_t line_index) {
    uint32_t second;
    EditorVertex inserted;

    if(project == NULL || hitbox == NULL) return false;
    if(hitbox->vertex_count >= EDITOR_HITBOX_VERTEX_MAX ||
            line_index >= hitbox->vertex_count) return false;
    second = (line_index + 1) % hitbox->vertex_count;
    inserted = (EditorVertex){
        .id = project->next_vertex_id++,
        .position = {
            (hitbox->vertices[line_index].position.x +
                hitbox->vertices[second].position.x) * 0.5f,
            (hitbox->vertices[line_index].position.y +
                hitbox->vertices[second].position.y) * 0.5f
        }
    };
    if(second == 0) {
        hitbox->vertices[hitbox->vertex_count] = inserted;
    } else {
        for(uint32_t i = hitbox->vertex_count; i > second; i -= 1) {
            hitbox->vertices[i] = hitbox->vertices[i - 1];
        }
        hitbox->vertices[second] = inserted;
    }
    hitbox->vertex_count += 1;
    return true;
}

float editor_project_hitbox_line_length_get(const EditorHitbox *hitbox,
    uint32_t line_index) {
    uint32_t second;
    Vec2D delta;

    if(hitbox == NULL || line_index >= hitbox->vertex_count) return 0.0f;
    second = (line_index + 1) % hitbox->vertex_count;
    delta = (Vec2D){
        hitbox->vertices[second].position.x - hitbox->vertices[line_index].position.x,
        hitbox->vertices[second].position.y - hitbox->vertices[line_index].position.y
    };
    return sqrtf(delta.x * delta.x + delta.y * delta.y);
}

bool editor_project_hitbox_line_length_set(EditorHitbox *hitbox,
    uint32_t line_index, float length) {
    EditorVertex *first;
    EditorVertex *second;
    Vec2D direction;
    float current;

    if(hitbox == NULL || length <= 0.001f ||
            line_index >= hitbox->vertex_count) return false;
    first = &hitbox->vertices[line_index];
    second = &hitbox->vertices[(line_index + 1) % hitbox->vertex_count];
    if(first->position_locked && second->position_locked) return false;
    direction = (Vec2D){second->position.x - first->position.x,
        second->position.y - first->position.y};
    current = sqrtf(direction.x * direction.x + direction.y * direction.y);
    if(current <= 0.001f) return false;
    direction.x /= current;
    direction.y /= current;
    if(first->position_locked) {
        second->position.x = first->position.x + direction.x * length;
        second->position.y = first->position.y + direction.y * length;
    } else if(second->position_locked) {
        first->position.x = second->position.x - direction.x * length;
        first->position.y = second->position.y - direction.y * length;
    } else {
        Position midpoint = {(first->position.x + second->position.x) * 0.5f,
            (first->position.y + second->position.y) * 0.5f};
        first->position = (Position){midpoint.x - direction.x * length * 0.5f,
            midpoint.y - direction.y * length * 0.5f};
        second->position = (Position){midpoint.x + direction.x * length * 0.5f,
            midpoint.y + direction.y * length * 0.5f};
    }
    return true;
}

static Position editor_anchor_world_position_get(EditorObject *object,
    EditorAnchor *anchor) {
    EditorRigidBody *body = editor_project_rigid_body_get(object, anchor->rigid_body);
    if(body != NULL && anchor->position_follows_body) {
        Position offset = editor_position_rotate(anchor->position, body->rotation);
        return (Position){body->position.x + offset.x, body->position.y + offset.y};
    }
    return anchor->position;
}

static float editor_anchor_world_rotation_get(EditorObject *object,
    EditorAnchor *anchor) {
    EditorRigidBody *body = editor_project_rigid_body_get(object, anchor->rigid_body);
    return anchor->rotation +
        (body != NULL && anchor->rotation_follows_body ? body->rotation : 0.0f);
}

bool editor_project_joint_constraints_apply(EditorObject *object, EditorJoint *joint) {
    EditorAnchor *a;
    EditorAnchor *b;
    EditorRigidBody *body_a;
    EditorRigidBody *body_b;
    Position world_a;
    Position world_b;

    if(object == NULL || joint == NULL) return false;
    if(joint->kind == EDITOR_JOINT_SPRING) return true;
    a = editor_project_anchor_get(object, joint->anchor_a);
    b = editor_project_anchor_get(object, joint->anchor_b);
    if(a == NULL || b == NULL) return true;
    body_a = editor_project_rigid_body_get(object, a->rigid_body);
    body_b = editor_project_rigid_body_get(object, b->rigid_body);
    if(joint->kind == EDITOR_JOINT_WELD) {
        float rotation_delta = editor_anchor_world_rotation_get(object, a) -
            editor_anchor_world_rotation_get(object, b);
        if(body_b != NULL && b->rotation_follows_body && body_b != body_a) {
            body_b->rotation += rotation_delta;
        } else {
            b->rotation += rotation_delta;
        }
    }
    world_a = editor_anchor_world_position_get(object, a);
    world_b = editor_anchor_world_position_get(object, b);
    if(body_b != NULL && b->position_follows_body && body_b != body_a) {
        body_b->position.x += world_a.x - world_b.x;
        body_b->position.y += world_a.y - world_b.y;
    } else if(body_b != NULL && b->position_follows_body) {
        Position local = {world_a.x - body_b->position.x,
            world_a.y - body_b->position.y};
        b->position = editor_position_rotate(local, -body_b->rotation);
    } else {
        b->position = world_a;
    }
    return true;
}

bool editor_project_joint_kind_set(EditorObject *object, EditorJoint *joint,
    EditorJointKind kind) {
    if(object == NULL || joint == NULL || kind < EDITOR_JOINT_REVOLUTE ||
            kind > EDITOR_JOINT_SPRING) return false;
    joint->kind = kind;
    return editor_project_joint_constraints_apply(object, joint);
}

void editor_project_anchor_constraints_apply(EditorObject *object, EditorAnchorId anchor) {
    if(object == NULL || anchor == 0) return;
    for(size_t i = 0; i < object->joint_count; i += 1) {
        EditorJoint *joint = &object->joint_items[i];
        if(joint->anchor_a == anchor || joint->anchor_b == anchor) {
            (void)editor_project_joint_constraints_apply(object, joint);
        }
    }
}

EditorJoint *editor_project_joint_add(EditorProject *project, EditorObject *object,
    EditorJointKind kind) {
    EditorJoint *joint;

    if(project == NULL || object == NULL || object->joint_count >= EDITOR_JOINT_MAX) {
        return NULL;
    }
    joint = &object->joint_items[object->joint_count++];
    *joint = editor_project_joint_default_get(kind);
    joint->id = project->next_joint_id++;
    snprintf(joint->name, sizeof(joint->name), "joint_%u", joint->id);
    return joint;
}

bool editor_project_joint_remove(EditorObject *object, EditorJointId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->joint_count; i += 1) {
        if(object->joint_items[i].id != id) continue;
        for(size_t j = i + 1; j < object->joint_count; j += 1) {
            object->joint_items[j - 1] = object->joint_items[j];
        }
        object->joint_count -= 1;
        object->joint_items[object->joint_count] = (EditorJoint){0};
        return true;
    }
    return false;
}

bool editor_project_joint_anchor_set(EditorObject *object, EditorJoint *joint,
    uint32_t endpoint, EditorAnchorId anchor) {
    if(object == NULL || joint == NULL || endpoint > 1 ||
            (anchor != 0 && editor_project_anchor_get(object, anchor) == NULL)) return false;
    if(endpoint == 0) joint->anchor_a = anchor;
    else joint->anchor_b = anchor;
    if(joint->kind == EDITOR_JOINT_SPRING && joint->anchor_a != 0 &&
            joint->anchor_b != 0) {
        EditorAnchor *a = editor_project_anchor_get(object, joint->anchor_a);
        EditorAnchor *b = editor_project_anchor_get(object, joint->anchor_b);
        Position world_a = editor_anchor_world_position_get(object, a);
        Position world_b = editor_anchor_world_position_get(object, b);
        joint->rest_length = hypotf(world_b.x - world_a.x, world_b.y - world_a.y);
    }
    return editor_project_joint_constraints_apply(object, joint);
}

EditorSoftBody *editor_project_soft_body_add(EditorProject *project, EditorObject *object) {
    EditorSoftBody *body;

    if(project == NULL || object == NULL ||
            object->soft_body_count >= EDITOR_SOFT_BODY_MAX) return NULL;
    body = &object->soft_body_items[object->soft_body_count++];
    *body = (EditorSoftBody){
        .id = project->next_soft_body_id++,
        .visible = true
    };
    snprintf(body->name, sizeof(body->name), "soft_body_%u", body->id);
    return body;
}

bool editor_project_soft_body_remove(EditorObject *object, EditorSoftBodyId id) {
    if(object == NULL || id == 0) return false;
    for(size_t i = 0; i < object->soft_body_count; i += 1) {
        if(object->soft_body_items[i].id != id) continue;
        for(size_t j = i + 1; j < object->soft_body_count; j += 1) {
            object->soft_body_items[j - 1] = object->soft_body_items[j];
        }
        object->soft_body_count -= 1;
        object->soft_body_items[object->soft_body_count] = (EditorSoftBody){0};
        return true;
    }
    return false;
}

EditorSoftNode *editor_project_soft_node_add(EditorProject *project, EditorSoftBody *body,
    Position position) {
    EditorSoftNode *node;

    if(project == NULL || body == NULL || body->node_count >= EDITOR_SOFT_NODE_MAX) {
        return NULL;
    }
    node = &body->nodes[body->node_count++];
    *node = (EditorSoftNode){
        .id = project->next_soft_node_id++,
        .position = position,
        .node_mass = 1.0f,
        .visible = true
    };
    snprintf(node->name, sizeof(node->name), "node_%u", node->id);
    return node;
}

bool editor_project_soft_node_remove(EditorSoftBody *body, EditorSoftNodeId id) {
    if(body == NULL || id == 0) return false;
    for(size_t i = 0; i < body->node_count; i += 1) {
        if(body->nodes[i].id != id) continue;
        for(size_t j = 0; j < body->beam_count; j += 1) {
            EditorSoftBeam *beam = &body->beams[j];
            if(beam->node_a == id) beam->node_a = 0;
            if(beam->node_b == id) beam->node_b = 0;
        }
        for(size_t j = i + 1; j < body->node_count; j += 1) {
            body->nodes[j - 1] = body->nodes[j];
        }
        body->node_count -= 1;
        body->nodes[body->node_count] = (EditorSoftNode){0};
        return true;
    }
    return false;
}

EditorSoftBeam *editor_project_soft_beam_add(EditorProject *project, EditorSoftBody *body,
    EditorSoftNodeId node_a, EditorSoftNodeId node_b) {
    EditorSoftBeam *beam;
    bool found_a = node_a == 0;
    bool found_b = node_b == 0;

    if(project == NULL || body == NULL || (node_a != 0 && node_a == node_b) ||
            body->beam_count >= EDITOR_SOFT_BEAM_MAX) return NULL;
    for(size_t i = 0; i < body->node_count; i += 1) {
        if(body->nodes[i].id == node_a) found_a = true;
        if(body->nodes[i].id == node_b) found_b = true;
    }
    if(!found_a || !found_b) return NULL;
    beam = &body->beams[body->beam_count++];
    *beam = (EditorSoftBeam){
        .id = project->next_soft_beam_id++,
        .node_a = node_a,
        .node_b = node_b,
        .stiffness = 1.0f,
        .visible = true
    };
    snprintf(beam->name, sizeof(beam->name), "beam_%u", beam->id);
    return beam;
}

bool editor_project_soft_beam_remove(EditorSoftBody *body, EditorSoftBeamId id) {
    if(body == NULL || id == 0) return false;
    for(size_t i = 0; i < body->beam_count; i += 1) {
        if(body->beams[i].id != id) continue;
        for(size_t j = i + 1; j < body->beam_count; j += 1) {
            body->beams[j - 1] = body->beams[j];
        }
        body->beam_count -= 1;
        body->beams[body->beam_count] = (EditorSoftBeam){0};
        return true;
    }
    return false;
}
