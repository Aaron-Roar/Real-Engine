#include "editor_command.h"
#include "editor_auto_shape.h"
#include "editor_document.h"
#include "editor_object_commands.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

static EditorCommandResult observed_creation;

static bool position_near(Position position, float x, float y) {
    return fabsf(position.x - x) < 0.001f && fabsf(position.y - y) < 0.001f;
}

static int auto_shape_test(void) {
    EditorHitbox hitbox = {0};
    EditorSoftBody soft_body = {0};
    EditorVertex vertices[5] = {0};
    EditorSoftNode nodes[6] = {0};
    EditorVertexId vertex_ids[5] = {11, 12, 13, 14, 15};
    EditorSoftNodeId node_ids[6] = {21, 22, 23, 24, 25, 26};
    EditorAutoShapeConfig rectangle = {
        .kind = EDITOR_AUTO_SHAPE_RECTANGLE, .width = 8.0f, .height = 4.0f
    };
    EditorAutoShapeConfig circle = {
        .kind = EDITOR_AUTO_SHAPE_CIRCLE, .radius = 3.0f
    };
    EditorAutoShapeConfig triangle = {
        .kind = EDITOR_AUTO_SHAPE_TRIANGLE,
        .triangle_kind = EDITOR_AUTO_TRIANGLE_ISOSCELES,
        .width = 6.0f, .height = 4.0f
    };

    hitbox.vertices = vertices;
    hitbox.vertex_count = 5;
    hitbox.vertex_capacity = 5;
    for(size_t i = 0; i < hitbox.vertex_count; i += 1)
        hitbox.vertices[i].id = vertex_ids[i];
    if(editor_result_check(editor_auto_shape_hitbox_apply(&hitbox, &rectangle)) ||
            !position_near(hitbox.vertices[0].position, -4.0f, -2.0f) ||
            !position_near(hitbox.vertices[1].position, 0.0f, -2.0f) ||
            !position_near(hitbox.vertices[2].position, 4.0f, -2.0f) ||
            !position_near(hitbox.vertices[3].position, 4.0f, 2.0f) ||
            !position_near(hitbox.vertices[4].position, -4.0f, 2.0f)) return 1;
    for(size_t i = 0; i < hitbox.vertex_count; i += 1)
        if(hitbox.vertices[i].id != vertex_ids[i]) return 1;

    soft_body.nodes = nodes;
    soft_body.node_count = 6;
    soft_body.node_capacity = 6;
    for(size_t i = 0; i < soft_body.node_count; i += 1)
        soft_body.nodes[i].id = node_ids[i];
    if(editor_result_check(editor_auto_shape_soft_body_apply(&soft_body, &circle)) ||
            !position_near(soft_body.nodes[0].position, 0.0f, -3.0f)) return 1;
    for(size_t i = 0; i < soft_body.node_count; i += 1) {
        float distance = sqrtf(soft_body.nodes[i].position.x *
            soft_body.nodes[i].position.x + soft_body.nodes[i].position.y *
            soft_body.nodes[i].position.y);
        if(fabsf(distance - 3.0f) > 0.001f ||
                soft_body.nodes[i].id != node_ids[i]) return 1;
    }
    {
        EditorVertexId selected_vertices[] = {11, 13, 15};
        EditorSoftNodeId selected_nodes[] = {21, 23, 25, 26};
        hitbox.vertices[1].position = (Position){91.0f, 92.0f};
        hitbox.vertices[3].position = (Position){93.0f, 94.0f};
        if(editor_result_check(editor_auto_shape_hitbox_points_apply(&hitbox,
                    &triangle, selected_vertices, 3)) ||
                !position_near(hitbox.vertices[1].position, 91.0f, 92.0f) ||
                !position_near(hitbox.vertices[3].position, 93.0f, 94.0f) ||
                !position_near(hitbox.vertices[0].position, 0.0f, -2.0f)) return 1;
        soft_body.nodes[1].position = (Position){81.0f, 82.0f};
        soft_body.nodes[3].position = (Position){83.0f, 84.0f};
        if(editor_result_check(editor_auto_shape_soft_body_points_apply(&soft_body,
                    &circle, selected_nodes, 4)) ||
                !position_near(soft_body.nodes[1].position, 81.0f, 82.0f) ||
                !position_near(soft_body.nodes[3].position, 83.0f, 84.0f)) return 1;
    }
    hitbox.vertex_count = 3;
    if(!editor_result_check(editor_auto_shape_hitbox_apply(&hitbox, &rectangle)) ||
            editor_result_check(editor_auto_shape_hitbox_apply(&hitbox, &triangle)) ||
            !position_near(hitbox.vertices[0].position, 0.0f, -2.0f)) return 1;
    triangle.triangle_kind = EDITOR_AUTO_TRIANGLE_EQUILATERAL;
    triangle.width = 4.0f;
    triangle.height = 999.0f;
    if(editor_result_check(editor_auto_shape_hitbox_apply(&hitbox, &triangle)) ||
            !position_near(hitbox.vertices[0].position, 0.0f, -sqrtf(3.0f)) ||
            !position_near(hitbox.vertices[1].position, 2.0f, sqrtf(3.0f)))
        return 1;
    triangle.triangle_kind = EDITOR_AUTO_TRIANGLE_SCALENE;
    triangle.width = 8.0f;
    triangle.height = 6.0f;
    triangle.apex_offset = 1.5f;
    if(editor_result_check(editor_auto_shape_hitbox_apply(&hitbox, &triangle)) ||
            !position_near(hitbox.vertices[0].position, 1.5f, -3.0f) ||
            !position_near(hitbox.vertices[1].position, 4.0f, 3.0f) ||
            !position_near(hitbox.vertices[2].position, -4.0f, 3.0f)) return 1;

    hitbox.vertex_count = 5;
    rectangle.width = 8.0f;
    rectangle.height = 4.0f;
    if(!editor_auto_shape_control_check(&rectangle, 5, 0) ||
            editor_auto_shape_control_check(&rectangle, 5, 1) ||
            !editor_auto_shape_control_check(&rectangle, 5, 2) ||
            editor_result_check(editor_auto_shape_control_set(
                &rectangle, 5, 3, (Position){6.0f, 4.0f})) ||
            !position_near((Position){rectangle.width, rectangle.height},
                12.0f, 8.0f) ||
            editor_result_check(editor_auto_shape_hitbox_apply(
                &hitbox, &rectangle)) ||
            !position_near(hitbox.vertices[3].position, 6.0f, 4.0f)) return 1;

    if(!editor_auto_shape_control_check(&circle, 6, 4) ||
            editor_result_check(editor_auto_shape_control_set(
                &circle, 6, 4, (Position){3.0f, 4.0f})) ||
            fabsf(circle.radius - 5.0f) > 0.001f) return 1;

    triangle.triangle_kind = EDITOR_AUTO_TRIANGLE_SCALENE;
    if(!editor_auto_shape_control_check(&triangle, 5, 0) ||
            editor_auto_shape_control_check(&triangle, 5, 1) ||
            !editor_auto_shape_control_check(&triangle, 5, 2) ||
            editor_auto_shape_control_check(&triangle, 5, 3) ||
            !editor_auto_shape_control_check(&triangle, 5, 4) ||
            editor_result_check(editor_auto_shape_control_set(
                &triangle, 5, 0, (Position){2.0f, -5.0f})) ||
            fabsf(triangle.height - 10.0f) > 0.001f ||
            fabsf(triangle.apex_offset - 2.0f) > 0.001f) return 1;
    return 0;
}

