#include "editor_project.h"

#include "yyjson.h"

#include <stdio.h>
#include <string.h>

static yyjson_mut_val *editor_json_position_write(yyjson_mut_doc *document,
    Position position) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_obj_add_real(document, value, "x", position.x);
    yyjson_mut_obj_add_real(document, value, "y", position.y);
    return value;
}

static bool editor_json_position_read(yyjson_val *value, Position *position) {
    yyjson_val *x;
    yyjson_val *y;
    if(!yyjson_is_obj(value) || position == NULL) return false;
    x = yyjson_obj_get(value, "x");
    y = yyjson_obj_get(value, "y");
    if(!yyjson_is_num(x) || !yyjson_is_num(y)) return false;
    *position = (Position){(float)yyjson_get_num(x), (float)yyjson_get_num(y)};
    return true;
}

static bool editor_json_uint(yyjson_val *object, const char *key, uint32_t *value) {
    yyjson_val *item = yyjson_obj_get(object, key);
    if(!yyjson_is_uint(item) || yyjson_get_uint(item) > UINT32_MAX || value == NULL) {
        return false;
    }
    *value = (uint32_t)yyjson_get_uint(item);
    return true;
}

static bool editor_json_uint64(yyjson_val *object, const char *key, uint64_t *value) {
    yyjson_val *item = yyjson_obj_get(object, key);
    if(!yyjson_is_uint(item) || value == NULL) return false;
    *value = yyjson_get_uint(item);
    return true;
}

static bool editor_json_real(yyjson_val *object, const char *key, float *value) {
    yyjson_val *item = yyjson_obj_get(object, key);
    if(!yyjson_is_num(item) || value == NULL) return false;
    *value = (float)yyjson_get_num(item);
    return true;
}

static bool editor_json_bool(yyjson_val *object, const char *key, bool *value) {
    yyjson_val *item = yyjson_obj_get(object, key);
    if(!yyjson_is_bool(item) || value == NULL) return false;
    *value = yyjson_get_bool(item);
    return true;
}

static bool editor_json_name(yyjson_val *object, char name[EDITOR_OBJECT_NAME_MAX]) {
    yyjson_val *item = yyjson_obj_get(object, "name");
    size_t length;
    if(!yyjson_is_str(item) || name == NULL) return false;
    length = yyjson_get_len(item);
    if(length == 0 || length >= EDITOR_OBJECT_NAME_MAX) return false;
    memcpy(name, yyjson_get_str(item), length + 1);
    return true;
}

static yyjson_mut_val *editor_json_hitbox_write(yyjson_mut_doc *document,
    const EditorHitbox *hitbox) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_val *vertices = yyjson_mut_arr(document);
    yyjson_mut_val *lines = yyjson_mut_arr(document);
    yyjson_mut_obj_add_uint(document, value, "id", hitbox->id);
    yyjson_mut_obj_add_strcpy(document, value, "name", hitbox->name);
    yyjson_mut_obj_add_bool(document, value, "visible", hitbox->visible);
    for(size_t i = 0; i < hitbox->vertex_count; i += 1) {
        yyjson_mut_val *vertex = yyjson_mut_obj(document);
        yyjson_mut_obj_add_uint(document, vertex, "id", hitbox->vertices[i].id);
        yyjson_mut_obj_add_strcpy(document, vertex, "name", hitbox->vertices[i].name);
        yyjson_mut_obj_add_val(document, vertex, "position",
            editor_json_position_write(document, hitbox->vertices[i].position));
        yyjson_mut_obj_add_bool(document, vertex, "position_locked",
            hitbox->vertices[i].position_locked);
        yyjson_mut_arr_add_val(vertices, vertex);
        yyjson_mut_arr_add_strcpy(document, lines, hitbox->line_names[i]);
    }
    yyjson_mut_obj_add_val(document, value, "vertices", vertices);
    yyjson_mut_obj_add_val(document, value, "lines", lines);
    return value;
}

