#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "editor_command.h"
#include "editor_config.h"
#include "editor_document.h"
#include "editor_workspace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ROHR_DEVELOPMENT_SOURCE_DIR
#define ROHR_DEVELOPMENT_SOURCE_DIR ""
#endif

#if defined(_WIN32)
#include <direct.h>
#endif

static int cli_error(EditorResult result) {
    editor_result_stderr_print(result);
    return 1;
}

static void cli_usage_print(void);

typedef struct CliHelpDomain {
    const char *selector;
    const char *name;
    const char *properties;
    const char *example;
    unsigned int depth;
} CliHelpDomain;

static const CliHelpDomain cli_help_domains[] = {
    {"--object", "object", "position <x> <y>, visibility <true|false>",
        "--object car --property position 10 20", 0},
    {"--body", "rigid body",
        "position <x> <y>, rotation <radians>, origin <x> <y>, mass <number>, "
        "friction <number>, restitution <0..1>, gravity <true|false>, "
        "static <true|false>, rotation-locked <true|false>, collision <true|false>, "
        "particle <true|false>, particle-radius <number>, "
        "particle-auto-fit <true|false>, outline-color <hex>, surface-color <hex>, "
        "particle-ring-color <hex>, particle-fill-color <hex>, visibility <true|false>, "
        "category <mask> <true|false>, collide-with <mask> <true|false>",
        "--object car --body chassis --property mass 5", 1},
    {"--hitbox", "hitbox", "visibility <true|false>, "
        "auto-shape <shape> <triangle-kind> <width> <height> <radius> <apex-offset>",
        "--body chassis --hitbox chassis_hitbox --property visibility true", 2},
    {"--joint", "joint", "visibility <true|false>, kind <revolute|weld|spring>, "
        "visual-size <0.25..3>, rest-length <number>, stiffness <number>, "
        "damping <number>, anchor-a <anchor|none>, anchor-b <anchor|none>",
        "--joint suspension --property stiffness 40", 1},
    {"--anchor", "anchor", "position <x> <y>, rotation <radians>, "
        "position-follows-body <true|false>, rotation-follows-body <true|false>, "
        "rigid-body <body|none>, visibility <true|false>",
        "--anchor wheel_anchor --property position 12 8", 1},
    {"--soft-body", "soft body", "position <x> <y>, rotation <radians>, "
        "origin <x> <y>, node-color <hex>, beam-color <hex>, area-color <hex>, "
        "visibility <true|false>, auto-shape <shape> <triangle-kind> <width> "
        "<height> <radius> <apex-offset>",
        "--soft-body cloth --property area-color ff8800ff", 1},
    {"--node", "soft-body node", "position <x> <y>, mass <number>, "
        "friction <number>, restitution <0..1>, gravity <true|false>, "
        "collision <true|false>, node-radius <number>, color <hex>, "
        "visibility <true|false>",
        "--soft-body cloth --node corner --property mass 1", 2},
    {"--beam", "soft-body beam", "stiffness <number>, damping <number>, "
        "color <hex>, visibility <true|false>, node-a <node|none>, node-b <node|none>",
        "--soft-body cloth --beam edge_1 --property damping 0.2", 2},
    {"--area", "soft-body area", "color <hex>, visibility <true|false>",
        "--soft-body cloth --area area_1 --property color 4488ffff", 2},
    {"--vertex", "hitbox vertex", "position <x> <y>, position-locked <true|false>",
        "--body chassis --vertex vertex_1 --property position 4 8", 3},
    {"--line", "hitbox line", "length <number>",
        "--body chassis --line line_1 --property length 20", 3},
    {"--sprite", "sprite", "path <file>, position <x> <y>, rotation <radians>, "
        "size <width> <height>, body <body|none>, "
        "follow-body-rotation <true|false>, visibility <true|false>",
        "--object car --sprite wheel --property path assets/wheel.png", 1},
    {"--animated-sprite", "animated sprite", "body <body|none>, "
        "position <x> <y>, rotation <radians>, scale <x> <y>, "
        "timing <ticks> <seconds>, starting-frame <index>, "
        "direction <left|right>, follow-body-rotation <true|false>, "
        "visibility <true|false>",
        "--object car --animated-sprite wheel_animation --property scale 2 2", 2}
};

static bool cli_help_flag_check(const char *argument) {
    return argument != NULL && (strcmp(argument, "--help") == 0 ||
        strcmp(argument, "-help") == 0 || strcmp(argument, "-h") == 0 ||
        strcmp(argument, "--h") == 0);
}

