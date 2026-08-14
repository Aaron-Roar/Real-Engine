#include "editor_command.h"
#include "editor_document.h"

#include <stdio.h>
#include <string.h>

static int cli_error(EditorResult result) {
    editor_result_stderr_print(result);
    return 1;
}

static void cli_usage_print(void) {
    puts("usage:\n"
        "  rohr project validate <project.rohr.json>\n"
        "  rohr object list <project.rohr.json>\n"
        "  rohr object add <project.rohr.json> <name> [x y]\n"
        "  rohr object rename <project.rohr.json> <id> <name>\n"
        "  rohr object delete <project.rohr.json> <id>\n"
        "  rohr object position <project.rohr.json> <object> <x> <y>\n"
        "  rohr rigid-body transform <project.rohr.json> <object> <body> <x> <y> <rotation>\n"
        "  rohr rigid-body origin <project.rohr.json> <object> <body> <x> <y>\n"
        "  rohr vertex position <project.rohr.json> <object> <body> <hitbox> <vertex> <x> <y>\n"
        "  rohr anchor transform <project.rohr.json> <object> <anchor> <x> <y> <rotation>\n"
        "  rohr soft-body transform <project.rohr.json> <object> <body> <x> <y> <rotation>\n"
        "  rohr soft-body origin <project.rohr.json> <object> <body> <x> <y>\n"
        "  rohr soft-node position <project.rohr.json> <object> <body> <node> <x> <y>\n"
        "  rohr viewport camera <project.rohr.json> <offset-x> <offset-y> <zoom>\n"
        "  rohr viewport coordinates <project.rohr.json> <local|world>\n"
        "  rohr <target> visibility <project.rohr.json> <object> [parent] [item] <true|false>\n"
        "    targets: object, rigid-body, hitbox, joint, anchor, soft-body, soft-node, soft-beam\n"
        "  rohr navigation set <project.rohr.json> <mode> <selection> <object>\n"
        "      <rigid-body> <hitbox> <joint> <anchor> <soft-body> <soft-node>\n"
        "      <soft-beam> <line> <vertex> <origin-kind>");
}

static int cli_object_command(int count, char **arguments) {
    EditorCommand command;
    EditorCommandResult command_result;
    EditorDocument document;
    EditorResult result;
    const char *action;
    const char *path;
    if(count < 4) {
        cli_usage_print();
        return 1;
    }
    action = arguments[2];
    path = arguments[3];
    result = editor_document_create(&document);
    if(editor_result_check(result)) return cli_error(result);
    result = editor_document_load(&document, path);
    if(editor_result_check(result)) return cli_error(result);
    if(strcmp(action, "list") == 0) {
        const EditorProject *project = editor_document_project_const_get(&document);
        for(size_t i = 0; i < project->object_count; i += 1)
            printf("%u\t%s\n", project->objects[i].id, project->objects[i].name);
        return 0;
    }
    result = editor_command_cli_parse(count, arguments, &path, &command);
    if(editor_result_check(result)) return cli_error(result);
    command_result = editor_command_execute(document.project, &command);
    if(command_result.kind == ERROR_RESULT_ERROR)
        return cli_error((EditorResult){.kind = ERROR_RESULT_ERROR,
            .result.error = command_result.result.error});
    if(command.type == EDITOR_COMMAND_OBJECT_ADD)
        printf("added object %u\n", command_result.result.object);
    document.dirty = true;
    result = editor_document_save(&document);
    return editor_result_check(result) ? cli_error(result) : 0;
}

int main(int count, char **arguments) {
    if(count == 4 && strcmp(arguments[1], "project") == 0 &&
            strcmp(arguments[2], "validate") == 0) {
        EditorDocument document;
        EditorResult result;
        result = editor_document_create(&document);
        if(editor_result_check(result)) return cli_error(result);
        result = editor_document_load(&document, arguments[3]);
        if(editor_result_check(result)) return cli_error(result);
        puts("valid");
        return 0;
    }
    if(count >= 2 && (strcmp(arguments[1], "object") == 0 ||
            strcmp(arguments[1], "rigid-body") == 0 ||
            strcmp(arguments[1], "hitbox") == 0 ||
            strcmp(arguments[1], "joint") == 0 ||
            strcmp(arguments[1], "vertex") == 0 ||
            strcmp(arguments[1], "anchor") == 0 ||
            strcmp(arguments[1], "soft-body") == 0 ||
            strcmp(arguments[1], "soft-node") == 0 ||
            strcmp(arguments[1], "soft-beam") == 0 ||
            strcmp(arguments[1], "viewport") == 0))
        return cli_object_command(count, arguments);
    if(count >= 2 && strcmp(arguments[1], "navigation") == 0)
        return cli_object_command(count, arguments);
    cli_usage_print();
    return 1;
}
