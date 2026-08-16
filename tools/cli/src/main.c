#include "editor_command.h"
#include "editor_document.h"
#include "editor_workspace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#endif

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
        "  Project: editor-cli [--project <project-directory>] "
            "generate-c|compile|build\n"
        "  Example: editor-cli --object car --body chassis --property mass 5\n"
        "  Example: editor-cli --body chassis --property position 10 20\n"
        "\nLegacy project management:\n"
        "  rohr project create <directory> <engine-root>\n"
        "  rohr project load <directory>\n"
        "  rohr project validate <directory>\n"
        "  rohr project save <directory>\n"
        "  rohr project generate-c <directory>");
}

static bool cli_absolute_path_get(char *output, size_t capacity,
        const char *path) {
#if defined(_WIN32)
    return output != NULL && path != NULL &&
        _fullpath(output, path, capacity) != NULL;
#else
    char *resolved;
    size_t length;
    if(output == NULL || path == NULL) return false;
    resolved = realpath(path, NULL);
    if(resolved == NULL) return false;
    length = strlen(resolved);
    if(length >= capacity) {
        free(resolved);
        return false;
    }
    memcpy(output, resolved, length + 1);
    free(resolved);
    return true;
#endif
}

static int cli_project_cmake_run(const EditorWorkspace *workspace,
        bool configure) {
    char root[EDITOR_WORKSPACE_PATH_MAX * 2];
    char build[EDITOR_WORKSPACE_PATH_MAX * 2];
    const char *configure_arguments[] = {
        "cmake", "-S", root, "-B", build, NULL};
    const char *compile_arguments[] = {"cmake", "--build", build, NULL};
    SDL_Process *process;
    int exit_code = 1;
    int count;
    if(workspace == NULL || !cli_absolute_path_get(root, sizeof(root),
            workspace->directory)) return cli_error(editor_result_error(
                EDITOR_ERROR_FILE_IO, "Could not resolve project path: %s",
                workspace == NULL ? "" : workspace->directory));
    count = snprintf(build, sizeof(build), "%s/build", root);
    if(count < 0 || (size_t)count >= sizeof(build)) return cli_error(
        editor_result_error(EDITOR_ERROR_CAPACITY,
            "Project build path is too long: %s", root));
    process = SDL_CreateProcess(configure ? configure_arguments :
        compile_arguments, false);
    if(process == NULL) return cli_error(editor_result_error(
        EDITOR_ERROR_FILE_IO, "Could not start CMake: %s", SDL_GetError()));
    if(!SDL_WaitProcess(process, true, &exit_code)) {
        SDL_DestroyProcess(process);
        return cli_error(editor_result_error(EDITOR_ERROR_FILE_IO,
            "Could not wait for CMake: %s", SDL_GetError()));
    }
    SDL_DestroyProcess(process);
    return exit_code;
}

static int cli_project_compile(const EditorWorkspace *workspace) {
    int result = cli_project_cmake_run(workspace, true);
    return result == 0 ? cli_project_cmake_run(workspace, false) : result;
}

static int cli_workspace_action_command(int count, char **arguments) {
    static EditorProject project;
    EditorWorkspace workspace = {0};
    EditorWorkspaceCommand load = {.type = EDITOR_WORKSPACE_COMMAND_LOAD};
    EditorWorkspaceCommand generate = {.type = EDITOR_WORKSPACE_COMMAND_GENERATE_C};
    const char *directory = ".";
    const char *operation = NULL;
    EditorResult result;
    for(int i = 1; i < count; i += 1) {
        if(strcmp(arguments[i], "--project") == 0) {
            if(i + 1 >= count) return cli_error(editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT, "--project requires a path"));
            directory = arguments[++i];
        } else if((strcmp(arguments[i], "generate-c") == 0 ||
                strcmp(arguments[i], "compile") == 0 ||
                strcmp(arguments[i], "build") == 0) && operation == NULL) {
            operation = arguments[i];
        } else {
            return cli_error(editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "Unexpected project operation argument: %s", arguments[i]));
        }
    }
    if(operation == NULL || strlen(directory) >= sizeof(load.directory))
        return cli_error(editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Project operation path is invalid or too long"));
    snprintf(load.directory, sizeof(load.directory), "%s", directory);
    result = editor_workspace_command_execute(&workspace, &project, &load);
    if(editor_result_check(result)) return cli_error(result);
    if(strcmp(operation, "generate-c") == 0 || strcmp(operation, "build") == 0) {
        snprintf(generate.directory, sizeof(generate.directory), "%s",
            workspace.directory);
        result = editor_workspace_command_execute(&workspace, &project, &generate);
        if(editor_result_check(result)) return cli_error(result);
    }
    return strcmp(operation, "generate-c") == 0 ? 0 :
        cli_project_compile(&workspace);
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
    if(count >= 2 && (strcmp(arguments[count - 1], "generate-c") == 0 ||
            strcmp(arguments[count - 1], "compile") == 0 ||
            strcmp(arguments[count - 1], "build") == 0))
        return cli_workspace_action_command(count, arguments);
    if(count >= 2 && arguments[1][0] == '-')
        return cli_object_command(count, arguments);
    cli_usage_print();
    return 1;
}
