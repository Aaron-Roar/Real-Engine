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
    EditorSoftBody *soft_body;
    EditorSoftNode *node;
    EditorCommand commands[8];
    char cli_text[512];

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){0});
    rigid_body = editor_project_rigid_body_add(&project, object);
    hitbox = rigid_body == NULL ? NULL : &rigid_body->hitboxes[0];
    anchor = editor_project_anchor_add(&project, object, (Position){0}, 0);
    soft_body = editor_project_soft_body_add(&project, object);
    node = editor_project_soft_node_add(&project, soft_body, (Position){0});
    if(object == NULL || rigid_body == NULL || hitbox == NULL || anchor == NULL ||
            soft_body == NULL || node == NULL) return 1;
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
    for(size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i += 1) {
        if(editor_command_execute(&project, &commands[i]).kind != ERROR_RESULT_VALUE ||
                editor_result_check(editor_command_cli_write(&commands[i],
                    "project.rohr.json", cli_text, sizeof(cli_text))) ||
                strstr(cli_text, "editor-cli ") != cli_text) return 1;
    }
    if(object->position.x != 1.0f || rigid_body->rotation != 0.5f ||
            anchor->rotation != 0.75f || node->position.x == 0.0f) return 1;
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
    result = editor_document_save_as(&document, path);
    if(editor_result_check(result)) return 1;

    result = editor_document_create(&loaded);
    if(editor_result_check(result)) return 1;
    result = editor_document_load(&loaded, path);
    if(editor_result_check(result) || loaded.project->object_count != 1 ||
            strcmp(loaded.project->objects[0].name, "FasterCar") != 0 ||
            loaded.project->objects[0].position.x != 12.0f ||
            loaded.project->objects[0].position.y != 34.0f) return 1;
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

    editor_document_destroy(&loaded);
    editor_document_destroy(&document);
    (void)remove(path);
    return 0;
}