static int auto_shape_command_test(void) {
    static EditorProject project;
    EditorObject *object;
    EditorRigidBody *body;
    EditorHitbox *hitbox;
    EditorSoftBody *soft_body;
    EditorCommand command;
    EditorCommand parsed;
    EditorCommandResult result;
    EditorResult parsed_result;
    const char *path;
    char output[1024];
    char point_ids[3][16];
    char *arguments[] = {"editor-cli", "--soft-body", "cloth",
        "--object", "ShapeObject", "--property", "auto-shape",
        "circle", "isosceles", "100", "100", "25", "0", "points",
        "1", "2", "3"};

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){0});
    body = editor_project_rigid_body_add(&project, object);
    hitbox = editor_project_hitbox_add(&project, body);
    soft_body = editor_project_soft_body_add(&project, object);
    if(object == NULL || body == NULL || hitbox == NULL || soft_body == NULL)
        return 1;
    snprintf(object->name, sizeof(object->name), "ShapeObject");
    snprintf(body->name, sizeof(body->name), "frame");
    snprintf(hitbox->name, sizeof(hitbox->name), "frame_hitbox");
    snprintf(soft_body->name, sizeof(soft_body->name), "cloth");
    for(size_t i = 0; i < 4; i += 1)
        if(editor_project_soft_node_add(&project, soft_body, (Position){0}) == NULL)
            return 1;
    for(size_t i = 0; i < 3; i += 1) {
        snprintf(point_ids[i], sizeof(point_ids[i]), "%u", soft_body->nodes[i].id);
        arguments[14 + i] = point_ids[i];
    }
    command = (EditorCommand){.type = EDITOR_COMMAND_AUTO_SHAPE,
        .data.auto_shape = {.kind = EDITOR_ITEM_HITBOX,
            .object = object->id, .parent = body->id, .item = hitbox->id,
            .config = {.kind = EDITOR_AUTO_SHAPE_CIRCLE, .radius = 12.0f,
                .width = 100.0f, .height = 100.0f,
                .triangle_kind = EDITOR_AUTO_TRIANGLE_ISOSCELES}}};
    command.data.auto_shape.points[0] = hitbox->vertices[0].id;
    command.data.auto_shape.points[1] = hitbox->vertices[1].id;
    command.data.auto_shape.points[2] = hitbox->vertices[2].id;
    command.data.auto_shape.point_count = 3;
    result = editor_command_execute(&project, &command);
    if(result.kind != ERROR_RESULT_VALUE ||
            !position_near(hitbox->vertices[0].position, 0.0f, -12.0f)) return 1;
    parsed_result = editor_command_cli_standard_write(&project, &command, &result,
        "project.rohr.json", output, sizeof(output));
    if(editor_result_check(parsed_result) ||
            strstr(output, "--property auto-shape circle") == NULL ||
            strstr(output, "--hitbox frame_hitbox") == NULL ||
            strstr(output, "points") == NULL) return 1;
    parsed_result = editor_command_cli_standard_parse(&project,
        (int)(sizeof(arguments) / sizeof(arguments[0])), arguments, &path, &parsed);
    if(editor_result_check(parsed_result) || parsed.type != EDITOR_COMMAND_AUTO_SHAPE ||
            parsed.data.auto_shape.kind != EDITOR_ITEM_SOFT_BODY ||
            parsed.data.auto_shape.item != soft_body->id ||
            parsed.data.auto_shape.config.kind != EDITOR_AUTO_SHAPE_CIRCLE ||
            parsed.data.auto_shape.config.radius != 25.0f ||
            parsed.data.auto_shape.point_count != 3) return 1;
    return editor_command_execute(&project, &parsed).kind == ERROR_RESULT_VALUE ? 0 : 1;
}

static void creation_observe(const EditorCommand *command,
        const EditorCommandResult *result, void *context) {
    (void)command;
    (void)context;
    if(result != NULL) observed_creation = *result;
}

static int creation_result_test(void) {
    static EditorProject project;
    EditorObject *object;
    EditorCommand command;
    EditorCommandResult result;
    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){0});
    if(object == NULL) return 1;
    command = (EditorCommand){.type = EDITOR_COMMAND_ITEM_ADD,
        .data.item_add = {.kind = EDITOR_ITEM_RIGID_BODY, .object = object->id}};
    observed_creation = (EditorCommandResult){0};
    editor_command_executed_callback_set(creation_observe, NULL);
    result = editor_command_execute(&project, &command);
    editor_command_executed_callback_set(NULL, NULL);
    if(result.kind != ERROR_RESULT_VALUE || !result.created.valid ||
            result.created.kind != EDITOR_ITEM_RIGID_BODY ||
            result.created.object != object->id || result.created.item == 0 ||
            result.created.name[0] == '\0' || !observed_creation.created.valid ||
            observed_creation.created.item != result.created.item) return 1;
    return 0;
}

static int standard_cli_commands_test(void) {
    static EditorProject project;
    EditorObject *object;
    EditorRigidBody *body;
    EditorCommand command;
    EditorCommand add;
    EditorCommandResult added;
    EditorResult result;
    const char *path;
    char output[1024];
    char *transform[] = {"editor-cli", "--body", "car_body", "--project",
        "project.rohr.json", "--object", "FastCar", "--property", "transform",
        "10", "20", "0.5"};
    char *mass_arguments[] = {"editor-cli", "--property", "mass", "5",
        "--body", "car_body"};
    char *add_body[] = {"editor-cli", "--object", "FastCar", "--body",
        "new_frame", "add"};
    char *position[] = {"editor-cli", "--body", "car_body", "--property",
        "position", "30", "40"};
    char *rotation[] = {"editor-cli", "--body", "car_body", "--property",
        "rotation", "1.25"};
    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){0});
    body = editor_project_rigid_body_add(&project, object);
    if(object == NULL || body == NULL) return 1;
    snprintf(object->name, sizeof(object->name), "FastCar");
    snprintf(body->name, sizeof(body->name), "car_body");
    body->position = (Position){2.0f, 3.0f};
    body->rotation = 0.75f;
    result = editor_command_cli_standard_parse(&project, 12, transform, &path, &command);
    if(editor_result_check(result) || strcmp(path, "project.rohr.json") != 0 ||
            command.type != EDITOR_COMMAND_RIGID_BODY_TRANSFORM ||
            command.data.rigid_body_transform.object != object->id ||
            command.data.rigid_body_transform.body != body->id) return 1;
    result = editor_command_cli_standard_write(&project, &command, NULL,
        "project.rohr.json", output, sizeof(output));
    if(editor_result_check(result) || strstr(output, "rigid-body") != NULL ||
            strstr(output, "transform project") != NULL ||
            strstr(output, "--property transform 10 20 0.5") == NULL) return 1;
    /* --property is terminal: selectors must precede it. */
    result = editor_command_cli_standard_parse(&project, 6, mass_arguments,
        &path, &command);
    if(!editor_result_check(result)) return 1;
    result = editor_command_cli_standard_parse(&project, 7, position,
        &path, &command);
    if(editor_result_check(result) || command.type != EDITOR_COMMAND_RIGID_BODY_TRANSFORM ||
            command.data.rigid_body_transform.position.x != 30.0f ||
            command.data.rigid_body_transform.rotation != 0.75f) return 1;
    result = editor_command_cli_standard_parse(&project, 6, rotation,
        &path, &command);
    if(editor_result_check(result) || command.data.rigid_body_transform.position.x != 2.0f ||
            command.data.rigid_body_transform.rotation != 1.25f) return 1;
    result = editor_command_cli_standard_parse(&project, 6, add_body, &path, &add);
    if(editor_result_check(result) || add.type != EDITOR_COMMAND_ITEM_ADD ||
            strcmp(add.data.item_add.name, "new_frame") != 0) return 1;
    added = editor_command_execute(&project, &add);
    if(added.kind != ERROR_RESULT_VALUE || !added.created.valid ||
            strcmp(added.created.name, "new_frame") != 0) return 1;
    result = editor_command_cli_standard_write(&project, &add, &added,
        "project.rohr.json", output, sizeof(output));
    if(editor_result_check(result) || strstr(output, "editor-cli --project") != output ||
            strstr(output, "--object FastCar") == NULL ||
            strstr(output, "--body new_frame add") == NULL) return 1;
    return 0;
}