static yyjson_mut_val *editor_json_body_write(yyjson_mut_doc *document,
    const EditorRigidBody *body) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_val *hitboxes = yyjson_mut_arr(document);
    yyjson_mut_obj_add_uint(document, value, "id", body->id);
    yyjson_mut_obj_add_strcpy(document, value, "name", body->name);
    yyjson_mut_obj_add_val(document, value, "position",
        editor_json_position_write(document, body->position));
    yyjson_mut_obj_add_real(document, value, "rotation", body->rotation);
    yyjson_mut_obj_add_real(document, value, "mass", body->mass_value);
    yyjson_mut_obj_add_real(document, value, "friction", body->friction);
    yyjson_mut_obj_add_real(document, value, "restitution", body->restitution);
    yyjson_mut_obj_add_bool(document, value, "static", body->static_body);
    yyjson_mut_obj_add_bool(document, value, "rotation_locked", body->rotation_locked);
    yyjson_mut_obj_add_bool(document, value, "gravity_enabled", body->gravity_enabled);
    yyjson_mut_obj_add_bool(document, value, "collision_enabled", body->collision_enabled);
    yyjson_mut_obj_add_uint(document, value, "collision_category",
        body->collision_category);
    yyjson_mut_obj_add_uint(document, value, "collision_with", body->collision_with);
    yyjson_mut_obj_add_bool(document, value, "visible", body->visible);
    for(size_t i = 0; i < body->hitbox_count; i += 1) {
        yyjson_mut_arr_add_val(hitboxes, editor_json_hitbox_write(document,
            &body->hitboxes[i]));
    }
    yyjson_mut_obj_add_val(document, value, "hitboxes", hitboxes);
    return value;
}

static yyjson_mut_val *editor_json_anchor_write(yyjson_mut_doc *document,
    const EditorAnchor *anchor) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_obj_add_uint(document, value, "id", anchor->id);
    yyjson_mut_obj_add_strcpy(document, value, "name", anchor->name);
    yyjson_mut_obj_add_val(document, value, "position",
        editor_json_position_write(document, anchor->position));
    yyjson_mut_obj_add_real(document, value, "rotation", anchor->rotation);
    yyjson_mut_obj_add_uint(document, value, "rigid_body", anchor->rigid_body);
    yyjson_mut_obj_add_bool(document, value, "position_follows_body",
        anchor->position_follows_body);
    yyjson_mut_obj_add_bool(document, value, "rotation_follows_body",
        anchor->rotation_follows_body);
    yyjson_mut_obj_add_bool(document, value, "visible", anchor->visible);
    return value;
}

static yyjson_mut_val *editor_json_joint_write(yyjson_mut_doc *document,
    const EditorJoint *joint) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_obj_add_uint(document, value, "id", joint->id);
    yyjson_mut_obj_add_strcpy(document, value, "name", joint->name);
    yyjson_mut_obj_add_uint(document, value, "kind", (uint32_t)joint->kind);
    yyjson_mut_obj_add_uint(document, value, "anchor_a", joint->anchor_a);
    yyjson_mut_obj_add_uint(document, value, "anchor_b", joint->anchor_b);
    yyjson_mut_obj_add_real(document, value, "rest_length", joint->rest_length);
    yyjson_mut_obj_add_real(document, value, "stiffness", joint->stiffness);
    yyjson_mut_obj_add_real(document, value, "damping", joint->damping);
    yyjson_mut_obj_add_real(document, value, "rest_angle", joint->rest_angle);
    yyjson_mut_obj_add_real(document, value, "visual_size", joint->visual_size);
    yyjson_mut_obj_add_bool(document, value, "visible", joint->visible);
    return value;
}

