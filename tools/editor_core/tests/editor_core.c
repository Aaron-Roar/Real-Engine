#include "editor_command.h"
#include "editor_document.h"
#include "editor_object_commands.h"

#include <stdio.h>
#include <string.h>

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

    editor_document_destroy(&loaded);
    editor_document_destroy(&document);
    (void)remove(path);
    return 0;
}