static int named_selector_commands_test(void) {
    static EditorProject project;
    EditorObject *object;
    EditorRigidBody *body;
    EditorHitbox *hitbox;
    EditorCommand command;
    EditorResult result;
    const char *path;
    char body_id[16];
    char cli_text[512];
    char *named_arguments[] = {"editor-cli", "rigid-body", "transform",
        "project.rohr.json", "--object", "fast car", "--body", "car body",
        "10", "20", "0.5"};
    char *id_arguments[] = {"editor-cli", "rigid-body", "set",
        "project.rohr.json", "--object", "FastCar", "--body-id", body_id,
        "mass", "5"};
    char *vertex_arguments[] = {"editor-cli", "vertex", "position",
        "project.rohr.json", "--object", "FastCar", "--body", "car_body",
        "--hitbox", "hitbox_1", "--vertex", "vertex_1", "3", "4"};
    char *missing_arguments[] = {"editor-cli", "rigid-body", "transform",
        "project.rohr.json", "--object", "FastCar", "--body", "missing",
        "0", "0", "0"};
    char *global_body_arguments[] = {"editor-cli", "rigid-body", "transform",
        "project.rohr.json", "--body", "car_body", "10", "20", "0.5"};
    char *reordered_arguments[] = {"editor-cli", "rigid-body", "transform",
        "project.rohr.json", "--body", "car_body", "--object", "FastCar",
        "10", "20", "0.5"};
    char *global_vertex_arguments[] = {"editor-cli", "vertex", "position",
        "project.rohr.json", "--vertex", "vertex_1", "3", "4"};

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){0});
    body = editor_project_rigid_body_add(&project, object);
    hitbox = body == NULL ? NULL : &body->hitboxes[0];
    if(object == NULL || body == NULL || hitbox == NULL) return 1;
    snprintf(object->name, sizeof(object->name), "FastCar");
    snprintf(body->name, sizeof(body->name), "car_body");
    snprintf(hitbox->name, sizeof(hitbox->name), "hitbox_1");
    snprintf(hitbox->vertices[0].name, sizeof(hitbox->vertices[0].name), "vertex_1");

    result = editor_command_cli_named_parse(&project, 11, named_arguments,
        &path, &command);
    if(editor_result_check(result) || command.type != EDITOR_COMMAND_RIGID_BODY_TRANSFORM ||
            command.data.rigid_body_transform.object != object->id ||
            command.data.rigid_body_transform.body != body->id) return 1;
    result = editor_command_cli_named_parse(&project, 9, global_body_arguments,
        &path, &command);
    if(editor_result_check(result) || command.data.rigid_body_transform.object != object->id ||
            command.data.rigid_body_transform.body != body->id) return 1;
    result = editor_command_cli_named_parse(&project, 11, reordered_arguments,
        &path, &command);
    if(editor_result_check(result) || command.data.rigid_body_transform.object != object->id ||
            command.data.rigid_body_transform.body != body->id) return 1;
    result = editor_command_cli_named_parse(&project, 8, global_vertex_arguments,
        &path, &command);
    if(editor_result_check(result) || command.data.vertex_position.object != object->id ||
            command.data.vertex_position.body != body->id ||
            command.data.vertex_position.hitbox != hitbox->id ||
            command.data.vertex_position.vertex != hitbox->vertices[0].id) return 1;
    result = editor_command_cli_named_write(&project, &command,
        "project.rohr.json", cli_text, sizeof(cli_text));
    if(editor_result_check(result) || strstr(cli_text, "--object FastCar") == NULL ||
            strstr(cli_text, "--body car_body") == NULL) return 1;

    snprintf(body_id, sizeof(body_id), "%u", body->id);
    result = editor_command_cli_named_parse(&project, 10, id_arguments,
        &path, &command);
    if(editor_result_check(result) || command.type != EDITOR_COMMAND_PROPERTY_SET ||
            command.data.property_set.item != body->id) return 1;

    result = editor_command_cli_named_parse(&project, 14, vertex_arguments,
        &path, &command);
    if(editor_result_check(result) || command.type != EDITOR_COMMAND_VERTEX_POSITION ||
            command.data.vertex_position.vertex != hitbox->vertices[0].id) return 1;
    result = editor_command_cli_named_write(&project, &command,
        "project.rohr.json", cli_text, sizeof(cli_text));
    if(editor_result_check(result) || strstr(cli_text, "--hitbox hitbox_1") == NULL ||
            strstr(cli_text, "--vertex vertex_1") == NULL) return 1;

    result = editor_command_cli_named_parse(&project, 11, missing_arguments,
        &path, &command);
    if(!editor_result_check(result) ||
            result.result.error.code != EDITOR_ERROR_NOT_FOUND) return 1;
    {
        EditorObject *other = editor_project_object_add(&project, (Position){0});
        EditorRigidBody *other_body = editor_project_rigid_body_add(&project, other);
        if(other == NULL || other_body == NULL) return 1;
        snprintf(other->name, sizeof(other->name), "OtherCar");
        snprintf(other_body->name, sizeof(other_body->name), "car_body");
        result = editor_command_cli_named_parse(&project, 9, global_body_arguments,
            &path, &command);
        if(!editor_result_check(result) ||
                result.result.error.code != EDITOR_ERROR_INVALID_ARGUMENT ||
                strstr(result.result.error.message, "--object") == NULL) return 1;
        result = editor_command_cli_named_parse(&project, 11, reordered_arguments,
            &path, &command);
        if(editor_result_check(result) ||
                command.data.rigid_body_transform.object != object->id) return 1;
        if(editor_result_check(editor_object_command_remove(&project, other->id))) return 1;
    }
    command = (EditorCommand){.type = EDITOR_COMMAND_ITEM_RENAME,
        .data.item_rename = {.kind = EDITOR_ITEM_RIGID_BODY,
            .object = object->id, .item = body->id}};
    snprintf(command.data.item_rename.name,
        sizeof(command.data.item_rename.name), "renamed_body");
    result = editor_command_cli_named_write(&project, &command,
        "project.rohr.json", cli_text, sizeof(cli_text));
    if(editor_result_check(result) || strstr(cli_text, "--object FastCar") == NULL ||
            strstr(cli_text, "--body-id ") == NULL) return 1;
    {
        EditorRigidBody *duplicate = editor_project_rigid_body_add(&project, object);
        if(duplicate == NULL) return 1;
        snprintf(duplicate->name, sizeof(duplicate->name), "car_body");
        result = editor_command_cli_named_parse(&project, 11, named_arguments,
            &path, &command);
        if(!editor_result_check(result) ||
                result.result.error.code != EDITOR_ERROR_INVALID_ARGUMENT ||
                strstr(result.result.error.message, "--body-id") == NULL) return 1;
        command = (EditorCommand){.type = EDITOR_COMMAND_NAVIGATION_SET,
            .data.navigation = {.mode = 2, .selection = 2,
                .object = object->id, .rigid_body = body->id}};
        result = editor_command_cli_named_write(&project, &command,
            "project.rohr.json", cli_text, sizeof(cli_text));
        if(editor_result_check(result) || strstr(cli_text, "--object FastCar") == NULL ||
                strstr(cli_text, "--body-id ") == NULL) return 1;
    }
    return 0;
}