static yyjson_mut_val *editor_json_soft_body_write(yyjson_mut_doc *document,
    const EditorSoftBody *body) {
    yyjson_mut_val *value = yyjson_mut_obj(document);
    yyjson_mut_val *nodes = yyjson_mut_arr(document);
    yyjson_mut_val *beams = yyjson_mut_arr(document);
    yyjson_mut_obj_add_uint(document, value, "id", body->id);
    yyjson_mut_obj_add_strcpy(document, value, "name", body->name);
    yyjson_mut_obj_add_val(document, value, "position",
        editor_json_position_write(document, body->position));
    yyjson_mut_obj_add_bool(document, value, "visible", body->visible);
    for(size_t i = 0; i < body->node_count; i += 1) {
        const EditorSoftNode *node = &body->nodes[i];
        yyjson_mut_val *item = yyjson_mut_obj(document);
        yyjson_mut_obj_add_uint(document, item, "id", node->id);
        yyjson_mut_obj_add_strcpy(document, item, "name", node->name);
        yyjson_mut_obj_add_val(document, item, "position",
            editor_json_position_write(document, node->position));
        yyjson_mut_obj_add_real(document, item, "mass", node->node_mass);
        yyjson_mut_obj_add_bool(document, item, "gravity_enabled", node->gravity_enabled);
        yyjson_mut_obj_add_bool(document, item, "collision_enabled", node->collision_enabled);
        yyjson_mut_obj_add_uint(document, item, "collision_category", node->collision_category);
        yyjson_mut_obj_add_uint(document, item, "collision_with", node->collision_with);
        yyjson_mut_obj_add_bool(document, item, "visible", node->visible);
        yyjson_mut_arr_add_val(nodes, item);
    }
    for(size_t i = 0; i < body->beam_count; i += 1) {
        const EditorSoftBeam *beam = &body->beams[i];
        yyjson_mut_val *item = yyjson_mut_obj(document);
        yyjson_mut_obj_add_uint(document, item, "id", beam->id);
        yyjson_mut_obj_add_strcpy(document, item, "name", beam->name);
        yyjson_mut_obj_add_uint(document, item, "node_a", beam->node_a);
        yyjson_mut_obj_add_uint(document, item, "node_b", beam->node_b);
        yyjson_mut_obj_add_real(document, item, "stiffness", beam->stiffness);
        yyjson_mut_obj_add_bool(document, item, "visible", beam->visible);
        yyjson_mut_arr_add_val(beams, item);
    }
    yyjson_mut_obj_add_val(document, value, "nodes", nodes);
    yyjson_mut_obj_add_val(document, value, "beams", beams);
    return value;
}

