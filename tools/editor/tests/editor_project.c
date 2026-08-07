#include "editor_project.h"

#include <math.h>

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
    EditorAnchorId generated_a;
    EditorAnchorId manual_anchor_id;
    EditorJointId joint_two_id;
    EditorSoftBody *soft_body;
    EditorSoftNode *node_a;
    EditorSoftNode *node_b;
    EditorSoftBeam *beam;
    Position first;
    Position second;

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){10.0f, 20.0f});
    if(object == NULL || !object->visible || project.selected != object->id) return 1;
    chassis = editor_project_rigid_body_add(&project, object);
    wheel = editor_project_rigid_body_add(&project, object);
    if(chassis == NULL || wheel == NULL || chassis->id == wheel->id ||
            !chassis->visible || chassis->hitbox_count != 1 ||
            wheel->hitbox_count != 1 || fabsf(chassis->mass_value - 1.0f) > 0.001f ||
            fabsf(chassis->friction - 0.5f) > 0.001f ||
            fabsf(chassis->restitution) > 0.001f || chassis->static_body ||
            chassis->rotation_locked) return 1;
    hitbox = &chassis->hitboxes[0];
    if(hitbox == NULL || !hitbox->visible || hitbox->vertex_count != 3) return 1;
    first = hitbox->vertices[0].position;
    second = hitbox->vertices[1].position;
    if(!editor_project_hitbox_vertex_insert(&project, hitbox, 0) ||
            hitbox->vertex_count != 4 ||
            !position_equal(hitbox->vertices[0].position, first) ||
            !position_equal(hitbox->vertices[1].position, (Position){
                (first.x + second.x) * 0.5f, (first.y + second.y) * 0.5f}) ||
            !position_equal(hitbox->vertices[2].position, second) ||
            !editor_project_hitbox_line_remove(hitbox, 0) ||
            hitbox->vertex_count != 3) return 1;

    joint = editor_project_joint_add(&project, object, EDITOR_JOINT_SPRING);
    if(joint == NULL || object->anchor_count != 2 ||
            !editor_project_anchor_get(object, joint->anchor_a)->generated ||
            fabsf(joint->rest_length - 40.0f) > 0.001f ||
            fabsf(joint->stiffness - 100.0f) > 0.001f ||
            fabsf(joint->damping - 10.0f) > 0.001f ||
            fabsf(joint->visual_size - 1.0f) > 0.001f) return 1;
    generated_a = joint->anchor_a;
    joint_two = editor_project_joint_add(&project, object, EDITOR_JOINT_WELD);
    joint_two_id = joint_two == NULL ? 0 : joint_two->id;
    if(joint_two == NULL || !position_equal(
            (Position){chassis->position.x +
                    editor_project_anchor_get(object, joint_two->anchor_a)->position.x,
                chassis->position.y +
                    editor_project_anchor_get(object, joint_two->anchor_a)->position.y},
            (Position){wheel->position.x +
                    editor_project_anchor_get(object, joint_two->anchor_b)->position.x,
                wheel->position.y +
                    editor_project_anchor_get(object, joint_two->anchor_b)->position.y}) ||
            !editor_project_joint_anchor_set(
            object, joint_two, 0, generated_a)) return 1;
    manual_anchor = editor_project_anchor_add(&project, object,
        (Position){5.0f, 6.0f}, chassis->id, false);
    manual_anchor_id = manual_anchor == NULL ? 0 : manual_anchor->id;
    if(manual_anchor == NULL || !editor_project_joint_anchor_set(
            object, joint, 0, manual_anchor_id) ||
            editor_project_anchor_get(object, generated_a) == NULL ||
            !editor_project_joint_anchor_set(
            object, joint_two, 0, manual_anchor_id) ||
            editor_project_anchor_get(object, generated_a) != NULL ||
            !editor_project_joint_remove(object, joint->id) ||
            editor_project_anchor_get(object, manual_anchor_id) == NULL ||
            !editor_project_joint_remove(object, joint_two_id) ||
            object->anchor_count != 1 ||
            editor_project_anchor_get(object, manual_anchor_id) == NULL) return 1;
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
    if(joint == NULL || !editor_project_rigid_body_remove(object, wheel->id) ||
            object->joint_count != 0) return 1;

    soft_body = editor_project_soft_body_add(&project, object);
    node_a = editor_project_soft_node_add(&project, soft_body, (Position){0});
    node_b = editor_project_soft_node_add(&project, soft_body, (Position){20.0f, 0.0f});
    if(soft_body == NULL || node_a == NULL || node_b == NULL) return 1;
    beam = editor_project_soft_beam_add(&project, soft_body, node_a->id, node_b->id);
    if(beam == NULL || !beam->visible ||
            !editor_project_soft_node_remove(soft_body, node_a->id) ||
            soft_body->beam_count != 0) return 1;

    editor_project_selection_clear(&project);
    if(editor_project_selected_get(&project) != NULL ||
            !editor_project_object_select(&project, object->id) ||
            !editor_project_object_remove(&project, object->id) ||
            project.object_count != 0) return 1;
    return 0;
}