static int transform_commands_test(void) {
    static EditorProject project;
    EditorObject *object;
    EditorRigidBody *rigid_body;
    EditorHitbox *hitbox;
    EditorAnchor *anchor;
    EditorJoint *joint;
    EditorSoftBody *soft_body;
    EditorSoftNode *node;
    EditorSoftNode *second_node;
    EditorSoftBeam *beam;
    EditorCommand commands[10];
    EditorCommand parsed;
    EditorResult parse_result;
    const char *parsed_path;
    char cli_text[512];
    char *camera_arguments[] = {"editor-cli", "viewport", "camera",
        "project.rohr.json", "31", "32", "2.5"};
    char *coordinates_arguments[] = {"editor-cli", "viewport", "coordinates",
        "project.rohr.json", "local"};

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){0});
    rigid_body = editor_project_rigid_body_add(&project, object);
    hitbox = rigid_body == NULL ? NULL : &rigid_body->hitboxes[0];
    anchor = editor_project_anchor_add(&project, object, (Position){0}, 0);
    joint = editor_project_joint_add(&project, object, EDITOR_JOINT_SPRING);
    soft_body = editor_project_soft_body_add(&project, object);
    node = editor_project_soft_node_add(&project, soft_body, (Position){0});
    second_node = editor_project_soft_node_add(&project, soft_body, (Position){1.0f, 0.0f});
    beam = editor_project_soft_beam_add(&project, soft_body,
        node == NULL ? 0 : node->id, second_node == NULL ? 0 : second_node->id);
    if(object == NULL || rigid_body == NULL || hitbox == NULL || anchor == NULL ||
            joint == NULL || soft_body == NULL || node == NULL || second_node == NULL ||
            beam == NULL) return 1;
    commands[0] = (EditorCommand){.type = EDITOR_COMMAND_OBJECT_POSITION,
        .data.object_position = {object->id, {1.0f, 2.0f}}};
    commands[1] = (EditorCommand){.type = EDITOR_COMMAND_RIGID_BODY_TRANSFORM,
        .data.rigid_body_transform = {object->id, rigid_body->id,
            {3.0f, 4.0f}, 0.5f}};
    commands[2] = (EditorCommand){.type = EDITOR_COMMAND_VERTEX_POSITION,
        .data.vertex_position = {object->id, rigid_body->id, hitbox->id,
            hitbox->vertices[0].id, {5.0f, 6.0f}}};
    commands[3] = (EditorCommand){.type = EDITOR_COMMAND_ANCHOR_TRANSFORM,
        .data.anchor_transform = {object->id, anchor->id, {7.0f, 8.0f}, 0.75f}};
    commands[4] = (EditorCommand){.type = EDITOR_COMMAND_SOFT_BODY_TRANSFORM,
        .data.soft_body_transform = {object->id, soft_body->id,
            {9.0f, 10.0f}, 1.0f}};
    commands[5] = (EditorCommand){.type = EDITOR_COMMAND_SOFT_NODE_POSITION,
        .data.soft_node_position = {object->id, soft_body->id, node->id,
            {11.0f, 12.0f}}};
    commands[6] = (EditorCommand){.type = EDITOR_COMMAND_RIGID_BODY_ORIGIN,
        .data.origin = {object->id, rigid_body->id, {2.0f, 3.0f}}};
    commands[7] = (EditorCommand){.type = EDITOR_COMMAND_SOFT_BODY_ORIGIN,
        .data.origin = {object->id, soft_body->id, {4.0f, 5.0f}}};
    commands[8] = (EditorCommand){.type = EDITOR_COMMAND_VIEWPORT_CAMERA,
        .data.viewport_camera = {{13.0f, 14.0f}, 2.0f}};
    commands[9] = (EditorCommand){.type = EDITOR_COMMAND_VIEWPORT_COORDINATES,
        .data.viewport_coordinates.local = true};
    for(size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i += 1) {
        if(editor_command_execute(&project, &commands[i]).kind != ERROR_RESULT_VALUE ||
                editor_result_check(editor_command_cli_write(&commands[i],
                    "project.rohr.json", cli_text, sizeof(cli_text))) ||
                strstr(cli_text, "editor-cli ") != cli_text) return 1;
    }
    if(object->position.x != 1.0f || rigid_body->rotation != 0.5f ||
            anchor->rotation != 0.75f || node->position.x == 0.0f ||
            project.viewport_camera_offset.x != 13.0f ||
            project.viewport_camera_zoom != 2.0f || !project.viewport_local_view) return 1;
    parse_result = editor_command_cli_parse(7, camera_arguments,
        &parsed_path, &parsed);
    if(editor_result_check(parse_result) ||
            parsed.type != EDITOR_COMMAND_VIEWPORT_CAMERA ||
            parsed.data.viewport_camera.offset.x != 31.0f ||
            parsed.data.viewport_camera.zoom != 2.5f) return 1;
    parse_result = editor_command_cli_parse(5, coordinates_arguments,
        &parsed_path, &parsed);
    if(editor_result_check(parse_result) ||
            parsed.type != EDITOR_COMMAND_VIEWPORT_COORDINATES ||
            !parsed.data.viewport_coordinates.local) return 1;
    {
        EditorCommand navigation = {.type = EDITOR_COMMAND_NAVIGATION_SET,
            .data.navigation = {.mode = 3, .selection = 3, .object = object->id,
                .rigid_body = rigid_body->id, .hitbox = hitbox->id}};
        char *navigation_arguments[] = {"editor-cli", "navigation", "set",
            "project.rohr.json", "hitbox", "hitbox", "1", "1", "1", "0",
            "0", "0", "0", "0", "0", "0", "none"};
        char *named_navigation_arguments[] = {"editor-cli", "navigation", "set",
            "project.rohr.json", "hitbox", "hitbox", "none",
            "--object", object->name, "--body", rigid_body->name,
            "--hitbox", hitbox->name};
        if(editor_command_execute(&project, &navigation).kind != ERROR_RESULT_VALUE ||
                project.navigation.mode != 3 || project.selected != object->id ||
                editor_result_check(editor_command_cli_write(&navigation,
                    "project.rohr.json", cli_text, sizeof(cli_text))) ||
                editor_result_check(editor_command_cli_parse(17, navigation_arguments,
                    &parsed_path, &parsed)) ||
                parsed.type != EDITOR_COMMAND_NAVIGATION_SET ||
                parsed.data.navigation.mode != 3 ||
                parsed.data.navigation.hitbox != 1) return 1;
        if(editor_result_check(editor_command_cli_named_write(&project, &navigation,
                    "project.rohr.json", cli_text, sizeof(cli_text))) ||
                strstr(cli_text, "--object ") == NULL ||
                strstr(cli_text, "--body ") == NULL ||
                strstr(cli_text, "--hitbox ") == NULL ||
                editor_result_check(editor_command_cli_named_parse(&project, 13,
                    named_navigation_arguments, &parsed_path, &parsed)) ||
                parsed.data.navigation.object != object->id ||
                parsed.data.navigation.rigid_body != rigid_body->id ||
                parsed.data.navigation.hitbox != hitbox->id) return 1;
    }
    {
        EditorCommand visibility[] = {
            {.type = EDITOR_COMMAND_VISIBILITY,
                .data.visibility = {EDITOR_VISIBILITY_OBJECT, object->id, 0, 0, false}},
            {.type = EDITOR_COMMAND_VISIBILITY,
                .data.visibility = {EDITOR_VISIBILITY_RIGID_BODY, object->id, 0,
                    rigid_body->id, false}},
            {.type = EDITOR_COMMAND_VISIBILITY,
                .data.visibility = {EDITOR_VISIBILITY_HITBOX, object->id,
                    rigid_body->id, hitbox->id, false}},
            {.type = EDITOR_COMMAND_VISIBILITY,
                .data.visibility = {EDITOR_VISIBILITY_JOINT, object->id, 0,
                    joint->id, false}},
            {.type = EDITOR_COMMAND_VISIBILITY,
                .data.visibility = {EDITOR_VISIBILITY_ANCHOR, object->id, 0,
                    anchor->id, false}},
            {.type = EDITOR_COMMAND_VISIBILITY,
                .data.visibility = {EDITOR_VISIBILITY_SOFT_BODY, object->id, 0,
                    soft_body->id, false}},
            {.type = EDITOR_COMMAND_VISIBILITY,
                .data.visibility = {EDITOR_VISIBILITY_SOFT_NODE, object->id,
                    soft_body->id, node->id, false}},
            {.type = EDITOR_COMMAND_VISIBILITY,
                .data.visibility = {EDITOR_VISIBILITY_SOFT_BEAM, object->id,
                    soft_body->id, beam->id, false}}
        };
        for(size_t i = 0; i < sizeof(visibility) / sizeof(visibility[0]); i += 1) {
            if(editor_command_execute(&project, &visibility[i]).kind != ERROR_RESULT_VALUE ||
                    editor_result_check(editor_command_cli_write(&visibility[i],
                        "project.rohr.json", cli_text, sizeof(cli_text)))) return 1;
        }
        if(object->visible || rigid_body->visible || hitbox->visible || joint->visible ||
                anchor->visible || soft_body->visible || node->visible || beam->visible)
            return 1;
    }
    return 0;
}

static int item_commands_test(void) {
    static EditorProject project;
    EditorCommand command;
    EditorCommandResult result;
    EditorObject *object;
    EditorRigidBody *rigid_body;
    EditorSoftBody *soft_body;
    uint32_t rigid_body_id;
    uint32_t hitbox_id;
    uint32_t anchor_id;
    uint32_t joint_id;
    uint32_t soft_body_id;
    uint32_t node_a;
    uint32_t node_b;
    uint32_t beam_id;
    char cli_text[512];
    const char *path;
    char *rename_arguments[] = {"editor-cli", "soft-node", "rename",
        "project.rohr.json", "1", "1", "1", "renamed node"};

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){0});
    if(object == NULL) return 1;
