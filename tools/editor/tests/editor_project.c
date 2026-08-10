#include "editor_project.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool position_equal(Position a, Position b) {
    return fabsf(a.x - b.x) < 0.001f && fabsf(a.y - b.y) < 0.001f;
}

int main(void) {
    static EditorProject project;
    EditorObject *object;
    EditorRigidBody *chassis;
    EditorRigidBody *wheel;
    EditorHitbox *hitbox;
    EditorJoint *joint;
    EditorJoint *joint_two;
    EditorAnchor *manual_anchor;
    EditorAnchor *second_anchor;
    EditorAnchorId manual_anchor_id;
    EditorAnchorId second_anchor_id;
    EditorJointId joint_two_id;
    EditorSoftBody *soft_body;
    EditorSoftNode *node_a;
    EditorSoftNode *node_b;
    EditorSoftBeam *beam;
    Position first;
    Position second;
    char formatted[EDITOR_OBJECT_NAME_MAX];

    editor_project_object_name_format(formatted, sizeof(formatted), "fast car");
    if(strcmp(formatted, "FastCar") != 0) return 1;
    editor_project_object_name_format(formatted, sizeof(formatted), "3d box");
    if(strcmp(formatted, "Object3dBox") != 0) return 1;
    editor_project_property_name_format(formatted, sizeof(formatted), "carBody");
    if(strcmp(formatted, "car_body") != 0) return 1;
    editor_project_property_name_format(formatted, sizeof(formatted), "HTTPServer");
    if(strcmp(formatted, "http_server") != 0) return 1;
    editor_project_property_name_format(formatted, sizeof(formatted), "struct");
    if(strcmp(formatted, "item_struct") != 0) return 1;

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){10.0f, 20.0f});
    if(object == NULL || strcmp(object->name, "Object1") != 0 ||
            !object->visible || project.selected != object->id) return 1;
    chassis = editor_project_rigid_body_add(&project, object);
    wheel = editor_project_rigid_body_add(&project, object);
    if(chassis == NULL || wheel == NULL || chassis->id == wheel->id ||
            !chassis->visible || chassis->hitbox_count != 1 ||
            wheel->hitbox_count != 1 || fabsf(chassis->mass_value - 1.0f) > 0.001f ||
            fabsf(chassis->friction - 0.5f) > 0.001f ||
            fabsf(chassis->restitution) > 0.001f || chassis->static_body ||
            chassis->rotation_locked || chassis->gravity_enabled) return 1;
    hitbox = &chassis->hitboxes[0];
    if(hitbox == NULL || !hitbox->visible || hitbox->vertex_count != 3) return 1;
    editor_project_property_name_format(hitbox->vertices[0].name,
        sizeof(hitbox->vertices[0].name), "front Point");
    editor_project_property_name_format(hitbox->line_names[0],
        sizeof(hitbox->line_names[0]), "upperEdge");
    if(strcmp(hitbox->vertices[0].name, "front_point") != 0 ||
            strcmp(hitbox->line_names[0], "upper_edge") != 0) return 1;
    first = hitbox->vertices[0].position;
    second = hitbox->vertices[1].position;
    if(!editor_project_hitbox_vertex_insert(&project, hitbox, 0) ||
            hitbox->vertex_count != 4 ||
            !position_equal(hitbox->vertices[0].position, first) ||
            !position_equal(hitbox->vertices[1].position, (Position){
                (first.x + second.x) * 0.5f, (first.y + second.y) * 0.5f}) ||
            !position_equal(hitbox->vertices[2].position, second) ||
            strcmp(hitbox->line_names[0], "upper_edge") != 0 ||
            strcmp(hitbox->vertices[1].name, "vertex_4") != 0 ||
            !editor_project_hitbox_line_remove(hitbox, 0) ||
            hitbox->vertex_count != 3 ||
            strcmp(hitbox->line_names[0], "upper_edge") != 0) return 1;

    joint = editor_project_joint_add(&project, object, EDITOR_JOINT_SPRING);
    if(joint == NULL || object->anchor_count != 0 || joint->anchor_a != 0 ||
            joint->anchor_b != 0 || fabsf(joint->rest_length) > 0.001f ||
            fabsf(joint->stiffness - 100.0f) > 0.001f ||
            fabsf(joint->damping - 10.0f) > 0.001f ||
            fabsf(joint->visual_size - 1.0f) > 0.001f) return 1;
    joint_two = editor_project_joint_add(&project, object, EDITOR_JOINT_WELD);
    joint_two_id = joint_two == NULL ? 0 : joint_two->id;
    if(joint_two == NULL || joint_two->anchor_a != 0 || joint_two->anchor_b != 0) return 1;
    manual_anchor = editor_project_anchor_add(&project, object,
        (Position){5.0f, 6.0f}, chassis->id);
    second_anchor = editor_project_anchor_add(&project, object,
        (Position){25.0f, 6.0f}, wheel->id);
    manual_anchor_id = manual_anchor == NULL ? 0 : manual_anchor->id;
    second_anchor_id = second_anchor == NULL ? 0 : second_anchor->id;
    if(manual_anchor == NULL || second_anchor == NULL ||
            !editor_project_joint_anchor_set(object, joint, 0, manual_anchor_id) ||
            !editor_project_joint_anchor_set(object, joint, 1, second_anchor_id) ||
            fabsf(joint->rest_length - 20.0f) > 0.001f ||
            !editor_project_joint_remove(object, joint->id) ||
            editor_project_anchor_get(object, manual_anchor_id) == NULL ||
            editor_project_anchor_get(object, second_anchor_id) == NULL) return 1;
    if(!editor_project_joint_anchor_set(object, joint_two, 0, manual_anchor_id) ||
            !editor_project_joint_anchor_set(object, joint_two, 1, second_anchor_id) ||
            !editor_project_joint_remove(object, joint_two_id) ||
            object->anchor_count != 2 ||
            editor_project_anchor_get(object, manual_anchor_id) == NULL ||
            editor_project_anchor_get(object, second_anchor_id) == NULL) return 1;
    manual_anchor = editor_project_anchor_get(object, manual_anchor_id);
    if(manual_anchor == NULL || !editor_project_anchor_position_lock_set(
            object, manual_anchor, false) || !editor_project_anchor_rotation_lock_set(
            object, manual_anchor, false)) return 1;
    chassis->position = (Position){10.0f, 0.0f};
    chassis->rotation = 1.57079632679f;
    if(!editor_project_anchor_position_lock_set(object, manual_anchor, true) ||
            !position_equal(manual_anchor->position, (Position){6.0f, 5.0f}) ||
            !editor_project_anchor_position_lock_set(object, manual_anchor, false) ||
            !position_equal(manual_anchor->position, (Position){5.0f, 6.0f}) ||
            !editor_project_anchor_rotation_lock_set(object, manual_anchor, true) ||
            fabsf(manual_anchor->rotation + 1.57079632679f) > 0.001f ||
            !editor_project_anchor_rotation_lock_set(object, manual_anchor, false) ||
            fabsf(manual_anchor->rotation) > 0.001f) return 1;
    joint = editor_project_joint_add(&project, object, EDITOR_JOINT_SPRING);
    if(joint == NULL || !editor_project_joint_anchor_set(
            object, joint, 0, manual_anchor_id) ||
            !editor_project_joint_anchor_set(object, joint, 1, second_anchor_id) ||
            !editor_project_rigid_body_remove(object, wheel->id) ||
            object->joint_count != 1 ||
            editor_project_anchor_get(object, second_anchor_id) == NULL ||
            editor_project_anchor_get(object, second_anchor_id)->rigid_body != 0 ||
            !editor_project_joint_remove(object, joint->id)) return 1;

    soft_body = editor_project_soft_body_add(&project, object);
    node_a = editor_project_soft_node_add(&project, soft_body, (Position){0});
    node_b = editor_project_soft_node_add(&project, soft_body, (Position){20.0f, 0.0f});
    if(soft_body == NULL || node_a == NULL || node_b == NULL) return 1;
    if(node_a->gravity_enabled || node_b->gravity_enabled) return 1;
    beam = editor_project_soft_beam_add(&project, soft_body, 0, 0);
    if(beam == NULL || !beam->visible || beam->node_a != 0 || beam->node_b != 0) return 1;
    beam->node_a = node_a->id;
    beam->node_b = node_b->id;
    if(!editor_project_soft_beam_remove(soft_body, beam->id) ||
            soft_body->node_count != 2) return 1;
    beam = editor_project_soft_beam_add(&project, soft_body, node_a->id, node_b->id);
    if(beam == NULL ||
            !editor_project_soft_node_remove(soft_body, node_a->id) ||
            soft_body->beam_count != 1 || beam->node_a != 0 ||
            soft_body->node_count != 1 || beam->node_b != soft_body->nodes[0].id) return 1;

    {
        static EditorProject weld_project;
        EditorObject *weld_object;
        EditorRigidBody *body_a;
        EditorRigidBody *body_b;
        EditorAnchor *anchor_a;
        EditorAnchor *anchor_b;
        EditorJoint *weld;

        editor_project_init(&weld_project);
        weld_object = editor_project_object_add(&weld_project, (Position){0});
        body_a = editor_project_rigid_body_add(&weld_project, weld_object);
        body_b = editor_project_rigid_body_add(&weld_project, weld_object);
        if(body_a == NULL || body_b == NULL) return 1;
        body_a->rotation = 0.25f;
        body_b->rotation = 1.0f;
        anchor_a = editor_project_anchor_add(&weld_project, weld_object,
            (Position){0.0f, 0.0f}, body_a->id);
        anchor_b = editor_project_anchor_add(&weld_project, weld_object,
            (Position){20.0f, 0.0f}, body_b->id);
        weld = editor_project_joint_add(&weld_project, weld_object, EDITOR_JOINT_WELD);
        if(anchor_a == NULL || anchor_b == NULL || weld == NULL ||
                !editor_project_joint_anchor_set(
                    weld_object, weld, 0, anchor_a->id) ||
                !editor_project_joint_anchor_set(
                    weld_object, weld, 1, anchor_b->id) ||
                fabsf(weld->rest_angle - 0.75f) > 0.001f) return 1;
        body_a->rotation = 0.5f;
        editor_project_rigid_body_constraints_apply(weld_object, body_a->id);
        if(fabsf(body_b->rotation - 1.25f) > 0.001f) return 1;
    }

    {
        static EditorProject loaded;
        const char *path = "editor_project_round_trip.json";
        EditorObject *loaded_object;

        if(!editor_project_save(&project, path) ||
                !editor_project_load(&loaded, path)) return 1;
        (void)remove(path);
        loaded_object = editor_project_selected_get(&loaded);
        if(loaded_object == NULL || loaded.object_count != project.object_count ||
                loaded_object->id != object->id ||
                loaded_object->rigid_body_count != object->rigid_body_count ||
                loaded_object->anchor_count != object->anchor_count ||
                loaded_object->joint_count != object->joint_count ||
                loaded_object->soft_body_count != object->soft_body_count ||
                strcmp(loaded_object->name, object->name) != 0 ||
                !position_equal(loaded_object->position, object->position) ||
                loaded.next_id != project.next_id ||
                loaded.next_vertex_id != project.next_vertex_id ||
                loaded.next_rigid_body_id != project.next_rigid_body_id ||
                loaded.next_anchor_id != project.next_anchor_id ||
                loaded.next_soft_node_id != project.next_soft_node_id ||
                loaded.next_soft_beam_id != project.next_soft_beam_id ||
                strcmp(loaded_object->rigid_bodies[0].hitboxes[0].vertices[0].name,
                    "front_point") != 0 ||
                strcmp(loaded_object->rigid_bodies[0].hitboxes[0].line_names[0],
                    "upper_edge") != 0) return 1;
    }

    editor_project_selection_clear(&project);
    if(editor_project_selected_get(&project) != NULL ||
            !editor_project_object_select(&project, object->id) ||
            !editor_project_object_remove(&project, object->id) ||
            project.object_count != 0) return 1;
    return 0;
}
