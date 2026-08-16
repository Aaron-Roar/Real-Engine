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
            "[--engine-root <path>] <operation>\n"
        "  Project operations: create, load, validate, save, generate-c, "
            "compile, build\n"
        "  Example: editor-cli --object car --body chassis --property mass 5\n"
        "  Example: editor-cli --body chassis --property position 10 20\n"
        "  Example: editor-cli --project ./game load\n"
        "  Example: editor-cli --engine-root ../rohr --project ./game create");
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
    EditorWorkspaceCommand command = {0};
    const char *directory = ".";
    const char *engine_root = NULL;
    const char *operation = arguments[count - 1];
    bool project_set = false;
    EditorResult result;
    for(int i = 1; i + 1 < count; i += 1) {
        if(strcmp(arguments[i], "--project") == 0) {
            if(i + 1 >= count - 1) return cli_error(editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT, "--project requires a path"));
            if(project_set) return cli_error(editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT, "--project may only be specified once"));
            directory = arguments[++i];
            project_set = true;
        } else if(strcmp(arguments[i], "--engine-root") == 0) {
            if(i + 1 >= count - 1) return cli_error(editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT, "--engine-root requires a path"));
            if(engine_root != NULL) return cli_error(editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT,
                "--engine-root may only be specified once"));
            engine_root = arguments[++i];
        } else {
            return cli_error(editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "Unexpected project operation argument: %s", arguments[i]));
        }
    }
    if(strlen(directory) >= sizeof(load.directory))
        return cli_error(editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Project operation path is invalid or too long"));
    if(strcmp(operation, "create") == 0) {
        if(engine_root == NULL || strlen(engine_root) >= sizeof(command.engine_root))
            return cli_error(editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "Project creation requires --engine-root <path>"));
        command.type = EDITOR_WORKSPACE_COMMAND_CREATE;
        snprintf(command.directory, sizeof(command.directory), "%s", directory);
        snprintf(command.engine_root, sizeof(command.engine_root), "%s", engine_root);
        result = editor_workspace_command_execute(&workspace, &project, &command);
        return editor_result_check(result) ? cli_error(result) : 0;
    }
    if(engine_root != NULL) return cli_error(editor_result_error(
        EDITOR_ERROR_INVALID_ARGUMENT,
        "--engine-root is only valid with the create operation"));
    snprintf(load.directory, sizeof(load.directory), "%s", directory);
    result = editor_workspace_command_execute(&workspace, &project, &load);
    if(editor_result_check(result)) return cli_error(result);
    if(strcmp(operation, "load") == 0 || strcmp(operation, "validate") == 0) {
        puts(strcmp(operation, "validate") == 0 ? "valid" : "loaded");
        return 0;
    }
    if(strcmp(operation, "save") == 0) {
        command.type = EDITOR_WORKSPACE_COMMAND_SAVE;
        result = editor_workspace_command_execute(&workspace, &project, &command);
        return editor_result_check(result) ? cli_error(result) : 0;
    }
    if(strcmp(operation, "generate-c") == 0 || strcmp(operation, "build") == 0) {
        command.type = EDITOR_WORKSPACE_COMMAND_GENERATE_C;
        snprintf(command.directory, sizeof(command.directory), "%s",
            workspace.directory);
        result = editor_workspace_command_execute(&workspace, &project, &command);
        if(editor_result_check(result)) return cli_error(result);
    }
    return strcmp(operation, "generate-c") == 0 ? 0 :
        cli_project_compile(&workspace);
}

static bool cli_workspace_action_check(int count, char **arguments) {
    const char *operation;
    if(count < 2) return false;
    operation = arguments[count - 1];
    if(strcmp(operation, "create") != 0 && strcmp(operation, "load") != 0 &&
            strcmp(operation, "validate") != 0 && strcmp(operation, "save") != 0 &&
            strcmp(operation, "generate-c") != 0 &&
            strcmp(operation, "compile") != 0 && strcmp(operation, "build") != 0)
        return false;
    for(int i = 1; i + 1 < count; i += 2) {
        if((strcmp(arguments[i], "--project") != 0 &&
                strcmp(arguments[i], "--engine-root") != 0) || i + 1 >= count - 1)
            return false;
    }
    return true;
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
    if(cli_workspace_action_check(count, arguments))
        return cli_workspace_action_command(count, arguments);
    if(count >= 2 && arguments[1][0] == '-')
        return cli_object_command(count, arguments);
    cli_usage_print();
    return 1;
}