#define ITEM_ADD(value) do { \
    command = (value); \
    result = editor_command_execute(&project, &command); \
    if(result.kind != ERROR_RESULT_VALUE || editor_result_check( \
            editor_command_cli_write(&command, "project.rohr.json", cli_text, \
                sizeof(cli_text)))) return 1; \
} while(0)
    ITEM_ADD(((EditorCommand){.type = EDITOR_COMMAND_ITEM_ADD,
        .data.item_add = {.kind = EDITOR_ITEM_RIGID_BODY, .object = object->id}}));
    rigid_body_id = result.result.object;
    rigid_body = editor_project_rigid_body_get(object, rigid_body_id);
    if(rigid_body == NULL) return 1;
    ITEM_ADD(((EditorCommand){.type = EDITOR_COMMAND_ITEM_ADD,
        .data.item_add = {.kind = EDITOR_ITEM_HITBOX, .object = object->id,
            .parent = rigid_body_id}}));
    hitbox_id = result.result.object;
    ITEM_ADD(((EditorCommand){.type = EDITOR_COMMAND_ITEM_ADD,
        .data.item_add = {.kind = EDITOR_ITEM_ANCHOR, .object = object->id,
            .parent = rigid_body_id, .position = {2.0f, 3.0f}}}));
    anchor_id = result.result.object;
    ITEM_ADD(((EditorCommand){.type = EDITOR_COMMAND_ITEM_ADD,
        .data.item_add = {.kind = EDITOR_ITEM_JOINT, .object = object->id,
            .option = EDITOR_JOINT_SPRING}}));
    joint_id = result.result.object;
    ITEM_ADD(((EditorCommand){.type = EDITOR_COMMAND_ITEM_ADD,
        .data.item_add = {.kind = EDITOR_ITEM_SOFT_BODY, .object = object->id}}));
    soft_body_id = result.result.object;
    soft_body = NULL;
    for(size_t i = 0; i < object->soft_body_count; i += 1)
        if(object->soft_body_items[i].id == soft_body_id)
            soft_body = &object->soft_body_items[i];
    if(soft_body == NULL) return 1;
    ITEM_ADD(((EditorCommand){.type = EDITOR_COMMAND_ITEM_ADD,
        .data.item_add = {.kind = EDITOR_ITEM_SOFT_NODE, .object = object->id,
            .parent = soft_body_id}}));
    node_a = result.result.object;
    ITEM_ADD(((EditorCommand){.type = EDITOR_COMMAND_ITEM_ADD,
        .data.item_add = {.kind = EDITOR_ITEM_SOFT_NODE, .object = object->id,
            .parent = soft_body_id, .position = {1.0f, 0.0f}}}));
    node_b = result.result.object;
    ITEM_ADD(((EditorCommand){.type = EDITOR_COMMAND_ITEM_ADD,
        .data.item_add = {.kind = EDITOR_ITEM_SOFT_BEAM, .object = object->id,
            .parent = soft_body_id, .first = node_a, .second = node_b}}));
    beam_id = result.result.object;
#undef ITEM_ADD
    command = (EditorCommand){.type = EDITOR_COMMAND_ITEM_RENAME,
        .data.item_rename = {.kind = EDITOR_ITEM_SOFT_NODE, .object = object->id,
            .parent = soft_body_id, .item = node_a}};
    snprintf(command.data.item_rename.name, sizeof(command.data.item_rename.name),
        "%s", "renamed node");
    if(editor_command_execute(&project, &command).kind != ERROR_RESULT_VALUE ||
            strcmp(soft_body->nodes[0].name, "renamed_node") != 0 ||
            editor_result_check(editor_command_cli_parse(8, rename_arguments,
                &path, &command)) || command.type != EDITOR_COMMAND_ITEM_RENAME)
        return 1;
    {
        EditorItemRemoveCommand removals[] = {
            {EDITOR_ITEM_SOFT_BEAM, object->id, soft_body_id, beam_id, 0},
            {EDITOR_ITEM_SOFT_NODE, object->id, soft_body_id, node_b, 0},
            {EDITOR_ITEM_SOFT_BODY, object->id, 0, soft_body_id, 0},
            {EDITOR_ITEM_JOINT, object->id, 0, joint_id, 0},
            {EDITOR_ITEM_ANCHOR, object->id, 0, anchor_id, 0},
            {EDITOR_ITEM_HITBOX, object->id, rigid_body_id, hitbox_id, 0},
            {EDITOR_ITEM_RIGID_BODY, object->id, 0, rigid_body_id, 0}
        };
        for(size_t i = 0; i < sizeof(removals) / sizeof(removals[0]); i += 1) {
            command = (EditorCommand){.type = EDITOR_COMMAND_ITEM_REMOVE,
                .data.item_remove = removals[i]};
            if(editor_command_execute(&project, &command).kind != ERROR_RESULT_VALUE ||
                    editor_result_check(editor_command_cli_write(&command,
                        "project.rohr.json", cli_text, sizeof(cli_text)))) return 1;
        }
    }
    return 0;
}

static int property_commands_test(void) {
    static EditorProject project;
    EditorObject *object;
    EditorRigidBody *rigid_body;
    EditorHitbox *hitbox;
    EditorJoint *joint;
    EditorAnchor *anchor;
    EditorSoftBody *soft_body;
    EditorSoftNode *node;
    EditorSoftNode *second_node;
    EditorSoftBeam *beam;
    EditorCommand command;
    EditorCommand parsed;
    const char *path;
    char cli_text[512];
    char *node_arguments[] = {"editor-cli", "soft-node", "set",
        "project.rohr.json", "1", "1", "1", "friction", "0.75"};
    char *joint_arguments[] = {"editor-cli", "joint", "set",
        "project.rohr.json", "1", "1", "kind", "weld"};
    char *color_arguments[] = {"editor-cli", "rigid-body", "set",
        "project.rohr.json", "1", "1", "surface-color", "#336699CC"};
    char *named_color_arguments[] = {"editor-cli", "--project", "project.rohr.json",
        "--body", "rigid_body_1", "--property", "outline-color", "#11223344"};

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){0});
    rigid_body = editor_project_rigid_body_add(&project, object);
    hitbox = rigid_body == NULL ? NULL : &rigid_body->hitboxes[0];
    joint = editor_project_joint_add(&project, object, EDITOR_JOINT_SPRING);
    anchor = editor_project_anchor_add(&project, object, (Position){1.0f, 2.0f},
        rigid_body == NULL ? 0 : rigid_body->id);
    soft_body = editor_project_soft_body_add(&project, object);
    node = editor_project_soft_node_add(&project, soft_body, (Position){0});
    second_node = editor_project_soft_node_add(&project, soft_body,
        (Position){2.0f, 0.0f});
    beam = editor_project_soft_beam_add(&project, soft_body,
        node == NULL ? 0 : node->id, second_node == NULL ? 0 : second_node->id);
    if(object == NULL || rigid_body == NULL || hitbox == NULL || joint == NULL ||
            anchor == NULL || soft_body == NULL || node == NULL ||
            second_node == NULL || beam == NULL) return 1;
#define PROPERTY_SET(target_kind, target_parent, target_item, target_index, \
        property_kind, property_value_kind, member, property_value) do { \
    command = (EditorCommand){.type = EDITOR_COMMAND_PROPERTY_SET, \
        .data.property_set = {.kind = target_kind, .object = object->id, \
            .parent = target_parent, .item = target_item, .index = target_index, \
            .property = property_kind, .value_kind = property_value_kind}}; \
    command.data.property_set.value.member = property_value; \
    if(editor_command_execute(&project, &command).kind != ERROR_RESULT_VALUE || \
            editor_result_check(editor_command_cli_write(&command, \
                "project.rohr.json", cli_text, sizeof(cli_text)))) return 1; \
} while(0)
    PROPERTY_SET(EDITOR_ITEM_RIGID_BODY, 0, rigid_body->id, 0,
        EDITOR_PROPERTY_MASS, EDITOR_PROPERTY_VALUE_FLOAT, number, 5.0f);
    PROPERTY_SET(EDITOR_ITEM_RIGID_BODY, 0, rigid_body->id, 0,
        EDITOR_PROPERTY_COLLISION, EDITOR_PROPERTY_VALUE_BOOL, boolean, true);
    PROPERTY_SET(EDITOR_ITEM_RIGID_BODY, 0, rigid_body->id, 0,
        EDITOR_PROPERTY_PARTICLE, EDITOR_PROPERTY_VALUE_BOOL, boolean, true);
    PROPERTY_SET(EDITOR_ITEM_RIGID_BODY, 0, rigid_body->id, 0,
        EDITOR_PROPERTY_PARTICLE_RADIUS, EDITOR_PROPERTY_VALUE_FLOAT,
        number, 14.0f);
    PROPERTY_SET(EDITOR_ITEM_RIGID_BODY, 0, rigid_body->id, 0,
        EDITOR_PROPERTY_PARTICLE_AUTO_FIT, EDITOR_PROPERTY_VALUE_BOOL,
        boolean, false);
    PROPERTY_SET(EDITOR_ITEM_RIGID_BODY, 0, rigid_body->id, 0,
        EDITOR_PROPERTY_OUTLINE_COLOR, EDITOR_PROPERTY_VALUE_UINT, integer,
        UINT32_C(0x11223344));
    PROPERTY_SET(EDITOR_ITEM_VERTEX, rigid_body->id, hitbox->id,
        hitbox->vertices[0].id, EDITOR_PROPERTY_POSITION_LOCKED,
        EDITOR_PROPERTY_VALUE_BOOL, boolean, true);
    PROPERTY_SET(EDITOR_ITEM_LINE, rigid_body->id, hitbox->id, 0,
        EDITOR_PROPERTY_LINE_LENGTH, EDITOR_PROPERTY_VALUE_FLOAT, number, 3.0f);
    PROPERTY_SET(EDITOR_ITEM_JOINT, 0, joint->id, 0,
        EDITOR_PROPERTY_STIFFNESS, EDITOR_PROPERTY_VALUE_FLOAT, number, 12.0f);
    PROPERTY_SET(EDITOR_ITEM_JOINT, 0, joint->id, 0,
        EDITOR_PROPERTY_JOINT_KIND, EDITOR_PROPERTY_VALUE_UINT, integer,
        EDITOR_JOINT_WELD);
    PROPERTY_SET(EDITOR_ITEM_ANCHOR, 0, anchor->id, 0,
        EDITOR_PROPERTY_POSITION_FOLLOWS_BODY, EDITOR_PROPERTY_VALUE_BOOL,
        boolean, false);
    PROPERTY_SET(EDITOR_ITEM_SOFT_NODE, soft_body->id, node->id, 0,
        EDITOR_PROPERTY_FRICTION, EDITOR_PROPERTY_VALUE_FLOAT, number, 0.75f);
    PROPERTY_SET(EDITOR_ITEM_SOFT_NODE, soft_body->id, node->id, 0,
        EDITOR_PROPERTY_NODE_RADIUS, EDITOR_PROPERTY_VALUE_FLOAT, number, 9.0f);
    PROPERTY_SET(EDITOR_ITEM_SOFT_BEAM, soft_body->id, beam->id, 0,
        EDITOR_PROPERTY_DAMPING, EDITOR_PROPERTY_VALUE_FLOAT, number, 0.25f);
