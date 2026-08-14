#include "editor_command.h"
#include "editor_document.h"
#include "editor_object_commands.h"

#include <stdio.h>
#include <string.h>

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
        if(editor_command_execute(&project, &navigation).kind != ERROR_RESULT_VALUE ||
                project.navigation.mode != 3 || project.selected != object->id ||
                editor_result_check(editor_command_cli_write(&navigation,
                    "project.rohr.json", cli_text, sizeof(cli_text))) ||
                editor_result_check(editor_command_cli_parse(17, navigation_arguments,
                    &parsed_path, &parsed)) ||
                parsed.type != EDITOR_COMMAND_NAVIGATION_SET ||
                parsed.data.navigation.mode != 3 ||
                parsed.data.navigation.hitbox != 1) return 1;
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
    PROPERTY_SET(EDITOR_ITEM_SOFT_BEAM, soft_body->id, beam->id, 0,
        EDITOR_PROPERTY_DAMPING, EDITOR_PROPERTY_VALUE_FLOAT, number, 0.25f);
#undef PROPERTY_SET
    if(rigid_body->mass_value != 5.0f || !rigid_body->particle ||
            !hitbox->vertices[0].position_locked || joint->kind != EDITOR_JOINT_WELD ||
            joint->stiffness != 12.0f || anchor->position_follows_body ||
            node->friction != 0.75f || beam->damping != 0.25f) return 1;
    if(editor_result_check(editor_command_cli_parse(9, node_arguments,
                &path, &parsed)) || parsed.type != EDITOR_COMMAND_PROPERTY_SET ||
            parsed.data.property_set.property != EDITOR_PROPERTY_FRICTION ||
            parsed.data.property_set.value.number != 0.75f ||
            editor_result_check(editor_command_cli_parse(8, joint_arguments,
                &path, &parsed)) || parsed.data.property_set.value.integer !=
                EDITOR_JOINT_WELD) return 1;
    command = (EditorCommand){.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = {.kind = EDITOR_ITEM_RIGID_BODY,
            .object = object->id, .item = rigid_body->id,
            .property = EDITOR_PROPERTY_RESTITUTION,
            .value_kind = EDITOR_PROPERTY_VALUE_FLOAT,
            .value.number = 2.0f}};
    if(editor_command_execute(&project, &command).kind != ERROR_RESULT_ERROR) return 1;
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
    if(transform_commands_test() != 0) return 1;
    if(item_commands_test() != 0) return 1;
    if(property_commands_test() != 0) return 1;
    if(relationship_commands_test() != 0) return 1;
    if(collision_filter_commands_test() != 0) return 1;

    editor_document_destroy(&loaded);
    editor_document_destroy(&document);
    (void)remove(path);
    return 0;
}