static const CliHelpDomain *cli_help_domain_get(int count, char **arguments) {
    const CliHelpDomain *selected = NULL;
    for(int i = 1; i < count; i += 1) {
        for(size_t j = 0; j < sizeof(cli_help_domains) / sizeof(cli_help_domains[0]);
                j += 1) {
            if(strcmp(arguments[i], cli_help_domains[j].selector) == 0 ||
                    (strncmp(arguments[i], cli_help_domains[j].selector,
                        strlen(cli_help_domains[j].selector)) == 0 &&
                     strcmp(arguments[i] + strlen(cli_help_domains[j].selector),
                        "-id") == 0) ||
                    (strcmp(cli_help_domains[j].selector, "--line") == 0 &&
                     strcmp(arguments[i], "--line-index") == 0)) {
                if(selected == NULL || cli_help_domains[j].depth >= selected->depth)
                    selected = &cli_help_domains[j];
            }
        }
    }
    return selected;
}

static const char *cli_help_property_get(int count, char **arguments) {
    for(int i = 1; i + 1 < count; i += 1)
        if(strcmp(arguments[i], "--property") == 0 &&
                !cli_help_flag_check(arguments[i + 1])) return arguments[i + 1];
    return NULL;
}

static void cli_help_print(int count, char **arguments) {
    const CliHelpDomain *domain = cli_help_domain_get(count, arguments);
    const char *property = cli_help_property_get(count, arguments);
    cli_usage_print();
    puts("\nGeneral examples:\n"
        "  rohr-cli --project ./objects/project.rohr.json --object car "
            "--body chassis --property mass 5\n"
        "  rohr-cli --project ./game validate\n"
        "  rohr-cli --project ./game build");
    if(domain == NULL) {
        puts("\nSelectors:\n"
            "  --object, --body, --hitbox, --joint, --anchor, --soft-body,\n"
            "  --node, --beam, --area, --vertex, --line, --sprite,\n"
            "  --animated-sprite, --frame-index\n"
            "  Every named selector also accepts its -id form.");
        return;
    }
    printf("\nSelected depth: %s\n", domain->name);
    if(property != NULL) printf("Value required:\n  --property %s\n", property);
    printf("What can be set:\n  %s\n\nExample:\n  rohr-cli %s\n",
        domain->properties, domain->example);
    puts("Operations at this depth:\n  rename <new-name>, delete");
}

static bool cli_help_request_check(int count, char **arguments) {
    for(int i = 1; i < count; i += 1)
        if(cli_help_flag_check(arguments[i])) return true;
    if(count < 2) return true;
    if(strcmp(arguments[count - 1], "--property") == 0) return true;
    for(int i = 1; i + 1 < count; i += 1)
        if(strcmp(arguments[i], "--property") == 0 && i + 2 == count) return true;
    if(cli_help_domain_get(count, arguments) != NULL &&
            cli_help_property_get(count, arguments) == NULL) {
        bool operation = false;
        for(int i = 1; i < count; i += 1) {
            if(arguments[i][0] == '-') {
                if(i + 1 < count) i += 1;
            } else {
                operation = true;
                break;
            }
        }
        if(!operation) return true;
    }
    return false;
}