#undef PROPERTY_SET
    if(rigid_body->mass_value != 5.0f || !rigid_body->particle ||
            rigid_body->particle_radius != 14.0f || rigid_body->particle_auto_fit ||
            rigid_body->border_color != UINT32_C(0x11223344) ||
            !hitbox->vertices[0].position_locked || joint->kind != EDITOR_JOINT_WELD ||
            joint->stiffness != 12.0f || anchor->position_follows_body ||
            node->friction != 0.75f || node->radius != 9.0f ||
            beam->damping != 0.25f) return 1;
    if(editor_result_check(editor_command_cli_parse(9, node_arguments,
                &path, &parsed)) || parsed.type != EDITOR_COMMAND_PROPERTY_SET ||
            parsed.data.property_set.property != EDITOR_PROPERTY_FRICTION ||
            parsed.data.property_set.value.number != 0.75f ||
            editor_result_check(editor_command_cli_parse(8, joint_arguments,
                &path, &parsed)) || parsed.data.property_set.value.integer !=
                EDITOR_JOINT_WELD ||
            editor_result_check(editor_command_cli_parse(8, color_arguments,
                &path, &parsed)) || parsed.data.property_set.property !=
                EDITOR_PROPERTY_SURFACE_COLOR || parsed.data.property_set.value.integer !=
                UINT32_C(0x336699cc) ||
            editor_result_check(editor_command_cli_standard_parse(&project, 8,
                named_color_arguments, &path, &parsed)) ||
            parsed.data.property_set.property != EDITOR_PROPERTY_OUTLINE_COLOR ||
            parsed.data.property_set.value.integer != UINT32_C(0x11223344)) return 1;
    command = (EditorCommand){.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = {.kind = EDITOR_ITEM_RIGID_BODY,
            .object = object->id, .item = rigid_body->id,
            .property = EDITOR_PROPERTY_RESTITUTION,
            .value_kind = EDITOR_PROPERTY_VALUE_FLOAT,
            .value.number = 2.0f}};
    {
        float restitution = rigid_body->restitution;
        if(editor_command_execute(&project, &command).kind != ERROR_RESULT_ERROR ||
                rigid_body->restitution != restitution) return 1;
    }
    return 0;
}

static int relationship_commands_test(void) {
    static EditorProject project;
    EditorObject *object;
    EditorRigidBody *body;
    EditorAnchor *anchor_a;
    EditorAnchor *anchor_b;
    EditorJoint *joint;
    EditorSoftBody *soft_body;
    EditorSoftNode *node_a;
    EditorSoftNode *node_b;
    EditorSoftBeam *beam;
    EditorCommand command;
    EditorCommand parsed;
    const char *path;
    char cli_text[512];
    char *joint_arguments[] = {"editor-cli", "joint", "connect",
        "project.rohr.json", "1", "1", "anchor-b", "none"};
    char *beam_arguments[] = {"editor-cli", "soft-beam", "connect",
        "project.rohr.json", "1", "1", "1", "node-a", "2"};

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){0});
    body = editor_project_rigid_body_add(&project, object);
    anchor_a = editor_project_anchor_add(&project, object, (Position){0}, 0);
    anchor_b = editor_project_anchor_add(&project, object, (Position){1.0f, 0.0f}, 0);
    joint = editor_project_joint_add(&project, object, EDITOR_JOINT_SPRING);
    soft_body = editor_project_soft_body_add(&project, object);
    node_a = editor_project_soft_node_add(&project, soft_body, (Position){0});
    node_b = editor_project_soft_node_add(&project, soft_body, (Position){1.0f, 0.0f});
    beam = editor_project_soft_beam_add(&project, soft_body, 0, 0);
    if(object == NULL || body == NULL || anchor_a == NULL || anchor_b == NULL ||
            joint == NULL || soft_body == NULL || node_a == NULL || node_b == NULL ||
            beam == NULL) return 1;
#define RELATIONSHIP_SET(relation_kind, relation_parent, relation_item, \
        relation_endpoint, relation_target) do { \
    command = (EditorCommand){.type = EDITOR_COMMAND_RELATIONSHIP_SET, \
        .data.relationship_set = {relation_kind, object->id, relation_parent, \
            relation_item, relation_endpoint, relation_target}}; \
    if(editor_command_execute(&project, &command).kind != ERROR_RESULT_VALUE || \
            editor_result_check(editor_command_cli_write(&command, \
                "project.rohr.json", cli_text, sizeof(cli_text)))) return 1; \
} while(0)
    RELATIONSHIP_SET(EDITOR_RELATIONSHIP_JOINT_ANCHOR, 0, joint->id, 0,
        anchor_a->id);
    RELATIONSHIP_SET(EDITOR_RELATIONSHIP_JOINT_ANCHOR, 0, joint->id, 1,
        anchor_b->id);
    RELATIONSHIP_SET(EDITOR_RELATIONSHIP_ANCHOR_RIGID_BODY, 0, anchor_a->id, 0,
        body->id);
    RELATIONSHIP_SET(EDITOR_RELATIONSHIP_SOFT_BEAM_NODE, soft_body->id, beam->id, 0,
        node_a->id);
    RELATIONSHIP_SET(EDITOR_RELATIONSHIP_SOFT_BEAM_NODE, soft_body->id, beam->id, 1,
        node_b->id);
#undef RELATIONSHIP_SET
    if(joint->anchor_a != anchor_a->id || joint->anchor_b != anchor_b->id ||
            anchor_a->rigid_body != body->id || beam->node_a != node_a->id ||
            beam->node_b != node_b->id) return 1;
    if(editor_result_check(editor_command_cli_parse(8, joint_arguments, &path,
                &parsed)) || parsed.type != EDITOR_COMMAND_RELATIONSHIP_SET ||
            parsed.data.relationship_set.endpoint != 1 ||
            parsed.data.relationship_set.target != 0 ||
            editor_result_check(editor_command_cli_parse(9, beam_arguments, &path,
                &parsed)) || parsed.data.relationship_set.kind !=
                EDITOR_RELATIONSHIP_SOFT_BEAM_NODE ||
            parsed.data.relationship_set.endpoint != 0 ||
            parsed.data.relationship_set.target != 2) return 1;
    command = (EditorCommand){.type = EDITOR_COMMAND_RELATIONSHIP_SET,
        .data.relationship_set = {EDITOR_RELATIONSHIP_SOFT_BEAM_NODE,
            object->id, soft_body->id, beam->id, 0, UINT32_MAX}};
    if(editor_command_execute(&project, &command).kind != ERROR_RESULT_ERROR) return 1;
    return 0;
}

