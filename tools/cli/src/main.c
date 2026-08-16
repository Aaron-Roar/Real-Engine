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
        "  editor-cli [--project <project.rohr.json>] <selectors> <operation>\n"
        "  Selectors are order-independent and precede the operation.\n"
        "  Properties: --property <name> <values...>\n"
        "  Structure: add, delete, or rename [new-name]\n"
        "  Generate: editor-cli [--project <project-directory>] generate-c\n"
        "  Example: editor-cli --object car --body chassis --property mass 5\n"
        "  Example: editor-cli --body chassis --property position 10 20\n"
        "\nLegacy project management:\n"
        "  rohr project create <directory> <engine-root>\n"
        "  rohr project load <directory>\n"
        "  rohr project validate <directory>\n"
        "  rohr project save <directory>\n"
        "  rohr project generate-c <directory>");
}

static int cli_generate_c_command(int count, char **arguments) {
    static EditorProject project;
    EditorWorkspace workspace = {0};
    EditorWorkspaceCommand load = {.type = EDITOR_WORKSPACE_COMMAND_LOAD};
    EditorWorkspaceCommand generate = {.type = EDITOR_WORKSPACE_COMMAND_GENERATE_C};
    const char *directory = ".";
    bool operation_found = false;
    EditorResult result;
    for(int i = 1; i < count; i += 1) {
        if(strcmp(arguments[i], "--project") == 0) {
            if(i + 1 >= count) return cli_error(editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT, "--project requires a path"));
            directory = arguments[++i];
        } else if(strcmp(arguments[i], "generate-c") == 0 && !operation_found) {
            operation_found = true;
        } else {
            return cli_error(editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "Unexpected generate-c argument: %s", arguments[i]));
        }
    }
    if(!operation_found || strlen(directory) >= sizeof(load.directory))
        return cli_error(editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Generate C project path is invalid or too long"));
    snprintf(load.directory, sizeof(load.directory), "%s", directory);
    result = editor_workspace_command_execute(&workspace, &project, &load);
    if(editor_result_check(result)) return cli_error(result);
    snprintf(generate.directory, sizeof(generate.directory), "%s", workspace.directory);
    result = editor_workspace_command_execute(&workspace, &project, &generate);
    return editor_result_check(result) ? cli_error(result) : 0;
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
    action = count > 2 ? arguments[2] : "";
    path = arguments[1][0] == '-' ? "./objects/project.rohr.json" : arguments[3];
    if(arguments[1][0] == '-') for(int i = 1; i + 1 < count; i += 1)
        if(strcmp(arguments[i], "--project") == 0) path = arguments[i + 1];
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
    result = arguments[1][0] == '-' ? editor_command_cli_standard_parse(
        document.project, count, arguments, &path, &command) :
        editor_command_cli_named_parse(document.project, count, arguments,
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
    for(int i = 1; i < count; i += 1)
        if(strcmp(arguments[i], "generate-c") == 0)
            return cli_generate_c_command(count, arguments);
    if(count >= 2 && arguments[1][0] == '-')
        return cli_object_command(count, arguments);
    cli_usage_print();
    return 1;
}