bool editor_project_save(const EditorProject *project, const char *path) {
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *objects;
    yyjson_mut_val *collision_masks;
    bool success;
    if(project == NULL || path == NULL || path[0] == '\0') return false;
    document = yyjson_mut_doc_new(NULL);
    if(document == NULL) return false;
    root = yyjson_mut_obj(document);
    objects = yyjson_mut_arr(document);
    collision_masks = yyjson_mut_arr(document);
    yyjson_mut_doc_set_root(document, root);
    yyjson_mut_obj_add_uint(document, root, "format_version", EDITOR_PROJECT_FORMAT_VERSION);
    yyjson_mut_obj_add_uint(document, root, "selected", project->selected);
    yyjson_mut_obj_add_uint(document, root, "next_object_id", project->next_id);
    yyjson_mut_obj_add_uint(document, root, "next_vertex_id", project->next_vertex_id);
    yyjson_mut_obj_add_uint(document, root, "next_rigid_body_id", project->next_rigid_body_id);
    yyjson_mut_obj_add_uint(document, root, "next_hitbox_id", project->next_hitbox_id);
    yyjson_mut_obj_add_uint(document, root, "next_joint_id", project->next_joint_id);
    yyjson_mut_obj_add_uint(document, root, "next_anchor_id", project->next_anchor_id);
    yyjson_mut_obj_add_uint(document, root, "next_soft_body_id", project->next_soft_body_id);
    yyjson_mut_obj_add_uint(document, root, "next_soft_node_id", project->next_soft_node_id);
    yyjson_mut_obj_add_uint(document, root, "next_soft_beam_id", project->next_soft_beam_id);
    for(size_t i = 0; i < project->collision_mask_count; i += 1) {
        yyjson_mut_arr_add_strcpy(document, collision_masks,
            project->collision_masks[i].name);
    }
    yyjson_mut_obj_add_val(document, root, "collision_masks", collision_masks);
    for(size_t i = 0; i < project->object_count; i += 1) {
        const EditorObject *object = &project->objects[i];
        yyjson_mut_val *value = yyjson_mut_obj(document);
        yyjson_mut_val *bodies = yyjson_mut_arr(document);
        yyjson_mut_val *anchors = yyjson_mut_arr(document);
        yyjson_mut_val *joint_values = yyjson_mut_arr(document);
        yyjson_mut_val *soft_body_values = yyjson_mut_arr(document);
        yyjson_mut_obj_add_uint(document, value, "id", object->id);
        yyjson_mut_obj_add_strcpy(document, value, "name", object->name);
        yyjson_mut_obj_add_val(document, value, "position",
            editor_json_position_write(document, object->position));
        yyjson_mut_obj_add_bool(document, value, "visible", object->visible);
        for(size_t j = 0; j < object->rigid_body_count; j += 1)
            yyjson_mut_arr_add_val(bodies, editor_json_body_write(document,
                &object->rigid_bodies[j]));
        for(size_t j = 0; j < object->anchor_count; j += 1)
            yyjson_mut_arr_add_val(anchors, editor_json_anchor_write(document,
                &object->anchors[j]));
        for(size_t j = 0; j < object->joint_count; j += 1)
            yyjson_mut_arr_add_val(joint_values, editor_json_joint_write(document,
                &object->joint_items[j]));
        for(size_t j = 0; j < object->soft_body_count; j += 1)
            yyjson_mut_arr_add_val(soft_body_values, editor_json_soft_body_write(document,
                &object->soft_body_items[j]));
        yyjson_mut_obj_add_val(document, value, "rigid_bodies", bodies);
        yyjson_mut_obj_add_val(document, value, "anchors", anchors);
        yyjson_mut_obj_add_val(document, value, "joints", joint_values);
        yyjson_mut_obj_add_val(document, value, "soft_bodies", soft_body_values);
        yyjson_mut_arr_add_val(objects, value);
    }
    yyjson_mut_obj_add_val(document, root, "objects", objects);
    success = yyjson_mut_write_file(path, document, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(document);
    return success;
}

static bool editor_json_hitbox_read(yyjson_val *value, EditorHitbox *hitbox,
    EditorProject *project) {
    yyjson_val *vertices = yyjson_obj_get(value, "vertices");
    yyjson_val *lines = yyjson_obj_get(value, "lines");
    size_t count;
    if(!yyjson_is_obj(value) || !editor_json_uint(value, "id", &hitbox->id) ||
            hitbox->id == 0 || !editor_json_name(value, hitbox->name) ||
            !editor_json_bool(value, "visible", &hitbox->visible) ||
            !yyjson_is_arr(vertices) || (lines != NULL && !yyjson_is_arr(lines))) return false;
    editor_project_property_name_format(hitbox->name, sizeof(hitbox->name), hitbox->name);
    count = yyjson_arr_size(vertices);
    if(count < EDITOR_HITBOX_VERTEX_MIN || count > EDITOR_HITBOX_VERTEX_MAX ||
            (lines != NULL && yyjson_arr_size(lines) != count)) return false;
    hitbox->vertex_count = (uint32_t)count;
    for(size_t i = 0; i < count; i += 1) {
        yyjson_val *item = yyjson_arr_get(vertices, i);
        yyjson_val *line = lines == NULL ? NULL : yyjson_arr_get(lines, i);
        yyjson_val *name = yyjson_obj_get(item, "name");
        if(!yyjson_is_obj(item) || !editor_json_uint(item, "id", &hitbox->vertices[i].id) ||
                hitbox->vertices[i].id == 0 || !editor_json_position_read(
                    yyjson_obj_get(item, "position"), &hitbox->vertices[i].position) ||
                !editor_json_bool(item, "position_locked",
                    &hitbox->vertices[i].position_locked) ||
                (name != NULL && (!yyjson_is_str(name) || yyjson_get_len(name) == 0 ||
                    yyjson_get_len(name) >= EDITOR_OBJECT_NAME_MAX)) ||
                (line != NULL && (!yyjson_is_str(line) || yyjson_get_len(line) == 0 ||
                    yyjson_get_len(line) >= EDITOR_OBJECT_NAME_MAX))) {
            return false;
        }
        if(name == NULL) snprintf(hitbox->vertices[i].name,
            sizeof(hitbox->vertices[i].name), "vertex_%zu", i + 1);
        else memcpy(hitbox->vertices[i].name, yyjson_get_str(name),
            yyjson_get_len(name) + 1);
        if(line == NULL) snprintf(hitbox->line_names[i],
            sizeof(hitbox->line_names[i]), "line_%zu", i + 1);
        else memcpy(hitbox->line_names[i], yyjson_get_str(line), yyjson_get_len(line) + 1);
        editor_project_property_name_format(hitbox->vertices[i].name,
            sizeof(hitbox->vertices[i].name), hitbox->vertices[i].name);
        editor_project_property_name_format(hitbox->line_names[i],
            sizeof(hitbox->line_names[i]), hitbox->line_names[i]);
        if(project->next_vertex_id <= hitbox->vertices[i].id)
            project->next_vertex_id = hitbox->vertices[i].id + 1;
    }
    if(project->next_hitbox_id <= hitbox->id) project->next_hitbox_id = hitbox->id + 1;
    return true;
}

static bool editor_json_body_read(yyjson_val *value, EditorRigidBody *body,
    EditorProject *project) {
    yyjson_val *hitboxes = yyjson_obj_get(value, "hitboxes");
    yyjson_val *collision_enabled = yyjson_obj_get(value, "collision_enabled");
    yyjson_val *collision_category = yyjson_obj_get(value, "collision_category");
    yyjson_val *collision_with = yyjson_obj_get(value, "collision_with");
    uint32_t count;
    *body = editor_project_rigid_body_default_get();
    if(!yyjson_is_obj(value) || !editor_json_uint(value, "id", &body->id) || body->id == 0 ||
            !editor_json_name(value, body->name) || !editor_json_position_read(
                yyjson_obj_get(value, "position"), &body->position) ||
            !editor_json_real(value, "rotation", &body->rotation) ||
            !editor_json_real(value, "mass", &body->mass_value) ||
            !editor_json_real(value, "friction", &body->friction) ||
            !editor_json_real(value, "restitution", &body->restitution) ||
            !editor_json_bool(value, "static", &body->static_body) ||
            !editor_json_bool(value, "rotation_locked", &body->rotation_locked) ||
            !editor_json_bool(value, "gravity_enabled", &body->gravity_enabled) ||
            !editor_json_bool(value, "visible", &body->visible) || !yyjson_is_arr(hitboxes)) {
        return false;
    }
    if(collision_enabled != NULL && (!editor_json_bool(value, "collision_enabled",
                &body->collision_enabled) ||
            !editor_json_uint64(value, "collision_category", &body->collision_category) ||
            !editor_json_uint64(value, "collision_with", &body->collision_with))) return false;
    if(collision_enabled == NULL &&
            (collision_category != NULL || collision_with != NULL)) return false;
    editor_project_property_name_format(body->name, sizeof(body->name), body->name);
    count = (uint32_t)yyjson_arr_size(hitboxes);
    if(count > EDITOR_BODY_HITBOX_MAX) return false;
    body->hitbox_count = count;
    for(size_t i = 0; i < count; i += 1)
        if(!editor_json_hitbox_read(yyjson_arr_get(hitboxes, i), &body->hitboxes[i], project))
            return false;
    if(project->next_rigid_body_id <= body->id) project->next_rigid_body_id = body->id + 1;
    return true;
}

static bool editor_json_anchor_read(yyjson_val *value, EditorAnchor *anchor,
    EditorProject *project) {
    if(!yyjson_is_obj(value) || !editor_json_uint(value, "id", &anchor->id) ||
            anchor->id == 0 || !editor_json_name(value, anchor->name) ||
            !editor_json_position_read(yyjson_obj_get(value, "position"), &anchor->position) ||
            !editor_json_real(value, "rotation", &anchor->rotation) ||
            !editor_json_uint(value, "rigid_body", &anchor->rigid_body) ||
            !editor_json_bool(value, "position_follows_body", &anchor->position_follows_body) ||
            !editor_json_bool(value, "rotation_follows_body", &anchor->rotation_follows_body) ||
            !editor_json_bool(value, "visible", &anchor->visible)) return false;
    editor_project_property_name_format(anchor->name, sizeof(anchor->name), anchor->name);
    if(project->next_anchor_id <= anchor->id) project->next_anchor_id = anchor->id + 1;
    return true;
}

static bool editor_json_joint_read(yyjson_val *value, EditorJoint *joint,
    EditorProject *project) {
    uint32_t kind;
    if(!yyjson_is_obj(value) || !editor_json_uint(value, "id", &joint->id) ||
            joint->id == 0 || !editor_json_name(value, joint->name) ||
            !editor_json_uint(value, "kind", &kind) || kind > EDITOR_JOINT_SPRING ||
            !editor_json_uint(value, "anchor_a", &joint->anchor_a) ||
            !editor_json_uint(value, "anchor_b", &joint->anchor_b) ||
            !editor_json_real(value, "rest_length", &joint->rest_length) ||
            !editor_json_real(value, "stiffness", &joint->stiffness) ||
            !editor_json_real(value, "damping", &joint->damping) ||
            !editor_json_real(value, "rest_angle", &joint->rest_angle) ||
            !editor_json_real(value, "visual_size", &joint->visual_size) ||
            !editor_json_bool(value, "visible", &joint->visible)) return false;
    editor_project_property_name_format(joint->name, sizeof(joint->name), joint->name);
    joint->kind = (EditorJointKind)kind;
    if(project->next_joint_id <= joint->id) project->next_joint_id = joint->id + 1;
    return true;
}

static bool editor_json_soft_body_read(yyjson_val *value, EditorSoftBody *body,
    EditorProject *project) {
    yyjson_val *nodes = yyjson_obj_get(value, "nodes");
    yyjson_val *beams = yyjson_obj_get(value, "beams");
    if(!yyjson_is_obj(value) || !editor_json_uint(value, "id", &body->id) || body->id == 0 ||
            !editor_json_name(value, body->name) || !editor_json_position_read(
                yyjson_obj_get(value, "position"), &body->position) ||
            !editor_json_bool(value, "visible", &body->visible) || !yyjson_is_arr(nodes) ||
            !yyjson_is_arr(beams) || yyjson_arr_size(nodes) > EDITOR_SOFT_NODE_MAX ||
            yyjson_arr_size(beams) > EDITOR_SOFT_BEAM_MAX) return false;
    editor_project_property_name_format(body->name, sizeof(body->name), body->name);
    body->node_count = yyjson_arr_size(nodes);
    body->beam_count = yyjson_arr_size(beams);
    for(size_t i = 0; i < body->node_count; i += 1) {
        yyjson_val *item = yyjson_arr_get(nodes, i);
        EditorSoftNode *node = &body->nodes[i];
        yyjson_val *collision_enabled = yyjson_obj_get(item, "collision_enabled");
        *node = (EditorSoftNode){
            .collision_enabled = true,
            .collision_category = UINT64_C(1),
            .collision_with = UINT64_C(1)
        };
        if(!yyjson_is_obj(item) || !editor_json_uint(item, "id", &node->id) || node->id == 0 ||
                !editor_json_name(item, node->name) || !editor_json_position_read(
                    yyjson_obj_get(item, "position"), &node->position) ||
                !editor_json_real(item, "mass", &node->node_mass) ||
                !editor_json_bool(item, "gravity_enabled", &node->gravity_enabled) ||
                !editor_json_bool(item, "visible", &node->visible)) return false;
        if(collision_enabled != NULL &&
                (!editor_json_bool(item, "collision_enabled", &node->collision_enabled) ||
                !editor_json_uint64(item, "collision_category", &node->collision_category) ||
                !editor_json_uint64(item, "collision_with", &node->collision_with))) return false;
        editor_project_property_name_format(node->name, sizeof(node->name), node->name);
        if(project->next_soft_node_id <= node->id) project->next_soft_node_id = node->id + 1;
    }
    for(size_t i = 0; i < body->beam_count; i += 1) {
        yyjson_val *item = yyjson_arr_get(beams, i);
        EditorSoftBeam *beam = &body->beams[i];
        if(!yyjson_is_obj(item) || !editor_json_uint(item, "id", &beam->id) || beam->id == 0 ||
                !editor_json_name(item, beam->name) ||
                !editor_json_uint(item, "node_a", &beam->node_a) ||
                !editor_json_uint(item, "node_b", &beam->node_b) ||
                !editor_json_real(item, "stiffness", &beam->stiffness) ||
                !editor_json_bool(item, "visible", &beam->visible)) return false;
        editor_project_property_name_format(beam->name, sizeof(beam->name), beam->name);
        if(project->next_soft_beam_id <= beam->id) project->next_soft_beam_id = beam->id + 1;
    }
    if(project->next_soft_body_id <= body->id) project->next_soft_body_id = body->id + 1;
    return true;
}

static bool editor_json_references_valid(EditorProject *project) {
    uint64_t valid_masks = project->collision_mask_count == 64 ? UINT64_MAX :
        (UINT64_C(1) << project->collision_mask_count) - 1;
    for(size_t i = 0; i < project->object_count; i += 1) {
        EditorObject *object = &project->objects[i];
        for(size_t j = 0; j < object->rigid_body_count; j += 1) {
            EditorRigidBody *body = &object->rigid_bodies[j];
            if((body->collision_category & ~valid_masks) != 0 ||
                    (body->collision_with & ~valid_masks) != 0) return false;
        }
        for(size_t j = 0; j < object->anchor_count; j += 1)
            if(object->anchors[j].rigid_body != 0 && editor_project_rigid_body_get(
                    object, object->anchors[j].rigid_body) == NULL) return false;
        for(size_t j = 0; j < object->joint_count; j += 1) {
            EditorJoint *joint = &object->joint_items[j];
            if((joint->anchor_a != 0 && editor_project_anchor_get(object, joint->anchor_a) == NULL) ||
                    (joint->anchor_b != 0 && editor_project_anchor_get(
                        object, joint->anchor_b) == NULL)) return false;
        }
        for(size_t j = 0; j < object->soft_body_count; j += 1) {
            EditorSoftBody *body = &object->soft_body_items[j];
            for(size_t k = 0; k < body->node_count; k += 1) {
                EditorSoftNode *node = &body->nodes[k];
                if((node->collision_category & ~valid_masks) != 0 ||
                        (node->collision_with & ~valid_masks) != 0) return false;
            }
            for(size_t k = 0; k < body->beam_count; k += 1) {
                bool found_a = body->beams[k].node_a == 0;
                bool found_b = body->beams[k].node_b == 0;
                for(size_t n = 0; n < body->node_count; n += 1) {
                    found_a = found_a || body->nodes[n].id == body->beams[k].node_a;
                    found_b = found_b || body->nodes[n].id == body->beams[k].node_b;
                }
                if(!found_a || !found_b) return false;
            }
        }
    }
    return project->selected == 0 || editor_project_selected_get(project) != NULL;
}

bool editor_project_load(EditorProject *project, const char *path) {
    static EditorProject loaded;
    yyjson_doc *document;
    yyjson_val *root;
    yyjson_val *objects;
    yyjson_val *collision_masks;
    uint32_t version;
    bool success = false;
    if(project == NULL || path == NULL || path[0] == '\0') return false;
    document = yyjson_read_file(path, 0, NULL, NULL);
    if(document == NULL) return false;
    root = yyjson_doc_get_root(document);
    objects = yyjson_obj_get(root, "objects");
    collision_masks = yyjson_obj_get(root, "collision_masks");
    editor_project_init(&loaded);
    if(!yyjson_is_obj(root) || !editor_json_uint(root, "format_version", &version) ||
            (version == 0 || version > EDITOR_PROJECT_FORMAT_VERSION) ||
            !editor_json_uint(root, "selected", &loaded.selected) || !yyjson_is_arr(objects) ||
            yyjson_arr_size(objects) > EDITOR_OBJECT_MAX ||
            !editor_json_uint(root, "next_object_id", &loaded.next_id) ||
            !editor_json_uint(root, "next_vertex_id", &loaded.next_vertex_id) ||
            !editor_json_uint(root, "next_rigid_body_id", &loaded.next_rigid_body_id) ||
            !editor_json_uint(root, "next_hitbox_id", &loaded.next_hitbox_id) ||
            !editor_json_uint(root, "next_joint_id", &loaded.next_joint_id) ||
            !editor_json_uint(root, "next_anchor_id", &loaded.next_anchor_id) ||
            !editor_json_uint(root, "next_soft_body_id", &loaded.next_soft_body_id) ||
            !editor_json_uint(root, "next_soft_node_id", &loaded.next_soft_node_id) ||
            !editor_json_uint(root, "next_soft_beam_id", &loaded.next_soft_beam_id) ||
            loaded.next_id == 0 || loaded.next_vertex_id == 0 ||
            loaded.next_rigid_body_id == 0 || loaded.next_hitbox_id == 0 ||
            loaded.next_joint_id == 0 || loaded.next_anchor_id == 0 ||
            loaded.next_soft_body_id == 0 || loaded.next_soft_node_id == 0 ||
            loaded.next_soft_beam_id == 0) goto done;
    if(version >= 3) {
        if(!yyjson_is_arr(collision_masks) || yyjson_arr_size(collision_masks) == 0 ||
                yyjson_arr_size(collision_masks) > EDITOR_COLLISION_MASK_MAX) goto done;
        loaded.collision_mask_count = yyjson_arr_size(collision_masks);
        for(size_t i = 0; i < loaded.collision_mask_count; i += 1) {
            yyjson_val *name = yyjson_arr_get(collision_masks, i);
            if(!yyjson_is_str(name) || yyjson_get_len(name) == 0 ||
                    yyjson_get_len(name) >= EDITOR_OBJECT_NAME_MAX) goto done;
            memcpy(loaded.collision_masks[i].name, yyjson_get_str(name),
                yyjson_get_len(name) + 1);
            editor_project_property_name_format(loaded.collision_masks[i].name,
                sizeof(loaded.collision_masks[i].name), loaded.collision_masks[i].name);
        }
    }
    loaded.object_count = yyjson_arr_size(objects);
    for(size_t i = 0; i < loaded.object_count; i += 1) {
        yyjson_val *value = yyjson_arr_get(objects, i);
        EditorObject *object = &loaded.objects[i];
        yyjson_val *bodies = yyjson_obj_get(value, "rigid_bodies");
        yyjson_val *anchors = yyjson_obj_get(value, "anchors");
        yyjson_val *joint_values = yyjson_obj_get(value, "joints");
        yyjson_val *soft_body_values = yyjson_obj_get(value, "soft_bodies");
        if(!yyjson_is_obj(value) || !editor_json_uint(value, "id", &object->id) ||
                object->id == 0 || !editor_json_name(value, object->name) ||
                !editor_json_position_read(yyjson_obj_get(value, "position"), &object->position) ||
                !editor_json_bool(value, "visible", &object->visible) ||
                !yyjson_is_arr(bodies) || yyjson_arr_size(bodies) > EDITOR_RIGID_BODY_MAX ||
                !yyjson_is_arr(anchors) || yyjson_arr_size(anchors) > EDITOR_ANCHOR_MAX ||
                !yyjson_is_arr(joint_values) ||
                    yyjson_arr_size(joint_values) > EDITOR_JOINT_MAX ||
                !yyjson_is_arr(soft_body_values) ||
                    yyjson_arr_size(soft_body_values) > EDITOR_SOFT_BODY_MAX) goto done;
        editor_project_object_name_format(object->name, sizeof(object->name), object->name);
        object->rigid_body_count = yyjson_arr_size(bodies);
        object->anchor_count = yyjson_arr_size(anchors);
        object->joint_count = yyjson_arr_size(joint_values);
        object->soft_body_count = yyjson_arr_size(soft_body_values);
        for(size_t j = 0; j < object->rigid_body_count; j += 1)
            if(!editor_json_body_read(yyjson_arr_get(bodies, j),
                    &object->rigid_bodies[j], &loaded)) goto done;
        for(size_t j = 0; j < object->anchor_count; j += 1)
            if(!editor_json_anchor_read(yyjson_arr_get(anchors, j),
                    &object->anchors[j], &loaded)) goto done;
        for(size_t j = 0; j < object->joint_count; j += 1)
            if(!editor_json_joint_read(yyjson_arr_get(joint_values, j),
                    &object->joint_items[j], &loaded)) goto done;
        for(size_t j = 0; j < object->soft_body_count; j += 1)
            if(!editor_json_soft_body_read(yyjson_arr_get(soft_body_values, j),
                    &object->soft_body_items[j], &loaded)) goto done;
        if(loaded.next_id <= object->id) loaded.next_id = object->id + 1;
    }
    if(!editor_json_references_valid(&loaded)) goto done;
    *project = loaded;
    success = true;
done:
    yyjson_doc_free(document);
    return success;
}
