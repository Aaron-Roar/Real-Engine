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

EditorRigidBody *editor_project_rigid_body_add(EditorProject *project,
    EditorObject *object) {
    EditorRigidBody *body;

    if(project == NULL || object == NULL ||
            object->rigid_body_count >= EDITOR_RIGID_BODY_MAX) return NULL;
    body = &object->rigid_bodies[object->rigid_body_count++];
    *body = (EditorRigidBody){
        .id = project->next_rigid_body_id++,
        .visible = true
    };
    snprintf(body->name, sizeof(body->name), "rigid_body_%u", body->id);
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
        for(size_t j = i + 1; j < object->rigid_body_count; j += 1) {
            object->rigid_bodies[j - 1] = object->rigid_bodies[j];
        }
        object->rigid_body_count -= 1;
        object->rigid_bodies[object->rigid_body_count] = (EditorRigidBody){0};
        for(size_t j = object->joint_count; j > 0; j -= 1) {
            EditorJoint *joint = &object->joint_items[j - 1];
            if(joint->body_a == id || joint->body_b == id) {
                (void)editor_project_joint_remove(object, joint->id);
            }
        }
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

EditorJoint *editor_project_joint_add(EditorProject *project, EditorObject *object,
    EditorJointKind kind) {
    EditorJoint *joint;

    if(project == NULL || object == NULL || object->joint_count >= EDITOR_JOINT_MAX) {
        return NULL;
    }
    joint = &object->joint_items[object->joint_count++];
    *joint = (EditorJoint){
        .id = project->next_joint_id++,
        .kind = kind,
        .visible = true
    };
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
        for(size_t j = body->beam_count; j > 0; j -= 1) {
            EditorSoftBeam *beam = &body->beams[j - 1];
            if(beam->node_a == id || beam->node_b == id) {
                (void)editor_project_soft_beam_remove(body, beam->id);
            }
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
    bool found_a = false;
    bool found_b = false;

    if(project == NULL || body == NULL || node_a == node_b ||
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
