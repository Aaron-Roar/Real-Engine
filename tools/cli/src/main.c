#include "editor_command.h"
#include "editor_document.h"
#include "editor_workspace.h"

#include <stdio.h>
#include <string.h>

static int cli_error(EditorResult result) {
    editor_result_stderr_print(result);
    return 1;
}

static void cli_usage_print(void) {
    puts("usage:\n"
        "  Entity selectors may appear after the project path in any order.\n"
        "  Prefer names: --object <name> --body <name> --joint <name> ...\n"
        "  IDs remain available: --object-id <id> --body-id <id> --joint-id <id> ...\n"
        "  Lines use --line <name> or --line-index <index>.\n"
        "  rohr project create <directory> <engine-root>\n"
        "  rohr project load <directory>\n"
        "  rohr project validate <directory>\n"
        "  rohr project save <directory>\n"
        "  rohr project generate-c <directory>\n"
        "  rohr object list <project.rohr.json>\n"
        "  rohr object add <project.rohr.json> <name> [x y]\n"
        "  rohr object rename <project.rohr.json> <id> <name>\n"
        "  rohr object delete <project.rohr.json> <id>\n"
        "  rohr object position <project.rohr.json> <object> <x> <y>\n"
        "  rohr rigid-body transform <project.rohr.json> --object <name> --body <name> <x> <y> <rotation>\n"
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
        "  rohr <target> set <project.rohr.json> <object> [parent] <item> [index] <property> <value>\n"
        "    targets: rigid-body, joint, anchor, soft-node, soft-beam, vertex, line\n"
        "  rohr joint connect <project.rohr.json> <object> <joint> <anchor-a|anchor-b> <anchor|none>\n"
        "  rohr anchor connect <project.rohr.json> <object> <anchor> rigid-body <body|none>\n"
        "  rohr soft-beam connect <project.rohr.json> <object> <body> <beam> <node-a|node-b> <node|none>\n"
        "  rohr collision-mask add <project.rohr.json> <name>\n"
        "  rohr rigid-body filter <project.rohr.json> <object> <body> <category|collide-with> <mask> <true|false>\n"
        "  rohr soft-node filter <project.rohr.json> <object> <body> <node> <category|collide-with> <mask> <true|false>\n"
        "  rohr navigation set <project.rohr.json> <mode> <selection> <object>\n"
        "      <rigid-body> <hitbox> <joint> <anchor> <soft-body> <soft-node>\n"
        "      <soft-beam> <line> <vertex> <origin-kind>");
}

static int cli_project_command(int count, char **arguments) {
    static EditorProject project;
    EditorWorkspace workspace = {0};
    EditorWorkspaceCommand command = {0};
    EditorResult result;
    const char *action;
    if(count < 4) {
        cli_usage_print();
        return 1;
    }
    action = arguments[2];
    if(strlen(arguments[3]) >= sizeof(command.directory))
        return cli_error(editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Project directory path is too long"));
    snprintf(command.directory, sizeof(command.directory), "%s", arguments[3]);
    if(strcmp(action, "create") == 0) {
        if(count != 5 || strlen(arguments[4]) >= sizeof(command.engine_root)) {
            cli_usage_print();
            return 1;
        }
        command.type = EDITOR_WORKSPACE_COMMAND_CREATE;
        snprintf(command.engine_root, sizeof(command.engine_root), "%s", arguments[4]);
    } else {
        EditorWorkspaceCommand load = {.type = EDITOR_WORKSPACE_COMMAND_LOAD};
        if(count != 4) {
            cli_usage_print();
            return 1;
        }
        snprintf(load.directory, sizeof(load.directory), "%s", arguments[3]);
        result = editor_workspace_command_execute(&workspace, &project, &load);
        if(editor_result_check(result)) return cli_error(result);
        if(strcmp(action, "validate") == 0 || strcmp(action, "load") == 0) {
            puts(strcmp(action, "validate") == 0 ? "valid" : "loaded");
            return 0;
        }
        if(strcmp(action, "save") == 0) command.type = EDITOR_WORKSPACE_COMMAND_SAVE;
        else if(strcmp(action, "generate-c") == 0)
            command.type = EDITOR_WORKSPACE_COMMAND_GENERATE_C;
        else {
            cli_usage_print();
            return 1;
        }
    }
    result = editor_workspace_command_execute(&workspace, &project, &command);
    return editor_result_check(result) ? cli_error(result) : 0;
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
    result = editor_command_cli_named_parse(document.project, count, arguments,
        &path, &command);
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
    if(count >= 2 && strcmp(arguments[1], "project") == 0)
        return cli_project_command(count, arguments);
    if(count >= 2 && (strcmp(arguments[1], "object") == 0 ||
            strcmp(arguments[1], "rigid-body") == 0 ||
            strcmp(arguments[1], "hitbox") == 0 ||
            strcmp(arguments[1], "line") == 0 ||
            strcmp(arguments[1], "joint") == 0 ||
            strcmp(arguments[1], "vertex") == 0 ||
            strcmp(arguments[1], "anchor") == 0 ||
            strcmp(arguments[1], "soft-body") == 0 ||
            strcmp(arguments[1], "soft-node") == 0 ||
            strcmp(arguments[1], "soft-beam") == 0 ||
            strcmp(arguments[1], "collision-mask") == 0 ||
            strcmp(arguments[1], "viewport") == 0))
        return cli_object_command(count, arguments);
    if(count >= 2 && strcmp(arguments[1], "navigation") == 0)
        return cli_object_command(count, arguments);
    cli_usage_print();
    return 1;
}