static void cli_usage_print(void) {
    puts("usage:\n"
        "  rohr-cli [--project <project.rohr.json>] <selectors> <operation>\n"
        "  Selectors are order-independent and precede the operation.\n"
        "  Properties: --property <name> <values...>\n"
        "  Structure: add, delete, or rename [new-name]\n"
        "  Project: rohr-cli [--project <project-directory>] "
            "<operation>\n"
        "  Project operations: create, validate, generate-c, compile, build\n"
        "  Example: rohr-cli --object car --body chassis --property mass 5\n"
        "  Example: rohr-cli --body chassis --property position 10 20\n"
        "  Example: rohr-cli --project ./game validate\n"
        "  Example: rohr-cli --project ./game create");
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

static bool cli_sdk_root_get(char *output, size_t capacity) {
    const char *base = SDL_GetBasePath();
    char root[EDITOR_WORKSPACE_PATH_MAX * 2];
    char config[EDITOR_WORKSPACE_PATH_MAX * 2];
    size_t length;
    SDL_PathInfo info;
    if(output == NULL || capacity == 0) return false;
    if(base != NULL && strlen(base) < sizeof(root)) {
        snprintf(root, sizeof(root), "%s", base);
        length = strlen(root);
        while(length > 0 && (root[length - 1] == '/' || root[length - 1] == '\\'))
            root[--length] = '\0';
        while(length > 0 && root[length - 1] != '/' && root[length - 1] != '\\')
            length -= 1;
        if(length > 0) root[length - 1] = '\0';
        const char *library_directories[] = {"lib", "lib64"};
        for(size_t i = 0; i < sizeof(library_directories) /
                sizeof(library_directories[0]); i += 1) {
            int count = snprintf(config, sizeof(config), "%s/%s/cmake/Rohr/RohrConfig.cmake",
                root, library_directories[i]);
            if(count >= 0 && (size_t)count < sizeof(config) &&
                    SDL_GetPathInfo(config, &info) && info.type == SDL_PATHTYPE_FILE) {
                count = snprintf(output, capacity, "%s", root);
                return count >= 0 && (size_t)count < capacity;
            }
        }
    }
    if(ROHR_DEVELOPMENT_SOURCE_DIR[0] != '\0') {
        int count = snprintf(output, capacity, "%s", ROHR_DEVELOPMENT_SOURCE_DIR);
        return count >= 0 && (size_t)count < capacity;
    }
    output[0] = '\0';
    return false;
}

static bool cli_rohr_cmake_option_get(char *output, size_t capacity) {
    char sdk[EDITOR_WORKSPACE_PATH_MAX * 2];
    int count;
    if(cli_sdk_root_get(sdk, sizeof(sdk))) {
        count = snprintf(output, capacity, "-DCMAKE_PREFIX_PATH=%s", sdk);
        return count >= 0 && (size_t)count < capacity;
    }
    if(ROHR_DEVELOPMENT_SOURCE_DIR[0] == '\0') return false;
    count = snprintf(output, capacity, "-DROHR_ENGINE_SOURCE_ROOT=%s",
        ROHR_DEVELOPMENT_SOURCE_DIR);
    return count >= 0 && (size_t)count < capacity;
}

static int cli_project_cmake_run(const EditorWorkspace *workspace,
        const EditorConfig *config, bool configure) {
    char root[EDITOR_WORKSPACE_PATH_MAX * 2];
    char build[EDITOR_WORKSPACE_PATH_MAX * 2];
    char rohr_option[EDITOR_WORKSPACE_PATH_MAX * 2];
    const char *configure_arguments[] = {
        "cmake", "-S", root, "-B", build, rohr_option, NULL};
    const char *compile_arguments[] = {"cmake", "--build", build, NULL};
    char configured_arguments[EDITOR_CONFIG_ARGUMENT_MAX]
        [EDITOR_CONFIG_ARGUMENT_LENGTH_MAX];
    const char *configured_output[EDITOR_CONFIG_ARGUMENT_MAX + 1];
    const char *const *process_arguments;
    const EditorConfigCommand *configured_command;
    char sdk[EDITOR_WORKSPACE_PATH_MAX * 2] = {0};
    SDL_Process *process;
    int exit_code = 1;
    int count;
    if(workspace == NULL || !cli_absolute_path_get(root, sizeof(root),
            workspace->directory))
        return cli_error(editor_result_error(
                EDITOR_ERROR_FILE_IO, "Could not resolve project path: %s",
                workspace == NULL ? "" : workspace->directory));
    count = snprintf(build, sizeof(build), "%s/build", root);
    if(count < 0 || (size_t)count >= sizeof(build)) return cli_error(
        editor_result_error(EDITOR_ERROR_CAPACITY,
            "Project build path is too long: %s", root));
    configured_command = editor_config_command_get(config,
        EDITOR_CONFIG_FRONTEND_CLI, configure ? EDITOR_CONFIG_OPERATION_CONFIGURE :
        EDITOR_CONFIG_OPERATION_COMPILE);
    if(configured_command != NULL && configured_command->count == 0) return 0;
    if(configured_command != NULL) {
        EditorResult result;
        (void)cli_sdk_root_get(sdk, sizeof(sdk));
        result = editor_config_command_expand(configured_command, root, build, sdk,
            configured_arguments, configured_output);
        if(editor_result_check(result)) return cli_error(result);
        process_arguments = configured_output;
    } else {
        if(configure && !cli_rohr_cmake_option_get(rohr_option,
                sizeof(rohr_option))) return cli_error(editor_result_error(
            EDITOR_ERROR_FILE_IO, "Could not locate the Rohr SDK or source tree"));
        process_arguments = configure ? configure_arguments : compile_arguments;
    }
    process = SDL_CreateProcess(process_arguments, false);
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

static int cli_project_compile(const EditorWorkspace *workspace,
        const EditorConfig *config) {
    int result = cli_project_cmake_run(workspace, config, true);
    return result == 0 ? cli_project_cmake_run(workspace, config, false) : result;
}

static EditorResult cli_project_config_load(EditorConfig *config,
        const EditorWorkspace *workspace) {
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    EditorResult result;
    editor_config_init(config);
    result = editor_config_sdk_path_get(path, sizeof(path), "editor.lua", true);
    if(editor_result_check(result)) return result;
    result = editor_config_file_merge(config, path, true);
    if(editor_result_check(result)) return result;
    if(snprintf(path, sizeof(path), "%s/editor.lua", workspace->directory) >=
            (int)sizeof(path)) return editor_result_error(EDITOR_ERROR_CAPACITY,
        "Project editor.lua path is too long: %s", workspace->directory);
    return editor_config_file_merge(config, path, false);
}

static int cli_workspace_action_command(int count, char **arguments) {
    static EditorProject project;
    EditorWorkspace workspace = {0};
    EditorWorkspaceCommand load = {.type = EDITOR_WORKSPACE_COMMAND_LOAD};
    EditorWorkspaceCommand command = {0};
    const char *directory = ".";
    const char *operation = arguments[count - 1];
    bool project_set = false;
    EditorResult result;
    EditorConfig config;
    for(int i = 1; i + 1 < count; i += 1) {
        if(strcmp(arguments[i], "--project") == 0) {
            if(i + 1 >= count - 1) return cli_error(editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT, "--project requires a path"));
            if(project_set) return cli_error(editor_result_error(
                EDITOR_ERROR_INVALID_ARGUMENT, "--project may only be specified once"));
            directory = arguments[++i];
            project_set = true;
        } else {
            return cli_error(editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "Unexpected project operation argument: %s", arguments[i]));
        }
    }
    if(strlen(directory) >= sizeof(load.directory))
        return cli_error(editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Project operation path is invalid or too long"));
    if(strcmp(operation, "create") == 0) {
        command.type = EDITOR_WORKSPACE_COMMAND_CREATE;
        snprintf(command.directory, sizeof(command.directory), "%s", directory);
        result = editor_workspace_command_execute(&workspace, &project, &command);
        return editor_result_check(result) ? cli_error(result) : 0;
    }
    snprintf(load.directory, sizeof(load.directory), "%s", directory);
    result = editor_workspace_command_execute(&workspace, &project, &load);
    if(editor_result_check(result)) return cli_error(result);
    if(strcmp(operation, "validate") == 0) {
        puts("valid");
        return 0;
    }
    if(strcmp(operation, "generate-c") == 0 || strcmp(operation, "build") == 0) {
        command.type = EDITOR_WORKSPACE_COMMAND_GENERATE_C;
        snprintf(command.directory, sizeof(command.directory), "%s",
            workspace.directory);
        result = editor_workspace_command_execute(&workspace, &project, &command);
        if(editor_result_check(result)) return cli_error(result);
    }
    if(strcmp(operation, "generate-c") == 0) return 0;
    result = cli_project_config_load(&config, &workspace);
    if(editor_result_check(result)) return cli_error(result);
    return cli_project_compile(&workspace, &config);
}

static bool cli_workspace_arguments_check(int count, char **arguments) {
    if(count < 2) return false;
    for(int i = 1; i + 1 < count; i += 2) {
        if(strcmp(arguments[i], "--project") != 0 || i + 1 >= count - 1)
            return false;
    }
    return true;
}

static bool cli_workspace_action_check(int count, char **arguments) {
    const char *operation;
    if(!cli_workspace_arguments_check(count, arguments)) return false;
    operation = arguments[count - 1];
    return strcmp(operation, "create") == 0 || strcmp(operation, "validate") == 0 ||
        strcmp(operation, "generate-c") == 0 || strcmp(operation, "compile") == 0 ||
        strcmp(operation, "build") == 0;
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
    if(cli_help_request_check(count, arguments)) {
        cli_help_print(count, arguments);
        return 0;
    }
    if(cli_workspace_action_check(count, arguments))
        return cli_workspace_action_command(count, arguments);
    if(cli_workspace_arguments_check(count, arguments))
        return cli_error(editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Unknown project operation: %s", arguments[count - 1]));
    if(count >= 2 && arguments[1][0] == '-')
        return cli_object_command(count, arguments);
    cli_usage_print();
    return 1;
}