static int collision_filter_commands_test(void) {
    static EditorProject project;
    EditorObject *object;
    EditorRigidBody *body;
    EditorSoftBody *soft_body;
    EditorSoftNode *node;
    EditorCommand command;
    EditorCommand parsed;
    EditorCommandResult result;
    const char *path;
    char cli_text[512];
    char *add_arguments[] = {"editor-cli", "collision-mask", "add",
        "project.rohr.json", "Player Body"};
    char *node_arguments[] = {"editor-cli", "soft-node", "filter",
        "project.rohr.json", "1", "1", "1", "collide-with", "player_body",
        "true"};

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){0});
    body = editor_project_rigid_body_add(&project, object);
    soft_body = editor_project_soft_body_add(&project, object);
    node = editor_project_soft_node_add(&project, soft_body, (Position){0});
    if(object == NULL || body == NULL || soft_body == NULL || node == NULL) return 1;
    command = (EditorCommand){.type = EDITOR_COMMAND_COLLISION_MASK_ADD};
    snprintf(command.data.collision_mask_add.name,
        sizeof(command.data.collision_mask_add.name), "%s", "Player Body");
    result = editor_command_execute(&project, &command);
    if(result.kind != ERROR_RESULT_VALUE || result.result.object != 1 ||
            project.collision_mask_count != 2 ||
            strcmp(project.collision_masks[1].name, "player_body") != 0 ||
            editor_result_check(editor_command_cli_write(&command,
                "project.rohr.json", cli_text, sizeof(cli_text)))) return 1;
    result = editor_command_execute(&project, &command);
    if(result.kind != ERROR_RESULT_VALUE || result.result.object != 1 ||
            project.collision_mask_count != 2) return 1;
    command = (EditorCommand){.type = EDITOR_COMMAND_COLLISION_FILTER_SET,
        .data.collision_filter_set = {.kind = EDITOR_ITEM_RIGID_BODY,
            .object = object->id, .item = body->id,
            .filter = EDITOR_COLLISION_FILTER_CATEGORY,
            .mask = "player_body", .enabled = true}};
    if(editor_command_execute(&project, &command).kind != ERROR_RESULT_VALUE ||
            (body->collision_category & (UINT64_C(1) << 1)) == 0 ||
            editor_result_check(editor_command_cli_write(&command,
                "project.rohr.json", cli_text, sizeof(cli_text)))) return 1;
    command = (EditorCommand){.type = EDITOR_COMMAND_COLLISION_FILTER_SET,
        .data.collision_filter_set = {.kind = EDITOR_ITEM_SOFT_NODE,
            .object = object->id, .parent = soft_body->id, .item = node->id,
            .filter = EDITOR_COLLISION_FILTER_COLLIDE_WITH,
            .mask = "player_body", .enabled = true}};
    if(editor_command_execute(&project, &command).kind != ERROR_RESULT_VALUE ||
            (node->collision_with & (UINT64_C(1) << 1)) == 0) return 1;
    if(editor_result_check(editor_command_cli_parse(5, add_arguments, &path,
                &parsed)) || parsed.type != EDITOR_COMMAND_COLLISION_MASK_ADD ||
            editor_result_check(editor_command_cli_parse(10, node_arguments, &path,
                &parsed)) || parsed.type != EDITOR_COMMAND_COLLISION_FILTER_SET ||
            parsed.data.collision_filter_set.kind != EDITOR_ITEM_SOFT_NODE ||
            parsed.data.collision_filter_set.filter !=
                EDITOR_COLLISION_FILTER_COLLIDE_WITH ||
            !parsed.data.collision_filter_set.enabled) return 1;
    snprintf(command.data.collision_filter_set.mask,
        sizeof(command.data.collision_filter_set.mask), "%s", "missing");
    if(editor_command_execute(&project, &command).kind != ERROR_RESULT_ERROR) return 1;
    return 0;
}

static int sprite_commands_test(void) {
    static EditorProject project;
    EditorObject *object;
    EditorRigidBody *body;
    EditorSprite *asset;
    EditorAnimatedSprite *animated;
    EditorCommand command;
    EditorCommand parsed;
    EditorCommandResult executed;
    EditorResult result;
    const char *path;
    char text[2048];
    char *arguments[32];
    int count = 0;

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){0});
    body = editor_project_rigid_body_add(&project, object);
    asset = editor_project_sprite_add(&project, object, "wheel",
        "assets/wheel image.png");
    animated = editor_project_animated_sprite_add(&project, object);
    if(object == NULL || body == NULL || asset == NULL || animated == NULL) return 1;
    asset->size = (Scale){32.0f, 24.0f};
    snprintf(animated->name, sizeof(animated->name), "%s", "rolling");

    command = (EditorCommand){.type = EDITOR_COMMAND_SPRITE_ADD,
        .data.sprite_add = {.object = object->id, .name = "second_frame",
            .path = "assets/second.png", .size = {16.0f, 20.0f}}};
    executed = (EditorCommandResult){.kind = ERROR_RESULT_VALUE};
    result = editor_command_cli_standard_write(&project, &command, &executed,
        "project.rohr.json", text, sizeof(text));
    if(editor_result_check(result) || strstr(text, "--object") == NULL ||
            strstr(text, "--sprite second_frame add") == NULL) return 1;
    count = 0;
    for(char *token = strtok(text, " "); token != NULL && count < 32;
            token = strtok(NULL, " ")) arguments[count++] = token;
    result = editor_command_cli_standard_parse(&project, count, arguments, &path,
        &parsed);
    if(editor_result_check(result) || parsed.type != EDITOR_COMMAND_SPRITE_ADD ||
            parsed.data.sprite_add.object != object->id) return 1;

    command = (EditorCommand){.type = EDITOR_COMMAND_ANIMATION_FRAME_ADD,
        .data.animation_frame_add = {.object = object->id, .sprite = animated->id,
            .name = "wheel_frame", .path = "assets/wheel.png",
            .size = {32.0f, 24.0f}}};
    executed = editor_command_execute(&project, &command);
    if(executed.kind != ERROR_RESULT_VALUE) return 1;
    result = editor_command_cli_standard_write(&project, &command, &executed,
        "project.rohr.json", text, sizeof(text));
    if(editor_result_check(result) || strstr(text, "--object") == NULL ||
            strstr(text, "--animated-sprite rolling") == NULL ||
            strstr(text, "frame-add wheel_frame assets/wheel.png 32 24") == NULL)
        return 1;
    count = 0;
    for(char *token = strtok(text, " "); token != NULL && count < 32;
            token = strtok(NULL, " ")) arguments[count++] = token;
    result = editor_command_cli_standard_parse(&project, count, arguments, &path,
        &parsed);
    if(editor_result_check(result) || parsed.type != EDITOR_COMMAND_ANIMATION_FRAME_ADD ||
            parsed.data.animation_frame_add.object != object->id ||
            parsed.data.animation_frame_add.sprite != animated->id ||
            strcmp(parsed.data.animation_frame_add.name, "wheel_frame") != 0 ||
            strcmp(parsed.data.animation_frame_add.path, "assets/wheel.png") != 0 ||
            parsed.data.animation_frame_add.size.x != 32.0f ||
            parsed.data.animation_frame_add.size.y != 24.0f) return 1;

    command = (EditorCommand){.type = EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET,
        .data.animation_frame_size_set = {.object = object->id,
            .sprite = animated->id, .index = 0, .size = {48.0f, 36.0f}}};
    executed = editor_command_execute(&project, &command);
    if(executed.kind != ERROR_RESULT_VALUE || animated->frames[0].size.x != 48.0f ||
            animated->frames[0].size.y != 36.0f) return 1;
    result = editor_command_cli_standard_write(&project, &command, &executed,
        "project.rohr.json", text, sizeof(text));
    if(editor_result_check(result) || strstr(text, "--frame-index 0") == NULL ||
            strstr(text, "frame-size-set 48 36") == NULL) return 1;
    count = 0;
    for(char *token = strtok(text, " "); token != NULL && count < 32;
            token = strtok(NULL, " ")) arguments[count++] = token;
    result = editor_command_cli_standard_parse(&project, count, arguments, &path,
        &parsed);
    if(editor_result_check(result) ||
            parsed.type != EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET ||
            parsed.data.animation_frame_size_set.index != 0 ||
            parsed.data.animation_frame_size_set.size.x != 48.0f ||
            parsed.data.animation_frame_size_set.size.y != 36.0f) return 1;

    command = (EditorCommand){.type = EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET,
        .data.animated_sprite_body_set = {.object = object->id,
            .sprite = animated->id, .body = body->id}};
    if(editor_command_execute(&project, &command).kind != ERROR_RESULT_VALUE) return 1;
    if(editor_project_animated_sprite_get(object, animated->id)->rigid_body != body->id)
        return 1;

    command = (EditorCommand){.type = EDITOR_COMMAND_SPRITE_POSITION_SET,
        .data.sprite_position_set = {.object = object->id, .sprite = asset->id,
            .position = {12.0f, -7.0f}}};
    executed = editor_command_execute(&project, &command);
    if(executed.kind != ERROR_RESULT_VALUE || asset->position.x != 12.0f ||
            asset->position.y != -7.0f) return 1;
    result = editor_command_cli_standard_write(&project, &command, &executed,
        "project.rohr.json", text, sizeof(text));
    if(editor_result_check(result) || strstr(text, "--property position 12 -7") == NULL)
        return 1;
    count = 0;
    for(char *token = strtok(text, " "); token != NULL && count < 32;
            token = strtok(NULL, " ")) arguments[count++] = token;
    result = editor_command_cli_standard_parse(&project, count, arguments, &path,
        &parsed);
    if(editor_result_check(result) ||
            parsed.type != EDITOR_COMMAND_SPRITE_POSITION_SET ||
            parsed.data.sprite_position_set.sprite != asset->id) return 1;

    command = (EditorCommand){.type = EDITOR_COMMAND_SPRITE_BODY_SET,
        .data.sprite_body_set = {.object = object->id, .sprite = asset->id,
            .body = body->id}};
    executed = editor_command_execute(&project, &command);
    if(executed.kind != ERROR_RESULT_VALUE || asset->rigid_body != body->id) return 1;
    result = editor_command_cli_standard_write(&project, &command, &executed,
        "project.rohr.json", text, sizeof(text));
    if(editor_result_check(result) || strstr(text, "--property body") == NULL) return 1;
    count = 0;
    for(char *token = strtok(text, " "); token != NULL && count < 32;
            token = strtok(NULL, " ")) arguments[count++] = token;
    result = editor_command_cli_standard_parse(&project, count, arguments, &path,
        &parsed);
    if(editor_result_check(result) || parsed.type != EDITOR_COMMAND_SPRITE_BODY_SET ||
            parsed.data.sprite_body_set.body != body->id) return 1;

    command = (EditorCommand){.type = EDITOR_COMMAND_SPRITE_FOLLOW_ROTATION_SET,
        .data.sprite_boolean_set = {.object = object->id, .sprite = asset->id,
            .enabled = false}};
    executed = editor_command_execute(&project, &command);
    if(executed.kind != ERROR_RESULT_VALUE || asset->follow_body_rotation) return 1;

    command = (EditorCommand){.type = EDITOR_COMMAND_ANIMATED_SPRITE_POSITION_SET,
        .data.animated_sprite_position_set = {.object = object->id,
            .sprite = animated->id, .position = {30.0f, 40.0f}}};
    executed = editor_command_execute(&project, &command);
    if(executed.kind != ERROR_RESULT_VALUE || animated->editor_position.x != 30.0f ||
            animated->editor_position.y != 40.0f) return 1;
    result = editor_command_cli_standard_write(&project, &command, &executed,
        "project.rohr.json", text, sizeof(text));
    if(editor_result_check(result) ||
            strstr(text, "--property position 30 40") == NULL) return 1;
    count = 0;
    for(char *token = strtok(text, " "); token != NULL && count < 32;
            token = strtok(NULL, " ")) arguments[count++] = token;
    result = editor_command_cli_standard_parse(&project, count, arguments, &path,
        &parsed);
    if(editor_result_check(result) ||
            parsed.type != EDITOR_COMMAND_ANIMATED_SPRITE_POSITION_SET ||
            parsed.data.animated_sprite_position_set.sprite != animated->id) return 1;
    editor_project_destroy(&project);
    return 0;
}

int main(void) {
    const char *path = "/tmp/rohr-editor-core-test.json";
    EditorDocument document;
    EditorDocument loaded;
    EditorObjectIdResult added;
    EditorResult result;
    static EditorProject direct_project;
    static EditorProject parsed_project;
    EditorCommand direct_command = {.type = EDITOR_COMMAND_OBJECT_ADD,
        .data.object_add = {.name = "TestObject", .position = {7.0f, 9.0f}}};
    EditorCommand parsed_command;
    EditorCommandResult command_result;
    const char *parsed_path;
    char cli_text[512];
    char *cli_arguments[] = {
        "editor-cli", "object", "add", "project.rohr.json",
        "TestObject", "7", "9"
    };

    result = editor_document_create(&document);
    if(editor_result_check(result)) return 1;
    added = editor_object_command_add(document.project,
        &(EditorObjectAddArgs){
            .name = "fast car",
            .position = {12.0f, 34.0f}
        });
    if(added.kind != ERROR_RESULT_VALUE) return 1;
    result = editor_object_command_rename(
        document.project, added.result.value, "faster car");
    if(editor_result_check(result)) return 1;
    document.project->viewport_camera_offset = (Vec2D){21.0f, 22.0f};
    document.project->viewport_camera_zoom = 1.5f;
    document.project->viewport_local_view = true;
    document.project->navigation = (EditorNavigationState){
        .mode = 1, .selection = 1, .object = added.result.value
    };
    result = editor_document_save_as(&document, path);
    if(editor_result_check(result)) return 1;

    result = editor_document_create(&loaded);
    if(editor_result_check(result)) return 1;
    result = editor_document_load(&loaded, path);
    if(editor_result_check(result) || loaded.project->object_count != 1 ||
            strcmp(loaded.project->objects[0].name, "FasterCar") != 0 ||
            loaded.project->objects[0].position.x != 12.0f ||
            loaded.project->objects[0].position.y != 34.0f ||
            loaded.project->viewport_camera_offset.x != 21.0f ||
            loaded.project->viewport_camera_offset.y != 22.0f ||
            loaded.project->viewport_camera_zoom != 1.5f ||
            !loaded.project->viewport_local_view ||
            loaded.project->navigation.mode != 1 ||
            loaded.project->navigation.selection != 1 ||
            loaded.project->navigation.object != added.result.value) return 1;
    result = editor_object_command_remove(loaded.project, added.result.value);
    if(editor_result_check(result) || loaded.project->object_count != 0) return 1;

    editor_project_init(&direct_project);
    editor_project_init(&parsed_project);
    command_result = editor_command_execute(&direct_project, &direct_command);
    if(command_result.kind != ERROR_RESULT_VALUE) return 1;
    result = editor_command_cli_parse(7, cli_arguments, &parsed_path, &parsed_command);
    if(editor_result_check(result) || strcmp(parsed_path, "project.rohr.json") != 0)
        return 1;
    command_result = editor_command_execute(&parsed_project, &parsed_command);
    if(command_result.kind != ERROR_RESULT_VALUE ||
            direct_project.object_count != parsed_project.object_count ||
            direct_project.objects[0].id != parsed_project.objects[0].id ||
            strcmp(direct_project.objects[0].name,
                parsed_project.objects[0].name) != 0 ||
            direct_project.objects[0].position.x !=
                parsed_project.objects[0].position.x ||
            direct_project.objects[0].position.y !=
                parsed_project.objects[0].position.y) return 1;
    result = editor_command_cli_write(&direct_command, "a project's/state.json",
        cli_text, sizeof(cli_text));
    if(editor_result_check(result) ||
            strstr(cli_text, "'a project'\\''s/state.json'") == NULL ||
            strstr(cli_text, "object add") == NULL) return 1;
#define RUN_TEST(name) do { if((name)() != 0) { \
    fprintf(stderr, "%s failed\n", #name); return 1; } } while(0)
    RUN_TEST(creation_result_test);
    RUN_TEST(standard_cli_commands_test);
    RUN_TEST(transform_commands_test);
    RUN_TEST(item_commands_test);
    RUN_TEST(property_commands_test);
    RUN_TEST(relationship_commands_test);
    RUN_TEST(collision_filter_commands_test);
    RUN_TEST(named_selector_commands_test);
    RUN_TEST(auto_shape_test);
    RUN_TEST(auto_shape_command_test);
    RUN_TEST(sprite_commands_test);
#undef RUN_TEST

    editor_document_destroy(&loaded);
    editor_document_destroy(&document);
    (void)remove(path);
    return 0;
}
