#include "rohr.h"
#include "editor_project.h"
#include "editor_workspace.h"
#include "editor_file_browser.h"
#include "editor_history.h"
#include "editor_shortcuts.h"
#include "editor_viewport.h"
#include "editor_layout.h"
#include "editor_navigation.h"
#include "editor_command.h"
#include "editor_object_commands.h"
#include "panels/editor_origin_panel.h"
#include "panels/editor_bulk_panel.h"
#include "panels/editor_build_settings_panel.h"
#include "panels/editor_build_notifications.h"
#include "panels/editor_generation_report.h"
#include "panels/editor_notification_panel.h"
#include "panels/editor_terminal_panel.h"
#include "panels/editor_visual_settings_panel.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef ROHR_DEVELOPMENT_SOURCE_DIR
#define ROHR_DEVELOPMENT_SOURCE_DIR ""
#endif

#if defined(_WIN32)
#include <direct.h>
#define editor_chdir _chdir
#else
#include <unistd.h>
#define editor_chdir chdir
#endif

#define EDITOR_VIEWPORT_MIN_WIDTH 100.0f
#define EDITOR_TOOLS_MIN_WIDTH 40.0f
#define EDITOR_DIVIDER_GRAB_WIDTH 6.0f

typedef enum EditorWorkspaceBrowserAction {
    EDITOR_WORKSPACE_BROWSER_NONE,
    EDITOR_WORKSPACE_BROWSER_NEW,
    EDITOR_WORKSPACE_BROWSER_LOAD,
    EDITOR_WORKSPACE_BROWSER_ADD_SPRITE,
    EDITOR_WORKSPACE_BROWSER_ADD_ANIMATION_FRAME
} EditorWorkspaceBrowserAction;

static const char *editor_project_relative_path_get(const char *project_directory,
        const char *path) {
    size_t directory_length;

    if(project_directory == NULL || path == NULL) return path;
    directory_length = strlen(project_directory);
    while(directory_length > 0 &&
            (project_directory[directory_length - 1] == '/' ||
             project_directory[directory_length - 1] == '\\'))
        directory_length -= 1;
    if(directory_length == 0) return path;
#ifdef _WIN32
    if(SDL_strncasecmp(project_directory, path, directory_length) != 0)
        return path;
#else
    if(strncmp(project_directory, path, directory_length) != 0) return path;
#endif
    if(path[directory_length] != '/' && path[directory_length] != '\\') return path;
    while(path[directory_length] == '/' || path[directory_length] == '\\')
        directory_length += 1;
    return path + directory_length;
}

typedef enum EditorCloseAction {
    EDITOR_CLOSE_NONE,
    EDITOR_CLOSE_PROJECT,
    EDITOR_CLOSE_PROGRAM
} EditorCloseAction;

typedef struct EditorColorPicker {
    bool open;
    bool opened_this_frame;
    uint32_t *target;
    uint32_t original;
    EditorProject *project;
    EditorItemKind kind;
    EditorObjectId object;
    uint32_t parent;
    uint32_t item;
    EditorPropertyKind property;
    EditorViewportState *bulk_state;
    EditorHistory *bulk_history;
    float hue;
    float saturation;
    float value;
    float opacity;
    char hex[10];
} EditorColorPicker;

float editor_viewport_width = WINDOW_WIDTH * 0.8f;
float editor_window_width = WINDOW_WIDTH;
float editor_window_height = WINDOW_HEIGHT;
float editor_viewport_bottom = WINDOW_HEIGHT;

static EditorTerminalPanel *editor_operation_terminal;
static const EditorWorkspace *editor_operation_workspace;
static const EditorProject *editor_operation_project;
static bool *editor_operation_enabled;
static EditorHistory *editor_operation_history;

static bool editor_project_absolute_path_get(char *absolute, size_t capacity,
        const char *project_directory) {
    if(absolute == NULL || capacity == 0 || project_directory == NULL) return false;
#if defined(_WIN32)
    return _fullpath(absolute, project_directory, capacity) != NULL;
#else
    {
        char *resolved = realpath(project_directory, NULL);
        size_t length;
        if(resolved == NULL) return false;
        length = strlen(resolved);
        if(length >= capacity) {
            free(resolved);
            return false;
        }
        memcpy(absolute, resolved, length + 1);
        free(resolved);
        return true;
    }
#endif
}

static bool editor_sdk_root_get(char *output, size_t capacity) {
    const char *base = SDL_GetBasePath();
    char root[EDITOR_WORKSPACE_PATH_MAX * 2];
    char config[EDITOR_WORKSPACE_PATH_MAX * 2];
    size_t length;
    SDL_PathInfo info;
    int count;
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
            count = snprintf(config, sizeof(config), "%s/%s/cmake/Rohr/RohrConfig.cmake",
                root, library_directories[i]);
            if(count >= 0 && (size_t)count < sizeof(config) &&
                    SDL_GetPathInfo(config, &info) && info.type == SDL_PATHTYPE_FILE) {
                count = snprintf(output, capacity, "%s", root);
                return count >= 0 && (size_t)count < capacity;
            }
        }
    }
    if(ROHR_DEVELOPMENT_SOURCE_DIR[0] != '\0') {
        count = snprintf(output, capacity, "%s", ROHR_DEVELOPMENT_SOURCE_DIR);
        return count >= 0 && (size_t)count < capacity;
    }
    output[0] = '\0';
    return false;
}

static bool editor_rohr_cmake_option_get(char *output, size_t capacity) {
    char sdk[EDITOR_WORKSPACE_PATH_MAX * 2];
    int count;
    if(editor_sdk_root_get(sdk, sizeof(sdk))) {
        count = snprintf(output, capacity, "-DCMAKE_PREFIX_PATH=%s", sdk);
        return count >= 0 && (size_t)count < capacity;
    }
    if(ROHR_DEVELOPMENT_SOURCE_DIR[0] == '\0') return false;
    count = snprintf(output, capacity, "-DROHR_ENGINE_SOURCE_ROOT=%s",
        ROHR_DEVELOPMENT_SOURCE_DIR);
    return count >= 0 && (size_t)count < capacity;
}

static bool editor_gui_config_load(EditorConfig *config,
        const char *project_directory) {
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    EditorResult result;
    editor_config_init(config);
    result = editor_config_sdk_path_get(path, sizeof(path), "editor.lua", true);
    if(editor_result_check(result)) return false;
    result = editor_config_file_merge(config, path, true);
    if(editor_result_check(result)) return false;
    if(snprintf(path, sizeof(path), "%s/editor.lua", project_directory) >=
            (int)sizeof(path)) return false;
    result = editor_config_file_merge(config, path, false);
    if(editor_result_check(result)) return false;
    if(snprintf(path, sizeof(path), "%s/.rohr/gui-overrides.lua",
            project_directory) >= (int)sizeof(path)) return false;
    result = editor_config_file_merge(config, path, false);
    return !editor_result_check(result);
}

static EditorResult editor_gui_state_resolve(EditorGuiState *state,
        char *state_path, size_t state_path_capacity) {
    EditorConfig config;
    char sdk_config_path[EDITOR_WORKSPACE_PATH_MAX * 2];
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    EditorResult result;
    if(state == NULL || state_path == NULL || state_path_capacity == 0)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "GUI state resolution received an invalid argument");
    editor_config_init(&config);
    result = editor_config_sdk_path_get(sdk_config_path,
        sizeof(sdk_config_path), "editor.lua", true);
    if(editor_result_check(result)) return result;
    result = editor_config_file_merge(&config, sdk_config_path, true);
    if(editor_result_check(result)) return result;
    if(config.config_path_override_set) {
        result = editor_config_sdk_path_resolve(path, sizeof(path),
            config.config_path_override);
        if(editor_result_check(result)) return result;
        result = editor_config_file_merge(&config, path, false);
        if(editor_result_check(result)) return result;
    }
    if(!config.gui_state_path_set) return editor_result_error(
        EDITOR_ERROR_SCHEMA_INVALID,
        "SDK editor config does not define editor.gui_state_path");
    result = editor_config_sdk_path_resolve(path, sizeof(path),
        config.gui_state_path);
    if(editor_result_check(result)) return result;
    result = editor_gui_state_load(state, path, true);
    if(editor_result_check(result)) return result;
    if(config.gui_state_path_override_set) {
        result = editor_config_sdk_path_resolve(state_path, state_path_capacity,
            config.gui_state_path_override);
        if(editor_result_check(result)) return result;
        return editor_gui_state_load(state, state_path, false);
    }
    if(strlen(path) >= state_path_capacity) return editor_result_error(
        EDITOR_ERROR_CAPACITY, "GUI state path is too long");
    snprintf(state_path, state_path_capacity, "%s", path);
    return editor_result_value(true);
}

static bool editor_gui_state_presentation_apply(const EditorGuiState *state) {
    GraphicsWindowPresentationConfig config;
    EngineResult result;
    if(state == NULL) return false;
    config = rohr_graphics_window_presentation_default_get();
    config.logical_width = state->logical_width;
    config.logical_height = state->logical_height;
    config.window_width = state->logical_width;
    config.window_height = state->logical_height;
    config.aspect_ratio_auto = strcmp(state->aspect_ratio, "auto") == 0;
    if(strcmp(state->window_mode, "windowed") == 0)
        config.mode = GRAPHICS_WINDOW_MODE_WINDOWED;
    else if(strcmp(state->window_mode, "borderless_fullscreen") == 0)
        config.mode = GRAPHICS_WINDOW_MODE_BORDERLESS_FULLSCREEN;
    else if(strcmp(state->window_mode, "fullscreen") == 0)
        config.mode = GRAPHICS_WINDOW_MODE_FULLSCREEN;
    else return false;
    result = rohr_graphics_window_presentation_set(config);
    if(!rohr_error_check(result)) return true;
    fprintf(stderr, "error %d: %s\n", (int)result.result.error,
        rohr_error_message_get(result));
    return false;
}

static bool editor_build_arguments_get(const char *project_directory,
        bool configure,
        char storage[EDITOR_CONFIG_ARGUMENT_MAX][EDITOR_CONFIG_ARGUMENT_LENGTH_MAX],
        const char *output[EDITOR_CONFIG_ARGUMENT_MAX + 1]) {
    EditorConfig config;
    const EditorConfigCommand *command;
    char project[EDITOR_WORKSPACE_PATH_MAX * 2];
    char build[EDITOR_WORKSPACE_PATH_MAX * 2];
    char sdk[EDITOR_WORKSPACE_PATH_MAX * 2] = {0};
    char option[EDITOR_WORKSPACE_PATH_MAX * 2];
    int count;
    if(!editor_project_absolute_path_get(project, sizeof(project), project_directory) ||
            !editor_gui_config_load(&config, project_directory)) return false;
    count = snprintf(build, sizeof(build), "%s/build", project);
    if(count < 0 || (size_t)count >= sizeof(build)) return false;
    (void)editor_sdk_root_get(sdk, sizeof(sdk));
    command = editor_config_command_get(&config, EDITOR_CONFIG_FRONTEND_GUI,
        configure ? EDITOR_CONFIG_OPERATION_CONFIGURE :
            EDITOR_CONFIG_OPERATION_COMPILE);
    if(command != NULL) return !editor_result_check(editor_config_command_expand(
        command, project, build, sdk, storage, output));
    if(configure) {
        if(!editor_rohr_cmake_option_get(option, sizeof(option))) return false;
        const char *arguments[] = {"cmake", "-S", project, "-B", build, option};
        for(size_t i = 0; i < 6; i += 1) {
            snprintf(storage[i], sizeof(storage[i]), "%s", arguments[i]);
            output[i] = storage[i];
        }
        output[6] = NULL;
    } else {
        const char *arguments[] = {"cmake", "--build", build};
        for(size_t i = 0; i < 3; i += 1) {
            snprintf(storage[i], sizeof(storage[i]), "%s", arguments[i]);
            output[i] = storage[i];
        }
        output[3] = NULL;
    }
    return true;
}

static bool editor_terminal_argument_write(char *output, size_t capacity,
        size_t *used, const char *argument) {
#if defined(_WIN32)
    char delimiter = '"';
#else
    char delimiter = '\'';
#endif
    if(*used + 1 >= capacity) return false;
    output[(*used)++] = delimiter;
    for(size_t i = 0; argument[i] != '\0'; i += 1) {
#if defined(_WIN32)
        if(argument[i] == '%') {
            if(*used + 2 >= capacity) return false;
            output[(*used)++] = '%';
            output[(*used)++] = '%';
            continue;
        }
        if(argument[i] == '"' && *used + 1 < capacity) output[(*used)++] = '\\';
#else
        if(argument[i] == '\'') {
            static const char escaped[] = "'\\''";
            if(*used + sizeof(escaped) - 1 >= capacity) return false;
            memcpy(output + *used, escaped, sizeof(escaped) - 1);
            *used += sizeof(escaped) - 1;
            continue;
        }
#endif
        if(*used + 1 >= capacity) return false;
        output[(*used)++] = argument[i];
    }
    if(*used + 2 > capacity) return false;
    output[(*used)++] = delimiter;
    output[*used] = '\0';
    return true;
}

static bool editor_cmake_command_get(char *output, size_t capacity,
        const char *project_directory, bool configure) {
    char storage[EDITOR_CONFIG_ARGUMENT_MAX][EDITOR_CONFIG_ARGUMENT_LENGTH_MAX];
    const char *arguments[EDITOR_CONFIG_ARGUMENT_MAX + 1];
    size_t used = 0;
    if(output == NULL || !editor_build_arguments_get(project_directory, configure,
            storage, arguments)) return false;
    output[0] = '\0';
    for(size_t i = 0; arguments[i] != NULL; i += 1) {
        if(i > 0) {
            if(used + 1 >= capacity) return false;
            output[used++] = ' ';
        }
        if(!editor_terminal_argument_write(output, capacity, &used, arguments[i]))
            return false;
    }
    return true;
}

static SDL_Process *editor_cmake_hidden_start(const char *project_directory,
        bool configure) {
    char storage[EDITOR_CONFIG_ARGUMENT_MAX][EDITOR_CONFIG_ARGUMENT_LENGTH_MAX];
    const char *arguments[EDITOR_CONFIG_ARGUMENT_MAX + 1];
    SDL_PropertiesID properties;
    SDL_Process *process;
    if(!editor_build_arguments_get(project_directory, configure, storage, arguments))
        return NULL;
    properties = SDL_CreateProperties();
    if(properties == 0) return NULL;
    if(!SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER,
            arguments) ||
            !SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,
                SDL_PROCESS_STDIO_NULL) ||
            !SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,
                SDL_PROCESS_STDIO_NULL)) {
        SDL_DestroyProperties(properties);
        return NULL;
    }
    process = SDL_CreateProcessWithProperties(properties);
    SDL_DestroyProperties(properties);
    return process;
}

static bool editor_cmake_compile_start(EditorTerminalPanel *terminal,
        SDL_Process **hidden_process, bool *hidden_compile_pending,
        char *hidden_directory, size_t hidden_directory_capacity,
        const char *project_directory, bool show_operations) {
    char configure_command[EDITOR_WORKSPACE_PATH_MAX * 4];
    char compile_command[EDITOR_WORKSPACE_PATH_MAX * 4];
    char command[EDITOR_WORKSPACE_PATH_MAX * 8 + 8];
    bool configure_enabled;
    bool compile_enabled;
    if(!editor_cmake_command_get(configure_command,
            sizeof(configure_command), project_directory, true) ||
            !editor_cmake_command_get(compile_command,
                sizeof(compile_command), project_directory, false)) return false;
    configure_enabled = configure_command[0] != '\0';
    compile_enabled = compile_command[0] != '\0';
    if(!configure_enabled && !compile_enabled) return true;
    if(show_operations) {
        int count;
        count = snprintf(command, sizeof(command), configure_enabled && compile_enabled ?
            "%s && %s" : "%s%s", configure_command, compile_command);
        if(count < 0 || (size_t)count >= sizeof(command)) return false;
        return editor_terminal_panel_command_execute_tracked(terminal, command);
    }
    if(hidden_process == NULL || hidden_compile_pending == NULL ||
            hidden_directory == NULL || *hidden_process != NULL ||
            strlen(project_directory) >= hidden_directory_capacity) return false;
    *hidden_process = editor_cmake_hidden_start(project_directory,
        configure_enabled);
    if(*hidden_process != NULL) {
        *hidden_compile_pending = configure_enabled && compile_enabled;
        snprintf(hidden_directory, hidden_directory_capacity, "%s", project_directory);
    }
    return *hidden_process != NULL;
}

static void editor_operation_command_write(const EditorCommand *editor_command,
        const EditorCommandResult *result, void *context) {
    char command[3072];
    (void)context;
    if(editor_operation_terminal == NULL || editor_operation_workspace == NULL ||
            editor_operation_enabled == NULL || !*editor_operation_enabled ||
            !editor_operation_workspace->open || editor_command == NULL) return;
    if(editor_result_check(editor_command_cli_standard_write(editor_operation_project,
            editor_command, result,
            editor_operation_workspace->config.editor_state_file,
            command, sizeof(command)))) return;
    editor_terminal_panel_operation_write(editor_operation_terminal, command);
}

static void editor_operation_command_executing(const EditorProject *project,
        const EditorCommand *command, void *context) {
    (void)context;
    editor_history_command_begin(editor_operation_history, project, command);
}

static void editor_operation_command_finished(const EditorCommand *command,
        const EditorCommandResult *result, void *context) {
    (void)context;
    editor_history_command_finish(editor_operation_history, command, result);
}

static EditorResult editor_workspace_operation_execute(EditorWorkspace *workspace,
        EditorProject *project, const EditorWorkspaceCommand *workspace_command) {
    EditorResult result = editor_workspace_command_execute(
        workspace, project, workspace_command);
    char command[3072];
    if(!editor_result_check(result) &&
            workspace_command->type != EDITOR_WORKSPACE_COMMAND_CREATE &&
            workspace_command->type != EDITOR_WORKSPACE_COMMAND_LOAD &&
            workspace_command->type != EDITOR_WORKSPACE_COMMAND_SAVE &&
            editor_operation_terminal != NULL &&
            editor_operation_enabled != NULL && *editor_operation_enabled &&
            !editor_result_check(editor_workspace_command_cli_write(
                workspace_command, command, sizeof(command))))
        editor_terminal_panel_operation_write(editor_operation_terminal, command);
    return result;
}

static EditorNavigationState editor_navigation_state_get(
        const EditorProject *project, const EditorViewportState *state) {
    EditorViewportMode persisted_mode;
    if(project == NULL || state == NULL) return (EditorNavigationState){0};
    persisted_mode = state->mode == EDITOR_VIEWPORT_AUTO_SHAPE ?
        state->auto_shape_parent_mode : state->mode;
    return (EditorNavigationState){
        .mode = (uint32_t)persisted_mode,
        .selection = (uint32_t)state->selection,
        .object = project->selected,
        .selected_line = state->selected_line,
        .selected_vertex = state->selected_vertex,
        .rigid_body = state->selected_rigid_body,
        .hitbox = state->selected_hitbox,
        .joint = state->selected_joint,
        .anchor = state->selected_anchor,
        .soft_body = state->selected_soft_body,
        .soft_node = state->selected_soft_node,
        .soft_beam = state->selected_soft_beam,
        .sprite = state->selected_sprite,
        .animated_sprite = state->selected_animated_sprite,
        .origin_kind = (uint32_t)state->selected_origin_kind
    };
}

static void editor_navigation_state_apply(EditorProject *project,
        EditorViewportState *state, const EditorNavigationState *navigation) {
    if(project == NULL || state == NULL || navigation == NULL) return;
    project->selected = navigation->object;
    state->mode = (EditorViewportMode)navigation->mode;
    state->selection = (EditorHierarchySelection)navigation->selection;
    state->selected_line = navigation->selected_line;
    state->selected_vertex = navigation->selected_vertex;
    state->selected_rigid_body = navigation->rigid_body;
    state->selected_hitbox = navigation->hitbox;
    state->selected_joint = navigation->joint;
    state->selected_anchor = navigation->anchor;
    state->selected_soft_body = navigation->soft_body;
    state->selected_soft_node = navigation->soft_node;
    state->selected_soft_beam = navigation->soft_beam;
    state->selected_sprite = navigation->sprite;
    state->selected_animated_sprite = navigation->animated_sprite;
    state->selected_origin_kind = (EditorOriginKind)navigation->origin_kind;
}

static bool editor_selection_path_check(const EditorViewportState *state,
        EditorHierarchySelection kind, EditorObjectId object, uint32_t parent,
        uint32_t container, uint32_t item) {
    return editor_viewport_selection_contains(state,
        (EditorSelectionRef){kind, object, parent, container, item});
}

static void editor_window_layout_sync(void) {
    Scale output = rohr_graphics_render_output_size_get();
    float logical_width;

    if(output.x <= 0.0f || output.y <= 0.0f) return;
    logical_width = roundf(EDITOR_WINDOW_HEIGHT * output.x / output.y);
    if(logical_width < EDITOR_VIEWPORT_MIN_WIDTH + EDITOR_TOOLS_MIN_WIDTH) {
        logical_width = EDITOR_VIEWPORT_MIN_WIDTH + EDITOR_TOOLS_MIN_WIDTH;
    }
    if(logical_width == editor_window_width) return;
    editor_window_width = logical_width;
    editor_viewport_width = fminf(editor_viewport_width,
        editor_window_width - EDITOR_TOOLS_MIN_WIDTH);
}

static bool editor_result_ok(EngineResult result) {
    if(!rohr_error_check(result)) return true;
    fprintf(stderr, "error %d: %s\n", (int)result.result.error,
        rohr_error_message_get(result));
    return false;
}

static uint64_t editor_project_hash_get(const EditorProject *project) {
    const unsigned char *bytes = (const unsigned char *)project;
    uint64_t hash = UINT64_C(1469598103934665603);

    if(project == NULL) return 0;
    for(size_t i = 0; i < sizeof(*project); i += 1) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static float editor_panel_content_height_get(const EditorProject *project,
    const EditorViewportState *state) {
    const EditorObject *object = NULL;
    float height = EDITOR_WINDOW_HEIGHT;

    if(project == NULL || state == NULL) return height;
    for(size_t i = 0; i < project->object_count; i += 1) {
        if(project->objects[i].id == project->selected) object = &project->objects[i];
    }
    if(state->mode == EDITOR_VIEWPORT_HIERARCHY) {
        return fmaxf(height, 80.0f + (float)project->object_count * 34.0f);
    }
    if(object == NULL) return height;
    if(state->mode == EDITOR_VIEWPORT_OBJECT) {
        return fmaxf(height, 350.0f + (float)(object->rigid_body_count +
            object->joint_count + object->soft_body_count) * 30.0f);
    }
    if(state->mode == EDITOR_VIEWPORT_RIGID_BODY) {
        for(size_t i = 0; i < object->rigid_body_count; i += 1) {
            if(object->rigid_bodies[i].id == state->selected_rigid_body) {
                return fmaxf(height, 554.0f +
                    (float)(object->rigid_bodies[i].hitbox_count +
                        project->collision_mask_count + 1 +
                        (object->rigid_bodies[i].particle ? 1 : 0)) * 30.0f);
            }
        }
    }
    if(state->mode == EDITOR_VIEWPORT_SOFT_BODY) {
        for(size_t i = 0; i < object->soft_body_count; i += 1) {
            if(object->soft_body_items[i].id == state->selected_soft_body) {
                return fmaxf(height, 576.0f + (float)(object->soft_body_items[i].node_count +
                    object->soft_body_items[i].beam_count +
                    object->soft_body_items[i].area_count) * 28.0f);
            }
        }
    }
    if(state->mode == EDITOR_VIEWPORT_HITBOX) {
        for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
            const EditorRigidBody *body = &object->rigid_bodies[body_index];
            if(body->id != state->selected_rigid_body) continue;
            for(size_t hitbox_index = 0; hitbox_index < body->hitbox_count; hitbox_index += 1) {
                if(body->hitboxes[hitbox_index].id == state->selected_hitbox) {
                    return fmaxf(height, 180.0f +
                        (float)body->hitboxes[hitbox_index].vertex_count * 27.0f);
                }
            }
        }
    }
    return height;
}

static float editor_panel_delete_y_get(const EditorProject *project,
    const EditorViewportState *state) {
    return editor_panel_content_height_get(project, state) - 50.0f;
}

static bool editor_use_executable_directory(void) {
    const char *base_path = SDL_GetBasePath();

    return base_path != NULL && editor_chdir(base_path) == 0;
}

static bool editor_text_create(
    FontAsset *font,
    const char *value,
    TextAsset *text
) {
    TextAssetResult result;

    if(font == NULL || value == NULL || text == NULL) return false;
    result = rohr_graphics_text_create(font, value, (Color){230, 234, 242, 255});
    if(rohr_error_check(result)) {
        fprintf(stderr, "error %d: %s\n", (int)result.result.error,
            rohr_error_message_get(result));
        return false;
    }
    *text = result.result.value;
    return true;
}

static bool editor_named_text_sync(FontAsset *font, const char *name,
    TextAsset *text, char *cache, size_t capacity) {
    if(font == NULL || name == NULL || text == NULL || cache == NULL || capacity == 0) {
        return false;
    }
    if(strcmp(cache, name) == 0) return true;
    rohr_graphics_text_destroy(text);
    if(!editor_text_create(font, name, text)) return false;
    snprintf(cache, capacity, "%s", name);
    return true;
}

static void editor_hex_color_format(char output[10], uint32_t color) {
    snprintf(output, 10, "#%08X", color);
}

static bool editor_hex_color_parse(const char *text, uint32_t *color) {
    const char *digits = text;
    char *end;
    unsigned long value;
    size_t length;
    if(text == NULL || color == NULL) return false;
    if(digits[0] == '#') digits += 1;
    length = strlen(digits);
    if(length != 6 && length != 8) return false;
    for(size_t i = 0; i < length; i += 1) {
        if(!((digits[i] >= '0' && digits[i] <= '9') ||
                (digits[i] >= 'a' && digits[i] <= 'f') ||
                (digits[i] >= 'A' && digits[i] <= 'F'))) return false;
    }
    value = strtoul(digits, &end, 16);
    if(*end != '\0') return false;
    *color = length == 6 ? ((uint32_t)value << 8) | UINT32_C(0xff) : (uint32_t)value;
    return true;
}

static void editor_color_rgb_to_hsv(uint32_t color, float *hue,
        float *saturation, float *value) {
    float red = (float)((color >> 24) & 0xff) / 255.0f;
    float green = (float)((color >> 16) & 0xff) / 255.0f;
    float blue = (float)((color >> 8) & 0xff) / 255.0f;
    float maximum = fmaxf(red, fmaxf(green, blue));
    float minimum = fminf(red, fminf(green, blue));
    float delta = maximum - minimum;
    *value = maximum;
    *saturation = maximum <= 0.0f ? 0.0f : delta / maximum;
    if(delta <= 0.0001f) *hue = 0.0f;
    else if(maximum == red) *hue = fmodf((green - blue) / delta, 6.0f) / 6.0f;
    else if(maximum == green) *hue = ((blue - red) / delta + 2.0f) / 6.0f;
    else *hue = ((red - green) / delta + 4.0f) / 6.0f;
    if(*hue < 0.0f) *hue += 1.0f;
}

static uint32_t editor_color_hsv_to_rgba(float hue, float saturation,
        float value, float opacity) {
    float scaled = hue * 6.0f;
    int sector = (int)floorf(scaled) % 6;
    float fraction = scaled - floorf(scaled);
    float p = value * (1.0f - saturation);
    float q = value * (1.0f - fraction * saturation);
    float t = value * (1.0f - (1.0f - fraction) * saturation);
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    switch(sector) {
        case 0: red = value; green = t; blue = p; break;
        case 1: red = q; green = value; blue = p; break;
        case 2: red = p; green = value; blue = t; break;
        case 3: red = p; green = q; blue = value; break;
        case 4: red = t; green = p; blue = value; break;
        default: red = value; green = p; blue = q; break;
    }
    return ((uint32_t)lroundf(red * 255.0f) << 24) |
        ((uint32_t)lroundf(green * 255.0f) << 16) |
        ((uint32_t)lroundf(blue * 255.0f) << 8) |
        (uint32_t)lroundf(fminf(100.0f, fmaxf(0.0f, opacity)) * 2.55f);
}

static void editor_color_picker_commit(EditorColorPicker *picker) {
    uint32_t value;
    EditorCommand command;
    if(picker == NULL || !picker->open || picker->target == NULL) return;
    value = *picker->target;
    picker->open = false;
    if(picker->project == NULL || value == picker->original) return;
    *picker->target = picker->original;
    if(picker->bulk_state != NULL && picker->bulk_history != NULL) {
        EditorPropertySetCommand property = {.property = picker->property,
            .value_kind = EDITOR_PROPERTY_VALUE_UINT, .value.integer = value};
        (void)editor_bulk_property_set(picker->project, picker->bulk_state,
            picker->bulk_history, &property);
        *picker->target = value;
        return;
    }
    command = (EditorCommand){.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = {picker->kind, picker->object, picker->parent,
            picker->item, 0, picker->property, EDITOR_PROPERTY_VALUE_UINT,
            {.integer = value}}};
    (void)editor_command_execute(picker->project, &command);
}

static void editor_color_picker_cancel(EditorColorPicker *picker) {
    if(picker == NULL || !picker->open) return;
    if(picker->target != NULL) *picker->target = picker->original;
    picker->open = false;
    picker->opened_this_frame = false;
    picker->target = NULL;
    picker->project = NULL;
    picker->bulk_state = NULL;
    picker->bulk_history = NULL;
}

static void editor_color_picker_open(EditorColorPicker *picker, uint32_t *target,
        EditorProject *project, EditorItemKind kind, EditorObjectId object,
        uint32_t parent, uint32_t item, EditorPropertyKind property) {
    if(picker == NULL || target == NULL) return;
    editor_color_picker_commit(picker);
    picker->open = true;
    picker->opened_this_frame = true;
    picker->target = target;
    picker->original = *target;
    picker->project = project;
    picker->kind = kind;
    picker->object = object;
    picker->parent = parent;
    picker->item = item;
    picker->property = property;
    picker->bulk_state = NULL;
    picker->bulk_history = NULL;
    editor_color_rgb_to_hsv(*target, &picker->hue, &picker->saturation, &picker->value);
    picker->opacity = (float)(*target & 0xff) * 100.0f / 255.0f;
    editor_hex_color_format(picker->hex, *target);
}

typedef struct EditorBulkColorContext {
    EditorColorPicker *picker;
    EditorProject *project;
    EditorViewportState *state;
    EditorHistory *history;
} EditorBulkColorContext;

static void editor_bulk_color_picker_open(void *context, uint32_t *color,
        EditorPropertyKind property) {
    EditorBulkColorContext *bulk = context;
    if(bulk == NULL || bulk->picker == NULL) return;
    editor_color_picker_open(bulk->picker, color, bulk->project,
        EDITOR_ITEM_OBJECT, 0, 0, 0, property);
    bulk->picker->bulk_state = bulk->state;
    bulk->picker->bulk_history = bulk->history;
}

static bool editor_point_in_rect(Position point, UIRect bounds) {
    return point.x >= bounds.x && point.x <= bounds.x + bounds.width &&
        point.y >= bounds.y && point.y <= bounds.y + bounds.height;
}

static bool editor_color_picker_draw(EditorColorPicker *picker, MouseState *mouse,
        TextAsset *hex_display, TextAsset *opacity_display,
        const TextAsset *opacity_label, bool *field_editing) {
    UIRect menu = {EDITOR_VIEWPORT_WIDTH * 0.5f - 180.0f,
        EDITOR_MENU_HEIGHT + 70.0f, 360.0f, 286.0f};
    UIRect hex_bounds = {menu.x + 12.0f, menu.y + 12.0f, 190.0f, 28.0f};
    UIRect preview = {menu.x + 274.0f, menu.y + 12.0f, 72.0f, 72.0f};
    UIRect palette = {menu.x + 12.0f, menu.y + 52.0f, 220.0f, 176.0f};
    UIRect hue = {menu.x + 240.0f, menu.y + 96.0f, 22.0f, 132.0f};
    UIRect opacity = {menu.x + 12.0f, menu.y + 242.0f, 120.0f, 28.0f};
    Position pointer;
    UIFieldResult hex_result;
    UIFieldResult opacity_result;
    UIButtonResult palette_interaction;
    UIButtonResult hue_interaction;
    if(picker == NULL || !picker->open || picker->target == NULL || mouse == NULL) {
        return false;
    }
    pointer = rohr_graphics_mouse_screen_position_get();
    rohr_ui_surface(menu, (Color){34, 38, 47, 255});
    rohr_ui_border(menu, 2.0f, (Color){5, 6, 8, 255});
    hex_result = rohr_ui_field("editor.color_picker.hex", (UIFieldBinding){
        .kind = UI_FIELD_STRING, .string = picker->hex,
        .string_capacity = sizeof(picker->hex)}, hex_display, hex_bounds, NULL);
    if(hex_result.changed) {
        uint32_t parsed;
        if(editor_hex_color_parse(picker->hex, &parsed)) {
            *picker->target = parsed;
            editor_color_rgb_to_hsv(parsed, &picker->hue,
                &picker->saturation, &picker->value);
            picker->opacity = (float)(parsed & 0xff) * 100.0f / 255.0f;
        }
    }
    for(int y = 0; y < 16; y += 1) {
        for(int x = 0; x < 20; x += 1) {
            float saturation = ((float)x + 0.5f) / 20.0f;
            float value = 1.0f - ((float)y + 0.5f) / 16.0f;
            uint32_t cell = editor_color_hsv_to_rgba(
                picker->hue, saturation, value, 100.0f);
            (void)rohr_graphics_screen_rect_draw(
                palette.x + (float)x * palette.width / 20.0f,
                palette.y + (float)y * palette.height / 16.0f,
                palette.width / 20.0f + 0.5f, palette.height / 16.0f + 0.5f,
                rohr_graphics_color_hex_create(cell));
        }
    }
    palette_interaction = rohr_ui_interaction("editor.color_picker.palette", palette);
    if(editor_point_in_rect(pointer, palette) &&
            (mouse->button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_PRESSED ||
            mouse->button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_DOWN)) {
        picker->saturation = fminf(1.0f, fmaxf(0.0f,
            (pointer.x - palette.x) / palette.width));
        picker->value = 1.0f - fminf(1.0f, fmaxf(0.0f,
            (pointer.y - palette.y) / palette.height));
        (void)palette_interaction;
        *picker->target = editor_color_hsv_to_rgba(picker->hue,
            picker->saturation, picker->value, picker->opacity);
        editor_hex_color_format(picker->hex, *picker->target);
    }
    rohr_ui_border((UIRect){palette.x + picker->saturation * palette.width - 5.0f,
        palette.y + (1.0f - picker->value) * palette.height - 5.0f,
        10.0f, 10.0f}, 2.0f, (Color){255, 255, 255, 255});
    for(int y = 0; y < 24; y += 1) {
        uint32_t cell = editor_color_hsv_to_rgba(
            ((float)y + 0.5f) / 24.0f, 1.0f, 1.0f, 100.0f);
        (void)rohr_graphics_screen_rect_draw(hue.x,
            hue.y + (float)y * hue.height / 24.0f, hue.width,
            hue.height / 24.0f + 0.5f, rohr_graphics_color_hex_create(cell));
    }
    hue_interaction = rohr_ui_interaction("editor.color_picker.hue", hue);
    if(editor_point_in_rect(pointer, hue) &&
            (mouse->button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_PRESSED ||
            mouse->button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_DOWN)) {
        picker->hue = fminf(0.9999f, fmaxf(0.0f,
            (pointer.y - hue.y) / hue.height));
        (void)hue_interaction;
        *picker->target = editor_color_hsv_to_rgba(picker->hue,
            picker->saturation, picker->value, picker->opacity);
        editor_hex_color_format(picker->hex, *picker->target);
    }
    rohr_ui_border((UIRect){hue.x - 3.0f,
        hue.y + picker->hue * hue.height - 2.0f, hue.width + 6.0f, 4.0f},
        1.0f, (Color){255, 255, 255, 255});
    opacity_result = rohr_ui_field("editor.color_picker.opacity", (UIFieldBinding){
        .kind = UI_FIELD_FLOAT, .number = &picker->opacity}, opacity_display,
        opacity, NULL);
    rohr_ui_label(opacity_label,
        (UIRect){opacity.x + opacity.width + 8.0f, opacity.y, 90.0f, opacity.height});
    if(opacity_result.changed) {
        picker->opacity = fminf(100.0f, fmaxf(0.0f, picker->opacity));
        *picker->target = editor_color_hsv_to_rgba(picker->hue,
            picker->saturation, picker->value, picker->opacity);
        editor_hex_color_format(picker->hex, *picker->target);
    }
    rohr_ui_surface(preview, rohr_graphics_color_hex_create(*picker->target));
    rohr_ui_border(preview, 2.0f, (Color){8, 9, 12, 255});
    *field_editing = *field_editing || hex_result.active || opacity_result.active;
    if(!picker->opened_this_frame &&
            mouse->button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_PRESSED &&
            !editor_point_in_rect(pointer, menu)) editor_color_picker_commit(picker);
    picker->opened_this_frame = false;
    return true;
}

static bool editor_color_swatch(const char *id, uint32_t *color, bool disabled,
        EditorColorPicker *picker, UIRect bounds, EditorProject *project,
        EditorItemKind kind, EditorObjectId object, uint32_t parent,
        uint32_t item, EditorPropertyKind property) {
    UIButtonStyle style = rohr_ui_button_style_default_get();
    Color displayed = disabled ? (Color){70, 72, 78, 255} :
        rohr_graphics_color_hex_create(*color);
    UIButtonResult result;
    style.idle = displayed;
    style.hovered = disabled ? displayed : (Color){displayed.red, displayed.green,
        displayed.blue, 220};
    style.pressed = displayed;
    style.disabled = displayed;
    if(disabled) {
        rohr_ui_button_disabled(bounds, &style);
        rohr_ui_border(bounds, 2.0f, (Color){18, 20, 24, 255});
        return false;
    }
    result = rohr_ui_button(id, NULL, bounds, &style);
    rohr_ui_border(bounds, 2.0f, (Color){8, 9, 12, 255});
    if(result.clicked) editor_color_picker_open(picker, color, project, kind,
        object, parent, item, property);
    return result.clicked;
}

static void editor_numeric_field_disabled_draw(
    TextAsset *display,
    float value,
    UIRect bounds
) {
    char text[32];

    if(display == NULL) return;
    snprintf(text, sizeof(text), "%.1f", value);
    (void)rohr_graphics_text_value_set(display, text);
    rohr_ui_button_disabled(bounds, NULL);
    rohr_ui_label(display, bounds);
}

static UIButtonStyle editor_delete_button_style_get(void) {
    UIButtonStyle style = rohr_ui_button_style_default_get();

    style.idle = (Color){145, 42, 48, 255};
    style.hovered = (Color){181, 53, 60, 255};
    style.pressed = (Color){112, 31, 37, 255};
    style.disabled = (Color){75, 35, 38, 210};
    return style;
}

static UIButtonStyle editor_selected_button_style_get(void) {
    UIButtonStyle style = rohr_ui_button_style_default_get();

    style.idle = (Color){118, 96, 35, 255};
    style.hovered = (Color){145, 119, 45, 255};
    return style;
}

static void editor_icon_line_draw(Position start, Position end, Color color) {
    Vec2D delta = {end.x - start.x, end.y - start.y};
    float length = sqrtf(delta.x * delta.x + delta.y * delta.y);

    if(length <= 0.0f) return;
    (void)rohr_graphics_screen_quad_draw(
        (Position){(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f},
        length, 1.5f, -atan2f(delta.y, delta.x), color);
}

static void editor_auto_shape_icon_draw(UIRect bounds, EditorAutoShapeKind kind,
        Color color) {
    Position center = {bounds.x + bounds.width * 0.5f, bounds.y + 18.0f};
    if(kind == EDITOR_AUTO_SHAPE_TRIANGLE) {
        Position top = {center.x, center.y - 9.0f};
        Position right = {center.x + 11.0f, center.y + 8.0f};
        Position left = {center.x - 11.0f, center.y + 8.0f};
        editor_icon_line_draw(top, right, color);
        editor_icon_line_draw(right, left, color);
        editor_icon_line_draw(left, top, color);
    } else if(kind == EDITOR_AUTO_SHAPE_RECTANGLE) {
        Position top_left = {center.x - 11.0f, center.y - 8.0f};
        Position top_right = {center.x + 11.0f, center.y - 8.0f};
        Position bottom_right = {center.x + 11.0f, center.y + 8.0f};
        Position bottom_left = {center.x - 11.0f, center.y + 8.0f};
        editor_icon_line_draw(top_left, top_right, color);
        editor_icon_line_draw(top_right, bottom_right, color);
        editor_icon_line_draw(bottom_right, bottom_left, color);
        editor_icon_line_draw(bottom_left, top_left, color);
    } else {
        Position previous = {center.x, center.y - 10.0f};
        for(size_t i = 1; i <= 16; i += 1) {
            float angle = -1.57079632679f + 6.28318530718f * (float)i / 16.0f;
            Position current = {center.x + cosf(angle) * 10.0f,
                center.y + sinf(angle) * 10.0f};
            editor_icon_line_draw(previous, current, color);
            previous = current;
        }
    }
}

static int editor_auto_shape_picker_draw(const char *id_prefix, UIRect bounds,
        size_t point_count, const TextAsset *triangle_label,
        const TextAsset *rectangle_label, const TextAsset *circle_label) {
    const TextAsset *labels[3] = {triangle_label, rectangle_label, circle_label};
    Color enabled_color = {230, 234, 242, 255};
    Color disabled_color = {105, 108, 116, 255};
    float gap = 4.0f;
    float width = (bounds.width - gap * 2.0f) / 3.0f;

    rohr_ui_surface(bounds, (Color){28, 31, 38, 255});
    rohr_ui_border(bounds, 2.0f, (Color){5, 6, 8, 255});
    for(size_t i = 0; i < 3; i += 1) {
        char id[96];
        UIRect button = {bounds.x + (width + gap) * (float)i, bounds.y,
            width, bounds.height};
        bool enabled = point_count >= (i == EDITOR_AUTO_SHAPE_RECTANGLE ? 4u : 3u);
        snprintf(id, sizeof(id), "%s.%zu", id_prefix, i);
        if(enabled) {
            UIButtonResult result = rohr_ui_button(id, labels[i], button, NULL);
            editor_auto_shape_icon_draw(button, (EditorAutoShapeKind)i, enabled_color);
            if(result.clicked) return (int)i;
        } else {
            rohr_ui_button_disabled(button, NULL);
            rohr_ui_label(labels[i], button);
            editor_auto_shape_icon_draw(button, (EditorAutoShapeKind)i, disabled_color);
        }
    }
    return -1;
}

static bool editor_visibility_toggle(const char *id, const TextAsset *visible_label,
    const TextAsset *hidden_label,
    UIRect bounds, EditorProject *project, EditorVisibilityKind kind,
    EditorObjectId object, uint32_t parent, uint32_t item, bool visible) {
    UIButtonResult result;

    if(id == NULL || visible_label == NULL || hidden_label == NULL ||
            project == NULL) return false;
    result = rohr_ui_button(id, visible ? visible_label : hidden_label, bounds, NULL);
    if(result.clicked) {
        EditorCommand command = {.type = EDITOR_COMMAND_VISIBILITY,
            .data.visibility = {kind, object, parent, item, !visible}};
        (void)editor_command_execute(project, &command);
    }
    return result.clicked;
}

static bool editor_checkbox(const char *id, const TextAsset *label,
    UIRect bounds, bool *checked) {
    UIButtonResult interaction;
    UIRect box;
    Color background;

    if(id == NULL || label == NULL || checked == NULL) return false;
    interaction = rohr_ui_interaction(id, bounds);
    if(interaction.clicked) *checked = !*checked;
    background = interaction.pressed ? (Color){58, 65, 78, 255} :
        interaction.hovered || interaction.focused ? (Color){67, 75, 90, 255} :
        (Color){48, 54, 66, 255};
    rohr_ui_surface(bounds, background);
    box = (UIRect){bounds.x + 4.0f, bounds.y + 4.0f,
        bounds.height - 8.0f, bounds.height - 8.0f};
    rohr_ui_surface(box, (Color){22, 25, 31, 255});
    rohr_ui_border(box, 2.0f, (Color){8, 9, 12, 255});
    if(*checked) {
        rohr_ui_surface((UIRect){box.x + 5.0f, box.y + 5.0f,
            box.width - 10.0f, box.height - 10.0f},
            (Color){225, 230, 240, 255});
    }
    rohr_ui_label(label, (UIRect){box.x + box.width + 8.0f, bounds.y,
        bounds.width - box.width - 12.0f, bounds.height});
    return interaction.clicked;
}

static void editor_property_float_set(EditorProject *project, EditorItemKind kind,
        EditorObjectId object, uint32_t parent, uint32_t item, uint32_t index,
        EditorPropertyKind property, float value) {
    EditorCommand command = {.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = {kind, object, parent, item, index, property,
            EDITOR_PROPERTY_VALUE_FLOAT, {.number = value}}};
    (void)editor_command_execute(project, &command);
}

static void editor_property_bool_set(EditorProject *project, EditorItemKind kind,
        EditorObjectId object, uint32_t parent, uint32_t item, uint32_t index,
        EditorPropertyKind property, bool value) {
    EditorCommand command = {.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = {kind, object, parent, item, index, property,
            EDITOR_PROPERTY_VALUE_BOOL, {.boolean = value}}};
    (void)editor_command_execute(project, &command);
}

static void editor_property_uint_set(EditorProject *project, EditorItemKind kind,
        EditorObjectId object, uint32_t parent, uint32_t item, uint32_t index,
        EditorPropertyKind property, uint32_t value) {
    EditorCommand command = {.type = EDITOR_COMMAND_PROPERTY_SET,
        .data.property_set = {kind, object, parent, item, index, property,
            EDITOR_PROPERTY_VALUE_UINT, {.integer = value}}};
    (void)editor_command_execute(project, &command);
}

static void editor_relationship_set(EditorProject *project,
        EditorRelationshipKind kind, EditorObjectId object, uint32_t parent,
        uint32_t item, uint32_t endpoint, uint32_t target) {
    EditorCommand command = {.type = EDITOR_COMMAND_RELATIONSHIP_SET,
        .data.relationship_set = {kind, object, parent, item, endpoint, target}};
    (void)editor_command_execute(project, &command);
}

static void editor_collision_filter_set(EditorProject *project, EditorItemKind kind,
        EditorObjectId object, uint32_t parent, uint32_t item,
        EditorCollisionFilterKind filter, const char *mask, bool enabled) {
    EditorCommand command = {.type = EDITOR_COMMAND_COLLISION_FILTER_SET,
        .data.collision_filter_set = {.kind = kind, .object = object,
            .parent = parent, .item = item, .filter = filter, .enabled = enabled}};
    snprintf(command.data.collision_filter_set.mask,
        sizeof(command.data.collision_filter_set.mask), "%s", mask);
    (void)editor_command_execute(project, &command);
}

static bool editor_checkbox_label_left(const char *id, const TextAsset *label,
    UIRect bounds, bool *checked) {
    UIButtonResult interaction;
    UIRect box;
    Color background;

    if(id == NULL || label == NULL || checked == NULL) return false;
    interaction = rohr_ui_interaction(id, bounds);
    if(interaction.clicked) *checked = !*checked;
    background = interaction.pressed ? (Color){58, 65, 78, 255} :
        interaction.hovered || interaction.focused ? (Color){67, 75, 90, 255} :
        (Color){48, 54, 66, 255};
    rohr_ui_surface(bounds, background);
    box = (UIRect){bounds.x + bounds.width - bounds.height + 4.0f,
        bounds.y + 4.0f, bounds.height - 8.0f, bounds.height - 8.0f};
    rohr_ui_surface(box, (Color){22, 25, 31, 255});
    rohr_ui_border(box, 2.0f, (Color){8, 9, 12, 255});
    if(*checked) {
        rohr_ui_surface((UIRect){box.x + 5.0f, box.y + 5.0f,
            box.width - 10.0f, box.height - 10.0f},
            (Color){225, 230, 240, 255});
    }
    rohr_ui_label(label, (UIRect){bounds.x + 4.0f, bounds.y,
        bounds.width - bounds.height - 4.0f, bounds.height});
    return interaction.clicked;
}

static bool editor_collision_mask_menu_draw(const char *id_prefix,
    EditorProject *project, RohrCollisionCategoryMask *active_masks,
    EditorItemKind target_kind, EditorObjectId object, uint32_t parent,
    uint32_t item, EditorCollisionFilterKind filter,
    FontAsset *font, TextAsset labels[EDITOR_COLLISION_MASK_MAX],
    char caches[EDITOR_COLLISION_MASK_MAX][EDITOR_OBJECT_NAME_MAX],
    char *name, size_t name_capacity, TextAsset *name_field,
    const TextAsset *add_label, float x, float y, float width,
    bool *field_active, size_t *row_count) {
    size_t inactive[EDITOR_COLLISION_MASK_MAX];
    size_t inactive_count = 0;
    UIFieldResult field_result;

    if(id_prefix == NULL || project == NULL || active_masks == NULL ||
            font == NULL || name == NULL || name_field == NULL ||
            add_label == NULL || field_active == NULL || row_count == NULL) return false;
    field_result = rohr_ui_field(id_prefix,
        (UIFieldBinding){.kind = UI_FIELD_STRING, .string = name,
            .string_capacity = name_capacity}, name_field,
        (UIRect){x, y, width * 0.72f, 28.0f}, NULL);
    *field_active = *field_active || field_result.active;
    {
        char add_id[112];
        snprintf(add_id, sizeof(add_id), "%s.add", id_prefix);
        if(rohr_ui_button(add_id, add_label,
                (UIRect){x + width * 0.72f, y, width * 0.28f, 28.0f}, NULL).clicked) {
            EditorCommand command = {.type = EDITOR_COMMAND_COLLISION_MASK_ADD};
            EditorCommandResult result;
            size_t mask_index;
            snprintf(command.data.collision_mask_add.name,
                sizeof(command.data.collision_mask_add.name), "%s", name);
            result = editor_command_execute(project, &command);
            mask_index = result.kind == ERROR_RESULT_VALUE ?
                (size_t)result.result.object : SIZE_MAX;
            if(mask_index < project->collision_mask_count) {
                editor_collision_filter_set(project, target_kind, object, parent,
                    item, filter, project->collision_masks[mask_index].name, true);
                name[0] = '\0';
                (void)rohr_graphics_text_value_set(name_field, "");
            }
        }
    }
    *row_count = 1;
    for(size_t mask = 0; mask < project->collision_mask_count; mask += 1) {
        char mask_id[112];
        uint64_t bit = UINT64_C(1) << mask;
        bool enabled = (*active_masks & bit) != 0;
        if(!enabled) {
            inactive[inactive_count++] = mask;
            continue;
        }
        if(!editor_named_text_sync(font, project->collision_masks[mask].name,
                &labels[mask], caches[mask], EDITOR_OBJECT_NAME_MAX)) return false;
        snprintf(mask_id, sizeof(mask_id), "%s.%zu", id_prefix, mask);
        if(editor_checkbox(mask_id, &labels[mask],
                (UIRect){x, y + (float)*row_count * 30.0f, width, 28.0f},
                &enabled)) editor_collision_filter_set(project, target_kind,
                    object, parent, item, filter,
                    project->collision_masks[mask].name, enabled);
        *row_count += 1;
    }
    for(size_t i = 1; i < inactive_count; i += 1) {
        size_t value = inactive[i];
        size_t j = i;
        while(j > 0 && strcmp(project->collision_masks[value].name,
                project->collision_masks[inactive[j - 1]].name) < 0) {
            inactive[j] = inactive[j - 1];
            j -= 1;
        }
        inactive[j] = value;
    }
    for(size_t i = 0; i < inactive_count; i += 1) {
        size_t mask = inactive[i];
        char mask_id[112];
        uint64_t bit = UINT64_C(1) << mask;
        bool enabled = false;
        if(!editor_named_text_sync(font, project->collision_masks[mask].name,
                &labels[mask], caches[mask], EDITOR_OBJECT_NAME_MAX)) return false;
        snprintf(mask_id, sizeof(mask_id), "%s.%zu", id_prefix, mask);
        if(editor_checkbox(mask_id, &labels[mask],
                (UIRect){x, y + (float)*row_count * 30.0f, width, 28.0f},
                &enabled)) editor_collision_filter_set(project, target_kind,
                    object, parent, item, filter,
                    project->collision_masks[mask].name, enabled);
        *row_count += 1;
    }
    rohr_ui_border((UIRect){x, y, width, (float)*row_count * 30.0f - 2.0f},
        2.0f, (Color){0, 0, 0, 255});
    return true;
}

static UIFieldResult editor_property_name_field(const char *id, char *name,
    size_t capacity, TextAsset *display, UIRect bounds) {
    UIFieldResult result = rohr_ui_field(id,
        (UIFieldBinding){.kind = UI_FIELD_STRING, .string = name,
            .string_capacity = capacity}, display, bounds, NULL);
    if(result.changed) editor_project_property_name_format(name, capacity, name);
    return result;
}

static EditorRigidBody *editor_selected_body_get(EditorObject *object,
    const EditorViewportState *state) {
    return object == NULL || state == NULL ? NULL :
        editor_project_rigid_body_get(object, state->selected_rigid_body);
}

static EditorHitbox *editor_selected_hitbox_get(EditorObject *object,
    const EditorViewportState *state) {
    EditorRigidBody *body = editor_selected_body_get(object, state);
    return body == NULL ? NULL : editor_project_hitbox_get(body, state->selected_hitbox);
}

static void editor_anchor_preview_set(EditorViewportState *state,
    EditorObject *object, EditorAnchorId id) {
    EditorAnchor *anchor;
    if(state == NULL || object == NULL || id == 0) return;
    anchor = editor_project_anchor_get(object, id);
    if(anchor == NULL) return;
    state->preview_anchor = anchor->id;
    state->preview_rigid_body = anchor->rigid_body;
}

static EditorJoint *editor_selected_joint_get(EditorObject *object,
    const EditorViewportState *state) {
    if(object == NULL || state == NULL) return NULL;
    for(size_t i = 0; i < object->joint_count; i += 1) {
        if(object->joint_items[i].id == state->selected_joint) {
            return &object->joint_items[i];
        }
    }
    return NULL;
}

static EditorSoftBody *editor_selected_soft_body_get(EditorObject *object,
        const EditorViewportState *state) {
    if(object == NULL || state == NULL) return NULL;
    for(size_t i = 0; i < object->soft_body_count; i += 1) {
        if(object->soft_body_items[i].id == state->selected_soft_body) {
            return &object->soft_body_items[i];
        }
    }
    return NULL;
}

static bool editor_auto_shape_apply(EditorProject *project,
        EditorViewportState *state, EditorViewportMode parent_mode,
        EditorAutoShapeConfig config) {
    EditorObject *object = editor_project_selected_get(project);
    EditorCommand command = {.type = EDITOR_COMMAND_AUTO_SHAPE};
    EditorCommandResult result;

    if(object == NULL || state == NULL) return false;
    command.data.auto_shape.object = object->id;
    command.data.auto_shape.config = config;
    command.data.auto_shape.point_count = state->auto_shape_point_count;
    memcpy(command.data.auto_shape.points, state->auto_shape_points,
        state->auto_shape_point_count * sizeof(*state->auto_shape_points));
    if(parent_mode == EDITOR_VIEWPORT_HITBOX) {
        EditorRigidBody *body = editor_selected_body_get(object, state);
        EditorHitbox *hitbox = editor_selected_hitbox_get(object, state);
        if(body == NULL || hitbox == NULL) return false;
        command.data.auto_shape.kind = EDITOR_ITEM_HITBOX;
        command.data.auto_shape.parent = body->id;
        command.data.auto_shape.item = hitbox->id;
    } else if(parent_mode == EDITOR_VIEWPORT_SOFT_BODY) {
        EditorSoftBody *body = editor_selected_soft_body_get(object, state);
        if(body == NULL) return false;
        command.data.auto_shape.kind = EDITOR_ITEM_SOFT_BODY;
        command.data.auto_shape.item = body->id;
    } else return false;
    result = editor_command_execute(project, &command);
    if(result.kind == ERROR_RESULT_ERROR) {
        fprintf(stderr, "%s\n", result.result.error.message);
        return false;
    }
    return true;
}

static size_t editor_auto_shape_hitbox_points_capture(
        EditorViewportState *state, const EditorObject *object,
        const EditorRigidBody *body, const EditorHitbox *hitbox) {
    if(state == NULL || object == NULL || body == NULL || hitbox == NULL) return 0;
    state->auto_shape_point_count = 0;
    for(size_t vertex = 0; vertex < hitbox->vertex_count; vertex += 1) {
        if(!editor_selection_path_check(state, EDITOR_SELECTION_VERTEX,
                object->id, body->id, hitbox->id,
                hitbox->vertices[vertex].id)) continue;
        state->auto_shape_points[state->auto_shape_point_count++] =
            hitbox->vertices[vertex].id;
    }
    return state->auto_shape_point_count;
}

static size_t editor_auto_shape_soft_body_points_capture(
        EditorViewportState *state, const EditorObject *object,
        const EditorSoftBody *body) {
    if(state == NULL || object == NULL || body == NULL) return 0;
    state->auto_shape_point_count = 0;
    for(size_t node = 0; node < body->node_count; node += 1) {
        if(!editor_selection_path_check(state, EDITOR_SELECTION_SOFT_NODE,
                object->id, body->id, 0, body->nodes[node].id)) continue;
        state->auto_shape_points[state->auto_shape_point_count++] =
            body->nodes[node].id;
    }
    return state->auto_shape_point_count;
}

static EditorSoftNode *editor_selected_soft_node_get(EditorSoftBody *body,
    const EditorViewportState *state) {
    if(body == NULL || state == NULL) return NULL;
    for(size_t i = 0; i < body->node_count; i += 1) {
        if(body->nodes[i].id == state->selected_soft_node) return &body->nodes[i];
    }
    return NULL;
}

static EditorSoftBeam *editor_selected_soft_beam_get(EditorSoftBody *body,
    const EditorViewportState *state) {
    if(body == NULL || state == NULL) return NULL;
    for(size_t i = 0; i < body->beam_count; i += 1) {
        if(body->beams[i].id == state->selected_soft_beam) return &body->beams[i];
    }
    return NULL;
}

static EditorSoftArea *editor_selected_soft_area_get(EditorSoftBody *body,
        const EditorViewportState *state) {
    if(body == NULL || state == NULL) return NULL;
    for(size_t i = 0; i < body->area_count; i += 1) {
        if(body->areas[i].id == state->selected_soft_area) return &body->areas[i];
    }
    return NULL;
}

static EditorSoftBeam *editor_soft_area_beam_get(EditorSoftBody *body,
        const EditorSoftArea *area, size_t edge) {
    EditorSoftNodeId a;
    EditorSoftNodeId b;
    if(body == NULL || area == NULL || edge >= area->node_count) return NULL;
    a = area->nodes[edge];
    b = area->nodes[(edge + 1) % area->node_count];
    for(size_t i = 0; i < body->beam_count; i += 1) {
        if((body->beams[i].node_a == a && body->beams[i].node_b == b) ||
                (body->beams[i].node_a == b && body->beams[i].node_b == a)) {
            return &body->beams[i];
        }
    }
    return NULL;
}

static bool editor_single_selected_delete(
    EditorProject *project,
    EditorViewportState *viewport_state
) {
    EditorObject *selected;

    if(project == NULL || viewport_state == NULL) return false;
    selected = editor_project_selected_get(project);
    if(selected == NULL) return false;
    if(viewport_state->selection == EDITOR_SELECTION_OBJECT) {
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_REMOVE,
            .data.item_remove = {EDITOR_ITEM_OBJECT, selected->id, 0, 0, 0}};
        size_t index = (size_t)(selected - project->objects);
        EditorCommandResult result = editor_command_execute(project, &command);
        if(result.kind == ERROR_RESULT_ERROR) return false;
        editor_viewport_hitbox_editor_exit(viewport_state);
        if(index < project->object_count) {
            (void)editor_project_object_select(project, project->objects[index].id);
            viewport_state->selection = EDITOR_SELECTION_OBJECT;
        }
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_HITBOX) {
        EditorRigidBody *body = editor_selected_body_get(selected, viewport_state);
        EditorHitbox *hitbox = editor_selected_hitbox_get(selected, viewport_state);
        size_t index;
        if(body == NULL || hitbox == NULL) return false;
        index = (size_t)(hitbox - body->hitboxes);
        {
            EditorCommand command = {.type = EDITOR_COMMAND_ITEM_REMOVE,
                .data.item_remove = {EDITOR_ITEM_HITBOX, selected->id, body->id,
                    hitbox->id, 0}};
            if(editor_command_execute(project, &command).kind == ERROR_RESULT_ERROR)
                return false;
        }
        viewport_state->mode = EDITOR_VIEWPORT_RIGID_BODY;
        if(index < body->hitbox_count) {
            viewport_state->selection = EDITOR_SELECTION_HITBOX;
            viewport_state->selected_hitbox = body->hitboxes[index].id;
        } else {
            viewport_state->selection = EDITOR_SELECTION_RIGID_BODY;
        }
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_RIGID_BODY) {
        EditorRigidBody *body = editor_selected_body_get(selected, viewport_state);
        size_t index;
        if(body == NULL) return false;
        index = (size_t)(body - selected->rigid_bodies);
        {
            EditorCommand command = {.type = EDITOR_COMMAND_ITEM_REMOVE,
                .data.item_remove = {EDITOR_ITEM_RIGID_BODY, selected->id, 0,
                    body->id, 0}};
            if(editor_command_execute(project, &command).kind == ERROR_RESULT_ERROR)
                return false;
        }
        viewport_state->mode = EDITOR_VIEWPORT_OBJECT;
        if(index < selected->rigid_body_count) {
            viewport_state->selection = EDITOR_SELECTION_RIGID_BODY;
            viewport_state->selected_rigid_body = selected->rigid_bodies[index].id;
        } else if(selected->joint_count > 0) {
            viewport_state->selection = EDITOR_SELECTION_JOINT;
            viewport_state->selected_joint = selected->joint_items[0].id;
        } else if(selected->soft_body_count > 0) {
            viewport_state->selection = EDITOR_SELECTION_SOFT_BODY;
            viewport_state->selected_soft_body = selected->soft_body_items[0].id;
        } else {
            viewport_state->selection = EDITOR_SELECTION_OBJECT;
        }
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_JOINT) {
        EditorJoint *joint = editor_selected_joint_get(selected, viewport_state);
        size_t index;
        if(joint == NULL) return false;
        index = (size_t)(joint - selected->joint_items);
        {
            EditorCommand command = {.type = EDITOR_COMMAND_ITEM_REMOVE,
                .data.item_remove = {EDITOR_ITEM_JOINT, selected->id, 0,
                    joint->id, 0}};
            if(editor_command_execute(project, &command).kind == ERROR_RESULT_ERROR)
                return false;
        }
        viewport_state->mode = EDITOR_VIEWPORT_OBJECT;
        if(index < selected->joint_count) {
            viewport_state->selection = EDITOR_SELECTION_JOINT;
            viewport_state->selected_joint = selected->joint_items[index].id;
        } else if(selected->soft_body_count > 0) {
            viewport_state->selection = EDITOR_SELECTION_SOFT_BODY;
            viewport_state->selected_soft_body = selected->soft_body_items[0].id;
        } else {
            viewport_state->selection = EDITOR_SELECTION_OBJECT;
        }
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_ANCHOR) {
        EditorAnchor *anchor = editor_project_anchor_get(
            selected, viewport_state->selected_anchor);
        size_t index;
        if(anchor == NULL) return false;
        index = (size_t)(anchor - selected->anchors);
        {
            EditorCommand command = {.type = EDITOR_COMMAND_ITEM_REMOVE,
                .data.item_remove = {EDITOR_ITEM_ANCHOR, selected->id, 0,
                    anchor->id, 0}};
            if(editor_command_execute(project, &command).kind == ERROR_RESULT_ERROR)
                return false;
        }
        viewport_state->mode = EDITOR_VIEWPORT_JOINT;
        if(index < selected->anchor_count) {
            viewport_state->selection = EDITOR_SELECTION_ANCHOR;
            viewport_state->selected_anchor = selected->anchors[index].id;
        } else if(selected->anchor_count > 0) {
            viewport_state->selection = EDITOR_SELECTION_ANCHOR;
            viewport_state->selected_anchor =
                selected->anchors[selected->anchor_count - 1].id;
        } else {
            viewport_state->selection = EDITOR_SELECTION_JOINT;
            viewport_state->selected_anchor = 0;
        }
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_SOFT_BODY) {
        EditorSoftBody *body = editor_selected_soft_body_get(selected, viewport_state);
        size_t index;
        if(body == NULL) return false;
        index = (size_t)(body - selected->soft_body_items);
        {
            EditorCommand command = {.type = EDITOR_COMMAND_ITEM_REMOVE,
                .data.item_remove = {EDITOR_ITEM_SOFT_BODY, selected->id, 0,
                    body->id, 0}};
            if(editor_command_execute(project, &command).kind == ERROR_RESULT_ERROR)
                return false;
        }
        viewport_state->mode = EDITOR_VIEWPORT_OBJECT;
        if(index < selected->soft_body_count) {
            viewport_state->selection = EDITOR_SELECTION_SOFT_BODY;
            viewport_state->selected_soft_body = selected->soft_body_items[index].id;
        } else {
            viewport_state->selection = EDITOR_SELECTION_OBJECT;
        }
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_SOFT_NODE) {
        EditorSoftBody *body = editor_selected_soft_body_get(selected, viewport_state);
        EditorSoftNode *node = editor_selected_soft_node_get(body, viewport_state);
        size_t index;
        if(body == NULL || node == NULL) return false;
        index = (size_t)(node - body->nodes);
        {
            EditorCommand command = {.type = EDITOR_COMMAND_ITEM_REMOVE,
                .data.item_remove = {EDITOR_ITEM_SOFT_NODE, selected->id, body->id,
                    node->id, 0}};
            if(editor_command_execute(project, &command).kind == ERROR_RESULT_ERROR)
                return false;
        }
        viewport_state->mode = EDITOR_VIEWPORT_SOFT_BODY;
        if(index < body->node_count) {
            viewport_state->selection = EDITOR_SELECTION_SOFT_NODE;
            viewport_state->selected_soft_node = body->nodes[index].id;
        } else if(body->beam_count > 0) {
            viewport_state->selection = EDITOR_SELECTION_SOFT_BEAM;
            viewport_state->selected_soft_beam = body->beams[0].id;
        } else {
            viewport_state->selection = EDITOR_SELECTION_SOFT_BODY;
        }
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_ORIGIN &&
            viewport_state->selected_origin_kind != EDITOR_ORIGIN_NONE) {
        viewport_state->mode = EDITOR_VIEWPORT_ORIGIN;
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_SOFT_BEAM) {
        EditorSoftBody *body = editor_selected_soft_body_get(selected, viewport_state);
        EditorSoftBeam *beam = editor_selected_soft_beam_get(body, viewport_state);
        size_t index;
        if(body == NULL || beam == NULL) return false;
        index = (size_t)(beam - body->beams);
        {
            EditorCommand command = {.type = EDITOR_COMMAND_ITEM_REMOVE,
                .data.item_remove = {EDITOR_ITEM_SOFT_BEAM, selected->id, body->id,
                    beam->id, 0}};
            if(editor_command_execute(project, &command).kind == ERROR_RESULT_ERROR)
                return false;
        }
        viewport_state->mode = EDITOR_VIEWPORT_SOFT_BODY;
        if(index < body->beam_count) {
            viewport_state->selection = EDITOR_SELECTION_SOFT_BEAM;
            viewport_state->selected_soft_beam = body->beams[index].id;
        } else {
            viewport_state->selection = EDITOR_SELECTION_SOFT_BODY;
        }
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_VERTEX) {
        EditorRigidBody *body = editor_selected_body_get(selected, viewport_state);
        EditorHitbox *hitbox = editor_selected_hitbox_get(selected, viewport_state);
        uint32_t index = viewport_state->selected_vertex;
        EditorCommand command;
        if(body == NULL || hitbox == NULL || index >= hitbox->vertex_count) return false;
        command = (EditorCommand){.type = EDITOR_COMMAND_ITEM_REMOVE,
            .data.item_remove = {EDITOR_ITEM_VERTEX, selected->id, body->id,
                hitbox->id, hitbox->vertices[index].id}};
        if(editor_command_execute(project, &command).kind == ERROR_RESULT_ERROR)
            return false;
        viewport_state->mode = EDITOR_VIEWPORT_HITBOX;
        if(index < hitbox->vertex_count) {
            viewport_state->selection = EDITOR_SELECTION_VERTEX;
            viewport_state->selected_vertex = index;
        } else if(hitbox->vertex_count > 0) {
            viewport_state->selection = EDITOR_SELECTION_LINE;
            viewport_state->selected_line = 0;
        } else {
            viewport_state->selection = EDITOR_SELECTION_HITBOX;
        }
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_LINE) {
        EditorRigidBody *body = editor_selected_body_get(selected, viewport_state);
        EditorHitbox *hitbox = editor_selected_hitbox_get(selected, viewport_state);
        uint32_t index = viewport_state->selected_line;
        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_REMOVE,
            .data.item_remove = {EDITOR_ITEM_LINE, selected->id,
                body == NULL ? 0 : body->id, hitbox == NULL ? 0 : hitbox->id, index}};
        if(editor_command_execute(project, &command).kind == ERROR_RESULT_ERROR)
            return false;
        viewport_state->mode = EDITOR_VIEWPORT_HITBOX;
        if(index < hitbox->vertex_count) {
            viewport_state->selection = EDITOR_SELECTION_LINE;
            viewport_state->selected_line = index;
        } else {
            viewport_state->selection = EDITOR_SELECTION_HITBOX;
        }
        return true;
    }
    return false;
}

static bool editor_selected_delete(EditorProject *project,
        EditorViewportState *viewport_state) {
    bool removed;
    EditorSelectionRef fallback;
    if(project == NULL || viewport_state == NULL) return false;
    if(viewport_state->selected_item_count > 1)
        return editor_navigation_multi_selection_delete(project, viewport_state,
            editor_operation_history);
    removed = editor_single_selected_delete(project, viewport_state);
    if(!removed) return false;
    editor_viewport_selection_clear(viewport_state);
    if(editor_viewport_selection_ref_get(project, viewport_state, &fallback))
        (void)editor_viewport_selection_set(project, viewport_state, fallback, false);
    return true;
}

static bool editor_selection_nudge(EditorProject *project,
        EditorViewportState *viewport_state, EditorHistory *history,
        Vec2D screen_delta) {
    bool moved;
    if(project == NULL || viewport_state == NULL || history == NULL) return false;
    if(viewport_state->selected_item_count < 2)
        return editor_viewport_selection_nudge(viewport_state, project, screen_delta);
    if(!editor_history_transaction_begin(history)) return false;
    for(size_t i = 0; i < viewport_state->selected_item_count; i += 1)
        if(!editor_history_transaction_object_track(history,
                viewport_state->selected_items[i].object)) {
            editor_history_transaction_cancel(history);
            return false;
        }
    moved = editor_viewport_selection_nudge(viewport_state, project, screen_delta);
    if(!moved) {
        editor_history_transaction_cancel(history);
        return false;
    }
    return editor_history_transaction_end(history);
}

typedef struct EditorHierarchyDragState {
    bool active;
    bool dragging;
    bool target_valid;
    bool transaction_active;
    bool source_was_selected;
    bool target_after;
    float start_y;
    EditorSelectionRef source;
    EditorSelectionRef target;
} EditorHierarchyDragState;

static bool editor_hierarchy_ref_equal(EditorSelectionRef first,
        EditorSelectionRef second) {
    return first.kind == second.kind && first.object == second.object &&
        first.parent == second.parent && first.container == second.container &&
        first.item == second.item;
}

static bool editor_hierarchy_object_child_check(EditorHierarchySelection kind) {
    return kind == EDITOR_SELECTION_RIGID_BODY || kind == EDITOR_SELECTION_JOINT ||
        kind == EDITOR_SELECTION_SOFT_BODY;
}

static bool editor_hierarchy_soft_child_check(EditorHierarchySelection kind) {
    return kind == EDITOR_SELECTION_SOFT_NODE ||
        kind == EDITOR_SELECTION_SOFT_BEAM || kind == EDITOR_SELECTION_SOFT_AREA;
}

static void editor_hierarchy_drag_row(EditorHierarchyDragState *drag,
        const EditorViewportState *selection, EditorSelectionRef ref,
        UIRect bounds, UIButtonResult result, Position pointer,
        MouseButtonState primary, float scroll_offset, bool last) {
    bool pointer_in_row;
    if(drag == NULL || selection == NULL) return;
    bounds.y -= scroll_offset;
    if(!drag->active && result.pressed && primary == MOUSE_BUTTON_STATE_PRESSED) {
        drag->active = true;
        drag->source = ref;
        drag->target = ref;
        drag->target_valid = true;
        drag->start_y = pointer.y;
        drag->source_was_selected = editor_viewport_selection_contains(selection, ref);
    }
    if(!drag->active || (primary != MOUSE_BUTTON_STATE_PRESSED &&
            primary != MOUSE_BUTTON_STATE_DOWN)) return;
    if(fabsf(pointer.y - drag->start_y) >= 4.0f) drag->dragging = true;
    if(drag->dragging && (editor_viewport_selection_contains(selection, ref) ||
            editor_hierarchy_ref_equal(drag->source, ref))) {
        Color border = {255, 215, 70, 255};
        (void)rohr_graphics_screen_rect_draw(bounds.x, bounds.y,
            bounds.width, 2.0f, border);
        (void)rohr_graphics_screen_rect_draw(bounds.x,
            bounds.y + bounds.height - 2.0f, bounds.width, 2.0f, border);
        (void)rohr_graphics_screen_rect_draw(bounds.x, bounds.y,
            2.0f, bounds.height, border);
        (void)rohr_graphics_screen_rect_draw(
            bounds.x + bounds.width - 2.0f, bounds.y,
            2.0f, bounds.height, border);
    }
    pointer_in_row = pointer.x >= bounds.x &&
        pointer.x <= bounds.x + bounds.width && pointer.y >= bounds.y &&
        (pointer.y <= bounds.y + bounds.height || last);
    if(drag->dragging && pointer_in_row &&
            (ref.kind == drag->source.kind ||
                (editor_hierarchy_object_child_check(ref.kind) &&
                    editor_hierarchy_object_child_check(drag->source.kind)) ||
                (editor_hierarchy_soft_child_check(ref.kind) &&
                    editor_hierarchy_soft_child_check(drag->source.kind))) &&
            ref.object == drag->source.object &&
            ref.parent == drag->source.parent &&
            ref.container == drag->source.container) {
        drag->target = ref;
        drag->target_valid = true;
        drag->target_after = last &&
            pointer.y >= bounds.y + bounds.height * 0.5f;
    }
}

static bool editor_hierarchy_drag_update(EditorHierarchyDragState *drag,
        EditorProject *project, EditorViewportState *selection,
        EditorHistory *history, MouseButtonState primary) {
    bool changed = false;
    if(drag == NULL || !drag->active) return false;
    if(drag->dragging && !drag->source_was_selected &&
            !editor_viewport_selection_contains(selection, drag->source)) {
        (void)editor_viewport_selection_set(project, selection,
            drag->source, false);
        changed = true;
    }
    if(drag->dragging && drag->target_valid &&
            !editor_hierarchy_ref_equal(drag->source, drag->target) &&
            (primary == MOUSE_BUTTON_STATE_PRESSED ||
                primary == MOUSE_BUTTON_STATE_DOWN)) {
        if(!drag->transaction_active) {
            if(!editor_history_transaction_begin(history)) return false;
            if(!(drag->source.kind == EDITOR_SELECTION_OBJECT ?
                    editor_history_transaction_object_order_track(history) :
                    editor_history_transaction_object_track(history,
                        drag->source.object))) {
                editor_history_transaction_cancel(history);
                return false;
            }
            drag->transaction_active = true;
        }
        changed = editor_navigation_selection_reorder(project, selection,
            drag->source, drag->target, drag->target_after, NULL);
    }
    if(primary == MOUSE_BUTTON_STATE_RELEASED) {
        if(drag->transaction_active) {
            if(!editor_history_transaction_end(history))
                editor_history_transaction_cancel(history);
            else changed = true;
        }
        if(drag->dragging && !drag->source_was_selected)
            (void)editor_viewport_selection_set(project, selection,
                drag->source, false);
        *drag = (EditorHierarchyDragState){0};
    }
    return changed;
}

static bool editor_open_item_delete(
    EditorProject *project,
    EditorViewportState *viewport_state
) {
    if(!editor_navigation_open_item_selection_set(viewport_state)) return false;
    return editor_selected_delete(project, viewport_state);
}
int main(void) {
    KeyboardState keyboard = {0};
    MouseState mouse = {0};
    float viewport_wheel_y = 0.0f;
    bool viewport_context_open = false;
    Position viewport_context_position = {0};
    FontAsset font = {0};
    FontAsset notification_font = {0};
    TextAsset hitbox_editor_label = {0};
    TextAsset auto_shape_label = {0};
    TextAsset triangle_label = {0};
    TextAsset rectangle_label = {0};
    TextAsset circle_label = {0};
    TextAsset equilateral_label = {0};
    TextAsset isosceles_label = {0};
    TextAsset scalene_label = {0};
    TextAsset width_label = {0};
    TextAsset height_label = {0};
    TextAsset apex_offset_label = {0};
    TextAsset file_label = {0};
    TextAsset edit_label = {0};
    TextAsset undo_label = {0};
    TextAsset redo_label = {0};
    TextAsset build_label = {0};
    TextAsset generate_c_label = {0};
    TextAsset compile_label = {0};
    TextAsset build_project_label = {0};
    TextAsset world_view_label = {0};
    TextAsset local_view_label = {0};
    TextAsset collision_label = {0};
    TextAsset particle_label = {0};
    TextAsset particle_ring_color_label = {0};
    TextAsset particle_fill_color_label = {0};
    TextAsset auto_fit_label = {0};
    TextAsset context_action_one_label = {0};
    TextAsset context_action_two_label = {0};
    TextAsset context_action_three_label = {0};
    TextAsset collision_category_label = {0};
    TextAsset collide_with_label = {0};
    TextAsset add_label = {0};
    TextAsset collision_mask_name_field = {0};
    char collision_mask_name[EDITOR_OBJECT_NAME_MAX] = {0};
    TextAsset collision_mask_labels[EDITOR_COLLISION_MASK_MAX] = {0};
    char collision_mask_cache[EDITOR_COLLISION_MASK_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    TextAsset view_label = {0};
    TextAsset settings_label = {0};
    TextAsset new_label = {0};
    TextAsset open_label = {0};
    TextAsset load_frame_label = {0};
    TextAsset create_project_label = {0};
    TextAsset save_label = {0};
    TextAsset close_label = {0};
    TextAsset exit_label = {0};
    TextAsset unsaved_changes_label = {0};
    TextAsset dont_save_label = {0};
    TextAsset cancel_label = {0};
    TextAsset reset_view_label = {0};
    TextAsset grid_label = {0};
    TextAsset terminal_label = {0};
    TextAsset terminal_menu_label = {0};
    TextAsset terminal_visible_label = {0};
    TextAsset terminal_editor_operations_label = {0};
    TextAsset terminal_generated_code_label = {0};
    TextAsset terminal_build_operations_label = {0};
    TextAsset preferences_label = {0};
    TextAsset file_browser_field = {0};
    TextAsset add_object_label = {0};
    TextAsset add_sprite_label = {0};
    TextAsset add_animated_sprite_label = {0};
    TextAsset add_frame_label = {0};
    TextAsset none_label = {0};
    TextAsset add_hitbox_label = {0};
    TextAsset hitbox_label = {0};
    TextAsset add_rigid_body_label = {0};
    TextAsset add_joint_label = {0};
    TextAsset add_anchor_label = {0};
    TextAsset add_soft_body_label = {0};
    TextAsset add_node_label = {0};
    TextAsset add_beam_label = {0};
    TextAsset visibility_visible_label = {0};
    TextAsset visibility_hidden_label = {0};
    TextAsset revolute_label = {0};
    TextAsset weld_label = {0};
    TextAsset spring_label = {0};
    TextAsset body_a_label = {0};
    TextAsset body_b_label = {0};
    TextAsset rigid_body_label = {0};
    TextAsset node_a_label = {0};
    TextAsset node_b_label = {0};
    TextAsset mass_label = {0};
    TextAsset radius_label = {0};
    TextAsset gravity_label = {0};
    TextAsset friction_label = {0};
    TextAsset restitution_label = {0};
    TextAsset rest_length_label = {0};
    TextAsset damping_label = {0};
    TextAsset visual_size_label = {0};
    TextAsset dynamic_label = {0};
    TextAsset static_label = {0};
    TextAsset rotation_locked_label = {0};
    TextAsset rotation_unlocked_label = {0};
    TextAsset stiffness_label = {0};
    TextAsset rotation_label = {0};
    TextAsset origin_label = {0};
    TextAsset position_body_label = {0};
    TextAsset position_global_label = {0};
    TextAsset rotation_body_label = {0};
    TextAsset rotation_global_label = {0};
    TextAsset rigid_body_labels[EDITOR_RIGID_BODY_MAX] = {0};
    TextAsset body_hitbox_labels[EDITOR_BODY_HITBOX_MAX] = {0};
    TextAsset joint_labels[EDITOR_JOINT_MAX] = {0};
    TextAsset anchor_labels[EDITOR_ANCHOR_MAX] = {0};
    TextAsset soft_body_labels[EDITOR_SOFT_BODY_MAX] = {0};
    TextAsset soft_node_labels[EDITOR_SOFT_NODE_MAX] = {0};
    TextAsset soft_beam_labels[EDITOR_SOFT_BEAM_MAX] = {0};
    TextAsset soft_area_labels[EDITOR_SOFT_AREA_MAX] = {0};
    TextAsset sprite_labels[64] = {0};
    TextAsset animated_sprite_labels[32] = {0};
    char rigid_body_cache[EDITOR_RIGID_BODY_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char body_hitbox_cache[EDITOR_BODY_HITBOX_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char joint_cache[EDITOR_JOINT_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char anchor_cache[EDITOR_ANCHOR_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char soft_body_cache[EDITOR_SOFT_BODY_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char soft_node_cache[EDITOR_SOFT_NODE_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char soft_beam_cache[EDITOR_SOFT_BEAM_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char soft_area_cache[EDITOR_SOFT_AREA_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char sprite_cache[64][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char animated_sprite_cache[32][EDITOR_OBJECT_NAME_MAX] = {{0}};
    TextAsset border_color_label = {0};
    TextAsset surface_color_label = {0};
    TextAsset node_color_label = {0};
    TextAsset beam_color_label = {0};
    TextAsset area_color_label = {0};
    TextAsset inherit_label = {0};
    TextAsset color_picker_hex_field = {0};
    TextAsset color_picker_opacity_field = {0};
    TextAsset opacity_label = {0};
    EditorColorPicker color_picker = {0};
    uint32_t boundary_beam_color = UINT32_C(0xffffffff);
    EditorSoftAreaId boundary_beam_color_area = 0;
    TextAsset vertices_label = {0};
    TextAsset lines_label = {0};
    TextAsset vertex_labels[EDITOR_HITBOX_VERTEX_MAX] = {0};
    TextAsset line_labels[EDITOR_HITBOX_VERTEX_MAX] = {0};
    char vertex_name_cache[EDITOR_HITBOX_VERTEX_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    char line_name_cache[EDITOR_HITBOX_VERTEX_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    TextAsset lock_label = {0};
    TextAsset unlock_label = {0};
    TextAsset add_vertex_label = {0};
    TextAsset delete_hitbox_label = {0};
    TextAsset delete_vertex_label = {0};
    TextAsset delete_line_label = {0};
    TextAsset delete_object_label = {0};
    TextAsset delete_rigid_body_label = {0};
    TextAsset delete_joint_label = {0};
    TextAsset delete_anchor_label = {0};
    TextAsset delete_soft_body_label = {0};
    TextAsset delete_node_label = {0};
    TextAsset delete_beam_label = {0};
    TextAsset constrained_label = {0};
    TextAsset x_label = {0};
    TextAsset y_label = {0};
    TextAsset length_label = {0};
    TextAsset object_name_label = {0};
    TextAsset name_label = {0};
    TextAsset path_label = {0};
    TextAsset scale_x_label = {0};
    TextAsset scale_y_label = {0};
    TextAsset ticks_per_frame_label = {0};
    TextAsset time_per_frame_label = {0};
    TextAsset starting_frame_label = {0};
    TextAsset direction_label = {0};
    TextAsset follow_rotation_label = {0};
    TextAsset left_label = {0};
    TextAsset right_label = {0};
    TextAsset delete_sprite_label = {0};
    TextAsset delete_animated_sprite_label = {0};
    TextAsset x_field = {0};
    TextAsset y_field = {0};
    TextAsset length_field = {0};
    TextAsset path_field = {0};
    TextAsset object_name_labels[EDITOR_OBJECT_MAX] = {0};
    char object_name_cache[EDITOR_OBJECT_MAX][EDITOR_OBJECT_NAME_MAX] = {{0}};
    ViewportId viewport = 0;
    static EditorProject project;
    uint64_t saved_project_hash;
    EditorViewportState viewport_state = {0};
    EditorFileBrowser file_browser;
    EditorWorkspace workspace = {0};
    EditorOriginPanel origin_panel = {0};
    EditorBulkPanel bulk_panel = {0};
    EditorBuildSettingsPanel build_settings_panel = {0};
    EditorVisualSettingsPanel visual_settings_panel = {0};
    EditorGuiState gui_state = {0};
    char gui_state_path[EDITOR_WORKSPACE_PATH_MAX * 2] = {0};
    EditorNotificationPanel notification_panel = {0};
    EditorTerminalPanel terminal_panel = {0};
    SDL_Process *hidden_build_process = NULL;
    bool hidden_compile_pending = false;
    char hidden_build_directory[EDITOR_WORKSPACE_PATH_MAX] = {0};
    EditorHistory history = {0};
    EditorHierarchyDragState hierarchy_drag = {0};
    EditorWorkspaceBrowserAction workspace_browser_action =
        EDITOR_WORKSPACE_BROWSER_NONE;
    EditorObjectId sprite_browser_object = 0;
    EditorAnimatedSpriteId animation_browser_sprite = 0;
    char startup_directory[EDITOR_FILE_BROWSER_PATH_MAX] = {0};
    bool running = true;
    bool field_editing = false;
    bool panel_resizing = false;
    bool terminal_resizing = false;
    bool terminal_editor_operations = true;
    bool terminal_generated_code = true;
    bool terminal_build_operations = true;
    bool collision_category_open = false;
    bool collide_with_open = false;
    bool auto_shape_picker_open = false;
    bool auto_shape_first_field_was_active = false;
    bool auto_shape_second_field_was_active = false;
    bool auto_shape_third_field_was_active = false;
    EditorAutoShapeConfig auto_shape_config = {
        .kind = EDITOR_AUTO_SHAPE_CIRCLE,
        .triangle_kind = EDITOR_AUTO_TRIANGLE_ISOSCELES,
        .width = 100.0f,
        .height = 100.0f,
        .radius = 50.0f
    };
    EditorCloseAction close_action = EDITOR_CLOSE_NONE;
    float panel_scroll_offset = 0.0f;
    EditorViewportMode panel_scroll_mode = EDITOR_VIEWPORT_HIERARCHY;

    editor_operation_terminal = &terminal_panel;
    editor_operation_workspace = &workspace;
    editor_operation_project = &project;
    editor_operation_enabled = &terminal_editor_operations;
    editor_project_init(&project);
    if(!editor_history_init(&history, &project)) goto fail;
    editor_operation_history = &history;
    editor_command_executing_callback_set(editor_operation_command_executing, NULL);
    editor_command_executed_callback_set(editor_operation_command_write, NULL);
    editor_command_finished_callback_set(editor_operation_command_finished, NULL);
    saved_project_hash = editor_project_hash_get(&project);
    editor_viewport_state_init(&viewport_state);
    editor_navigation_state_apply(&project, &viewport_state, &project.navigation);
    editor_file_browser_init(&file_browser);
    {
        char *directory = SDL_GetCurrentDirectory();
        if(directory == NULL || strlen(directory) >= sizeof(startup_directory)) {
            if(directory != NULL) SDL_free(directory);
            goto fail;
        }
        snprintf(startup_directory, sizeof(startup_directory), "%s", directory);
        SDL_free(directory);
    }

    if(!editor_use_executable_directory() ||
            !editor_result_ok(rohr_engine_init())) goto fail;
    {
        EditorResult result = editor_gui_state_resolve(&gui_state,
            gui_state_path, sizeof(gui_state_path));
        if(editor_result_check(result)) {
            editor_result_stderr_print(result);
            goto fail;
        }
    }
    if(!editor_result_ok(rohr_graphics_start()) ||
            !editor_gui_state_presentation_apply(&gui_state)) goto fail;
    if(!rohr_graphics_aspect_ratio_auto_set(true)) goto fail;
    editor_window_layout_sync();
    {
        ViewportConfig config = rohr_viewport_config_default_get();
        ViewportIdResult result;

        config.rectangle = (ViewportRectangle){
            0.0f, 0.0f, EDITOR_VIEWPORT_WIDTH, EDITOR_WINDOW_HEIGHT
        };
        result = rohr_viewport_create(config);
        if(rohr_error_check(result)) {
            fprintf(stderr, "error %d: %s\n", (int)result.result.error,
                rohr_error_message_get(result));
            goto fail;
        }
        viewport = result.result.value;
        if(!editor_result_ok(rohr_viewport_camera_clear(viewport)) ||
                !editor_result_ok(rohr_viewport_disable_set(viewport))) goto fail;
    }
    {
        FontAssetResult result = rohr_graphics_font_load((FontDescriptor){
            .file = "assets/JetBrainsMono-BoldItalic.ttf",
            .point_size = 12.0f
        });

        if(rohr_error_check(result)) {
            fprintf(stderr, "error %d: %s\n", (int)result.result.error,
                rohr_error_message_get(result));
            goto fail;
        }
        font = result.result.value;
    }
    {
        FontAssetResult result = rohr_graphics_font_load((FontDescriptor){
            .file = "assets/JetBrainsMono-BoldItalic.ttf",
            .point_size = 9.0f
        });

        if(rohr_error_check(result)) {
            fprintf(stderr, "error %d: %s\n", (int)result.result.error,
                rohr_error_message_get(result));
            goto fail;
        }
        notification_font = result.result.value;
    }
    if(!editor_text_create(&font, "Hitbox Editor", &hitbox_editor_label) ||
            !editor_text_create(&font, "Auto Shape", &auto_shape_label) ||
            !editor_text_create(&font, "Triangle", &triangle_label) ||
            !editor_text_create(&font, "Square", &rectangle_label) ||
            !editor_text_create(&font, "Circle", &circle_label) ||
            !editor_text_create(&font, "Equilateral", &equilateral_label) ||
            !editor_text_create(&font, "Isosceles", &isosceles_label) ||
            !editor_text_create(&font, "Scalene", &scalene_label) ||
            !editor_text_create(&font, "Width", &width_label) ||
            !editor_text_create(&font, "Height", &height_label) ||
            !editor_text_create(&font, "Apex X", &apex_offset_label) ||
            !editor_text_create(&font, "File", &file_label) ||
            !editor_text_create(&font, "Edit", &edit_label) ||
            !editor_text_create(&font, "Undo    Ctrl+Z", &undo_label) ||
            !editor_text_create(&font, "Redo    Ctrl+Y", &redo_label) ||
            !editor_text_create(&font, "Build", &build_label) ||
            !editor_text_create(&font, "Generate C", &generate_c_label) ||
            !editor_text_create(&font, "Compile", &compile_label) ||
            !editor_text_create(&font, "Build Project", &build_project_label) ||
            !editor_text_create(&font, "World", &world_view_label) ||
            !editor_text_create(&font, "Local", &local_view_label) ||
            !editor_text_create(&font, "Collision", &collision_label) ||
            !editor_text_create(&font, "Particle", &particle_label) ||
            !editor_text_create(&font, "Ring Color", &particle_ring_color_label) ||
            !editor_text_create(&font, "Fill Color", &particle_fill_color_label) ||
            !editor_text_create(&font, "Auto Fit", &auto_fit_label) ||
            !editor_text_create(&font, "Action 1", &context_action_one_label) ||
            !editor_text_create(&font, "Action 2", &context_action_two_label) ||
            !editor_text_create(&font, "Action 3", &context_action_three_label) ||
            !editor_text_create(&font, "Collision Category", &collision_category_label) ||
            !editor_text_create(&font, "Collide With", &collide_with_label) ||
            !editor_text_create(&font, "Add", &add_label) ||
            !editor_text_create(&font, "", &collision_mask_name_field) ||
            !editor_text_create(&font, "View", &view_label) ||
            !editor_text_create(&font, "Settings", &settings_label) ||
            !editor_text_create(&font, "New Project", &new_label) ||
            !editor_text_create(&font, "Load Project", &open_label) ||
            !editor_text_create(&font, "Load Frame", &load_frame_label) ||
            !editor_text_create(&font, "Create Project", &create_project_label) ||
            !editor_text_create(&font, "Save", &save_label) ||
            !editor_text_create(&font, "Close", &close_label) ||
            !editor_text_create(&font, "Exit", &exit_label) ||
            !editor_text_create(&font, "Save changes before closing?",
                &unsaved_changes_label) ||
            !editor_text_create(&font, "Don't Save", &dont_save_label) ||
            !editor_text_create(&font, "Cancel", &cancel_label) ||
            !editor_text_create(&font, "Reset View", &reset_view_label) ||
            !editor_text_create(&font, "Toggle Grid", &grid_label) ||
            !editor_text_create(&font, "Terminal", &terminal_label) ||
            !editor_text_create(&font, "Terminal", &terminal_menu_label) ||
            !editor_text_create(&font, "[ ] Visible", &terminal_visible_label) ||
            !editor_text_create(&font, "[ ] Show editor operations",
                &terminal_editor_operations_label) ||
            !editor_text_create(&font, "[ ] Show generated code",
                &terminal_generated_code_label) ||
            !editor_text_create(&font, "[ ] Show build operations",
                &terminal_build_operations_label) ||
            !editor_text_create(&font, "Build", &preferences_label) ||
            !editor_text_create(&font, "", &file_browser_field) ||
            !editor_text_create(&font, "Add Object", &add_object_label) ||
            !editor_text_create(&font, "Add Sprite", &add_sprite_label) ||
            !editor_text_create(&font, "Add Animated Sprite",
                &add_animated_sprite_label) ||
            !editor_text_create(&font, "Add Frame", &add_frame_label) ||
            !editor_text_create(&font, "None", &none_label) ||
            !editor_text_create(&font, "Add Hitbox", &add_hitbox_label) ||
            !editor_text_create(&font, "Hitbox", &hitbox_label) ||
            !editor_text_create(&font, "Add Rigid Body", &add_rigid_body_label) ||
            !editor_text_create(&font, "Add Joint", &add_joint_label) ||
            !editor_text_create(&font, "Add Anchor", &add_anchor_label) ||
            !editor_text_create(&font, "Add Soft Body", &add_soft_body_label) ||
            !editor_text_create(&font, "Add Node", &add_node_label) ||
            !editor_text_create(&font, "Add Beam", &add_beam_label) ||
            !editor_text_create(&font, "x", &visibility_visible_label) ||
            !editor_text_create(&font, "", &visibility_hidden_label) ||
            !editor_text_create(&font, "Revolute", &revolute_label) ||
            !editor_text_create(&font, "Weld", &weld_label) ||
            !editor_text_create(&font, "Spring", &spring_label) ||
            !editor_text_create(&font, "Anchor A", &body_a_label) ||
            !editor_text_create(&font, "Anchor B", &body_b_label) ||
            !editor_text_create(&font, "Rigid Body", &rigid_body_label) ||
            !editor_text_create(&font, "Node A", &node_a_label) ||
            !editor_text_create(&font, "Node B", &node_b_label) ||
            !editor_text_create(&font, "Outline Color", &border_color_label) ||
            !editor_text_create(&font, "Surface Color", &surface_color_label) ||
            !editor_text_create(&font, "Node Color", &node_color_label) ||
            !editor_text_create(&font, "Beam Color", &beam_color_label) ||
            !editor_text_create(&font, "Area Color", &area_color_label) ||
            !editor_text_create(&font, "Inherit", &inherit_label) ||
            !editor_text_create(&font, "Mass", &mass_label) ||
            !editor_text_create(&font, "Radius", &radius_label) ||
            !editor_text_create(&font, "Gravity", &gravity_label) ||
            !editor_text_create(&font, "Friction", &friction_label) ||
            !editor_text_create(&font, "Restitution", &restitution_label) ||
            !editor_text_create(&font, "Rest Length", &rest_length_label) ||
            !editor_text_create(&font, "Damping", &damping_label) ||
            !editor_text_create(&font, "Visual Size", &visual_size_label) ||
            !editor_text_create(&font, "Dynamic", &dynamic_label) ||
            !editor_text_create(&font, "Static", &static_label) ||
            !editor_text_create(&font, "Rotation: Locked", &rotation_locked_label) ||
            !editor_text_create(&font, "Rotation: Unlocked", &rotation_unlocked_label) ||
            !editor_text_create(&font, "Stiffness", &stiffness_label) ||
            !editor_text_create(&font, "Rotation", &rotation_label) ||
            !editor_text_create(&font, "Origin", &origin_label) ||
            !editor_text_create(&font, "Position: Body", &position_body_label) ||
            !editor_text_create(&font, "Position: Global", &position_global_label) ||
            !editor_text_create(&font, "Orientation: Body", &rotation_body_label) ||
            !editor_text_create(&font, "Orientation: Global", &rotation_global_label) ||
            !editor_text_create(&font, "Vertices", &vertices_label) ||
            !editor_text_create(&font, "Lines", &lines_label) ||
            !editor_text_create(&font, "Lock Position", &lock_label) ||
            !editor_text_create(&font, "Unlock Position", &unlock_label) ||
            !editor_text_create(&font, "Add Vertex", &add_vertex_label) ||
            !editor_text_create(&font, "Delete Hitbox", &delete_hitbox_label) ||
            !editor_text_create(&font, "Delete Vertex", &delete_vertex_label) ||
            !editor_text_create(&font, "Delete Line", &delete_line_label) ||
            !editor_text_create(&font, "Delete Object", &delete_object_label) ||
            !editor_text_create(&font, "Delete Rigid Body", &delete_rigid_body_label) ||
            !editor_text_create(&font, "Delete Joint", &delete_joint_label) ||
            !editor_text_create(&font, "Delete Anchor", &delete_anchor_label) ||
            !editor_text_create(&font, "Delete Soft Body", &delete_soft_body_label) ||
            !editor_text_create(&font, "Delete Node", &delete_node_label) ||
            !editor_text_create(&font, "Delete Beam", &delete_beam_label) ||
            !editor_text_create(&font, "Line distance fully constrained", &constrained_label) ||
            !editor_text_create(&font, "X", &x_label) ||
            !editor_text_create(&font, "Y", &y_label) ||
            !editor_text_create(&font, "Length", &length_label) ||
            !editor_text_create(&font, "Object Name", &object_name_label) ||
            !editor_text_create(&font, "Name", &name_label) ||
            !editor_text_create(&font, "Path", &path_label) ||
            !editor_text_create(&font, "Scale X", &scale_x_label) ||
            !editor_text_create(&font, "Scale Y", &scale_y_label) ||
            !editor_text_create(&font, "Ticks / Frame", &ticks_per_frame_label) ||
            !editor_text_create(&font, "Seconds / Frame", &time_per_frame_label) ||
            !editor_text_create(&font, "Starting Frame", &starting_frame_label) ||
            !editor_text_create(&font, "Direction", &direction_label) ||
            !editor_text_create(&font, "Follow Body Rotation", &follow_rotation_label) ||
            !editor_text_create(&font, "Left", &left_label) ||
            !editor_text_create(&font, "Right", &right_label) ||
            !editor_text_create(&font, "Delete Sprite", &delete_sprite_label) ||
            !editor_text_create(&font, "Delete Animated Sprite",
                &delete_animated_sprite_label) ||
            !editor_text_create(&font, "", &x_field) ||
            !editor_text_create(&font, "", &y_field) ||
            !editor_text_create(&font, "", &length_field) ||
            !editor_text_create(&font, "", &path_field) ||
            !editor_origin_panel_create(&origin_panel, &font) ||
            !editor_bulk_panel_create(&bulk_panel, &font) ||
            !editor_build_settings_panel_create(&build_settings_panel, &font) ||
            !editor_visual_settings_panel_create(&visual_settings_panel, &font) ||
            !editor_notification_panel_create(&notification_panel, &font,
                &notification_font) ||
            !editor_terminal_panel_create(&terminal_panel, &font)) goto fail;
    if(!editor_visual_settings_panel_state_set(&visual_settings_panel,
            &gui_state, gui_state_path)) goto fail;
    terminal_panel.visible = true;
    if(!editor_terminal_panel_project_open(&terminal_panel, startup_directory)) goto fail;
    if(!editor_text_create(&font, "#FFFFFFFF", &color_picker_hex_field) ||
            !editor_text_create(&font, "100.0", &color_picker_opacity_field) ||
            !editor_text_create(&font, "Opacity %", &opacity_label)) goto fail;
    for(uint32_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
        char name[32];
        snprintf(name, sizeof(name), "vertex_%u", i + 1);
        if(!editor_text_create(&font, name, &vertex_labels[i])) goto fail;
        snprintf(name, sizeof(name), "line_%u", i + 1);
        if(!editor_text_create(&font, name, &line_labels[i])) goto fail;
    }

    while(running) {
        SDL_Event event;
        editor_project_particle_auto_fit_update(&project);
        EditorNavigationState navigation_before = editor_navigation_state_get(
            &project, &viewport_state);
        EditorSelectionRef prior_pointer_selection = {0};
        bool prior_pointer_selection_valid = editor_viewport_selection_ref_get(
            &project, &viewport_state, &prior_pointer_selection);
        bool pointer_selection_handled = false;
        EDITOR_VIEWPORT_BOTTOM = editor_terminal_panel_viewport_bottom_get(
            &terminal_panel);
        viewport_wheel_y = 0.0f;
        rohr_controller_key_states_update(&keyboard);
        rohr_controller_mouse_states_update(&mouse);
        while((event = rohr_engine_event_poll()).type != 0) {
            EditorHistoryShortcutResult shortcut = build_settings_panel.open ||
                visual_settings_panel.open ?
                (EditorHistoryShortcutResult){0} :
                editor_history_shortcut_handle(&event, workspace.open, &history);
            if(shortcut.consumed) {
                if(shortcut.restored) {
                    rohr_ui_field_focus_clear();
                    field_editing = false;
                    editor_navigation_state_apply(
                        &project, &viewport_state, &project.navigation);
                }
                continue;
            }
            bool terminal_consumed = editor_terminal_panel_event_add(&terminal_panel,
                &event, EDITOR_VIEWPORT_WIDTH, EDITOR_VIEWPORT_BOTTOM);
            if(event.type == SDL_EVENT_MOUSE_WHEEL && !terminal_consumed)
                viewport_wheel_y += event.wheel.y;
            rohr_ui_event_add(&event);
            rohr_controller_key_event_add(
                &keyboard,
                rohr_controller_keyboard_event_capture(&event));
            rohr_controller_mouse_event_add(
                &mouse,
                rohr_controller_mouse_event_capture(&event));
            if(event.type == SDL_EVENT_QUIT) {
                if(!workspace.open ||
                        editor_project_hash_get(&project) == saved_project_hash) {
                    running = false;
                } else {
                    close_action = EDITOR_CLOSE_PROGRAM;
                }
            }
        }
        editor_terminal_panel_update(&terminal_panel);
        {
            int exit_code;
            if(editor_terminal_panel_command_completion_take(&terminal_panel,
                    &exit_code)) {
                if(exit_code == 0) {
                    editor_build_notification_compile_success(&notification_panel,
                        workspace.directory, true);
                } else {
                    editor_build_notification_process_failure(
                        &notification_panel, "Configure-and-compile", exit_code,
                        true);
                }
            }
        }
        if(hidden_build_process != NULL) {
            int exit_code;
            if(SDL_WaitProcess(hidden_build_process, false, &exit_code)) {
                bool configure_finished = hidden_compile_pending;
                bool compile = configure_finished && exit_code == 0;
                hidden_compile_pending = false;
                SDL_DestroyProcess(hidden_build_process);
                hidden_build_process = compile ? editor_cmake_hidden_start(
                    hidden_build_directory, false) : NULL;
                if(exit_code != 0) {
                    editor_build_notification_process_failure(
                        &notification_panel,
                        configure_finished ? "Configure" : "Compile", exit_code,
                        false);
                } else if(compile && hidden_build_process == NULL) {
                    editor_build_notification_start_failure(&notification_panel,
                        "Build project",
                        "The compile command could not be started after configure "
                        "completed successfully.");
                } else if(!configure_finished && exit_code == 0) {
                    editor_build_notification_compile_success(&notification_panel,
                        workspace.directory, false);
                }
            }
        }
        editor_history_continuous_set(&history, field_editing ||
            mouse.button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_PRESSED ||
            mouse.button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_DOWN);
        if(notification_panel.report_open &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            notification_panel.report_open = false;
        } else if(notification_panel.log_open &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            notification_panel.log_open = false;
        } else if(build_settings_panel.open &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            build_settings_panel.open = false;
            rohr_ui_field_focus_clear();
        } else if(visual_settings_panel.open &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            visual_settings_panel.open = false;
        } else if(file_browser.active &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            if(!editor_file_browser_selection_clear(&file_browser)) {
                file_browser.active = false;
                workspace_browser_action = EDITOR_WORKSPACE_BROWSER_NONE;
                sprite_browser_object = 0;
                animation_browser_sprite = 0;
            }
        } else if(close_action != EDITOR_CLOSE_NONE &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            close_action = EDITOR_CLOSE_NONE;
        } else if(auto_shape_picker_open &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            auto_shape_picker_open = false;
        } else if((collision_category_open || collide_with_open) &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            collision_category_open = false;
            collide_with_open = false;
        } else if(color_picker.open &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            editor_color_picker_commit(&color_picker);
        } else if(!field_editing &&
                !editor_terminal_panel_focused_check(&terminal_panel) &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            if(viewport_state.selected_item_count > 1) {
                editor_viewport_selection_clear(&viewport_state);
                viewport_state.selection = EDITOR_SELECTION_NONE;
            } else if(editor_viewport_hitbox_editor_active_get(&viewport_state)) {
                editor_viewport_back(&viewport_state);
            } else if(viewport_state.mode == EDITOR_VIEWPORT_HIERARCHY &&
                    viewport_state.selection == EDITOR_SELECTION_OBJECT) {
                editor_project_selection_clear(&project);
                viewport_state.selection = EDITOR_SELECTION_NONE;
            }
        }
        if(workspace.open && !build_settings_panel.open &&
                !visual_settings_panel.open && !field_editing &&
                !color_picker.open &&
                !editor_terminal_panel_focused_check(&terminal_panel) &&
                !file_browser.active &&
                close_action == EDITOR_CLOSE_NONE &&
                viewport_state.selection != EDITOR_SELECTION_NONE &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_DELETE)) {
            (void)editor_selected_delete(&project, &viewport_state);
        }
        if(workspace.open && !build_settings_panel.open &&
                !visual_settings_panel.open && !field_editing &&
                !color_picker.open &&
                !editor_terminal_panel_focused_check(&terminal_panel) &&
                !file_browser.active &&
                close_action == EDITOR_CLOSE_NONE) {
            Position pointer = rohr_graphics_mouse_screen_position_get();
            bool pointer_in_viewport = pointer.x >= 0.0f &&
                pointer.x < EDITOR_VIEWPORT_WIDTH && pointer.y >= EDITOR_MENU_HEIGHT &&
                pointer.y < EDITOR_VIEWPORT_BOTTOM;
            bool up = rohr_controller_key_pressed_get(&keyboard, SDLK_UP);
            bool down = rohr_controller_key_pressed_get(&keyboard, SDLK_DOWN);
            bool left = rohr_controller_key_pressed_get(&keyboard, SDLK_LEFT);
            bool right = rohr_controller_key_pressed_get(&keyboard, SDLK_RIGHT);
            bool enter = rohr_controller_key_pressed_get(&keyboard, SDLK_RETURN) ||
                rohr_controller_key_pressed_get(&keyboard, SDLK_KP_ENTER);
            if(pointer_in_viewport) {
                if(up) (void)editor_selection_nudge(&project,
                    &viewport_state, &history, (Vec2D){0.0f, -1.0f});
                if(down) (void)editor_selection_nudge(&project,
                    &viewport_state, &history, (Vec2D){0.0f, 1.0f});
                if(left) (void)editor_selection_nudge(&project,
                    &viewport_state, &history, (Vec2D){-1.0f, 0.0f});
                if(right) (void)editor_selection_nudge(&project,
                    &viewport_state, &history, (Vec2D){1.0f, 0.0f});
                if(enter && viewport_state.selection != EDITOR_SELECTION_NONE) {
                    (void)editor_navigation_selected_open(&project, &viewport_state);
                }
            } else {
                bool moved = false;
                if(up) moved = rohr_ui_navigation_move(UI_NAVIGATION_UP) || moved;
                if(down) moved = rohr_ui_navigation_move(UI_NAVIGATION_DOWN) || moved;
                if(left && !rohr_ui_navigation_move(UI_NAVIGATION_LEFT)) {
                    if(editor_viewport_hitbox_editor_active_get(&viewport_state)) {
                        editor_viewport_back(&viewport_state);
                    }
                } else if(left) {
                    moved = true;
                }
                if(right && !rohr_ui_navigation_move(UI_NAVIGATION_RIGHT)) {
                    (void)rohr_ui_navigation_activate();
                } else if(right) {
                    moved = true;
                }
                if(enter && !rohr_ui_navigation_activate() &&
                        viewport_state.selection != EDITOR_SELECTION_NONE) {
                    (void)editor_navigation_selected_open(&project, &viewport_state);
                }
                if(moved) {
                    UIRect focused;
                    if(rohr_ui_navigation_focus_bounds_get(&focused)) {
                        if(focused.y < 8.0f) panel_scroll_offset += focused.y - 8.0f;
                        if(focused.y + focused.height > EDITOR_WINDOW_HEIGHT - 8.0f) {
                            panel_scroll_offset += focused.y + focused.height -
                                (EDITOR_WINDOW_HEIGHT - 8.0f);
                        }
                    }
                }
            }
        }
        if(!running) break;
        editor_window_layout_sync();

        {
            Position pointer = rohr_graphics_mouse_screen_position_get();
            MouseButtonState primary = mouse.button_states[MOUSE_BUTTON_LEFT];

            if(file_browser.active || build_settings_panel.open ||
                    visual_settings_panel.open) {
                panel_resizing = false;
            } else if(!panel_resizing && primary == MOUSE_BUTTON_STATE_PRESSED &&
                    fabsf(pointer.x - EDITOR_VIEWPORT_WIDTH) <=
                        EDITOR_DIVIDER_GRAB_WIDTH) {
                panel_resizing = true;
            }
            if(panel_resizing && (primary == MOUSE_BUTTON_STATE_PRESSED ||
                    primary == MOUSE_BUTTON_STATE_DOWN)) {
                EDITOR_VIEWPORT_WIDTH = fmaxf(EDITOR_VIEWPORT_MIN_WIDTH,
                    fminf(pointer.x, editor_window_width - EDITOR_TOOLS_MIN_WIDTH));
            }
            if(primary == MOUSE_BUTTON_STATE_RELEASED ||
                    primary == MOUSE_BUTTON_STATE_UP) {
                panel_resizing = false;
            }
            if(!terminal_panel.visible) {
                terminal_resizing = false;
            } else if(!terminal_resizing && primary == MOUSE_BUTTON_STATE_PRESSED &&
                    pointer.x >= 0.0f && pointer.x < EDITOR_VIEWPORT_WIDTH &&
                    fabsf(pointer.y - EDITOR_VIEWPORT_BOTTOM) <= 6.0f) {
                terminal_resizing = true;
            }
            if(terminal_resizing && (primary == MOUSE_BUTTON_STATE_PRESSED ||
                    primary == MOUSE_BUTTON_STATE_DOWN)) {
                bool large_overlay = !workspace.open || file_browser.active ||
                    build_settings_panel.open || visual_settings_panel.open ||
                    notification_panel.log_open || notification_panel.report_open ||
                    close_action != EDITOR_CLOSE_NONE || color_picker.open;
                float minimum_bottom = EDITOR_MENU_HEIGHT +
                    (large_overlay ? 340.0f : 100.0f);
                float bottom = fmaxf(minimum_bottom,
                    fminf(pointer.y, EDITOR_ACTION_BAR_TOP - 80.0f));
                terminal_panel.height = EDITOR_ACTION_BAR_TOP - bottom;
                EDITOR_VIEWPORT_BOTTOM = bottom;
            }
            if(primary == MOUSE_BUTTON_STATE_RELEASED ||
                    primary == MOUSE_BUTTON_STATE_UP) terminal_resizing = false;
        }

        rohr_graphics_layer_set(EDITOR_GRAPHICS_LAYER_CONTENT);
        rohr_graphics_background_draw((Color){18, 21, 27, 255});
        (void)rohr_graphics_screen_rect_draw(
            0.0f, 0.0f, EDITOR_VIEWPORT_WIDTH, EDITOR_VIEWPORT_BOTTOM,
            (Color){25, 29, 37, 255});
        (void)rohr_graphics_screen_rect_draw(
            EDITOR_VIEWPORT_WIDTH, 0.0f, EDITOR_TOOLS_WIDTH, EDITOR_WINDOW_HEIGHT,
            (Color){38, 43, 53, 255});

        (void)rohr_graphics_screen_clip_set(
            EDITOR_VIEWPORT_WIDTH, 0.0f, EDITOR_TOOLS_WIDTH, EDITOR_WINDOW_HEIGHT);
        rohr_ui_frame_begin((UIInput){
            .pointer = rohr_graphics_mouse_screen_position_get(),
            .primary_button = mouse.button_states[MOUSE_BUTTON_LEFT]
        });
        UIRect build_settings_bounds = {
            fmaxf(30.0f, EDITOR_VIEWPORT_WIDTH * 0.08f),
            EDITOR_MENU_HEIGHT + 34.0f,
            fmaxf(560.0f, EDITOR_VIEWPORT_WIDTH * 0.84f),
            fminf(500.0f, EDITOR_VIEWPORT_BOTTOM - EDITOR_MENU_HEIGHT - 68.0f)
        };
        if(build_settings_panel.open || visual_settings_panel.open ||
                notification_panel.report_open ||
                notification_panel.log_open)
            rohr_ui_modal_set(build_settings_bounds);
        if(panel_scroll_mode != viewport_state.mode) {
            panel_scroll_mode = viewport_state.mode;
            panel_scroll_offset = 0.0f;
            collision_category_open = false;
            collide_with_open = false;
        }
        panel_scroll_offset = rohr_ui_scroll_region_begin("editor.tools.scroll",
            (UIRect){EDITOR_VIEWPORT_WIDTH, EDITOR_MENU_HEIGHT,
                EDITOR_TOOLS_WIDTH, EDITOR_WINDOW_HEIGHT - EDITOR_MENU_HEIGHT},
            fmaxf(editor_panel_content_height_get(&project, &viewport_state),
                editor_bulk_panel_content_height_get(&viewport_state)),
            panel_scroll_offset, 42.0f).offset;
        viewport_state.preview_rigid_body = 0;
        viewport_state.preview_anchor = 0;
        viewport_state.preview_soft_node = 0;
        field_editing = false;
        Position hierarchy_pointer = rohr_graphics_mouse_screen_position_get();
        MouseButtonState hierarchy_primary =
            mouse.button_states[MOUSE_BUTTON_LEFT];
        if(viewport_state.selected_item_count > 1) {
            EditorBulkColorContext bulk_color = {.picker = &color_picker,
                .project = &project, .state = &viewport_state,
                .history = &history};
            field_editing = editor_bulk_panel_draw(&bulk_panel, &project,
                &viewport_state, &history, EDITOR_VIEWPORT_WIDTH,
                EDITOR_TOOLS_WIDTH, editor_bulk_color_picker_open, &bulk_color);
        } else if(viewport_state.mode == EDITOR_VIEWPORT_ORIGIN) {
            field_editing = editor_origin_panel_draw(&origin_panel,
                &(EditorPanelContext){
                    .project = &project,
                    .navigation = &viewport_state,
                    .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH
                });
        } else if(viewport_state.mode == EDITOR_VIEWPORT_PARTICLE) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorRigidBody *body = editor_selected_body_get(selected, &viewport_state);
            if(body != NULL && body->particle) {
                UIFieldResult radius_result = {0};
                rohr_ui_label(&particle_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                    42.0f, EDITOR_TOOLS_WIDTH - 20.0f, 30.0f});
                rohr_ui_label(&radius_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    84.0f, 52.0f, 26.0f});
                if(body->particle_auto_fit) {
                    editor_numeric_field_disabled_draw(&length_field,
                        body->particle_radius,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 62.0f, 84.0f,
                            fmaxf(30.0f, EDITOR_TOOLS_WIDTH - 166.0f), 26.0f});
                } else {
                    radius_result = rohr_ui_field("editor.particle.radius",
                        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                            .number = &body->particle_radius}, &length_field,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 62.0f, 84.0f,
                            fmaxf(30.0f, EDITOR_TOOLS_WIDTH - 166.0f), 26.0f}, NULL);
                }
                if(radius_result.changed) body->particle_radius =
                    fmaxf(0.0f, body->particle_radius);
                if(editor_checkbox_label_left("editor.particle.auto_fit", &auto_fit_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + EDITOR_TOOLS_WIDTH - 100.0f,
                            84.0f, 90.0f, 26.0f}, &body->particle_auto_fit) &&
                        body->particle_auto_fit) {
                    body->particle_radius = editor_project_particle_auto_radius_get(body);
                }
                rohr_ui_label(&particle_ring_color_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 120.0f, 104.0f, 26.0f});
                (void)editor_color_swatch("editor.particle.ring_color",
                    &body->particle_ring_color, false, &color_picker,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 114.0f, 120.0f,
                        EDITOR_TOOLS_WIDTH - 124.0f, 26.0f}, &project,
                    EDITOR_ITEM_RIGID_BODY, selected->id, 0, body->id,
                    EDITOR_PROPERTY_PARTICLE_RING_COLOR);
                rohr_ui_label(&particle_fill_color_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 156.0f, 104.0f, 26.0f});
                (void)editor_color_swatch("editor.particle.fill_color",
                    &body->particle_fill_color, false, &color_picker,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 114.0f, 156.0f,
                        EDITOR_TOOLS_WIDTH - 124.0f, 26.0f}, &project,
                    EDITOR_ITEM_RIGID_BODY, selected->id, 0, body->id,
                    EDITOR_PROPERTY_PARTICLE_FILL_COLOR);
                field_editing = radius_result.active;
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_RIGID_BODY) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorRigidBody *body = editor_selected_body_get(selected, &viewport_state);
            if(body != NULL) {
                size_t body_index = (size_t)(body - selected->rigid_bodies);
                UIFieldResult name_result;
                char edited_name[EDITOR_OBJECT_NAME_MAX];
                snprintf(edited_name, sizeof(edited_name), "%s", body->name);
                if(!editor_named_text_sync(&font, body->name,
                        &rigid_body_labels[body_index], rigid_body_cache[body_index],
                        EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&name_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f,
                    42.0f, 48.0f, 30.0f});
                name_result = editor_property_name_field("editor.rigid_body.name",
                    edited_name, sizeof(edited_name), &rigid_body_labels[body_index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 88.0f, 42.0f,
                        EDITOR_TOOLS_WIDTH - 96.0f, 30.0f});
                if(name_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
                        .data.item_rename = {.kind = EDITOR_ITEM_RIGID_BODY,
                            .object = selected->id, .item = body->id}};
                    snprintf(command.data.item_rename.name,
                        sizeof(command.data.item_rename.name), "%s", edited_name);
                    (void)editor_command_execute(&project, &command);
                }
                (void)editor_visibility_toggle("editor.rigid_body.visibility",
                    &visibility_visible_label, &visibility_hidden_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        44.0f, 26.0f, 26.0f}, &project,
                    EDITOR_VISIBILITY_RIGID_BODY, selected->id, 0, body->id, body->visible);
                rohr_ui_label(&x_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    80.0f, 24.0f, 26.0f});
                {
                    Position edited_position = body->position;
                    float edited_rotation = body->rotation;
                    UIFieldResult x_result = rohr_ui_field("editor.rigid_body.x",
                        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                            .number = &edited_position.x},
                        &x_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 34.0f, 80.0f,
                            EDITOR_TOOLS_WIDTH - 44.0f, 26.0f}, NULL);
                    rohr_ui_label(&y_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        112.0f, 24.0f, 26.0f});
                    UIFieldResult y_result = rohr_ui_field("editor.rigid_body.y",
                        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                            .number = &edited_position.y},
                        &y_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 34.0f, 112.0f,
                            EDITOR_TOOLS_WIDTH - 44.0f, 26.0f}, NULL);
                    rohr_ui_label(&rotation_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        144.0f, 76.0f, 26.0f});
                    UIFieldResult rotation_result = rohr_ui_field("editor.rigid_body.rotation",
                        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                            .number = &edited_rotation},
                        &length_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 86.0f, 144.0f,
                            EDITOR_TOOLS_WIDTH - 96.0f, 26.0f}, NULL);
                    field_editing = name_result.active || x_result.active || y_result.active ||
                        rotation_result.active;
                    if(x_result.changed || y_result.changed || rotation_result.changed) {
                        EditorCommand command = {.type = EDITOR_COMMAND_RIGID_BODY_TRANSFORM,
                            .data.rigid_body_transform = {selected->id, body->id,
                                edited_position, edited_rotation}};
                        (void)editor_command_execute(&project, &command);
                    }
                }
                rohr_ui_label(&mass_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    180.0f, 76.0f, 26.0f});
                {
                    float edited_value = body->mass_value;
                    UIFieldResult result = rohr_ui_field("editor.rigid_body.mass",
                        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                            .number = &edited_value},
                        &length_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 86.0f, 180.0f,
                            EDITOR_TOOLS_WIDTH - 96.0f, 26.0f}, NULL);
                    if(result.changed && edited_value < 0.0f) edited_value = 0.0f;
                    if(result.changed) editor_property_float_set(&project,
                        EDITOR_ITEM_RIGID_BODY, selected->id, 0, body->id, 0,
                        EDITOR_PROPERTY_MASS, edited_value);
                    field_editing = field_editing || result.active;
                }
                rohr_ui_label(&friction_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    212.0f, 76.0f, 26.0f});
                {
                    float edited_value = body->friction;
                    UIFieldResult result = rohr_ui_field("editor.rigid_body.friction",
                        (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &edited_value},
                        &length_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 86.0f, 212.0f,
                            EDITOR_TOOLS_WIDTH - 96.0f, 26.0f}, NULL);
                    if(result.changed && edited_value < 0.0f) edited_value = 0.0f;
                    if(result.changed) editor_property_float_set(&project,
                        EDITOR_ITEM_RIGID_BODY, selected->id, 0, body->id, 0,
                        EDITOR_PROPERTY_FRICTION, edited_value);
                    field_editing = field_editing || result.active;
                }
                rohr_ui_label(&restitution_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 244.0f, 96.0f, 26.0f});
                {
                    float edited_value = body->restitution;
                    UIFieldResult result = rohr_ui_field("editor.rigid_body.restitution",
                        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                            .number = &edited_value}, &length_field,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 106.0f, 244.0f,
                            EDITOR_TOOLS_WIDTH - 116.0f, 26.0f}, NULL);
                    if(result.changed) edited_value =
                        fminf(1.0f, fmaxf(0.0f, edited_value));
                    if(result.changed) editor_property_float_set(&project,
                        EDITOR_ITEM_RIGID_BODY, selected->id, 0, body->id, 0,
                        EDITOR_PROPERTY_RESTITUTION, edited_value);
                    field_editing = field_editing || result.active;
                }
                {
                    rohr_ui_label(&border_color_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        276.0f, 104.0f, 26.0f});
                    (void)editor_color_swatch("editor.rigid_body.border_color",
                        &body->border_color, false, &color_picker,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 114.0f,
                            276.0f, EDITOR_TOOLS_WIDTH - 124.0f, 26.0f}, &project,
                        EDITOR_ITEM_RIGID_BODY, selected->id, 0, body->id,
                        EDITOR_PROPERTY_OUTLINE_COLOR);
                    rohr_ui_label(&surface_color_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        308.0f, 104.0f, 26.0f});
                    (void)editor_color_swatch("editor.rigid_body.surface_color",
                        &body->surface_color, false, &color_picker,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 114.0f,
                            308.0f, EDITOR_TOOLS_WIDTH - 124.0f, 26.0f}, &project,
                        EDITOR_ITEM_RIGID_BODY, selected->id, 0, body->id,
                        EDITOR_PROPERTY_SURFACE_COLOR);
                }
                {
                    bool edited = body->gravity_enabled;
                    if(editor_checkbox("editor.rigid_body.gravity", &gravity_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 340.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 28.0f},
                        &edited)) editor_property_bool_set(&project,
                            EDITOR_ITEM_RIGID_BODY, selected->id, 0, body->id, 0,
                            EDITOR_PROPERTY_GRAVITY, edited);
                }
                {
                    const TextAsset *options[] = {&dynamic_label, &static_label};
                    UIDropdownResult result = rohr_ui_dropdown("editor.rigid_body.motion",
                        options, 2, body->static_body ? 1 : 0,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 372.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, NULL);
                    if(result.changed) editor_property_bool_set(&project,
                        EDITOR_ITEM_RIGID_BODY, selected->id, 0, body->id, 0,
                        EDITOR_PROPERTY_STATIC, result.selected_index == 1);
                }
                {
                    const TextAsset *options[] = {
                        &rotation_unlocked_label, &rotation_locked_label
                    };
                    UIDropdownResult result = rohr_ui_dropdown("editor.rigid_body.rotation_lock",
                        options, 2, body->rotation_locked ? 1 : 0,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 404.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, NULL);
                    if(result.changed) editor_property_bool_set(&project,
                        EDITOR_ITEM_RIGID_BODY, selected->id, 0, body->id, 0,
                        EDITOR_PROPERTY_ROTATION_LOCKED, result.selected_index == 1);
                }
                {
                    float row_x = EDITOR_VIEWPORT_WIDTH + 10.0f;
                    float row_width = EDITOR_TOOLS_WIDTH - 20.0f;
                    float controls_bottom = 468.0f;

                    bool collision_enabled = body->collision_enabled;
                    if(editor_checkbox("editor.rigid_body.collision", &collision_label,
                            (UIRect){row_x, 436.0f, row_width * 0.52f, 28.0f},
                            &collision_enabled)) {
                        editor_property_bool_set(&project, EDITOR_ITEM_RIGID_BODY,
                            selected->id, 0, body->id, 0, EDITOR_PROPERTY_COLLISION,
                            collision_enabled);
                        if(!collision_enabled) {
                            collision_category_open = false;
                            collide_with_open = false;
                        }
                    }
                    if(body->collision_enabled) {
                        bool particle = body->particle;
                        if(editor_checkbox_label_left("editor.rigid_body.particle",
                            &particle_label,
                            (UIRect){row_x + row_width * 0.54f, 436.0f,
                                row_width * 0.46f, 28.0f}, &particle))
                            editor_property_bool_set(&project, EDITOR_ITEM_RIGID_BODY,
                                selected->id, 0, body->id, 0,
                                EDITOR_PROPERTY_PARTICLE, particle);
                        if(!body->particle &&
                                viewport_state.selection == EDITOR_SELECTION_PARTICLE)
                            viewport_state.selection = EDITOR_SELECTION_RIGID_BODY;
                    }
                    if(body->collision_enabled && rohr_ui_button(
                            "editor.rigid_body.collision_category", &collision_category_label,
                            (UIRect){row_x, 468.0f, row_width, 28.0f}, NULL).clicked) {
                        collision_category_open = !collision_category_open;
                        collide_with_open = false;
                    }
                    if(body->collision_enabled) {
                        rohr_ui_border((UIRect){row_x, 468.0f, row_width, 28.0f}, 2.0f,
                            (Color){0, 0, 0, 255});
                    }
                    controls_bottom = 500.0f;
                    if(body->collision_enabled && collision_category_open) {
                        size_t rows = 0;
                        if(!editor_collision_mask_menu_draw(
                                "editor.rigid_body.collision_category.mask",
                                &project, &body->collision_category,
                                EDITOR_ITEM_RIGID_BODY, selected->id, 0, body->id,
                                EDITOR_COLLISION_FILTER_CATEGORY, &font,
                                collision_mask_labels, collision_mask_cache,
                                collision_mask_name, sizeof(collision_mask_name),
                                &collision_mask_name_field, &add_label, row_x,
                                controls_bottom, row_width, &field_editing, &rows)) goto fail;
                        controls_bottom += (float)rows * 30.0f;
                    }
                    if(body->collision_enabled && rohr_ui_button(
                            "editor.rigid_body.collide_with", &collide_with_label,
                            (UIRect){row_x, controls_bottom, row_width, 28.0f}, NULL).clicked) {
                        collide_with_open = !collide_with_open;
                        collision_category_open = false;
                    }
                    if(body->collision_enabled) {
                        rohr_ui_border((UIRect){row_x, controls_bottom, row_width, 28.0f},
                            2.0f, (Color){0, 0, 0, 255});
                        controls_bottom += 32.0f;
                    }
                    if(body->collision_enabled && collide_with_open) {
                        size_t rows = 0;
                        if(!editor_collision_mask_menu_draw(
                                "editor.rigid_body.collide_with.mask", &project,
                                &body->collision_with, EDITOR_ITEM_RIGID_BODY,
                                selected->id, 0, body->id,
                                EDITOR_COLLISION_FILTER_COLLIDE_WITH, &font,
                                collision_mask_labels,
                                collision_mask_cache, collision_mask_name,
                                sizeof(collision_mask_name), &collision_mask_name_field,
                                &add_label, row_x, controls_bottom, row_width,
                                &field_editing, &rows)) goto fail;
                        controls_bottom += (float)rows * 30.0f;
                    }
                    if((collision_category_open || collide_with_open) &&
                            mouse.button_states[MOUSE_BUTTON_LEFT] ==
                                MOUSE_BUTTON_STATE_PRESSED) {
                        Position pointer = rohr_graphics_mouse_screen_position_get();
                        bool in_controls = pointer.x >= row_x &&
                            pointer.x <= row_x + row_width && pointer.y >= 436.0f &&
                            pointer.y <= controls_bottom;
                        if(!in_controls) {
                            collision_category_open = false;
                            collide_with_open = false;
                        }
                    }
                    {
                        float hitbox_button_y = body->collision_enabled ?
                            controls_bottom + 6.0f : 468.0f;
                        UIButtonStyle origin_style = editor_selected_button_style_get();
                        UIButtonResult origin_result = rohr_ui_button(
                            "editor.rigid_body.origin", &origin_label,
                            (UIRect){row_x, hitbox_button_y, row_width, 28.0f},
                            viewport_state.selection == EDITOR_SELECTION_ORIGIN &&
                                viewport_state.selected_origin_kind ==
                                    EDITOR_ORIGIN_RIGID_BODY ? &origin_style : NULL);
                        if(origin_result.clicked || origin_result.focus_changed) {
                            viewport_state.selection = EDITOR_SELECTION_ORIGIN;
                            viewport_state.selected_origin_kind = EDITOR_ORIGIN_RIGID_BODY;
                            if(origin_result.double_clicked)
                                viewport_state.mode = EDITOR_VIEWPORT_ORIGIN;
                        }
                        hitbox_button_y += 34.0f;
                        if(body->particle) {
                            UIButtonStyle particle_style = editor_selected_button_style_get();
                            UIButtonResult particle_result = rohr_ui_button(
                                "editor.rigid_body.particle_item", &particle_label,
                                (UIRect){row_x, hitbox_button_y, row_width, 28.0f},
                                viewport_state.selection == EDITOR_SELECTION_PARTICLE ?
                                    &particle_style : NULL);
                            if(particle_result.clicked || particle_result.focus_changed) {
                                viewport_state.selection = EDITOR_SELECTION_PARTICLE;
                                if(particle_result.double_clicked)
                                    viewport_state.mode = EDITOR_VIEWPORT_PARTICLE;
                            }
                            hitbox_button_y += 34.0f;
                        }
                        if(rohr_ui_button("editor.rigid_body.add_hitbox", &add_hitbox_label,
                                (UIRect){row_x, hitbox_button_y, row_width, 32.0f},
                                NULL).clicked) {
                            EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
                                .data.item_add = {.kind = EDITOR_ITEM_HITBOX,
                                    .object = selected->id, .parent = body->id}};
                            EditorCommandResult added = editor_command_execute(
                                &project, &command);
                            if(added.kind == ERROR_RESULT_VALUE) {
                                viewport_state.selection = EDITOR_SELECTION_HITBOX;
                                viewport_state.selected_hitbox = added.result.object;
                            }
                        }
                        for(size_t i = 0; i < body->hitbox_count; i += 1) {
                            EditorHitbox *box = &body->hitboxes[i];
                            UIButtonStyle selected_style = editor_selected_button_style_get();
                            UIButtonResult result;
                            char id[64];
                            char visibility_id[72];
                            float y = hitbox_button_y + 42.0f + (float)i * 30.0f;
                            if(!editor_named_text_sync(&font, box->name,
                                    &body_hitbox_labels[i], body_hitbox_cache[i],
                                    EDITOR_OBJECT_NAME_MAX)) goto fail;
                            snprintf(id, sizeof(id), "editor.hitbox.%u", box->id);
                            snprintf(visibility_id, sizeof(visibility_id),
                                "editor.hitbox.%u.visibility", box->id);
                            (void)editor_visibility_toggle(visibility_id,
                                &visibility_visible_label, &visibility_hidden_label,
                                (UIRect){row_x, y, 26.0f, 26.0f}, &project,
                                EDITOR_VISIBILITY_HITBOX, selected->id, body->id,
                                box->id, box->visible);
                            result = rohr_ui_button(id, &body_hitbox_labels[i],
                                (UIRect){row_x + 32.0f, y, row_width - 32.0f, 26.0f},
                                (viewport_state.selection == EDITOR_SELECTION_HITBOX &&
                                    viewport_state.selected_hitbox == box->id) ||
                                    editor_selection_path_check(&viewport_state,
                                        EDITOR_SELECTION_HITBOX, selected->id,
                                        body->id, 0, box->id) ?
                                    &selected_style : NULL);
                            editor_hierarchy_drag_row(&hierarchy_drag, &viewport_state,
                                (EditorSelectionRef){EDITOR_SELECTION_HITBOX,
                                    selected->id, body->id, 0, box->id},
                                (UIRect){row_x + 32.0f, y,
                                    row_width - 32.0f, 26.0f}, result,
                                hierarchy_pointer, hierarchy_primary,
                                panel_scroll_offset, i + 1 == body->hitbox_count);
                            if(result.clicked || result.focus_changed) {
                                viewport_state.selection = EDITOR_SELECTION_HITBOX;
                                viewport_state.selected_hitbox = box->id;
                                if(result.double_clicked) (void)editor_navigation_selected_open(
                                    &project, &viewport_state);
                            }
                        }
                    }
                }
                {
                    UIButtonStyle style = editor_delete_button_style_get();
                    if(rohr_ui_button("editor.rigid_body.delete", &delete_rigid_body_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                                editor_panel_delete_y_get(&project, &viewport_state),
                                EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, &style).clicked) {
                        (void)editor_open_item_delete(&project, &viewport_state);
                    }
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_HITBOX) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorRigidBody *body = editor_selected_body_get(selected, &viewport_state);
            EditorHitbox *hitbox = editor_selected_hitbox_get(selected, &viewport_state);

            if(hitbox != NULL && body != NULL) {
                size_t index = (size_t)(hitbox - body->hitboxes);
                UIFieldResult name_result;
                char edited_name[EDITOR_OBJECT_NAME_MAX];
                snprintf(edited_name, sizeof(edited_name), "%s", hitbox->name);
                if(!editor_named_text_sync(&font, hitbox->name,
                        &body_hitbox_labels[index], body_hitbox_cache[index],
                        EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&name_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f,
                    44.0f, 48.0f, 28.0f});
                name_result = editor_property_name_field("editor.hitbox.name",
                    edited_name, sizeof(edited_name), &body_hitbox_labels[index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 88.0f, 44.0f,
                        EDITOR_TOOLS_WIDTH - 96.0f, 28.0f});
                field_editing = name_result.active;
                if(name_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
                        .data.item_rename = {.kind = EDITOR_ITEM_HITBOX,
                            .object = selected->id, .parent = body->id,
                            .item = hitbox->id}};
                    snprintf(command.data.item_rename.name,
                        sizeof(command.data.item_rename.name), "%s", edited_name);
                    (void)editor_command_execute(&project, &command);
                }
                (void)editor_visibility_toggle("editor.hitbox.visibility",
                    &visibility_visible_label, &visibility_hidden_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        47.0f, 26.0f, 26.0f}, &project, EDITOR_VISIBILITY_HITBOX,
                    selected->id, body->id, hitbox->id, hitbox->visible);
            }
            if(hitbox != NULL && rohr_ui_button("editor.hitbox.auto_shape",
                    &auto_shape_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                        78.0f, EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, NULL).clicked)
                auto_shape_picker_open = !auto_shape_picker_open;
            if(hitbox != NULL) {
                if(!auto_shape_picker_open) {
                    rohr_ui_label(&vertices_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 110.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 24.0f});
                    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
                    char id[64];
                    float y = 138.0f + (float)i * 27.0f;
                    UIButtonStyle selected_style = editor_selected_button_style_get();
                    const UIButtonStyle *style = (viewport_state.selection ==
                        EDITOR_SELECTION_VERTEX && viewport_state.selected_vertex == i) ||
                        editor_selection_path_check(&viewport_state,
                            EDITOR_SELECTION_VERTEX, selected->id, body->id,
                            hitbox->id, hitbox->vertices[i].id) ?
                        &selected_style : NULL;
                    UIButtonResult result;
                    if(!editor_named_text_sync(&font, hitbox->vertices[i].name,
                            &vertex_labels[i], vertex_name_cache[i],
                            EDITOR_OBJECT_NAME_MAX)) goto fail;
                    snprintf(id, sizeof(id), "editor.vertex.%u", hitbox->vertices[i].id);
                    result = rohr_ui_button(id, &vertex_labels[i],
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f, y,
                            EDITOR_TOOLS_WIDTH - 26.0f, 23.0f}, style);
                    if(result.clicked || result.focus_changed) {
                        viewport_state.selection = EDITOR_SELECTION_VERTEX;
                        viewport_state.selected_vertex = i;
                        if(result.double_clicked) {
                            (void)editor_navigation_selected_open(&project, &viewport_state);
                        }
                    }
                    }
                    {
                    float base = 146.0f + (float)hitbox->vertex_count * 27.0f;
                    rohr_ui_label(&lines_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                        base, EDITOR_TOOLS_WIDTH - 20.0f, 24.0f});
                    for(uint32_t i = 0; i < hitbox->vertex_count; i += 1) {
                        char id[64];
                        UIButtonStyle selected_style = editor_selected_button_style_get();
                        const UIButtonStyle *style = (viewport_state.selection ==
                            EDITOR_SELECTION_LINE && viewport_state.selected_line == i) ||
                            editor_selection_path_check(&viewport_state,
                                EDITOR_SELECTION_LINE, selected->id, body->id,
                                hitbox->id, i) ?
                            &selected_style : NULL;
                        UIButtonResult result;
                        if(!editor_named_text_sync(&font, hitbox->line_names[i],
                                &line_labels[i], line_name_cache[i],
                                EDITOR_OBJECT_NAME_MAX)) goto fail;
                        snprintf(id, sizeof(id), "editor.line.%u", i);
                        result = rohr_ui_button(id, &line_labels[i],
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f,
                                base + 28.0f + (float)i * 27.0f,
                                EDITOR_TOOLS_WIDTH - 26.0f, 23.0f}, style);
                        if(result.clicked || result.focus_changed) {
                            viewport_state.selection = EDITOR_SELECTION_LINE;
                            viewport_state.selected_line = i;
                            if(result.double_clicked) {
                                (void)editor_navigation_selected_open(&project, &viewport_state);
                            }
                        }
                    }
                    {
                        UIButtonStyle delete_style = editor_delete_button_style_get();
                        float delete_y = editor_panel_delete_y_get(
                            &project, &viewport_state);

                        if(rohr_ui_button("editor.hitbox.delete", &delete_hitbox_label,
                                (UIRect){EDITOR_VIEWPORT_WIDTH + 18.0f, delete_y,
                                    EDITOR_TOOLS_WIDTH - 26.0f, 30.0f},
                                &delete_style).clicked) {
                            (void)editor_open_item_delete(&project, &viewport_state);
                        }
                    }
                    }
                }
                if(auto_shape_picker_open) {
                    size_t auto_shape_point_count =
                        editor_auto_shape_hitbox_points_capture(&viewport_state,
                            selected, body, hitbox);
                    int selected_shape = editor_auto_shape_picker_draw(
                        "editor.hitbox.auto_shape.option",
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 110.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 62.0f},
                        auto_shape_point_count > 0 ? auto_shape_point_count :
                            hitbox->vertex_count, &triangle_label,
                        &rectangle_label, &circle_label);
                    if(selected_shape >= 0) {
                        auto_shape_config.kind = (EditorAutoShapeKind)selected_shape;
                        viewport_state.auto_shape_parent_mode = EDITOR_VIEWPORT_HITBOX;
                        (void)editor_auto_shape_apply(&project, &viewport_state,
                            EDITOR_VIEWPORT_HITBOX, auto_shape_config);
                        viewport_state.mode = EDITOR_VIEWPORT_AUTO_SHAPE;
                        viewport_state.selection = EDITOR_SELECTION_HITBOX;
                        auto_shape_first_field_was_active = false;
                        auto_shape_second_field_was_active = false;
                        auto_shape_third_field_was_active = false;
                        auto_shape_picker_open = false;
                    }
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_AUTO_SHAPE) {
            UIFieldResult first_result = {0};
            UIFieldResult second_result = {0};
            UIFieldResult third_result = {0};
            bool non_field_changed = false;
            bool first_active;
            bool second_active;
            bool third_active;
            bool commit;

            rohr_ui_label(auto_shape_config.kind == EDITOR_AUTO_SHAPE_TRIANGLE ?
                    &triangle_label : auto_shape_config.kind == EDITOR_AUTO_SHAPE_RECTANGLE ?
                    &rectangle_label : &circle_label,
                (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 44.0f,
                    EDITOR_TOOLS_WIDTH - 20.0f, 30.0f});
            if(auto_shape_config.kind == EDITOR_AUTO_SHAPE_CIRCLE) {
                rohr_ui_label(&radius_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                    88.0f, 80.0f, 28.0f});
                first_result = rohr_ui_field("editor.auto_shape.radius",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &auto_shape_config.radius}, &length_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 94.0f, 88.0f,
                        EDITOR_TOOLS_WIDTH - 104.0f, 28.0f}, NULL);
            } else {
                rohr_ui_label(&width_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                    88.0f, 80.0f, 28.0f});
                first_result = rohr_ui_field("editor.auto_shape.width",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &auto_shape_config.width}, &x_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 94.0f, 88.0f,
                        EDITOR_TOOLS_WIDTH - 104.0f, 28.0f}, NULL);
                rohr_ui_label(auto_shape_config.kind == EDITOR_AUTO_SHAPE_RECTANGLE ?
                        &length_label : &height_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                    124.0f, 80.0f, 28.0f});
                if(auto_shape_config.kind == EDITOR_AUTO_SHAPE_TRIANGLE &&
                        auto_shape_config.triangle_kind ==
                            EDITOR_AUTO_TRIANGLE_EQUILATERAL) {
                    float calculated_height = auto_shape_config.width * sqrtf(3.0f) * 0.5f;
                    editor_numeric_field_disabled_draw(&y_field, calculated_height,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 94.0f, 124.0f,
                            EDITOR_TOOLS_WIDTH - 104.0f, 28.0f});
                } else {
                    second_result = rohr_ui_field("editor.auto_shape.height",
                        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                            .number = &auto_shape_config.height}, &y_field,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 94.0f, 124.0f,
                            EDITOR_TOOLS_WIDTH - 104.0f, 28.0f}, NULL);
                }
                if(auto_shape_config.kind == EDITOR_AUTO_SHAPE_TRIANGLE) {
                    const TextAsset *triangle_options[3] = {&equilateral_label,
                        &isosceles_label, &scalene_label};
                    UIDropdownResult triangle_result = rohr_ui_dropdown(
                        "editor.auto_shape.triangle_kind", triangle_options, 3,
                        (size_t)auto_shape_config.triangle_kind,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 164.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, NULL);
                    if(triangle_result.changed) {
                        auto_shape_config.triangle_kind =
                            (EditorAutoTriangleKind)triangle_result.selected_index;
                        non_field_changed = true;
                    }
                    if(auto_shape_config.triangle_kind == EDITOR_AUTO_TRIANGLE_SCALENE) {
                        rohr_ui_label(&apex_offset_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 202.0f,
                                80.0f, 28.0f});
                        third_result = rohr_ui_field("editor.auto_shape.apex_offset",
                            (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                                .number = &auto_shape_config.apex_offset}, &length_field,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 94.0f, 202.0f,
                                EDITOR_TOOLS_WIDTH - 104.0f, 28.0f}, NULL);
                    }
                }
            }
            first_active = first_result.active && !first_result.submitted;
            second_active = second_result.active && !second_result.submitted;
            third_active = third_result.active && !third_result.submitted;
            commit = non_field_changed ||
                (auto_shape_first_field_was_active && !first_active) ||
                (auto_shape_second_field_was_active && !second_active) ||
                (auto_shape_third_field_was_active && !third_active) ||
                first_result.submitted || second_result.submitted ||
                third_result.submitted;
            if(commit) (void)editor_auto_shape_apply(&project, &viewport_state,
                viewport_state.auto_shape_parent_mode, auto_shape_config);
            auto_shape_first_field_was_active = first_active;
            auto_shape_second_field_was_active = second_active;
            auto_shape_third_field_was_active = third_active;
            field_editing = first_active || second_active || third_active;
        } else if(viewport_state.mode == EDITOR_VIEWPORT_VERTEX) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorRigidBody *body = editor_selected_body_get(selected, &viewport_state);
            EditorHitbox *hitbox = editor_selected_hitbox_get(selected, &viewport_state);
            if(body != NULL && hitbox != NULL &&
                    viewport_state.selected_vertex < hitbox->vertex_count) {
                EditorVertex *vertex = &hitbox->vertices[viewport_state.selected_vertex];
                UISliderConfig slider = rohr_ui_slider_config_default_get();
                UIFieldResult name_result;
                uint32_t vertex_index = viewport_state.selected_vertex;
                char edited_name[EDITOR_OBJECT_NAME_MAX];
                snprintf(edited_name, sizeof(edited_name), "%s", vertex->name);
                if(!editor_named_text_sync(&font, vertex->name,
                        &vertex_labels[vertex_index], vertex_name_cache[vertex_index],
                        EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&name_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    48.0f, 48.0f, 24.0f});
                name_result = editor_property_name_field("editor.vertex.name",
                    edited_name, sizeof(edited_name), &vertex_labels[vertex_index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 56.0f, 48.0f,
                        EDITOR_TOOLS_WIDTH - 64.0f, 24.0f});
                field_editing = name_result.active;
                if(name_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
                        .data.item_rename = {.kind = EDITOR_ITEM_VERTEX,
                            .object = selected->id, .parent = body->id,
                            .item = hitbox->id, .index = vertex->id}};
                    snprintf(command.data.item_rename.name,
                        sizeof(command.data.item_rename.name), "%s", edited_name);
                    (void)editor_command_execute(&project, &command);
                }
                if(rohr_ui_button("editor.vertex.lock",
                        vertex->position_locked ? &unlock_label : &lock_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 82.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, NULL).clicked) {
                    editor_property_bool_set(&project, EDITOR_ITEM_VERTEX,
                        selected->id, body->id, hitbox->id, vertex->id,
                        EDITOR_PROPERTY_POSITION_LOCKED, !vertex->position_locked);
                }
                rohr_ui_label(&x_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 122.0f, 20.0f, 22.0f});
                rohr_ui_label(&y_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 192.0f, 20.0f, 22.0f});
                slider.length = EDITOR_TOOLS_WIDTH - 42.0f;
                slider.min_value = -EDITOR_VIEWPORT_WIDTH * 0.5f;
                slider.max_value = EDITOR_VIEWPORT_WIDTH * 0.5f;
                slider.center = (Position){EDITOR_VIEWPORT_WIDTH + 72.0f, 157.0f};
                if(vertex->position_locked) {
                    editor_numeric_field_disabled_draw(&x_field, vertex->position.x,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 28.0f, 122.0f,
                            EDITOR_TOOLS_WIDTH - 38.0f, 24.0f});
                    editor_numeric_field_disabled_draw(&y_field, vertex->position.y,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 28.0f, 192.0f,
                            EDITOR_TOOLS_WIDTH - 38.0f, 24.0f});
                } else {
                    Position edited_position = vertex->position;
                    UIFieldResult x_result = rohr_ui_field("editor.vertex.x.field",
                        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                            .number = &edited_position.x}, &x_field,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 28.0f, 122.0f,
                            EDITOR_TOOLS_WIDTH - 38.0f, 24.0f}, NULL);
                    UIFieldResult y_result = rohr_ui_field("editor.vertex.y.field",
                        (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                            .number = &edited_position.y}, &y_field,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 28.0f, 192.0f,
                            EDITOR_TOOLS_WIDTH - 38.0f, 24.0f}, NULL);
                    field_editing = name_result.active || x_result.active || y_result.active;
                    UISliderResult x_slider = rohr_ui_slider(
                        "editor.vertex.x", edited_position.x, &slider);
                    edited_position.x = x_slider.value;
                    slider.center.y = 227.0f;
                    UISliderResult y_slider = rohr_ui_slider(
                        "editor.vertex.y", edited_position.y, &slider);
                    edited_position.y = y_slider.value;
                    if(x_result.changed || y_result.changed ||
                            x_slider.changed || y_slider.changed) {
                        EditorCommand command = {.type = EDITOR_COMMAND_VERTEX_POSITION,
                            .data.vertex_position = {selected->id, body->id, hitbox->id,
                                vertex->id, edited_position}};
                        (void)editor_command_execute(&project, &command);
                    }
                }
                {
                    UIButtonStyle delete_style = editor_delete_button_style_get();
                    UIRect delete_bounds = {EDITOR_VIEWPORT_WIDTH + 10.0f,
                        editor_panel_delete_y_get(&project, &viewport_state),
                        EDITOR_TOOLS_WIDTH - 20.0f, 30.0f};
                    if(hitbox->vertex_count <= EDITOR_HITBOX_VERTEX_MIN) {
                        rohr_ui_button_disabled(delete_bounds, &delete_style);
                        rohr_ui_label(&delete_vertex_label, delete_bounds);
                    } else if(rohr_ui_button("editor.vertex.delete", &delete_vertex_label,
                            delete_bounds, &delete_style).clicked) {
                        (void)editor_open_item_delete(&project, &viewport_state);
                    }
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_LINE) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorRigidBody *body = editor_selected_body_get(selected, &viewport_state);
            EditorHitbox *hitbox = editor_selected_hitbox_get(selected, &viewport_state);
            if(body != NULL && hitbox != NULL &&
                    viewport_state.selected_line < hitbox->vertex_count) {
                uint32_t line = viewport_state.selected_line;
                EditorVertex *a = &hitbox->vertices[line];
                EditorVertex *b = &hitbox->vertices[(line + 1) % hitbox->vertex_count];
                bool constrained = a->position_locked && b->position_locked;
                bool vertex_inserted = false;
                float length = editor_project_hitbox_line_length_get(hitbox, line);
                UISliderConfig slider = rohr_ui_slider_config_default_get();
                UIFieldResult name_result;
                char edited_name[EDITOR_OBJECT_NAME_MAX];
                snprintf(edited_name, sizeof(edited_name), "%s",
                    hitbox->line_names[line]);
                if(!editor_named_text_sync(&font, hitbox->line_names[line],
                        &line_labels[line], line_name_cache[line],
                        EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&name_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    48.0f, 48.0f, 24.0f});
                name_result = editor_property_name_field("editor.line.name",
                    edited_name, sizeof(edited_name),
                    &line_labels[line], (UIRect){EDITOR_VIEWPORT_WIDTH + 56.0f,
                        48.0f, EDITOR_TOOLS_WIDTH - 64.0f, 24.0f});
                field_editing = name_result.active;
                if(name_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
                        .data.item_rename = {.kind = EDITOR_ITEM_LINE,
                            .object = selected->id,
                            .parent = body == NULL ? 0 : body->id,
                            .item = hitbox->id, .index = line}};
                    snprintf(command.data.item_rename.name,
                        sizeof(command.data.item_rename.name), "%s",
                        edited_name);
                    (void)editor_command_execute(&project, &command);
                }
                if(rohr_ui_button("editor.line.add_vertex", &add_vertex_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 82.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, NULL).clicked) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
                        .data.item_add = {.kind = EDITOR_ITEM_VERTEX,
                            .object = selected->id,
                            .parent = body == NULL ? 0 : body->id,
                            .first = hitbox->id, .index = line}};
                    if(editor_command_execute(&project, &command).kind ==
                            ERROR_RESULT_VALUE) {
                        editor_viewport_hitbox_editor_enter(&viewport_state);
                        vertex_inserted = true;
                    }
                }
                if(!vertex_inserted) {
                    rohr_ui_label(&length_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        150.0f, 52.0f, 26.0f});
                    if(constrained) {
                        editor_numeric_field_disabled_draw(&length_field, length,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 60.0f,
                                150.0f, EDITOR_TOOLS_WIDTH - 70.0f, 26.0f});
                        rohr_ui_label(&constrained_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 5.0f,
                                190.0f, EDITOR_TOOLS_WIDTH - 10.0f, 38.0f});
                    } else {
                        UIFieldResult length_result = rohr_ui_field(
                            "editor.line.length.field",
                            (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &length},
                            &length_field,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 60.0f, 150.0f,
                                EDITOR_TOOLS_WIDTH - 70.0f, 26.0f}, NULL);
                        field_editing = name_result.active || length_result.active;
                        if(length_result.changed) {
                            editor_property_float_set(&project, EDITOR_ITEM_LINE,
                                selected->id, body->id, hitbox->id, line,
                                EDITOR_PROPERTY_LINE_LENGTH, length);
                        }
                        slider.center = (Position){EDITOR_VIEWPORT_WIDTH +
                            EDITOR_TOOLS_WIDTH * 0.5f, 202.0f};
                        slider.length = EDITOR_TOOLS_WIDTH - 36.0f;
                        slider.min_value = 5.0f;
                        slider.max_value = EDITOR_VIEWPORT_WIDTH;
                        {
                            UISliderResult result = rohr_ui_slider(
                                "editor.line.length", length, &slider);
                            if(result.changed) editor_property_float_set(&project,
                                EDITOR_ITEM_LINE, selected->id, body->id, hitbox->id,
                                line, EDITOR_PROPERTY_LINE_LENGTH, result.value);
                        }
                    }
                    {
                        UIButtonStyle delete_style = editor_delete_button_style_get();
                        UIRect delete_bounds = {EDITOR_VIEWPORT_WIDTH + 10.0f,
                            editor_panel_delete_y_get(&project, &viewport_state),
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f};
                        if(hitbox->vertex_count <= EDITOR_HITBOX_VERTEX_MIN) {
                            rohr_ui_button_disabled(delete_bounds, &delete_style);
                            rohr_ui_label(&delete_line_label, delete_bounds);
                        } else if(rohr_ui_button("editor.line.delete", &delete_line_label,
                                delete_bounds, &delete_style).clicked) {
                            (void)editor_open_item_delete(&project, &viewport_state);
                        }
                    }
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_JOINT) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorJoint *joint = editor_selected_joint_get(selected, &viewport_state);
            if(joint != NULL) {
                size_t index = (size_t)(joint - selected->joint_items);
                const TextAsset *kind_options[] = {
                    &revolute_label, &weld_label, &spring_label
                };
                UIButtonStyle delete_style = editor_delete_button_style_get();
                UIFieldResult name_result;
                UIFieldResult x_result;
                UIFieldResult y_result;
                UIFieldResult rotation_result;
                char edited_name[EDITOR_OBJECT_NAME_MAX];
                snprintf(edited_name, sizeof(edited_name), "%s", joint->name);
                if(!editor_named_text_sync(&font, joint->name, &joint_labels[index],
                        joint_cache[index], EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&name_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f,
                    40.0f, 48.0f, 30.0f});
                name_result = editor_property_name_field("editor.joint.name", edited_name,
                    sizeof(edited_name), &joint_labels[index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 88.0f, 40.0f,
                        EDITOR_TOOLS_WIDTH - 96.0f, 30.0f});
                field_editing = name_result.active;
                if(name_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
                        .data.item_rename = {.kind = EDITOR_ITEM_JOINT,
                            .object = selected->id, .item = joint->id}};
                    snprintf(command.data.item_rename.name,
                        sizeof(command.data.item_rename.name), "%s", edited_name);
                    (void)editor_command_execute(&project, &command);
                }
                rohr_ui_label(&visual_size_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f, 76.0f,
                        EDITOR_TOOLS_WIDTH - 48.0f, 24.0f});
                (void)editor_visibility_toggle("editor.joint.visibility",
                    &visibility_visible_label, &visibility_hidden_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                        94.0f, 24.0f, 24.0f}, &project, EDITOR_VISIBILITY_JOINT,
                    selected->id, 0, joint->id, joint->visible);
                {
                    UISliderConfig slider = rohr_ui_slider_config_default_get();
                    slider.center = (Position){EDITOR_VIEWPORT_WIDTH +
                        (EDITOR_TOOLS_WIDTH + 40.0f) * 0.5f, 106.0f};
                    slider.length = EDITOR_TOOLS_WIDTH - 60.0f;
                    slider.min_value = 0.25f;
                    slider.max_value = 3.0f;
                    UISliderResult visual_size = rohr_ui_slider(
                        "editor.joint.visual_size", joint->visual_size, &slider);
                    if(visual_size.changed) editor_property_float_set(&project,
                        EDITOR_ITEM_JOINT, selected->id, 0, joint->id, 0,
                        EDITOR_PROPERTY_VISUAL_SIZE, visual_size.value);
                }
                {
                    UIDropdownResult result = rohr_ui_dropdown("editor.joint.kind",
                        kind_options, 3, (size_t)joint->kind,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 118.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL);
                    if(result.changed) editor_property_uint_set(&project,
                        EDITOR_ITEM_JOINT, selected->id, 0, joint->id, 0,
                        EDITOR_PROPERTY_JOINT_KIND, (uint32_t)result.selected_index);
                }
                {
                    const TextAsset *anchor_options[EDITOR_ANCHOR_MAX + 1];
                    size_t selected_a = 0;
                    size_t selected_b = 0;
                    anchor_options[0] = &none_label;
                    for(size_t i = 0; i < selected->anchor_count; i += 1) {
                        if(!editor_named_text_sync(&font, selected->anchors[i].name,
                                &anchor_labels[i], anchor_cache[i],
                                EDITOR_OBJECT_NAME_MAX)) goto fail;
                        anchor_options[i + 1] = &anchor_labels[i];
                        if(selected->anchors[i].id == joint->anchor_a) selected_a = i + 1;
                        if(selected->anchors[i].id == joint->anchor_b) selected_b = i + 1;
                    }
                rohr_ui_label(&body_a_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    158.0f, 55.0f, 28.0f});
                rohr_ui_label(&body_b_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    194.0f, 55.0f, 28.0f});
                {
                    UIDropdownResult result = rohr_ui_dropdown("editor.joint.anchor_a",
                        anchor_options, selected->anchor_count + 1, selected_a,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 63.0f, 158.0f,
                            EDITOR_TOOLS_WIDTH - 73.0f, 28.0f}, NULL);
                    EditorAnchorId preview = result.hovered_index > 0 ?
                        selected->anchors[result.hovered_index - 1].id : joint->anchor_a;
                    if(result.button_hovered || result.hovered_index >= 0) {
                        editor_anchor_preview_set(&viewport_state, selected, preview);
                    }
                    if(result.changed) editor_relationship_set(&project,
                        EDITOR_RELATIONSHIP_JOINT_ANCHOR, selected->id, 0,
                        joint->id, 0, result.selected_index == 0 ? 0 :
                            selected->anchors[result.selected_index - 1].id);
                }
                {
                    UIDropdownResult result = rohr_ui_dropdown("editor.joint.anchor_b",
                        anchor_options, selected->anchor_count + 1, selected_b,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 63.0f, 194.0f,
                            EDITOR_TOOLS_WIDTH - 73.0f, 28.0f}, NULL);
                    EditorAnchorId preview = result.hovered_index > 0 ?
                        selected->anchors[result.hovered_index - 1].id : joint->anchor_b;
                    if(result.button_hovered || result.hovered_index >= 0) {
                        editor_anchor_preview_set(&viewport_state, selected, preview);
                    }
                    if(result.changed) editor_relationship_set(&project,
                        EDITOR_RELATIONSHIP_JOINT_ANCHOR, selected->id, 0,
                        joint->id, 1, result.selected_index == 0 ? 0 :
                            selected->anchors[result.selected_index - 1].id);
                }
                }
                if(rohr_ui_button("editor.joint.add_anchor", &add_anchor_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 232.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL).clicked) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
                        .data.item_add = {.kind = EDITOR_ITEM_ANCHOR,
                            .object = selected->id,
                            .parent = selected->rigid_body_count > 0 ?
                                selected->rigid_bodies[0].id : 0}};
                    EditorCommandResult added = editor_command_execute(&project, &command);
                    if(added.kind == ERROR_RESULT_VALUE)
                        viewport_state.selected_anchor = added.result.object;
                }
                {
                    size_t anchor_start = 0;
                    for(size_t i = 0; i < selected->anchor_count; i += 1) {
                        if(selected->anchors[i].id == viewport_state.selected_anchor && i >= 6) {
                            anchor_start = i - 5;
                        }
                    }
                for(size_t i = anchor_start; i < selected->anchor_count &&
                        i < anchor_start + 6; i += 1) {
                    EditorAnchor *anchor = &selected->anchors[i];
                    UIButtonStyle selected_style = editor_selected_button_style_get();
                    char id[64];
                    char visibility_id[72];
                    if(!editor_named_text_sync(&font, anchor->name, &anchor_labels[i],
                            anchor_cache[i], EDITOR_OBJECT_NAME_MAX)) goto fail;
                    snprintf(id, sizeof(id), "editor.anchor.%u", anchor->id);
                    snprintf(visibility_id, sizeof(visibility_id),
                        "editor.anchor.%u.visibility", anchor->id);
                    (void)editor_visibility_toggle(visibility_id,
                        &visibility_visible_label, &visibility_hidden_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                                270.0f + (float)(i - anchor_start) * 27.0f,
                                23.0f, 23.0f}, &project, EDITOR_VISIBILITY_ANCHOR,
                        selected->id, 0, anchor->id, anchor->visible);
                    UIButtonResult result = rohr_ui_button(id, &anchor_labels[i],
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f,
                                270.0f + (float)(i - anchor_start) * 27.0f,
                                EDITOR_TOOLS_WIDTH - 48.0f, 23.0f},
                            (viewport_state.selected_anchor == anchor->id) ||
                                editor_selection_path_check(&viewport_state,
                                    EDITOR_SELECTION_ANCHOR, selected->id,
                                    0, 0, anchor->id) ?
                                &selected_style : NULL);
                    editor_hierarchy_drag_row(&hierarchy_drag, &viewport_state,
                        (EditorSelectionRef){EDITOR_SELECTION_ANCHOR,
                            selected->id, 0, 0, anchor->id},
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f,
                            270.0f + (float)(i - anchor_start) * 27.0f,
                            EDITOR_TOOLS_WIDTH - 48.0f, 23.0f}, result,
                        hierarchy_pointer, hierarchy_primary, panel_scroll_offset,
                        i + 1 == selected->anchor_count);
                    if(result.clicked || result.focus_changed) {
                        viewport_state.selection = EDITOR_SELECTION_ANCHOR;
                        viewport_state.selected_anchor = anchor->id;
                        if(result.double_clicked) (void)editor_navigation_selected_open(
                            &project, &viewport_state);
                    }
                }
                }
                if(joint->kind == EDITOR_JOINT_REVOLUTE) {
                    rohr_ui_label(&damping_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 442.0f, 76.0f, 26.0f});
                    {
                        float edited_value = joint->damping;
                        UIFieldResult result = rohr_ui_field(
                            "editor.joint.revolute.damping",
                            (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                                .number = &edited_value}, &length_field,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 86.0f, 442.0f,
                                EDITOR_TOOLS_WIDTH - 96.0f, 26.0f}, NULL);
                        if(result.changed && edited_value < 0.0f) edited_value = 0.0f;
                        if(result.changed) editor_property_float_set(&project,
                            EDITOR_ITEM_JOINT, selected->id, 0, joint->id, 0,
                            EDITOR_PROPERTY_DAMPING, edited_value);
                        field_editing = field_editing || result.active;
                    }
                } else if(joint->kind == EDITOR_JOINT_SPRING) {
                    rohr_ui_label(&rest_length_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 442.0f, 96.0f, 26.0f});
                    {
                        float edited_value = joint->rest_length;
                        UIFieldResult result = rohr_ui_field("editor.joint.spring.rest_length",
                            (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                                .number = &edited_value}, &length_field,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 106.0f, 442.0f,
                                EDITOR_TOOLS_WIDTH - 116.0f, 26.0f}, NULL);
                        if(result.changed && edited_value < 0.0f) edited_value = 0.0f;
                        if(result.changed) editor_property_float_set(&project,
                            EDITOR_ITEM_JOINT, selected->id, 0, joint->id, 0,
                            EDITOR_PROPERTY_REST_LENGTH, edited_value);
                        field_editing = field_editing || result.active;
                    }
                    rohr_ui_label(&stiffness_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 474.0f, 90.0f, 26.0f});
                    {
                        float edited_value = joint->stiffness;
                        UIFieldResult result = rohr_ui_field("editor.joint.spring.stiffness",
                            (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                                .number = &edited_value}, &length_field,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 474.0f,
                                EDITOR_TOOLS_WIDTH - 110.0f, 26.0f}, NULL);
                        if(result.changed && edited_value < 0.0f) edited_value = 0.0f;
                        if(result.changed) editor_property_float_set(&project,
                            EDITOR_ITEM_JOINT, selected->id, 0, joint->id, 0,
                            EDITOR_PROPERTY_STIFFNESS, edited_value);
                        field_editing = field_editing || result.active;
                    }
                    rohr_ui_label(&damping_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 506.0f, 76.0f, 26.0f});
                    {
                        float edited_value = joint->damping;
                        UIFieldResult result = rohr_ui_field("editor.joint.spring.damping",
                            (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                                .number = &edited_value}, &length_field,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 86.0f, 506.0f,
                                EDITOR_TOOLS_WIDTH - 96.0f, 26.0f}, NULL);
                        if(result.changed && edited_value < 0.0f) edited_value = 0.0f;
                        if(result.changed) editor_property_float_set(&project,
                            EDITOR_ITEM_JOINT, selected->id, 0, joint->id, 0,
                            EDITOR_PROPERTY_DAMPING, edited_value);
                        field_editing = field_editing || result.active;
                    }
                }
                {
                    EditorAnchor *anchor = editor_project_anchor_get(
                        selected, viewport_state.selected_anchor);
                    if(anchor != NULL && viewport_state.mode == EDITOR_VIEWPORT_ANCHOR) {
                        UIFieldResult x_result;
                        UIFieldResult y_result;
                        UIFieldResult rotation_result;
                        Position edited_position = anchor->position;
                        float edited_rotation = anchor->rotation;
                        rohr_ui_label(&x_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                            442.0f, 24.0f, 26.0f});
                        x_result = rohr_ui_field("editor.anchor.x",
                            (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                                .number = &edited_position.x}, &x_field,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 34.0f, 442.0f,
                                EDITOR_TOOLS_WIDTH - 44.0f, 26.0f}, NULL);
                        rohr_ui_label(&y_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                            476.0f, 24.0f, 26.0f});
                        y_result = rohr_ui_field("editor.anchor.y",
                            (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                                .number = &edited_position.y}, &y_field,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 34.0f, 476.0f,
                                EDITOR_TOOLS_WIDTH - 44.0f, 26.0f}, NULL);
                        field_editing = x_result.active || y_result.active;
                        rohr_ui_label(&rigid_body_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 510.0f,
                                90.0f, 28.0f});
                        {
                            const TextAsset *body_options[EDITOR_RIGID_BODY_MAX + 1];
                            size_t selected_body = 0;
                            body_options[0] = &none_label;
                            for(size_t i = 0; i < selected->rigid_body_count; i += 1) {
                                if(!editor_named_text_sync(&font, selected->rigid_bodies[i].name,
                                        &rigid_body_labels[i], rigid_body_cache[i],
                                        EDITOR_OBJECT_NAME_MAX)) goto fail;
                                body_options[i + 1] = &rigid_body_labels[i];
                                if(selected->rigid_bodies[i].id == anchor->rigid_body) {
                                    selected_body = i + 1;
                                }
                            }
                            UIDropdownResult result = rohr_ui_dropdown(
                                "editor.anchor.rigid_body", body_options,
                                selected->rigid_body_count + 1, selected_body,
                                (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 510.0f,
                                    EDITOR_TOOLS_WIDTH - 110.0f, 28.0f}, NULL);
                            if(result.button_hovered) {
                                viewport_state.preview_rigid_body = anchor->rigid_body;
                            } else if(result.hovered_index > 0) {
                                viewport_state.preview_rigid_body =
                                    selected->rigid_bodies[result.hovered_index - 1].id;
                            }
                            if(result.changed) editor_relationship_set(&project,
                                EDITOR_RELATIONSHIP_ANCHOR_RIGID_BODY, selected->id,
                                0, anchor->id, 0, result.selected_index == 0 ? 0 :
                                    selected->rigid_bodies[result.selected_index - 1].id);
                        }
                        rohr_ui_label(&rotation_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 544.0f,
                                76.0f, 26.0f});
                        rotation_result = rohr_ui_field("editor.anchor.rotation",
                            (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                                .number = &edited_rotation}, &length_field,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 86.0f, 544.0f,
                                EDITOR_TOOLS_WIDTH - 96.0f, 26.0f}, NULL);
                        field_editing = field_editing || rotation_result.active;
                        if(x_result.changed || y_result.changed || rotation_result.changed) {
                            EditorCommand command = {.type = EDITOR_COMMAND_ANCHOR_TRANSFORM,
                                .data.anchor_transform = {selected->id, anchor->id,
                                    edited_position, edited_rotation}};
                            (void)editor_command_execute(&project, &command);
                        }
                        {
                            const TextAsset *position_options[] = {
                                &position_global_label, &position_body_label
                            };
                            const TextAsset *rotation_options[] = {
                                &rotation_global_label, &rotation_body_label
                            };
                            UIDropdownResult position_result = rohr_ui_dropdown(
                                "editor.anchor.position_lock", position_options, 2,
                                anchor->position_follows_body ? 1 : 0,
                                (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 578.0f,
                                    EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, NULL);
                            UIDropdownResult rotation_result = rohr_ui_dropdown(
                                "editor.anchor.rotation_lock", rotation_options, 2,
                                anchor->rotation_follows_body ? 1 : 0,
                                (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 610.0f,
                                    EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, NULL);
                            if(position_result.changed) {
                                editor_property_bool_set(&project, EDITOR_ITEM_ANCHOR,
                                    selected->id, 0, anchor->id, 0,
                                    EDITOR_PROPERTY_POSITION_FOLLOWS_BODY,
                                    position_result.selected_index == 1);
                            }
                            if(rotation_result.changed) {
                                editor_property_bool_set(&project, EDITOR_ITEM_ANCHOR,
                                    selected->id, 0, anchor->id, 0,
                                    EDITOR_PROPERTY_ROTATION_FOLLOWS_BODY,
                                    rotation_result.selected_index == 1);
                            }
                        }
                    }
                }
                if(rohr_ui_button("editor.joint.delete", &delete_joint_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                            editor_panel_delete_y_get(&project, &viewport_state),
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f},
                        &delete_style).clicked) {
                    (void)editor_open_item_delete(&project, &viewport_state);
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_ANCHOR) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorAnchor *anchor = editor_project_anchor_get(
                selected, viewport_state.selected_anchor);
            if(anchor != NULL) {
                size_t anchor_index = (size_t)(anchor - selected->anchors);
                UIFieldResult x_result;
                UIFieldResult y_result;
                UIFieldResult rotation_result;
                UIFieldResult name_result;
                Position edited_position = anchor->position;
                float edited_rotation = anchor->rotation;
                char edited_name[EDITOR_OBJECT_NAME_MAX];
                snprintf(edited_name, sizeof(edited_name), "%s", anchor->name);
                if(!editor_named_text_sync(&font, anchor->name,
                        &anchor_labels[anchor_index], anchor_cache[anchor_index],
                        EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&name_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f,
                    42.0f, 48.0f, 30.0f});
                name_result = editor_property_name_field("editor.anchor.name", edited_name,
                    sizeof(edited_name), &anchor_labels[anchor_index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 88.0f, 42.0f,
                        EDITOR_TOOLS_WIDTH - 96.0f, 30.0f});
                if(name_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
                        .data.item_rename = {.kind = EDITOR_ITEM_ANCHOR,
                            .object = selected->id, .item = anchor->id}};
                    snprintf(command.data.item_rename.name,
                        sizeof(command.data.item_rename.name), "%s", edited_name);
                    (void)editor_command_execute(&project, &command);
                }
                (void)editor_visibility_toggle("editor.anchor.visibility",
                    &visibility_visible_label, &visibility_hidden_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        44.0f, 26.0f, 26.0f}, &project, EDITOR_VISIBILITY_ANCHOR,
                    selected->id, 0, anchor->id, anchor->visible);
                rohr_ui_label(&x_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    90.0f, 24.0f, 26.0f});
                x_result = rohr_ui_field("editor.anchor.x",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_position.x}, &x_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 34.0f, 90.0f,
                        EDITOR_TOOLS_WIDTH - 44.0f, 26.0f}, NULL);
                rohr_ui_label(&y_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    124.0f, 24.0f, 26.0f});
                y_result = rohr_ui_field("editor.anchor.y",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_position.y}, &y_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 34.0f, 124.0f,
                        EDITOR_TOOLS_WIDTH - 44.0f, 26.0f}, NULL);
                if(x_result.changed || y_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ANCHOR_TRANSFORM,
                        .data.anchor_transform = {selected->id, anchor->id,
                            edited_position, edited_rotation}};
                    (void)editor_command_execute(&project, &command);
                }
                rohr_ui_label(&rigid_body_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 158.0f, 90.0f, 28.0f});
                {
                    const TextAsset *body_options[EDITOR_RIGID_BODY_MAX + 1];
                    size_t selected_body = 0;
                    body_options[0] = &none_label;
                    for(size_t i = 0; i < selected->rigid_body_count; i += 1) {
                        if(!editor_named_text_sync(&font, selected->rigid_bodies[i].name,
                                &rigid_body_labels[i], rigid_body_cache[i],
                                EDITOR_OBJECT_NAME_MAX)) goto fail;
                        body_options[i + 1] = &rigid_body_labels[i];
                        if(selected->rigid_bodies[i].id == anchor->rigid_body) {
                            selected_body = i + 1;
                        }
                    }
                    UIDropdownResult result = rohr_ui_dropdown(
                        "editor.anchor.rigid_body", body_options,
                        selected->rigid_body_count + 1, selected_body,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 158.0f,
                            EDITOR_TOOLS_WIDTH - 110.0f, 28.0f}, NULL);
                    if(result.button_hovered) {
                        viewport_state.preview_rigid_body = anchor->rigid_body;
                    } else if(result.hovered_index > 0) {
                        viewport_state.preview_rigid_body =
                            selected->rigid_bodies[result.hovered_index - 1].id;
                    }
                    if(result.changed) editor_relationship_set(&project,
                        EDITOR_RELATIONSHIP_ANCHOR_RIGID_BODY, selected->id, 0,
                        anchor->id, 0, result.selected_index == 0 ? 0 :
                            selected->rigid_bodies[result.selected_index - 1].id);
                }
                rohr_ui_label(&rotation_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 192.0f, 76.0f, 26.0f});
                rotation_result = rohr_ui_field("editor.anchor.rotation",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_rotation}, &length_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 86.0f, 192.0f,
                        EDITOR_TOOLS_WIDTH - 96.0f, 26.0f}, NULL);
                if(rotation_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ANCHOR_TRANSFORM,
                        .data.anchor_transform = {selected->id, anchor->id,
                            edited_position, edited_rotation}};
                    (void)editor_command_execute(&project, &command);
                }
                field_editing = name_result.active || x_result.active || y_result.active ||
                    rotation_result.active;
                {
                    const TextAsset *position_options[] = {
                        &position_global_label, &position_body_label
                    };
                    const TextAsset *rotation_options[] = {
                        &rotation_global_label, &rotation_body_label
                    };
                    UIDropdownResult position_result = rohr_ui_dropdown(
                        "editor.anchor.position_lock", position_options, 2,
                        anchor->position_follows_body ? 1 : 0,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 226.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, NULL);
                    UIDropdownResult orientation_result = rohr_ui_dropdown(
                        "editor.anchor.rotation_lock", rotation_options, 2,
                        anchor->rotation_follows_body ? 1 : 0,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 258.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, NULL);
                    if(position_result.changed) {
                        editor_property_bool_set(&project, EDITOR_ITEM_ANCHOR,
                            selected->id, 0, anchor->id, 0,
                            EDITOR_PROPERTY_POSITION_FOLLOWS_BODY,
                            position_result.selected_index == 1);
                    }
                    if(orientation_result.changed) {
                        editor_property_bool_set(&project, EDITOR_ITEM_ANCHOR,
                            selected->id, 0, anchor->id, 0,
                            EDITOR_PROPERTY_ROTATION_FOLLOWS_BODY,
                            orientation_result.selected_index == 1);
                    }
                }
                {
                    UIButtonStyle delete_style = editor_delete_button_style_get();
                    if(rohr_ui_button("editor.anchor.delete", &delete_anchor_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                                editor_panel_delete_y_get(&project, &viewport_state),
                                EDITOR_TOOLS_WIDTH - 20.0f, 34.0f},
                            &delete_style).clicked) {
                        (void)editor_open_item_delete(&project, &viewport_state);
                    }
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SOFT_BODY) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorSoftBody *body = editor_selected_soft_body_get(selected, &viewport_state);
            if(body != NULL) {
                size_t body_index = (size_t)(body - selected->soft_body_items);
                UIButtonStyle delete_style = editor_delete_button_style_get();
                UIFieldResult name_result;
                UIFieldResult x_result;
                UIFieldResult y_result;
                UIFieldResult rotation_result;
                Position edited_position = body->position;
                float edited_rotation = body->rotation;
                char edited_name[EDITOR_OBJECT_NAME_MAX];
                snprintf(edited_name, sizeof(edited_name), "%s", body->name);
                if(!editor_named_text_sync(&font, body->name,
                        &soft_body_labels[body_index], soft_body_cache[body_index],
                        EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&name_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f,
                    42.0f, 48.0f, 30.0f});
                name_result = editor_property_name_field("editor.soft_body.name", edited_name,
                    sizeof(edited_name), &soft_body_labels[body_index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 88.0f, 42.0f,
                        EDITOR_TOOLS_WIDTH - 96.0f, 30.0f});
                if(name_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
                        .data.item_rename = {.kind = EDITOR_ITEM_SOFT_BODY,
                            .object = selected->id, .item = body->id}};
                    snprintf(command.data.item_rename.name,
                        sizeof(command.data.item_rename.name), "%s", edited_name);
                    (void)editor_command_execute(&project, &command);
                }
                (void)editor_visibility_toggle("editor.soft_body.visibility",
                    &visibility_visible_label, &visibility_hidden_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        44.0f, 26.0f, 26.0f}, &project, EDITOR_VISIBILITY_SOFT_BODY,
                    selected->id, 0, body->id, body->visible);
                rohr_ui_label(&x_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    118.0f, 50.0f, 26.0f});
                x_result = rohr_ui_field("editor.soft_body.x",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_position.x},
                    &x_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 60.0f, 118.0f,
                        EDITOR_TOOLS_WIDTH - 70.0f, 26.0f}, NULL);
                rohr_ui_label(&y_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    154.0f, 50.0f, 26.0f});
                y_result = rohr_ui_field("editor.soft_body.y",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_position.y},
                    &y_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 60.0f, 154.0f,
                        EDITOR_TOOLS_WIDTH - 70.0f, 26.0f}, NULL);
                rohr_ui_label(&rotation_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    190.0f, 82.0f, 26.0f});
                rotation_result = rohr_ui_field("editor.soft_body.rotation",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &edited_rotation},
                    &length_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 92.0f, 190.0f,
                        EDITOR_TOOLS_WIDTH - 102.0f, 26.0f}, NULL);
                field_editing = name_result.active || x_result.active || y_result.active ||
                    rotation_result.active;
                if(x_result.changed || y_result.changed || rotation_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_SOFT_BODY_TRANSFORM,
                        .data.soft_body_transform = {selected->id, body->id,
                            edited_position, edited_rotation}};
                    (void)editor_command_execute(&project, &command);
                }
                {
                    rohr_ui_label(&node_color_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        226.0f, 90.0f, 26.0f});
                    (void)editor_color_swatch("editor.soft_body.node_color",
                        &body->node_color, false, &color_picker,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 226.0f,
                            EDITOR_TOOLS_WIDTH - 110.0f, 26.0f}, &project,
                        EDITOR_ITEM_SOFT_BODY, selected->id, 0, body->id,
                        EDITOR_PROPERTY_NODE_COLOR);
                    rohr_ui_label(&beam_color_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        258.0f, 90.0f, 26.0f});
                    (void)editor_color_swatch("editor.soft_body.beam_color",
                        &body->beam_color, false, &color_picker,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 258.0f,
                            EDITOR_TOOLS_WIDTH - 110.0f, 26.0f}, &project,
                        EDITOR_ITEM_SOFT_BODY, selected->id, 0, body->id,
                        EDITOR_PROPERTY_BEAM_COLOR);
                    rohr_ui_label(&area_color_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        290.0f, 90.0f, 26.0f});
                    (void)editor_color_swatch("editor.soft_body.area_color",
                        &body->area_color, false, &color_picker,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 290.0f,
                            EDITOR_TOOLS_WIDTH - 110.0f, 26.0f}, &project,
                        EDITOR_ITEM_SOFT_BODY, selected->id, 0, body->id,
                        EDITOR_PROPERTY_AREA_COLOR);
                }
                {
                    UIButtonStyle origin_style = editor_selected_button_style_get();
                    UIButtonResult origin_result = rohr_ui_button(
                        "editor.soft_body.origin", &origin_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 326.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 28.0f},
                        viewport_state.selection == EDITOR_SELECTION_ORIGIN &&
                            viewport_state.selected_origin_kind == EDITOR_ORIGIN_SOFT_BODY ?
                            &origin_style : NULL);
                    if(origin_result.clicked || origin_result.focus_changed) {
                        viewport_state.selection = EDITOR_SELECTION_ORIGIN;
                        viewport_state.selected_origin_kind = EDITOR_ORIGIN_SOFT_BODY;
                        if(origin_result.double_clicked)
                            viewport_state.mode = EDITOR_VIEWPORT_ORIGIN;
                    }
                }
                if(rohr_ui_button("editor.soft_body.auto_shape", &auto_shape_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 360.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL).clicked)
                    auto_shape_picker_open = !auto_shape_picker_open;
                if(!auto_shape_picker_open) {
                    if(rohr_ui_button("editor.soft_body.add_node", &add_node_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 396.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL).clicked) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
                        .data.item_add = {.kind = EDITOR_ITEM_SOFT_NODE,
                            .object = selected->id, .parent = body->id,
                            .position = {(float)body->node_count * 24.0f, 0.0f}}};
                    EditorCommandResult node = editor_command_execute(&project, &command);
                    if(node.kind == ERROR_RESULT_VALUE) {
                        viewport_state.selection = EDITOR_SELECTION_SOFT_NODE;
                        viewport_state.selected_soft_node = node.result.object;
                    }
                }
                    if(rohr_ui_button("editor.soft_body.add_beam", &add_beam_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 432.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 30.0f}, NULL).clicked) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
                        .data.item_add = {.kind = EDITOR_ITEM_SOFT_BEAM,
                            .object = selected->id, .parent = body->id}};
                    EditorCommandResult beam = editor_command_execute(&project, &command);
                    if(beam.kind == ERROR_RESULT_VALUE) {
                        viewport_state.selection = EDITOR_SELECTION_SOFT_BEAM;
                        viewport_state.selected_soft_beam = beam.result.object;
                    }
                }
                    {
                    editor_project_soft_body_hierarchy_sync(body);
                    for(size_t hierarchy_index = 0;
                            hierarchy_index < body->hierarchy_count;
                            hierarchy_index += 1) {
                        EditorSoftHierarchyItem item = body->hierarchy[hierarchy_index];
                        UIButtonStyle selected_style = editor_selected_button_style_get();
                        EditorHierarchySelection selection_kind;
                        EditorVisibilityKind visibility_kind;
                        TextAsset *label = NULL;
                        const char *name = NULL;
                        char *cache = NULL;
                        bool visible = false;
                        bool item_selected = false;
                        float y = 476.0f + (float)hierarchy_index * 28.0f;
                        char id[64];
                        char visibility_id[72];
                        if(item.kind == EDITOR_SOFT_HIERARCHY_NODE) {
                            for(size_t i = 0; i < body->node_count; i += 1)
                                if(body->nodes[i].id == item.id) {
                                    name = body->nodes[i].name;
                                    visible = body->nodes[i].visible;
                                    label = &soft_node_labels[i];
                                    cache = soft_node_cache[i];
                                }
                            selection_kind = EDITOR_SELECTION_SOFT_NODE;
                            visibility_kind = EDITOR_VISIBILITY_SOFT_NODE;
                            item_selected = viewport_state.selection == selection_kind &&
                                viewport_state.selected_soft_node == item.id;
                            snprintf(id, sizeof(id), "editor.soft_node.%u", item.id);
                            snprintf(visibility_id, sizeof(visibility_id),
                                "editor.soft_node.%u.visibility", item.id);
                        } else if(item.kind == EDITOR_SOFT_HIERARCHY_BEAM) {
                            for(size_t i = 0; i < body->beam_count; i += 1)
                                if(body->beams[i].id == item.id) {
                                    name = body->beams[i].name;
                                    visible = body->beams[i].visible;
                                    label = &soft_beam_labels[i];
                                    cache = soft_beam_cache[i];
                                }
                            selection_kind = EDITOR_SELECTION_SOFT_BEAM;
                            visibility_kind = EDITOR_VISIBILITY_SOFT_BEAM;
                            item_selected = viewport_state.selection == selection_kind &&
                                viewport_state.selected_soft_beam == item.id;
                            snprintf(id, sizeof(id), "editor.soft_beam.%u", item.id);
                            snprintf(visibility_id, sizeof(visibility_id),
                                "editor.soft_beam.%u.visibility", item.id);
                        } else {
                            for(size_t i = 0; i < body->area_count; i += 1)
                                if(body->areas[i].id == item.id) {
                                    name = body->areas[i].name;
                                    visible = body->areas[i].visible;
                                    label = &soft_area_labels[i];
                                    cache = soft_area_cache[i];
                                }
                            selection_kind = EDITOR_SELECTION_SOFT_AREA;
                            visibility_kind = EDITOR_VISIBILITY_SOFT_AREA;
                            item_selected = viewport_state.selection == selection_kind &&
                                viewport_state.selected_soft_area == item.id;
                            snprintf(id, sizeof(id), "editor.soft_area.%u", item.id);
                            snprintf(visibility_id, sizeof(visibility_id),
                                "editor.soft_area.%u.visibility", item.id);
                        }
                        if(name == NULL || label == NULL || cache == NULL) continue;
                        if(!editor_named_text_sync(&font, name, label, cache,
                                EDITOR_OBJECT_NAME_MAX)) goto fail;
                        (void)editor_visibility_toggle(visibility_id,
                            &visibility_visible_label, &visibility_hidden_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, y, 24.0f, 24.0f},
                            &project, visibility_kind, selected->id,
                            body->id, item.id, visible);
                        {
                            UIButtonResult result = rohr_ui_button(id, label,
                                (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f, y,
                                    EDITOR_TOOLS_WIDTH - 48.0f, 24.0f},
                                item_selected ||
                                    editor_selection_path_check(&viewport_state,
                                        selection_kind, selected->id,
                                        body->id, 0, item.id) ?
                                    &selected_style : NULL);
                            editor_hierarchy_drag_row(&hierarchy_drag, &viewport_state,
                                (EditorSelectionRef){selection_kind,
                                    selected->id, body->id, 0, item.id},
                                (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f, y,
                                    EDITOR_TOOLS_WIDTH - 48.0f, 24.0f}, result,
                                hierarchy_pointer, hierarchy_primary,
                                panel_scroll_offset,
                                hierarchy_index + 1 == body->hierarchy_count);
                            if(result.clicked || result.focus_changed) {
                                viewport_state.selection = selection_kind;
                                if(selection_kind == EDITOR_SELECTION_SOFT_NODE)
                                    viewport_state.selected_soft_node = item.id;
                                else if(selection_kind == EDITOR_SELECTION_SOFT_BEAM)
                                    viewport_state.selected_soft_beam = item.id;
                                else {
                                    viewport_state.selected_soft_area = item.id;
                                    viewport_state.soft_area_candidates[0] = item.id;
                                    viewport_state.soft_area_candidate_count = 1;
                                }
                                if(result.double_clicked) (void)editor_navigation_selected_open(
                                    &project, &viewport_state);
                            }
                        }
                    }
                    }
                }
                if(auto_shape_picker_open) {
                    size_t auto_shape_point_count =
                        editor_auto_shape_soft_body_points_capture(&viewport_state,
                            selected, body);
                    int selected_shape = editor_auto_shape_picker_draw(
                        "editor.soft_body.auto_shape.option",
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 394.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 62.0f},
                        auto_shape_point_count > 0 ? auto_shape_point_count :
                            body->node_count, &triangle_label,
                        &rectangle_label, &circle_label);
                    if(selected_shape >= 0) {
                        auto_shape_config.kind = (EditorAutoShapeKind)selected_shape;
                        viewport_state.auto_shape_parent_mode = EDITOR_VIEWPORT_SOFT_BODY;
                        (void)editor_auto_shape_apply(&project, &viewport_state,
                            EDITOR_VIEWPORT_SOFT_BODY, auto_shape_config);
                        viewport_state.mode = EDITOR_VIEWPORT_AUTO_SHAPE;
                        viewport_state.selection = EDITOR_SELECTION_SOFT_BODY;
                        auto_shape_first_field_was_active = false;
                        auto_shape_second_field_was_active = false;
                        auto_shape_third_field_was_active = false;
                        auto_shape_picker_open = false;
                    }
                }
                if(rohr_ui_button("editor.soft_body.delete", &delete_soft_body_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                            editor_panel_delete_y_get(&project, &viewport_state),
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f},
                        &delete_style).clicked) {
                    (void)editor_open_item_delete(&project, &viewport_state);
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SOFT_NODE) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorSoftBody *body = editor_selected_soft_body_get(selected, &viewport_state);
            EditorSoftNode *node = editor_selected_soft_node_get(body, &viewport_state);
            if(node != NULL) {
                size_t index = (size_t)(node - body->nodes);
                UIButtonStyle delete_style = editor_delete_button_style_get();
                UIFieldResult x_result;
                UIFieldResult y_result;
                UIFieldResult mass_result;
                UIFieldResult radius_result;
                UIFieldResult friction_result;
                UIFieldResult restitution_result;
                UIFieldResult name_result;
                Position edited_position = node->position;
                float edited_mass = node->node_mass;
                float edited_friction = node->friction;
                float edited_restitution = node->restitution;
                char edited_name[EDITOR_OBJECT_NAME_MAX];
                snprintf(edited_name, sizeof(edited_name), "%s", node->name);
                if(!editor_named_text_sync(&font, node->name, &soft_node_labels[index],
                        soft_node_cache[index], EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&name_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f,
                    40.0f, 48.0f, 30.0f});
                name_result = editor_property_name_field("editor.soft_node.name", edited_name,
                    sizeof(edited_name), &soft_node_labels[index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 88.0f, 40.0f,
                        EDITOR_TOOLS_WIDTH - 96.0f, 30.0f});
                if(name_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
                        .data.item_rename = {.kind = EDITOR_ITEM_SOFT_NODE,
                            .object = selected->id, .parent = body->id,
                            .item = node->id}};
                    snprintf(command.data.item_rename.name,
                        sizeof(command.data.item_rename.name), "%s", edited_name);
                    (void)editor_command_execute(&project, &command);
                }
                (void)editor_visibility_toggle("editor.soft_node.visibility",
                    &visibility_visible_label, &visibility_hidden_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        44.0f, 26.0f, 26.0f}, &project, EDITOR_VISIBILITY_SOFT_NODE,
                    selected->id, body->id, node->id, node->visible);
                rohr_ui_label(&x_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 122.0f, 50.0f, 26.0f});
                x_result = rohr_ui_field("editor.soft_node.x",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_position.x},
                    &x_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 60.0f, 122.0f,
                        EDITOR_TOOLS_WIDTH - 70.0f, 26.0f}, NULL);
                rohr_ui_label(&y_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 158.0f, 50.0f, 26.0f});
                y_result = rohr_ui_field("editor.soft_node.y",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_position.y},
                    &y_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 60.0f, 158.0f,
                        EDITOR_TOOLS_WIDTH - 70.0f, 26.0f}, NULL);
                if(x_result.changed || y_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_SOFT_NODE_POSITION,
                        .data.soft_node_position = {selected->id, body->id,
                            node->id, edited_position}};
                    (void)editor_command_execute(&project, &command);
                }
                rohr_ui_label(&mass_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 194.0f, 68.0f, 26.0f});
                mass_result = rohr_ui_field("editor.soft_node.mass",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &edited_mass},
                    &length_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 78.0f, 194.0f,
                        EDITOR_TOOLS_WIDTH - 88.0f, 26.0f}, NULL);
                if(mass_result.changed) {
                    edited_mass = fmaxf(0.0f, edited_mass);
                    editor_property_float_set(&project, EDITOR_ITEM_SOFT_NODE,
                        selected->id, body->id, node->id, 0,
                        EDITOR_PROPERTY_MASS, edited_mass);
                }
                rohr_ui_label(&radius_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    230.0f, 68.0f, 26.0f});
                radius_result = rohr_ui_field("editor.soft_node.radius",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &node->radius},
                    &length_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 78.0f, 230.0f,
                        EDITOR_TOOLS_WIDTH - 88.0f, 26.0f}, NULL);
                if(radius_result.changed && node->radius <= 0.0f) node->radius = 0.1f;
                rohr_ui_label(&friction_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    266.0f, 68.0f, 26.0f});
                friction_result = rohr_ui_field("editor.soft_node.friction",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &edited_friction},
                    &length_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 78.0f, 266.0f,
                        EDITOR_TOOLS_WIDTH - 88.0f, 26.0f}, NULL);
                if(friction_result.changed) edited_friction = fmaxf(0.0f, edited_friction);
                if(friction_result.changed) editor_property_float_set(&project,
                    EDITOR_ITEM_SOFT_NODE, selected->id, body->id, node->id, 0,
                    EDITOR_PROPERTY_FRICTION, edited_friction);
                rohr_ui_label(&restitution_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    302.0f, 96.0f, 26.0f});
                restitution_result = rohr_ui_field("editor.soft_node.restitution",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_restitution},
                    &length_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 106.0f, 302.0f,
                        EDITOR_TOOLS_WIDTH - 116.0f, 26.0f}, NULL);
                if(restitution_result.changed) edited_restitution =
                    fminf(1.0f, fmaxf(0.0f, edited_restitution));
                if(restitution_result.changed) editor_property_float_set(&project,
                    EDITOR_ITEM_SOFT_NODE, selected->id, body->id, node->id, 0,
                    EDITOR_PROPERTY_RESTITUTION, edited_restitution);
                {
                    bool gravity_enabled = node->gravity_enabled;
                    if(editor_checkbox("editor.soft_node.gravity", &gravity_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 338.0f,
                        EDITOR_TOOLS_WIDTH - 20.0f, 28.0f},
                    &gravity_enabled)) editor_property_bool_set(&project,
                        EDITOR_ITEM_SOFT_NODE, selected->id, body->id, node->id, 0,
                        EDITOR_PROPERTY_GRAVITY, gravity_enabled);
                }
                {
                    float row_x = EDITOR_VIEWPORT_WIDTH + 10.0f;
                    float row_width = EDITOR_TOOLS_WIDTH - 20.0f;
                    float controls_bottom = 406.0f;

                    bool collision_enabled = node->collision_enabled;
                    if(editor_checkbox("editor.soft_node.collision", &collision_label,
                            (UIRect){row_x, 374.0f, row_width, 28.0f},
                            &collision_enabled)) {
                        editor_property_bool_set(&project, EDITOR_ITEM_SOFT_NODE,
                            selected->id, body->id, node->id, 0,
                            EDITOR_PROPERTY_COLLISION, collision_enabled);
                        if(!collision_enabled) {
                            collision_category_open = false;
                            collide_with_open = false;
                        }
                    }
                    if(node->collision_enabled && rohr_ui_button(
                            "editor.soft_node.collision_category",
                            &collision_category_label,
                            (UIRect){row_x, controls_bottom, row_width, 28.0f},
                            NULL).clicked) {
                        collision_category_open = !collision_category_open;
                        collide_with_open = false;
                    }
                    if(node->collision_enabled) {
                        rohr_ui_border((UIRect){row_x, controls_bottom, row_width, 28.0f},
                            2.0f, (Color){0, 0, 0, 255});
                        controls_bottom += 32.0f;
                    }
                    if(node->collision_enabled && collision_category_open) {
                        size_t rows = 0;
                        if(!editor_collision_mask_menu_draw(
                                "editor.soft_node.collision_category.mask", &project,
                                &node->collision_category, EDITOR_ITEM_SOFT_NODE,
                                selected->id, body->id, node->id,
                                EDITOR_COLLISION_FILTER_CATEGORY, &font,
                                collision_mask_labels,
                                collision_mask_cache, collision_mask_name,
                                sizeof(collision_mask_name), &collision_mask_name_field,
                                &add_label, row_x, controls_bottom, row_width,
                                &field_editing, &rows)) goto fail;
                        controls_bottom += (float)rows * 30.0f;
                    }
                    if(node->collision_enabled && rohr_ui_button(
                            "editor.soft_node.collide_with", &collide_with_label,
                            (UIRect){row_x, controls_bottom, row_width, 28.0f},
                            NULL).clicked) {
                        collide_with_open = !collide_with_open;
                        collision_category_open = false;
                    }
                    if(node->collision_enabled) {
                        rohr_ui_border((UIRect){row_x, controls_bottom, row_width, 28.0f},
                            2.0f, (Color){0, 0, 0, 255});
                        controls_bottom += 32.0f;
                    }
                    if(node->collision_enabled && collide_with_open) {
                        size_t rows = 0;
                        if(!editor_collision_mask_menu_draw(
                                "editor.soft_node.collide_with.mask", &project,
                                &node->collision_with, EDITOR_ITEM_SOFT_NODE,
                                selected->id, body->id, node->id,
                                EDITOR_COLLISION_FILTER_COLLIDE_WITH, &font,
                                collision_mask_labels,
                                collision_mask_cache, collision_mask_name,
                                sizeof(collision_mask_name), &collision_mask_name_field,
                                &add_label, row_x, controls_bottom, row_width,
                                &field_editing, &rows)) goto fail;
                        controls_bottom += (float)rows * 30.0f;
                    }
                    if((collision_category_open || collide_with_open) &&
                            mouse.button_states[MOUSE_BUTTON_LEFT] ==
                                MOUSE_BUTTON_STATE_PRESSED) {
                        Position pointer = rohr_graphics_mouse_screen_position_get();
                        if(pointer.x < row_x || pointer.x > row_x + row_width ||
                                pointer.y < 374.0f || pointer.y > controls_bottom) {
                            collision_category_open = false;
                            collide_with_open = false;
                        }
                    }
                }
                field_editing = name_result.active || x_result.active || y_result.active ||
                    mass_result.active || radius_result.active || friction_result.active ||
                    restitution_result.active || field_editing;
                {
                    bool inherit = !node->color_overridden;
                    float field_width = fmaxf(34.0f, EDITOR_TOOLS_WIDTH - 196.0f);
                    if(!node->color_overridden) node->color = body->node_color;
                    rohr_ui_label(&node_color_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        620.0f, 90.0f, 26.0f});
                    if(editor_checkbox_label_left("editor.soft_node.color_inherit",
                            &inherit_label, (UIRect){EDITOR_VIEWPORT_WIDTH +
                                EDITOR_TOOLS_WIDTH - 92.0f,
                                620.0f, 82.0f, 26.0f}, &inherit)) {
                        node->color_overridden = !inherit;
                        node->color = body->node_color;
                    }
                    (void)editor_color_swatch("editor.soft_node.color", &node->color,
                        inherit, &color_picker,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 620.0f,
                            field_width, 26.0f}, &project, EDITOR_ITEM_SOFT_NODE,
                        selected->id, body->id, node->id, EDITOR_PROPERTY_COLOR);
                }
                if(rohr_ui_button("editor.soft_node.delete", &delete_node_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                            editor_panel_delete_y_get(&project, &viewport_state),
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, &delete_style).clicked) {
                    (void)editor_open_item_delete(&project, &viewport_state);
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SOFT_BEAM) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorSoftBody *body = editor_selected_soft_body_get(selected, &viewport_state);
            EditorSoftBeam *beam = editor_selected_soft_beam_get(body, &viewport_state);
            if(beam != NULL) {
                size_t index = (size_t)(beam - body->beams);
                UIButtonStyle delete_style = editor_delete_button_style_get();
                UIFieldResult stiffness_result;
                UIFieldResult damping_result;
                UIFieldResult name_result;
                float edited_stiffness = beam->stiffness;
                float edited_damping = beam->damping;
                char edited_name[EDITOR_OBJECT_NAME_MAX];
                snprintf(edited_name, sizeof(edited_name), "%s", beam->name);
                if(!editor_named_text_sync(&font, beam->name, &soft_beam_labels[index],
                        soft_beam_cache[index], EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&name_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f,
                    40.0f, 48.0f, 30.0f});
                name_result = editor_property_name_field("editor.soft_beam.name", edited_name,
                    sizeof(edited_name), &soft_beam_labels[index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 88.0f, 40.0f,
                        EDITOR_TOOLS_WIDTH - 96.0f, 30.0f});
                if(name_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
                        .data.item_rename = {.kind = EDITOR_ITEM_SOFT_BEAM,
                            .object = selected->id, .parent = body->id,
                            .item = beam->id}};
                    snprintf(command.data.item_rename.name,
                        sizeof(command.data.item_rename.name), "%s", edited_name);
                    (void)editor_command_execute(&project, &command);
                }
                (void)editor_visibility_toggle("editor.soft_beam.visibility",
                    &visibility_visible_label, &visibility_hidden_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        44.0f, 26.0f, 26.0f}, &project, EDITOR_VISIBILITY_SOFT_BEAM,
                    selected->id, body->id, beam->id, beam->visible);
                rohr_ui_label(&node_a_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 122.0f, 70.0f, 28.0f});
                rohr_ui_label(&node_b_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 158.0f, 70.0f, 28.0f});
                {
                    const TextAsset *node_options[EDITOR_SOFT_NODE_MAX + 1];
                    size_t selected_a = 0;
                    size_t selected_b = 0;
                    node_options[0] = &none_label;
                    for(size_t i = 0; i < body->node_count; i += 1) {
                        if(!editor_named_text_sync(&font, body->nodes[i].name,
                                &soft_node_labels[i], soft_node_cache[i],
                                EDITOR_OBJECT_NAME_MAX)) goto fail;
                        node_options[i + 1] = &soft_node_labels[i];
                        if(body->nodes[i].id == beam->node_a) selected_a = i + 1;
                        if(body->nodes[i].id == beam->node_b) selected_b = i + 1;
                    }
                    UIDropdownResult a_result = rohr_ui_dropdown("editor.soft_beam.node_a",
                        node_options, body->node_count + 1, selected_a,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 80.0f, 122.0f,
                            EDITOR_TOOLS_WIDTH - 90.0f, 28.0f}, NULL);
                    UIDropdownResult b_result = rohr_ui_dropdown("editor.soft_beam.node_b",
                        node_options, body->node_count + 1, selected_b,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 80.0f, 158.0f,
                            EDITOR_TOOLS_WIDTH - 90.0f, 28.0f}, NULL);
                    if(a_result.button_hovered) viewport_state.preview_soft_node = beam->node_a;
                    else if(a_result.hovered_index > 0) viewport_state.preview_soft_node =
                        body->nodes[a_result.hovered_index - 1].id;
                    if(b_result.button_hovered) viewport_state.preview_soft_node = beam->node_b;
                    else if(b_result.hovered_index > 0) viewport_state.preview_soft_node =
                        body->nodes[b_result.hovered_index - 1].id;
                    if(a_result.changed) editor_relationship_set(&project,
                        EDITOR_RELATIONSHIP_SOFT_BEAM_NODE, selected->id, body->id,
                        beam->id, 0, a_result.selected_index == 0 ? 0 :
                            body->nodes[a_result.selected_index - 1].id);
                    if(b_result.changed) editor_relationship_set(&project,
                        EDITOR_RELATIONSHIP_SOFT_BEAM_NODE, selected->id, body->id,
                        beam->id, 1, b_result.selected_index == 0 ? 0 :
                            body->nodes[b_result.selected_index - 1].id);
                }
                rohr_ui_label(&stiffness_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    196.0f, 90.0f, 26.0f});
                stiffness_result = rohr_ui_field("editor.soft_beam.stiffness",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_stiffness},
                    &length_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 196.0f,
                        EDITOR_TOOLS_WIDTH - 110.0f, 26.0f}, NULL);
                if(stiffness_result.changed) {
                    edited_stiffness = fmaxf(0.0f, edited_stiffness);
                    editor_property_float_set(&project, EDITOR_ITEM_SOFT_BEAM,
                        selected->id, body->id, beam->id, 0,
                        EDITOR_PROPERTY_STIFFNESS, edited_stiffness);
                }
                rohr_ui_label(&damping_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    232.0f, 90.0f, 26.0f});
                damping_result = rohr_ui_field("editor.soft_beam.damping",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &edited_damping},
                    &length_field, (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 232.0f,
                        EDITOR_TOOLS_WIDTH - 110.0f, 26.0f}, NULL);
                if(damping_result.changed) {
                    edited_damping = fmaxf(0.0f, edited_damping);
                    editor_property_float_set(&project, EDITOR_ITEM_SOFT_BEAM,
                        selected->id, body->id, beam->id, 0,
                        EDITOR_PROPERTY_DAMPING, edited_damping);
                }
                field_editing = name_result.active || stiffness_result.active ||
                    damping_result.active;
                {
                    bool inherit = !beam->color_overridden;
                    float field_width = fmaxf(34.0f, EDITOR_TOOLS_WIDTH - 196.0f);
                    if(!beam->color_overridden) beam->color = body->beam_color;
                    rohr_ui_label(&beam_color_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        268.0f, 90.0f, 26.0f});
                    if(editor_checkbox_label_left("editor.soft_beam.color_inherit",
                            &inherit_label, (UIRect){EDITOR_VIEWPORT_WIDTH +
                                EDITOR_TOOLS_WIDTH - 92.0f,
                                268.0f, 82.0f, 26.0f}, &inherit)) {
                        beam->color_overridden = !inherit;
                        beam->color = body->beam_color;
                    }
                    (void)editor_color_swatch("editor.soft_beam.color", &beam->color,
                        inherit, &color_picker,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 268.0f,
                            field_width, 26.0f}, &project, EDITOR_ITEM_SOFT_BEAM,
                        selected->id, body->id, beam->id, EDITOR_PROPERTY_COLOR);
                }
                if(rohr_ui_button("editor.soft_beam.delete", &delete_beam_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                            editor_panel_delete_y_get(&project, &viewport_state),
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, &delete_style).clicked) {
                    (void)editor_open_item_delete(&project, &viewport_state);
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SOFT_AREA) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorSoftBody *body = editor_selected_soft_body_get(selected, &viewport_state);
            EditorSoftArea *area = editor_selected_soft_area_get(body, &viewport_state);
            if(area != NULL) {
                size_t index = (size_t)(area - body->areas);
                UIFieldResult name_result;
                float controls_y = 42.0f;
                for(size_t candidate_index = 0;
                        candidate_index < viewport_state.soft_area_candidate_count;
                        candidate_index += 1) {
                    EditorSoftArea *candidate = NULL;
                    UIButtonStyle selected_style = editor_selected_button_style_get();
                    UIButtonResult result;
                    char id[64];
                    size_t candidate_body_index;
                    for(size_t i = 0; i < body->area_count; i += 1) {
                        if(body->areas[i].id ==
                                viewport_state.soft_area_candidates[candidate_index]) {
                            candidate = &body->areas[i];
                        }
                    }
                    if(candidate == NULL) continue;
                    candidate_body_index = (size_t)(candidate - body->areas);
                    if(!editor_named_text_sync(&font, candidate->name,
                            &soft_area_labels[candidate_body_index],
                            soft_area_cache[candidate_body_index],
                            EDITOR_OBJECT_NAME_MAX)) goto fail;
                    snprintf(id, sizeof(id), "editor.soft_area.candidate.%u", candidate->id);
                    result = rohr_ui_button(id, &soft_area_labels[candidate_body_index],
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, controls_y,
                            EDITOR_TOOLS_WIDTH - 20.0f, 28.0f},
                        candidate->id == area->id ? &selected_style : NULL);
                    if(result.clicked || result.focus_changed) {
                        viewport_state.selected_soft_area = candidate->id;
                        viewport_state.selection = EDITOR_SELECTION_SOFT_AREA;
                        boundary_beam_color_area = 0;
                    }
                    controls_y += 32.0f;
                }
                controls_y += 10.0f;
                if(!editor_named_text_sync(&font, area->name, &soft_area_labels[index],
                        soft_area_cache[index], EDITOR_OBJECT_NAME_MAX)) goto fail;
                if(!area->color_overridden) area->color = body->area_color;
                rohr_ui_label(&name_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    controls_y, 48.0f, 30.0f});
                name_result = editor_property_name_field("editor.soft_area.name", area->name,
                    sizeof(area->name), &soft_area_labels[index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 58.0f, controls_y,
                        EDITOR_TOOLS_WIDTH - 68.0f, 30.0f});
                controls_y += 40.0f;
                rohr_ui_label(&area_color_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    controls_y, 90.0f, 26.0f});
                {
                    bool inherit = !area->color_overridden;
                    float field_width = fmaxf(34.0f, EDITOR_TOOLS_WIDTH - 196.0f);
                    if(editor_checkbox_label_left("editor.soft_area.color_inherit",
                            &inherit_label, (UIRect){EDITOR_VIEWPORT_WIDTH +
                                EDITOR_TOOLS_WIDTH - 92.0f,
                                controls_y, 82.0f, 26.0f}, &inherit)) {
                        area->color_overridden = !inherit;
                        area->color = body->area_color;
                    }
                    (void)editor_color_swatch("editor.soft_area.color", &area->color,
                        inherit, &color_picker,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, controls_y,
                            field_width, 26.0f}, &project, EDITOR_ITEM_SOFT_AREA,
                        selected->id, body->id, area->id, EDITOR_PROPERTY_COLOR);
                }
                field_editing = name_result.active;
                controls_y += 36.0f;
                if(boundary_beam_color_area != area->id) {
                    EditorSoftBeam *first = editor_soft_area_beam_get(body, area, 0);
                    boundary_beam_color = first != NULL && first->color_overridden ?
                        first->color : body->beam_color;
                    boundary_beam_color_area = area->id;
                }
                {
                    bool inherit = true;
                    float field_width = fmaxf(34.0f, EDITOR_TOOLS_WIDTH - 196.0f);
                    for(size_t edge = 0; edge < area->node_count; edge += 1) {
                        EditorSoftBeam *beam = editor_soft_area_beam_get(body, area, edge);
                        if(beam != NULL && beam->color_overridden) inherit = false;
                    }
                    rohr_ui_label(&beam_color_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        controls_y, 90.0f, 26.0f});
                    if(editor_checkbox_label_left("editor.soft_area.beam_color_inherit",
                            &inherit_label, (UIRect){EDITOR_VIEWPORT_WIDTH +
                                EDITOR_TOOLS_WIDTH - 92.0f,
                                controls_y, 82.0f, 26.0f}, &inherit)) {
                        for(size_t edge = 0; edge < area->node_count; edge += 1) {
                            EditorSoftBeam *beam = editor_soft_area_beam_get(body, area, edge);
                            if(beam == NULL) continue;
                            beam->color_overridden = !inherit;
                            beam->color = body->beam_color;
                        }
                        boundary_beam_color = body->beam_color;
                    }
                    (void)editor_color_swatch("editor.soft_area.boundary_beam_color",
                        &boundary_beam_color, inherit, &color_picker,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, controls_y,
                            field_width, 26.0f}, NULL, EDITOR_ITEM_SOFT_BEAM,
                        0, 0, 0, EDITOR_PROPERTY_COLOR);
                    if(!inherit && color_picker.target == &boundary_beam_color) {
                        for(size_t edge = 0; edge < area->node_count; edge += 1) {
                            EditorSoftBeam *beam = editor_soft_area_beam_get(body, area, edge);
                            if(beam == NULL) continue;
                            beam->color = boundary_beam_color;
                            beam->color_overridden = true;
                        }
                    }
                }
                controls_y += 36.0f;
                for(size_t edge = 0; edge < area->node_count; edge += 1) {
                    EditorSoftBeam *beam = editor_soft_area_beam_get(body, area, edge);
                    UIButtonStyle selected_style = editor_selected_button_style_get();
                    char id[64];
                    size_t beam_index;
                    UIButtonResult result;
                    if(beam == NULL) continue;
                    beam_index = (size_t)(beam - body->beams);
                    if(!editor_named_text_sync(&font, beam->name,
                            &soft_beam_labels[beam_index], soft_beam_cache[beam_index],
                            EDITOR_OBJECT_NAME_MAX)) goto fail;
                    snprintf(id, sizeof(id), "editor.soft_area.beam.%u", beam->id);
                    result = rohr_ui_button(id, &soft_beam_labels[beam_index],
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                            controls_y + (float)edge * 34.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, &selected_style);
                    if(result.clicked || result.focus_changed) {
                        viewport_state.selection = EDITOR_SELECTION_SOFT_BEAM;
                        viewport_state.selected_soft_beam = beam->id;
                        viewport_state.mode = EDITOR_VIEWPORT_SOFT_BEAM;
                    }
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SPRITE) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorSprite *sprite = editor_project_sprite_get(selected,
                viewport_state.selected_sprite);
            if(sprite != NULL) {
                size_t sprite_index = (size_t)(sprite - selected->sprites);
                char edited_name[EDITOR_OBJECT_NAME_MAX];
                char edited_path[EDITOR_ASSET_PATH_MAX];
                Position edited_position = sprite->position;
                float width = sprite->size.x;
                float height = sprite->size.y;
                bool visible = sprite->visible;
                UIButtonStyle delete_style = editor_delete_button_style_get();
                snprintf(edited_name, sizeof(edited_name), "%s", sprite->name);
                snprintf(edited_path, sizeof(edited_path), "%s", sprite->path);
                if(sprite_index < 64 && !editor_named_text_sync(&font, sprite->name,
                        &sprite_labels[sprite_index], sprite_cache[sprite_index],
                        EDITOR_OBJECT_NAME_MAX)) goto fail;
                rohr_ui_label(&name_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    42.0f, 70.0f, 28.0f});
                UIFieldResult name_result = rohr_ui_field("editor.sprite.name",
                    (UIFieldBinding){.kind = UI_FIELD_STRING, .string = edited_name,
                        .string_capacity = sizeof(edited_name)},
                    &sprite_labels[(size_t)(sprite - selected->sprites) < 64 ?
                        (size_t)(sprite - selected->sprites) : 0],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 82.0f, 42.0f,
                        EDITOR_TOOLS_WIDTH - 92.0f, 28.0f}, NULL);
                rohr_ui_label(&path_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    80.0f, 70.0f, 28.0f});
                UIFieldResult path_result = rohr_ui_field("editor.sprite.path",
                    (UIFieldBinding){.kind = UI_FIELD_STRING, .string = edited_path,
                        .string_capacity = sizeof(edited_path)}, &path_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 82.0f, 80.0f,
                        EDITOR_TOOLS_WIDTH - 92.0f, 28.0f}, NULL);
                rohr_ui_label(&x_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    118.0f, 70.0f, 28.0f});
                UIFieldResult x_result = rohr_ui_field("editor.sprite.x",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_position.x}, &x_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 82.0f, 118.0f,
                        EDITOR_TOOLS_WIDTH - 92.0f, 28.0f}, NULL);
                rohr_ui_label(&y_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    156.0f, 70.0f, 28.0f});
                UIFieldResult y_result = rohr_ui_field("editor.sprite.y",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_position.y}, &y_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 82.0f, 156.0f,
                        EDITOR_TOOLS_WIDTH - 92.0f, 28.0f}, NULL);
                rohr_ui_label(&width_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    194.0f, 70.0f, 28.0f});
                UIFieldResult width_result = rohr_ui_field("editor.sprite.width",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &width}, &x_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 82.0f, 194.0f,
                        EDITOR_TOOLS_WIDTH - 92.0f, 28.0f}, NULL);
                rohr_ui_label(&height_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    232.0f, 70.0f, 28.0f});
                UIFieldResult height_result = rohr_ui_field("editor.sprite.height",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &height}, &y_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 82.0f, 232.0f,
                        EDITOR_TOOLS_WIDTH - 92.0f, 28.0f}, NULL);
                bool visible_changed = editor_checkbox("editor.sprite.visible",
                    &terminal_visible_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                        270.0f, EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, &visible);
                if(name_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_RENAME,
                        .data.sprite_rename = {.object = selected->id,
                            .sprite = sprite->id}};
                    snprintf(command.data.sprite_rename.name,
                        sizeof(command.data.sprite_rename.name), "%s", edited_name);
                    (void)editor_command_execute(&project, &command);
                }
                if(path_result.changed && edited_path[0] != '\0') {
                    EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_PATH_SET,
                        .data.sprite_path_set = {.object = selected->id,
                            .sprite = sprite->id}};
                    snprintf(command.data.sprite_path_set.path,
                        sizeof(command.data.sprite_path_set.path), "%s", edited_path);
                    (void)editor_command_execute(&project, &command);
                }
                if(x_result.changed || y_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_POSITION_SET,
                        .data.sprite_position_set = {.object = selected->id,
                            .sprite = sprite->id, .position = edited_position}};
                    (void)editor_command_execute(&project, &command);
                }
                if(width_result.changed || height_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_SIZE_SET,
                        .data.sprite_size_set = {.object = selected->id, .sprite = sprite->id,
                            .size = {width, height}}};
                    (void)editor_command_execute(&project, &command);
                }
                if(visible_changed) {
                    EditorCommand command = {.type =
                        EDITOR_COMMAND_SPRITE_VISIBILITY_SET,
                        .data.sprite_visibility_set = {.object = selected->id,
                            .sprite = sprite->id, .visible = visible}};
                    (void)editor_command_execute(&project, &command);
                }
                field_editing = name_result.active || path_result.active ||
                    x_result.active || y_result.active || width_result.active ||
                    height_result.active;
                if(rohr_ui_button("editor.sprite.delete", &delete_sprite_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                            editor_panel_delete_y_get(&project, &viewport_state),
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, &delete_style).clicked) {
                    EditorCommand command = {.type = EDITOR_COMMAND_SPRITE_REMOVE,
                        .data.sprite_remove = {.object = selected->id,
                            .sprite = sprite->id}};
                    if(editor_command_execute(&project, &command).kind ==
                            ERROR_RESULT_VALUE) {
                        viewport_state.mode = EDITOR_VIEWPORT_HIERARCHY;
                        viewport_state.selection = EDITOR_SELECTION_NONE;
                        viewport_state.selected_sprite = 0;
                    }
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_ANIMATED_SPRITE) {
            EditorObject *selected = editor_project_selected_get(&project);
            EditorAnimatedSprite *sprite = editor_project_animated_sprite_get(selected,
                viewport_state.selected_animated_sprite);
            if(sprite != NULL && selected != NULL) {
                size_t animated_index = (size_t)(sprite -
                    selected->animated_sprite_items);
                char edited_name[EDITOR_OBJECT_NAME_MAX];
                EditorRigidBody *attached_body = editor_project_rigid_body_get(
                    selected, sprite->rigid_body);
                Position edited_position = attached_body != NULL ?
                    attached_body->position : sprite->editor_position;
                float scale_x = sprite->scale.x, scale_y = sprite->scale.y;
                float ticks = (float)sprite->ticks_per_frame;
                float seconds = (float)sprite->time_per_frame;
                float starting = (float)sprite->starting_frame;
                bool visible = sprite->visible;
                bool visible_changed;
                const TextAsset *body_options[EDITOR_RIGID_BODY_MAX + 1];
                const TextAsset *direction_options[2] = {&left_label, &right_label};
                size_t body_selected = 0;
                UIButtonStyle delete_style = editor_delete_button_style_get();
                snprintf(edited_name, sizeof(edited_name), "%s", sprite->name);
                if(animated_index < 32 && !editor_named_text_sync(&font, sprite->name,
                        &animated_sprite_labels[animated_index],
                        animated_sprite_cache[animated_index],
                        EDITOR_OBJECT_NAME_MAX)) goto fail;
                visible_changed = rohr_ui_button("editor.animated_sprite.visible",
                    visible ? &visibility_visible_label : &visibility_hidden_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 44.0f, 26.0f, 26.0f},
                    NULL).clicked;
                if(visible_changed) visible = !visible;
                rohr_ui_label(&name_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 42.0f,
                    42.0f, 40.0f, 28.0f});
                UIFieldResult name_result = rohr_ui_field("editor.animated_sprite.name",
                    (UIFieldBinding){.kind = UI_FIELD_STRING, .string = edited_name,
                        .string_capacity = sizeof(edited_name)},
                    &animated_sprite_labels[(size_t)(sprite -
                        selected->animated_sprite_items) < 32 ?
                        (size_t)(sprite - selected->animated_sprite_items) : 0],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 88.0f, 42.0f,
                        EDITOR_TOOLS_WIDTH - 96.0f, 28.0f}, NULL);
                body_options[0] = &none_label;
                for(size_t i = 0; i < selected->rigid_body_count &&
                        i < EDITOR_RIGID_BODY_MAX; i += 1) {
                    (void)editor_named_text_sync(&font, selected->rigid_bodies[i].name,
                        &rigid_body_labels[i], rigid_body_cache[i],
                        EDITOR_OBJECT_NAME_MAX);
                    body_options[i + 1] = &rigid_body_labels[i];
                    if(selected->rigid_bodies[i].id == sprite->rigid_body)
                        body_selected = i + 1;
                }
                rohr_ui_label(&rigid_body_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    80.0f, 90.0f, 28.0f});
                UIDropdownResult body_result = rohr_ui_dropdown(
                    "editor.animated_sprite.body", body_options,
                    selected->rigid_body_count + 1, body_selected,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 80.0f,
                        EDITOR_TOOLS_WIDTH - 110.0f, 28.0f}, NULL);
                rohr_ui_label(&x_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    118.0f, 90.0f, 28.0f});
                UIFieldResult position_x = rohr_ui_field(
                    "editor.animated_sprite.x",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_position.x}, &x_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 118.0f,
                        EDITOR_TOOLS_WIDTH - 110.0f, 28.0f}, NULL);
                rohr_ui_label(&y_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    156.0f, 90.0f, 28.0f});
                UIFieldResult position_y = rohr_ui_field(
                    "editor.animated_sprite.y",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT,
                        .number = &edited_position.y}, &y_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 156.0f,
                        EDITOR_TOOLS_WIDTH - 110.0f, 28.0f}, NULL);
                rohr_ui_label(&scale_x_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    194.0f, 90.0f, 28.0f});
                UIFieldResult sx = rohr_ui_field("editor.animated_sprite.scale_x",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &scale_x}, &x_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 194.0f,
                        EDITOR_TOOLS_WIDTH - 110.0f, 28.0f}, NULL);
                rohr_ui_label(&scale_y_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    232.0f, 90.0f, 28.0f});
                UIFieldResult sy = rohr_ui_field("editor.animated_sprite.scale_y",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &scale_y}, &y_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 232.0f,
                        EDITOR_TOOLS_WIDTH - 110.0f, 28.0f}, NULL);
                rohr_ui_label(&ticks_per_frame_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 270.0f, 110.0f, 28.0f});
                UIFieldResult tick_result = rohr_ui_field(
                    "editor.animated_sprite.ticks",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &ticks}, &length_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 120.0f, 270.0f,
                        EDITOR_TOOLS_WIDTH - 130.0f, 28.0f}, NULL);
                rohr_ui_label(&time_per_frame_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 308.0f, 110.0f, 28.0f});
                UIFieldResult time_result = rohr_ui_field(
                    "editor.animated_sprite.seconds",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &seconds}, &length_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 120.0f, 308.0f,
                        EDITOR_TOOLS_WIDTH - 130.0f, 28.0f}, NULL);
                rohr_ui_label(&starting_frame_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, 346.0f, 110.0f, 28.0f});
                UIFieldResult start_result = rohr_ui_field(
                    "editor.animated_sprite.start",
                    (UIFieldBinding){.kind = UI_FIELD_FLOAT, .number = &starting}, &length_field,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 120.0f, 346.0f,
                        EDITOR_TOOLS_WIDTH - 130.0f, 28.0f}, NULL);
                rohr_ui_label(&direction_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                    384.0f, 90.0f, 28.0f});
                UIDropdownResult direction_result = rohr_ui_dropdown(
                    "editor.animated_sprite.direction", direction_options, 2,
                    sprite->direction == DIRECTION_LEFT ? 0 : 1,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 100.0f, 384.0f,
                        EDITOR_TOOLS_WIDTH - 110.0f, 28.0f}, NULL);
                bool follow = sprite->follow_body_rotation;
                bool follow_changed = editor_checkbox("editor.animated_sprite.follow",
                    &follow_rotation_label, (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                        422.0f, EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, &follow);
                if(name_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ANIMATED_SPRITE_RENAME,
                        .data.animated_sprite_rename = {.object = selected->id,
                            .sprite = sprite->id}};
                    snprintf(command.data.animated_sprite_rename.name,
                        sizeof(command.data.animated_sprite_rename.name), "%s", edited_name);
                    (void)editor_command_execute(&project, &command);
                }
                if(body_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET,
                        .data.animated_sprite_body_set = {.object = selected->id,
                            .sprite = sprite->id, .body = body_result.selected_index == 0 ? 0 :
                                selected->rigid_bodies[body_result.selected_index - 1].id}};
                    (void)editor_command_execute(&project, &command);
                }
                if(position_x.changed || position_y.changed) {
                    if(attached_body != NULL) {
                        EditorCommand command = {
                            .type = EDITOR_COMMAND_RIGID_BODY_TRANSFORM,
                            .data.rigid_body_transform = {selected->id,
                                attached_body->id, edited_position,
                                attached_body->rotation}};
                        (void)editor_command_execute(&project, &command);
                    } else {
                        EditorCommand command = {
                            .type = EDITOR_COMMAND_ANIMATED_SPRITE_POSITION_SET,
                            .data.animated_sprite_position_set = {selected->id,
                                sprite->id, edited_position}};
                        (void)editor_command_execute(&project, &command);
                    }
                }
                if(sx.changed || sy.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ANIMATED_SPRITE_SCALE_SET,
                        .data.animated_sprite_scale_set = {.object = selected->id,
                            .sprite = sprite->id, .scale = {scale_x, scale_y}}};
                    (void)editor_command_execute(&project, &command);
                }
                if(tick_result.changed || time_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ANIMATED_SPRITE_TIMING_SET,
                        .data.animated_sprite_timing_set = {.object = selected->id,
                            .sprite = sprite->id, .ticks = ticks < 0.0f ? 0 : (Tick)ticks,
                            .time = seconds < 0.0f ? 0.0 : (Time)seconds}};
                    (void)editor_command_execute(&project, &command);
                }
                if(start_result.changed && starting >= 0.0f) {
                    EditorCommand command = {
                        .type = EDITOR_COMMAND_ANIMATED_SPRITE_STARTING_FRAME_SET,
                        .data.animated_sprite_starting_frame_set = {
                            .object = selected->id, .sprite = sprite->id,
                            .frame = (uint32_t)starting}};
                    (void)editor_command_execute(&project, &command);
                }
                if(direction_result.changed) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ANIMATED_SPRITE_DIRECTION_SET,
                        .data.animated_sprite_direction_set = {.object = selected->id,
                            .sprite = sprite->id, .direction =
                                direction_result.selected_index == 0 ? DIRECTION_LEFT :
                                    DIRECTION_RIGHT}};
                    (void)editor_command_execute(&project, &command);
                }
                if(follow_changed || visible_changed) {
                    EditorCommand command = {.type = follow_changed ?
                        EDITOR_COMMAND_ANIMATED_SPRITE_FOLLOW_ROTATION_SET :
                        EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET,
                        .data.animated_sprite_boolean_set = {.object = selected->id,
                            .sprite = sprite->id,
                            .enabled = follow_changed ? follow : visible}};
                    (void)editor_command_execute(&project, &command);
                }
                field_editing = name_result.active || position_x.active ||
                    position_y.active || sx.active || sy.active ||
                    tick_result.active || time_result.active || start_result.active;
                if(rohr_ui_button("editor.animated_sprite.add_frame", &add_frame_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 458.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 28.0f}, NULL).clicked) {
                    sprite_browser_object = selected->id;
                    animation_browser_sprite = sprite->id;
                    workspace_browser_action =
                        EDITOR_WORKSPACE_BROWSER_ADD_ANIMATION_FRAME;
                    if(!editor_file_browser_open(&file_browser,
                            EDITOR_FILE_BROWSER_OPEN_PNG, workspace.directory, &font)) {
                        sprite_browser_object = 0;
                        animation_browser_sprite = 0;
                        workspace_browser_action = EDITOR_WORKSPACE_BROWSER_NONE;
                    }
                }
                for(size_t frame = 0; frame < sprite->frame_count; frame += 1) {
                    EditorAnimationFrame *asset = &sprite->frames[frame];
                    char id[80];
                    size_t asset_index = frame < 64 ? frame : 63;
                    (void)editor_named_text_sync(&font, asset->name,
                        &sprite_labels[asset_index], sprite_cache[asset_index],
                        EDITOR_OBJECT_NAME_MAX);
                    snprintf(id, sizeof(id), "editor.animated_sprite.frame.%zu", frame);
                    if(rohr_ui_button(id, &sprite_labels[asset_index],
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                                494.0f + (float)frame * 30.0f,
                                EDITOR_TOOLS_WIDTH - 20.0f, 26.0f}, NULL).double_clicked) {
                        EditorCommand command = {.type = EDITOR_COMMAND_ANIMATION_FRAME_REMOVE,
                            .data.animation_frame_remove = {.object = selected->id,
                                .sprite = sprite->id, .index = frame}};
                        (void)editor_command_execute(&project, &command);
                        break;
                    }
                }
                if(rohr_ui_button("editor.animated_sprite.delete",
                        &delete_animated_sprite_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                            editor_panel_delete_y_get(&project, &viewport_state),
                            EDITOR_TOOLS_WIDTH - 20.0f, 34.0f}, &delete_style).clicked) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ANIMATED_SPRITE_REMOVE,
                        .data.animated_sprite_remove = {.object = selected->id,
                            .sprite = sprite->id}};
                    if(editor_command_execute(&project, &command).kind ==
                            ERROR_RESULT_VALUE) editor_viewport_back(&viewport_state);
                }
            }
        } else if(viewport_state.mode == EDITOR_VIEWPORT_OBJECT) {
            EditorObject *selected = editor_project_selected_get(&project);
            size_t selected_index = 0;
            if(selected != NULL) {
                selected_index = (size_t)(selected - project.objects);
                if(strcmp(object_name_cache[selected_index], selected->name) != 0) {
                    rohr_graphics_text_destroy(&object_name_labels[selected_index]);
                    if(!editor_text_create(&font, selected->name,
                            &object_name_labels[selected_index])) goto fail;
                    snprintf(object_name_cache[selected_index], EDITOR_OBJECT_NAME_MAX,
                        "%s", selected->name);
                }
                (void)editor_visibility_toggle("editor.object.visibility",
                    &visibility_visible_label, &visibility_hidden_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f,
                        56.0f, 26.0f, 26.0f}, &project, EDITOR_VISIBILITY_OBJECT,
                    selected->id, 0, 0, selected->visible);
                rohr_ui_label(&object_name_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f, 52.0f, 90.0f, 34.0f});
                {
                    char edited_name[EDITOR_OBJECT_NAME_MAX];
                    snprintf(edited_name, sizeof(edited_name), "%s", selected->name);
                    UIFieldResult name_result = rohr_ui_field("editor.object.name",
                    (UIFieldBinding){.kind = UI_FIELD_STRING,
                        .string = edited_name,
                        .string_capacity = sizeof(edited_name)},
                    &object_name_labels[selected_index],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 130.0f, 52.0f,
                        EDITOR_TOOLS_WIDTH - 140.0f, 34.0f}, NULL);
                    field_editing = name_result.active;
                    if(name_result.changed) {
                        EditorCommand command = {.type = EDITOR_COMMAND_ITEM_RENAME,
                            .data.item_rename = {.kind = EDITOR_ITEM_OBJECT,
                                .object = selected->id}};
                        EditorCommandResult command_result;
                        snprintf(command.data.item_rename.name,
                            sizeof(command.data.item_rename.name), "%s", edited_name);
                        command_result = editor_command_execute(&project, &command);
                        if(command_result.kind == ERROR_RESULT_VALUE)
                            snprintf(object_name_cache[selected_index],
                                EDITOR_OBJECT_NAME_MAX, "%s", selected->name);
                        (void)command_result;
                    }
                }
                if(rohr_ui_button("editor.add_rigid_body", &add_rigid_body_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 128.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 32.0f}, NULL).clicked) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
                        .data.item_add = {.kind = EDITOR_ITEM_RIGID_BODY,
                            .object = selected->id}};
                    EditorCommandResult body = editor_command_execute(&project, &command);
                    if(body.kind == ERROR_RESULT_VALUE) {
                        viewport_state.selection = EDITOR_SELECTION_RIGID_BODY;
                        viewport_state.selected_rigid_body = body.result.object;
                    }
                }
                if(rohr_ui_button("editor.add_joint", &add_joint_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 166.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 32.0f}, NULL).clicked) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
                        .data.item_add = {.kind = EDITOR_ITEM_JOINT,
                            .object = selected->id, .option = EDITOR_JOINT_SPRING}};
                    EditorCommandResult joint = editor_command_execute(&project, &command);
                    if(joint.kind == ERROR_RESULT_VALUE) {
                        viewport_state.selection = EDITOR_SELECTION_JOINT;
                        viewport_state.selected_joint = joint.result.object;
                    }
                }
                if(rohr_ui_button("editor.add_soft_body", &add_soft_body_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 204.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 32.0f}, NULL).clicked) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
                        .data.item_add = {.kind = EDITOR_ITEM_SOFT_BODY,
                            .object = selected->id}};
                    EditorCommandResult body = editor_command_execute(&project, &command);
                    if(body.kind == ERROR_RESULT_VALUE) {
                        viewport_state.selection = EDITOR_SELECTION_SOFT_BODY;
                        viewport_state.selected_soft_body = body.result.object;
                    }
                }
                if(rohr_ui_button("editor.add_sprite", &add_sprite_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 242.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 32.0f}, NULL).clicked) {
                    sprite_browser_object = selected->id;
                    workspace_browser_action = EDITOR_WORKSPACE_BROWSER_ADD_SPRITE;
                    if(!editor_file_browser_open(&file_browser,
                            EDITOR_FILE_BROWSER_OPEN_PNG, workspace.directory, &font)) {
                        sprite_browser_object = 0;
                        workspace_browser_action = EDITOR_WORKSPACE_BROWSER_NONE;
                    }
                }
                if(rohr_ui_button("editor.add_animated_sprite",
                        &add_animated_sprite_label,
                        (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 280.0f,
                            EDITOR_TOOLS_WIDTH - 20.0f, 32.0f}, NULL).clicked) {
                    EditorCommand command = {.type = EDITOR_COMMAND_ANIMATED_SPRITE_ADD,
                        .data.animated_sprite_add = {.object = selected->id}};
                    snprintf(command.data.animated_sprite_add.name,
                        sizeof(command.data.animated_sprite_add.name),
                        "animated_sprite_%u", project.next_animated_sprite_id);
                    EditorCommandResult added = editor_command_execute(&project, &command);
                    if(added.kind == ERROR_RESULT_VALUE) {
                        viewport_state.selection = EDITOR_SELECTION_ANIMATED_SPRITE;
                        viewport_state.selected_animated_sprite = added.result.object;
                    }
                }
                {
                    editor_project_object_hierarchy_sync(selected);
                    for(size_t hierarchy_index = 0;
                            hierarchy_index < selected->hierarchy_count;
                            hierarchy_index += 1) {
                        EditorHierarchyItem item = selected->hierarchy[hierarchy_index];
                        UIButtonStyle selected_style = editor_selected_button_style_get();
                        UIButtonResult result;
                        EditorHierarchySelection selection_kind;
                        EditorVisibilityKind visibility_kind;
                        TextAsset *label = NULL;
                        const char *name = NULL;
                        char *cache = NULL;
                        bool visible = false;
                        bool item_selected = false;
                        float y = 326.0f + (float)hierarchy_index * 30.0f;
                        char id[64];
                        char visibility_id[72];
                        if(item.kind == EDITOR_HIERARCHY_RIGID_BODY) {
                            for(size_t i = 0; i < selected->rigid_body_count; i += 1)
                                if(selected->rigid_bodies[i].id == item.id) {
                                    name = selected->rigid_bodies[i].name;
                                    visible = selected->rigid_bodies[i].visible;
                                    label = &rigid_body_labels[i];
                                    cache = rigid_body_cache[i];
                                }
                            selection_kind = EDITOR_SELECTION_RIGID_BODY;
                            visibility_kind = EDITOR_VISIBILITY_RIGID_BODY;
                            item_selected = viewport_state.selection == selection_kind &&
                                viewport_state.selected_rigid_body == item.id;
                            snprintf(id, sizeof(id), "editor.rigid_body.%u", item.id);
                            snprintf(visibility_id, sizeof(visibility_id),
                                "editor.rigid_body.%u.visibility", item.id);
                        } else if(item.kind == EDITOR_HIERARCHY_JOINT) {
                            for(size_t i = 0; i < selected->joint_count; i += 1)
                                if(selected->joint_items[i].id == item.id) {
                                    name = selected->joint_items[i].name;
                                    visible = selected->joint_items[i].visible;
                                    label = &joint_labels[i];
                                    cache = joint_cache[i];
                                }
                            selection_kind = EDITOR_SELECTION_JOINT;
                            visibility_kind = EDITOR_VISIBILITY_JOINT;
                            item_selected = viewport_state.selection == selection_kind &&
                                viewport_state.selected_joint == item.id;
                            snprintf(id, sizeof(id), "editor.joint.%u", item.id);
                            snprintf(visibility_id, sizeof(visibility_id),
                                "editor.joint.%u.visibility", item.id);
                        } else if(item.kind == EDITOR_HIERARCHY_SOFT_BODY) {
                            for(size_t i = 0; i < selected->soft_body_count; i += 1)
                                if(selected->soft_body_items[i].id == item.id) {
                                    name = selected->soft_body_items[i].name;
                                    visible = selected->soft_body_items[i].visible;
                                    label = &soft_body_labels[i];
                                    cache = soft_body_cache[i];
                                }
                            selection_kind = EDITOR_SELECTION_SOFT_BODY;
                            visibility_kind = EDITOR_VISIBILITY_SOFT_BODY;
                            item_selected = viewport_state.selection == selection_kind &&
                                viewport_state.selected_soft_body == item.id;
                            snprintf(id, sizeof(id), "editor.soft_body.%u", item.id);
                            snprintf(visibility_id, sizeof(visibility_id),
                                "editor.soft_body.%u.visibility", item.id);
                        } else if(item.kind == EDITOR_HIERARCHY_SPRITE) {
                            for(size_t i = 0; i < selected->sprite_count && i < 64;
                                    i += 1)
                                if(selected->sprites[i].id == item.id) {
                                    name = selected->sprites[i].name;
                                    visible = selected->sprites[i].visible;
                                    label = &sprite_labels[i];
                                    cache = sprite_cache[i];
                                }
                            selection_kind = EDITOR_SELECTION_SPRITE;
                            visibility_kind = EDITOR_VISIBILITY_OBJECT;
                            item_selected = viewport_state.selection == selection_kind &&
                                viewport_state.selected_sprite == item.id;
                            snprintf(id, sizeof(id), "editor.sprite.%u", item.id);
                            snprintf(visibility_id, sizeof(visibility_id),
                                "editor.sprite.%u.visibility", item.id);
                        } else {
                            for(size_t i = 0; i < selected->animated_sprite_count &&
                                    i < 32; i += 1)
                                if(selected->animated_sprite_items[i].id == item.id) {
                                    name = selected->animated_sprite_items[i].name;
                                    visible = selected->animated_sprite_items[i].visible;
                                    label = &animated_sprite_labels[i];
                                    cache = animated_sprite_cache[i];
                                }
                            selection_kind = EDITOR_SELECTION_ANIMATED_SPRITE;
                            visibility_kind = EDITOR_VISIBILITY_OBJECT;
                            item_selected = viewport_state.selection == selection_kind &&
                                viewport_state.selected_animated_sprite == item.id;
                            snprintf(id, sizeof(id), "editor.animated_sprite.%u", item.id);
                            snprintf(visibility_id, sizeof(visibility_id),
                                "editor.animated_sprite.%u.visibility", item.id);
                        }
                        if(name == NULL || label == NULL || cache == NULL) continue;
                        if(!editor_named_text_sync(&font, name, label, cache,
                                EDITOR_OBJECT_NAME_MAX)) goto fail;
                        if(selection_kind == EDITOR_SELECTION_SPRITE) {
                            bool changed = editor_checkbox(visibility_id,
                                &terminal_visible_label,
                                (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, y,
                                    26.0f, 26.0f}, &visible);
                            if(changed) {
                                EditorCommand command = {
                                    .type = EDITOR_COMMAND_SPRITE_VISIBILITY_SET,
                                    .data.sprite_visibility_set = {
                                        .object = selected->id, .sprite = item.id,
                                        .visible = visible}};
                                (void)editor_command_execute(&project, &command);
                            }
                        } else if(selection_kind == EDITOR_SELECTION_ANIMATED_SPRITE) {
                            bool changed = rohr_ui_button(visibility_id,
                                visible ? &visibility_visible_label :
                                    &visibility_hidden_label,
                                (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, y,
                                    26.0f, 26.0f}, NULL).clicked;
                            if(changed) {
                                EditorCommand command = {
                                    .type = EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET,
                                    .data.animated_sprite_boolean_set = {
                                        .object = selected->id, .sprite = item.id,
                                        .enabled = !visible}};
                                (void)editor_command_execute(&project, &command);
                            }
                        } else
                            (void)editor_visibility_toggle(visibility_id,
                            &visibility_visible_label, &visibility_hidden_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, y, 26.0f, 26.0f},
                            &project, visibility_kind, selected->id, 0, item.id, visible);
                        result = rohr_ui_button(id, label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 42.0f, y,
                                EDITOR_TOOLS_WIDTH - 50.0f, 26.0f},
                            item_selected ||
                                editor_selection_path_check(&viewport_state,
                                    selection_kind, selected->id, 0, 0, item.id) ?
                                &selected_style : NULL);
                        editor_hierarchy_drag_row(&hierarchy_drag, &viewport_state,
                            (EditorSelectionRef){selection_kind,
                                selected->id, 0, 0, item.id},
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 42.0f, y,
                                EDITOR_TOOLS_WIDTH - 50.0f, 26.0f}, result,
                            hierarchy_pointer, hierarchy_primary, panel_scroll_offset,
                            hierarchy_index + 1 == selected->hierarchy_count);
                        if(result.clicked || result.focus_changed) {
                            viewport_state.selection = selection_kind;
                            if(selection_kind == EDITOR_SELECTION_RIGID_BODY)
                                viewport_state.selected_rigid_body = item.id;
                            else if(selection_kind == EDITOR_SELECTION_JOINT)
                                viewport_state.selected_joint = item.id;
                            else if(selection_kind == EDITOR_SELECTION_SOFT_BODY)
                                viewport_state.selected_soft_body = item.id;
                            else if(selection_kind == EDITOR_SELECTION_SPRITE)
                                viewport_state.selected_sprite = item.id;
                            else viewport_state.selected_animated_sprite = item.id;
                            if(result.double_clicked) (void)editor_navigation_selected_open(
                                &project, &viewport_state);
                        }
                    }
                }
                {
                    UIButtonStyle delete_style = editor_delete_button_style_get();
                    if(rohr_ui_button("editor.object.delete", &delete_object_label,
                            (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f,
                                editor_panel_delete_y_get(&project, &viewport_state),
                                EDITOR_TOOLS_WIDTH - 20.0f, 34.0f},
                            &delete_style).clicked) {
                        (void)editor_open_item_delete(&project, &viewport_state);
                    }
                }
            }
        } else {
            UIButtonResult add_object = rohr_ui_button(
                "editor.add_object", &add_object_label,
                (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 42.0f,
                    EDITOR_TOOLS_WIDTH - 20.0f, 38.0f}, NULL);
            if(add_object.clicked) {
                EditorCommand command = {.type = EDITOR_COMMAND_ITEM_ADD,
                    .data.item_add = {.kind = EDITOR_ITEM_OBJECT}};
                snprintf(command.data.item_add.name,
                    sizeof(command.data.item_add.name), "Object%u", project.next_id);
                EditorCommandResult added = editor_command_execute(&project, &command);
                if(added.kind == ERROR_RESULT_VALUE) {
                    EditorObject *object = editor_object_query_get(
                        &project, added.result.object);
                    viewport_state.selection = EDITOR_SELECTION_OBJECT;
                    if(object != NULL) {
                        snprintf(command.data.item_add.name,
                            sizeof(command.data.item_add.name), "%s", object->name);
                    }
                }
            }
            (void)rohr_graphics_screen_rect_draw(
                EDITOR_VIEWPORT_WIDTH + 10.0f, 132.0f,
                EDITOR_TOOLS_WIDTH - 20.0f, 1.0f,
                (Color){75, 84, 100, 255});
            for(size_t i = 0; i < project.object_count; i += 1) {
                EditorObject *object = &project.objects[i];
                float y = 144.0f + (float)i * 34.0f;
                char object_button_id[64];
                char visibility_id[72];
                UIButtonResult object_result;

                if(strcmp(object_name_cache[i], object->name) != 0) {
                    rohr_graphics_text_destroy(&object_name_labels[i]);
                    if(!editor_text_create(&font, object->name,
                            &object_name_labels[i])) goto fail;
                    snprintf(object_name_cache[i], EDITOR_OBJECT_NAME_MAX,
                        "%s", object->name);
                }

                snprintf(object_button_id, sizeof(object_button_id),
                    "editor.object.%u", object->id);
                snprintf(visibility_id, sizeof(visibility_id),
                    "editor.object.%u.visibility", object->id);
                (void)editor_visibility_toggle(visibility_id,
                    &visibility_visible_label, &visibility_hidden_label,
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 8.0f, y + 1.0f, 26.0f, 26.0f},
                    &project, EDITOR_VISIBILITY_OBJECT, object->id, 0, 0,
                    object->visible);
                {
                    UIButtonStyle selected_style = editor_selected_button_style_get();
                    const UIButtonStyle *style = (viewport_state.selection ==
                            EDITOR_SELECTION_OBJECT && project.selected == object->id) ||
                            editor_selection_path_check(&viewport_state,
                                EDITOR_SELECTION_OBJECT, object->id,
                                0, 0, object->id) ?
                        &selected_style : NULL;
                object_result = rohr_ui_button(
                    object_button_id, &object_name_labels[i],
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f, y,
                        EDITOR_TOOLS_WIDTH - 48.0f, 28.0f}, style);
                editor_hierarchy_drag_row(&hierarchy_drag, &viewport_state,
                    (EditorSelectionRef){EDITOR_SELECTION_OBJECT,
                        object->id, 0, 0, object->id},
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 40.0f, y,
                        EDITOR_TOOLS_WIDTH - 48.0f, 28.0f}, object_result,
                    hierarchy_pointer, hierarchy_primary, panel_scroll_offset,
                    i + 1 == project.object_count);
                }
                if(object_result.clicked || object_result.focus_changed) {
                    (void)editor_project_object_select(&project, object->id);
                    viewport_state.selection = EDITOR_SELECTION_OBJECT;
                    if(object_result.double_clicked) {
                        (void)editor_navigation_selected_open(&project, &viewport_state);
                    }
                }
            }
        }
        if(editor_hierarchy_drag_update(&hierarchy_drag, &project,
                &viewport_state, &history, hierarchy_primary))
            pointer_selection_handled = true;
        rohr_ui_scroll_region_end();
        rohr_graphics_screen_clip_clear();
        (void)rohr_graphics_screen_clip_set(
            0.0f, EDITOR_MENU_HEIGHT, EDITOR_VIEWPORT_WIDTH,
            EDITOR_VIEWPORT_BOTTOM - EDITOR_MENU_HEIGHT);
        editor_viewport_asset_root_set(workspace.open ? workspace.directory : NULL);
        editor_viewport_draw(&project, &viewport_state,
            visual_settings_panel.state.grid_visible);
        rohr_graphics_screen_clip_clear();
        (void)rohr_graphics_screen_clip_set(0.0f, EDITOR_VIEWPORT_BOTTOM,
            EDITOR_VIEWPORT_WIDTH, EDITOR_WINDOW_HEIGHT - EDITOR_VIEWPORT_BOTTOM);
        editor_terminal_panel_draw(&terminal_panel, EDITOR_VIEWPORT_WIDTH,
            EDITOR_VIEWPORT_BOTTOM);
        rohr_graphics_screen_clip_clear();
        (void)rohr_graphics_screen_rect_draw(0.0f, EDITOR_ACTION_BAR_TOP,
            EDITOR_VIEWPORT_WIDTH, EDITOR_ACTION_BAR_HEIGHT,
            (Color){18, 21, 27, 255});
        rohr_graphics_layer_set(EDITOR_GRAPHICS_LAYER_OVERLAY);
        if(color_picker.open) {
            (void)editor_color_picker_draw(&color_picker, &mouse,
                &color_picker_hex_field, &color_picker_opacity_field,
                &opacity_label, &field_editing);
        }
        {
            Position pointer = rohr_graphics_mouse_screen_position_get();
            UIRect context_bounds;
            if(mouse.button_states[MOUSE_BUTTON_RIGHT] == MOUSE_BUTTON_STATE_PRESSED &&
                    pointer.x >= 0.0f && pointer.x < EDITOR_VIEWPORT_WIDTH &&
                    pointer.y >= EDITOR_MENU_HEIGHT &&
                    pointer.y < EDITOR_VIEWPORT_BOTTOM) {
                viewport_context_open = true;
                viewport_context_position = (Position){
                    fminf(pointer.x, EDITOR_VIEWPORT_WIDTH - 150.0f),
                    fminf(pointer.y, EDITOR_WINDOW_HEIGHT - 104.0f)
                };
            }
            context_bounds = (UIRect){viewport_context_position.x,
                viewport_context_position.y, 144.0f, 100.0f};
            if(viewport_context_open) {
                bool close_context = false;
                rohr_ui_surface(context_bounds, (Color){24, 27, 34, 255});
                rohr_ui_border(context_bounds, 2.0f, (Color){0, 0, 0, 255});
                close_context = rohr_ui_button("editor.viewport.context.action_1",
                    &context_action_one_label,
                    (UIRect){context_bounds.x + 4.0f, context_bounds.y + 4.0f,
                        context_bounds.width - 8.0f, 28.0f}, NULL).clicked;
                close_context = rohr_ui_button("editor.viewport.context.action_2",
                    &context_action_two_label,
                    (UIRect){context_bounds.x + 4.0f, context_bounds.y + 36.0f,
                        context_bounds.width - 8.0f, 28.0f}, NULL).clicked || close_context;
                close_context = rohr_ui_button("editor.viewport.context.action_3",
                    &context_action_three_label,
                    (UIRect){context_bounds.x + 4.0f, context_bounds.y + 68.0f,
                        context_bounds.width - 8.0f, 28.0f}, NULL).clicked || close_context;
                if(mouse.button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_PRESSED &&
                        (pointer.x < context_bounds.x ||
                        pointer.x > context_bounds.x + context_bounds.width ||
                        pointer.y < context_bounds.y ||
                        pointer.y > context_bounds.y + context_bounds.height)) {
                    close_context = true;
                }
                if(close_context) viewport_context_open = false;
            }
        }
        if(viewport_state.mode != EDITOR_VIEWPORT_HIERARCHY &&
                rohr_ui_button("editor.viewport.coordinates",
                    project.viewport_local_view ? &local_view_label : &world_view_label,
                    (UIRect){10.0f, EDITOR_MENU_HEIGHT + 10.0f, 84.0f, 30.0f},
                    NULL).clicked) {
            EditorCommand command = {.type = EDITOR_COMMAND_VIEWPORT_COORDINATES,
                .data.viewport_coordinates.local = !project.viewport_local_view};
            (void)editor_command_execute(&project, &command);
        }
        rohr_graphics_layer_set(EDITOR_GRAPHICS_LAYER_TOP_MENU);
        rohr_ui_surface((UIRect){0.0f, 0.0f, editor_window_width,
            EDITOR_MENU_HEIGHT}, (Color){32, 36, 45, 255});
        {
            UIDropdownResult file_menu;
            const TextAsset *file_options[] = {
                &new_label, &open_label, &save_label, &close_label, &exit_label
            };
            const TextAsset *view_options[] = {
                &reset_view_label, &grid_label, &terminal_label
            };
            const TextAsset *terminal_options[] = {
                &terminal_visible_label, &terminal_editor_operations_label,
                &terminal_generated_code_label, &terminal_build_operations_label
            };
            const TextAsset *build_options[] = {
                &generate_c_label, &compile_label, &build_project_label
            };
            const TextAsset *edit_options[] = {&undo_label, &redo_label};
            const TextAsset *settings_options[] = {
                &preferences_label, &visual_settings_panel.menu_label
            };
            const TextAsset *file_texts[] = {
                &file_label, &new_label, &open_label, &save_label, &close_label,
                &exit_label
            };
            const TextAsset *build_texts[] = {
                &build_label, &generate_c_label, &compile_label,
                &build_project_label};
            const TextAsset *edit_texts[] = {&edit_label, &undo_label, &redo_label};
            const TextAsset *view_texts[] = {
                &view_label, &reset_view_label, &grid_label, &terminal_label
            };
            const TextAsset *terminal_texts[] = {
                &terminal_menu_label, &terminal_visible_label,
                &terminal_editor_operations_label, &terminal_generated_code_label,
                &terminal_build_operations_label
            };
            const TextAsset *settings_texts[] = {&settings_label, &preferences_label,
                &visual_settings_panel.menu_label};
            UIComponentConfig menu_components = {
                .components = UI_COMPONENT_SIZE_TO_TEXT,
                .text_padding_x = 12.0f,
                .text_padding_y = 7.0f
            };
            float menu_x = 4.0f;
            UIRect file_bounds = rohr_ui_component_bounds_get(
                (UIRect){menu_x, 3.0f, 0.0f, 0.0f}, file_texts,
                sizeof(file_texts) / sizeof(file_texts[0]), menu_components);
            UIRect build_bounds;
            UIRect edit_bounds;
            UIRect view_bounds;
            UIRect settings_bounds;
            UIRect terminal_bounds;

            (void)rohr_graphics_text_value_set(&terminal_label,
                terminal_panel.visible ? "[x] Terminal" : "[ ] Terminal");
            (void)rohr_graphics_text_value_set(&grid_label,
                visual_settings_panel.state.grid_visible ?
                    "[x] Grid" : "[ ] Grid");
            (void)rohr_graphics_text_value_set(&terminal_visible_label,
                terminal_panel.visible ? "[x] Visible" : "[ ] Visible");
            (void)rohr_graphics_text_value_set(&terminal_editor_operations_label,
                terminal_editor_operations ? "[x] Show editor operations" :
                    "[ ] Show editor operations");
            (void)rohr_graphics_text_value_set(&terminal_generated_code_label,
                terminal_generated_code ? "[x] Show generated code" :
                    "[ ] Show generated code");
            (void)rohr_graphics_text_value_set(&terminal_build_operations_label,
                terminal_build_operations ? "[x] Show build operations" :
                    "[ ] Show build operations");

            menu_x += file_bounds.width + 4.0f;
            edit_bounds = rohr_ui_component_bounds_get(
                (UIRect){menu_x, 3.0f, 0.0f, 0.0f}, edit_texts,
                sizeof(edit_texts) / sizeof(edit_texts[0]), menu_components);
            menu_x += edit_bounds.width + 4.0f;
            build_bounds = rohr_ui_component_bounds_get(
                (UIRect){menu_x, 3.0f, 0.0f, 0.0f}, build_texts,
                sizeof(build_texts) / sizeof(build_texts[0]), menu_components);
            menu_x += build_bounds.width + 4.0f;
            view_bounds = rohr_ui_component_bounds_get(
                (UIRect){menu_x, 3.0f, 0.0f, 0.0f}, view_texts,
                sizeof(view_texts) / sizeof(view_texts[0]), menu_components);
            menu_x += view_bounds.width + 4.0f;
            terminal_bounds = rohr_ui_component_bounds_get(
                (UIRect){menu_x, 3.0f, 0.0f, 0.0f}, terminal_texts,
                sizeof(terminal_texts) / sizeof(terminal_texts[0]), menu_components);
            menu_x += terminal_bounds.width + 4.0f;
            settings_bounds = rohr_ui_component_bounds_get(
                (UIRect){menu_x, 3.0f, 0.0f, 0.0f}, settings_texts,
                sizeof(settings_texts) / sizeof(settings_texts[0]), menu_components);

            if(mouse.button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_PRESSED) {
                Position pointer = rohr_graphics_mouse_screen_position_get();
                bool top_menu_pressed = editor_point_in_rect(pointer, file_bounds) ||
                    editor_point_in_rect(pointer, edit_bounds) ||
                    editor_point_in_rect(pointer, build_bounds) ||
                    editor_point_in_rect(pointer, view_bounds) ||
                    editor_point_in_rect(pointer, terminal_bounds) ||
                    editor_point_in_rect(pointer, settings_bounds);
                if(top_menu_pressed) {
                    editor_file_browser_destroy(&file_browser);
                    workspace_browser_action = EDITOR_WORKSPACE_BROWSER_NONE;
                    sprite_browser_object = 0;
                    animation_browser_sprite = 0;
                    build_settings_panel.open = false;
                    build_settings_panel.build_requested = false;
                    visual_settings_panel.open = false;
                    notification_panel.log_open = false;
                    notification_panel.report_open = false;
                    close_action = EDITOR_CLOSE_NONE;
                    viewport_context_open = false;
                    auto_shape_picker_open = false;
                    collision_category_open = false;
                    collide_with_open = false;
                    editor_color_picker_cancel(&color_picker);
                    rohr_ui_field_focus_clear();
                }
            }

            rohr_ui_modal_controls_begin();
            file_menu = rohr_ui_menu("editor.menu.file", &file_label, file_options,
                sizeof(file_options) / sizeof(file_options[0]),
                file_bounds, NULL);
            if(file_menu.changed && file_menu.selected_index == 0) {
                workspace_browser_action = EDITOR_WORKSPACE_BROWSER_NEW;
                (void)editor_file_browser_open(&file_browser,
                    EDITOR_FILE_BROWSER_CREATE_DIRECTORY, startup_directory, &font);
            } else if(file_menu.changed && file_menu.selected_index == 1) {
                workspace_browser_action = EDITOR_WORKSPACE_BROWSER_LOAD;
                (void)editor_file_browser_open(&file_browser,
                    EDITOR_FILE_BROWSER_DIRECTORY, startup_directory, &font);
            } else if(file_menu.changed && file_menu.selected_index == 2) {
                EditorWorkspaceCommand command = {
                    .type = EDITOR_WORKSPACE_COMMAND_SAVE};
                snprintf(command.directory, sizeof(command.directory), "%s",
                    workspace.directory);
                if(workspace.open && !editor_result_check(
                        editor_workspace_operation_execute(&workspace, &project,
                            &command))) {
                    saved_project_hash = editor_project_hash_get(&project);
                }
            } else if(file_menu.changed && file_menu.selected_index == 3) {
                if(editor_project_hash_get(&project) == saved_project_hash) {
                    editor_workspace_close(&workspace, &project);
                    (void)editor_terminal_panel_project_open(
                        &terminal_panel, startup_directory);
                    editor_history_reset(&history);
                    saved_project_hash = editor_project_hash_get(&project);
                    editor_viewport_state_init(&viewport_state);
                    panel_scroll_offset = 0.0f;
                } else {
                    close_action = EDITOR_CLOSE_PROJECT;
                }
            } else if(file_menu.changed && file_menu.selected_index == 4) {
                if(!workspace.open ||
                        editor_project_hash_get(&project) == saved_project_hash) {
                    running = false;
                } else {
                    close_action = EDITOR_CLOSE_PROGRAM;
                }
            }
            {
                UIDropdownResult edit_menu = rohr_ui_menu("editor.menu.edit",
                    &edit_label, edit_options,
                    sizeof(edit_options) / sizeof(edit_options[0]),
                    edit_bounds, NULL);
                bool restored = false;
                if(edit_menu.changed && edit_menu.selected_index == 0)
                    restored = editor_history_undo(&history);
                else if(edit_menu.changed && edit_menu.selected_index == 1)
                    restored = editor_history_redo(&history);
                if(restored) editor_navigation_state_apply(
                    &project, &viewport_state, &project.navigation);
            }
            {
                UIDropdownResult build_menu = rohr_ui_menu("editor.menu.build",
                    &build_label, build_options,
                    sizeof(build_options) / sizeof(build_options[0]),
                    build_bounds, NULL);
                if(build_menu.changed && build_menu.selected_index == 0 &&
                        workspace.open) {
                    EditorWorkspaceCommand command = {
                        .type = EDITOR_WORKSPACE_COMMAND_GENERATE_C};
                    snprintf(command.directory, sizeof(command.directory), "%s",
                        workspace.directory);
                    if(!editor_result_check(editor_workspace_operation_execute(
                            &workspace, &project, &command))) {
                        bool tree_shown = terminal_generated_code &&
                            editor_generation_report_write(&terminal_panel, &project);
                        editor_build_notification_codegen_success(
                            &notification_panel, &project, tree_shown);
                    }
                } else if(build_menu.changed && build_menu.selected_index == 1 &&
                        workspace.open) {
                    if(!editor_cmake_compile_start(&terminal_panel,
                        &hidden_build_process, &hidden_compile_pending,
                        hidden_build_directory, sizeof(hidden_build_directory),
                        workspace.directory,
                        terminal_build_operations))
                        editor_build_notification_start_failure(
                            &notification_panel, "Compile project",
                            "The configure command could not be started.");
                } else if(build_menu.changed && build_menu.selected_index == 2 &&
                        workspace.open) {
                    EditorWorkspaceCommand command = {
                        .type = EDITOR_WORKSPACE_COMMAND_GENERATE_C};
                    snprintf(command.directory, sizeof(command.directory), "%s",
                        workspace.directory);
                    if(!editor_result_check(editor_workspace_operation_execute(
                            &workspace, &project, &command))) {
                        bool tree_shown = terminal_generated_code &&
                            editor_generation_report_write(&terminal_panel, &project);
                        editor_build_notification_codegen_success(
                            &notification_panel, &project, tree_shown);
                        if(!editor_cmake_compile_start(&terminal_panel,
                            &hidden_build_process, &hidden_compile_pending,
                            hidden_build_directory, sizeof(hidden_build_directory),
                            workspace.directory,
                            terminal_build_operations))
                            editor_build_notification_start_failure(
                                &notification_panel, "Build project",
                                "The configure command could not be started.");
                    }
                }
            }
            {
                UIDropdownResult view_menu = rohr_ui_menu(
                    "editor.menu.view", &view_label, view_options,
                sizeof(view_options) / sizeof(view_options[0]),
                view_bounds, NULL);
                if(view_menu.changed && view_menu.selected_index == 0) {
                    EditorCommand command = {
                        .type = EDITOR_COMMAND_VIEWPORT_CAMERA,
                        .data.viewport_camera = {{0.0f, 0.0f}, 1.0f}
                    };
                    (void)editor_command_execute(&project, &command);
                    viewport_state.camera_panning = false;
                    viewport_state.camera_pan_with_primary = false;
                } else if(view_menu.changed && view_menu.selected_index == 1) {
                    EditorResult result =
                        editor_visual_settings_panel_grid_visible_set(
                            &visual_settings_panel,
                            !visual_settings_panel.state.grid_visible);
                    if(editor_result_check(result)) editor_result_stderr_print(result);
                } else if(view_menu.changed && view_menu.selected_index == 2)
                    editor_terminal_panel_visible_toggle(&terminal_panel);
            }
            {
                UIDropdownResult terminal_menu = rohr_ui_menu(
                    "editor.menu.terminal", &terminal_menu_label, terminal_options,
                    sizeof(terminal_options) / sizeof(terminal_options[0]),
                    terminal_bounds, NULL);
                if(terminal_menu.changed && terminal_menu.selected_index == 0)
                    editor_terminal_panel_visible_toggle(&terminal_panel);
                else if(terminal_menu.changed && terminal_menu.selected_index == 1)
                    terminal_editor_operations = !terminal_editor_operations;
                else if(terminal_menu.changed && terminal_menu.selected_index == 2)
                    terminal_generated_code = !terminal_generated_code;
                else if(terminal_menu.changed && terminal_menu.selected_index == 3)
                    terminal_build_operations = !terminal_build_operations;
            }
            UIDropdownResult settings_menu = rohr_ui_menu("editor.menu.settings",
                &settings_label,
                settings_options, sizeof(settings_options) / sizeof(settings_options[0]),
                settings_bounds, NULL);
            if(settings_menu.changed && settings_menu.selected_index == 0 &&
                    workspace.open) {
                EditorResult result = editor_build_settings_panel_open(
                    &build_settings_panel, workspace.directory);
                if(editor_result_check(result)) editor_result_stderr_print(result);
            } else if(settings_menu.changed && settings_menu.selected_index == 1) {
                editor_visual_settings_panel_open(&visual_settings_panel);
            }
            rohr_ui_modal_controls_end();
        }
        rohr_graphics_layer_set(EDITOR_GRAPHICS_LAYER_MODAL);
        if(!notification_panel.report_open && !notification_panel.log_open &&
                !visual_settings_panel.open)
            editor_build_settings_panel_draw(&build_settings_panel,
                &notification_panel, workspace.directory, build_settings_bounds);
        if(!notification_panel.report_open && !notification_panel.log_open &&
                !build_settings_panel.open)
            editor_visual_settings_panel_draw(&visual_settings_panel,
                build_settings_bounds);
        if(build_settings_panel.build_requested) {
            build_settings_panel.build_requested = false;
            if(!editor_cmake_compile_start(&terminal_panel,
                    &hidden_build_process, &hidden_compile_pending,
                    hidden_build_directory, sizeof(hidden_build_directory),
                    workspace.directory, terminal_build_operations)) {
                editor_build_notification_start_failure(&notification_panel,
                    "Build configuration (GUI)",
                    "Parser error:\nThe build command could not be started.\n\n"
                    "Lua error:\nNo Lua runtime error.");
            }
        }
        rohr_graphics_layer_set(EDITOR_GRAPHICS_LAYER_NOTIFICATION);
        editor_notification_panel_toast_draw(&notification_panel,
            EDITOR_WINDOW_HEIGHT);
        rohr_graphics_layer_set(EDITOR_GRAPHICS_LAYER_MODAL);
        editor_notification_panel_log_draw(&notification_panel,
            build_settings_bounds);
        editor_notification_panel_report_draw(&notification_panel,
            build_settings_bounds);
        if(close_action != EDITOR_CLOSE_NONE) {
            UIRect dialog = {
                editor_window_width * 0.5f - 220.0f,
                EDITOR_MENU_HEIGHT +
                    (EDITOR_VIEWPORT_BOTTOM - EDITOR_MENU_HEIGHT) * 0.5f - 80.0f,
                440.0f,
                160.0f
            };
            rohr_ui_surface(dialog, (Color){42, 47, 58, 255});
            rohr_ui_border(dialog, 2.0f, (Color){8, 9, 12, 255});
            rohr_ui_label(&unsaved_changes_label, (UIRect){dialog.x + 20.0f,
                dialog.y + 20.0f, dialog.width - 40.0f, 42.0f});
            if(rohr_ui_button("editor.close.save", &save_label,
                    (UIRect){dialog.x + 18.0f, dialog.y + 102.0f,
                        120.0f, 36.0f}, NULL).clicked) {
                EditorWorkspaceCommand command = {
                    .type = EDITOR_WORKSPACE_COMMAND_SAVE};
                snprintf(command.directory, sizeof(command.directory), "%s",
                    workspace.directory);
                if(!editor_result_check(editor_workspace_operation_execute(
                        &workspace, &project, &command))) {
                    if(close_action == EDITOR_CLOSE_PROGRAM) {
                        running = false;
                    } else {
                        editor_workspace_close(&workspace, &project);
                        (void)editor_terminal_panel_project_open(
                            &terminal_panel, startup_directory);
                        editor_history_reset(&history);
                        saved_project_hash = editor_project_hash_get(&project);
                        editor_viewport_state_init(&viewport_state);
                        panel_scroll_offset = 0.0f;
                    }
                    close_action = EDITOR_CLOSE_NONE;
                }
            }
            if(rohr_ui_button("editor.close.dont_save", &dont_save_label,
                    (UIRect){dialog.x + 148.0f, dialog.y + 102.0f,
                        140.0f, 36.0f}, NULL).clicked) {
                if(close_action == EDITOR_CLOSE_PROGRAM) {
                    running = false;
                } else {
                    editor_workspace_close(&workspace, &project);
                    (void)editor_terminal_panel_project_open(
                        &terminal_panel, startup_directory);
                    editor_history_reset(&history);
                    saved_project_hash = editor_project_hash_get(&project);
                    editor_viewport_state_init(&viewport_state);
                    panel_scroll_offset = 0.0f;
                }
                close_action = EDITOR_CLOSE_NONE;
            }
            if(rohr_ui_button("editor.close.cancel", &cancel_label,
                    (UIRect){dialog.x + 298.0f, dialog.y + 102.0f,
                        124.0f, 36.0f}, NULL).clicked) {
                close_action = EDITOR_CLOSE_NONE;
            }
        }
        rohr_graphics_layer_set(EDITOR_GRAPHICS_LAYER_OVERLAY);
        if(!workspace.open && !file_browser.active) {
            UIRect dialog = {editor_window_width * 0.5f - 230.0f,
                EDITOR_MENU_HEIGHT +
                    (EDITOR_VIEWPORT_BOTTOM - EDITOR_MENU_HEIGHT) * 0.5f - 90.0f,
                460.0f, 180.0f};
            rohr_ui_surface((UIRect){0.0f, EDITOR_MENU_HEIGHT, editor_window_width,
                EDITOR_VIEWPORT_BOTTOM - EDITOR_MENU_HEIGHT},
                (Color){12, 14, 18, 238});
            rohr_ui_surface(dialog, (Color){42, 47, 58, 255});
            rohr_ui_border(dialog, 2.0f, (Color){8, 9, 12, 255});
            if(rohr_ui_button("editor.start.new", &new_label,
                    (UIRect){dialog.x + 30.0f, dialog.y + 58.0f,
                        190.0f, 58.0f}, NULL).clicked) {
                workspace_browser_action = EDITOR_WORKSPACE_BROWSER_NEW;
                (void)editor_file_browser_open(&file_browser,
                    EDITOR_FILE_BROWSER_CREATE_DIRECTORY, startup_directory, &font);
            }
            if(rohr_ui_button("editor.start.load", &open_label,
                    (UIRect){dialog.x + 240.0f, dialog.y + 58.0f,
                        190.0f, 58.0f}, NULL).clicked) {
                workspace_browser_action = EDITOR_WORKSPACE_BROWSER_LOAD;
                (void)editor_file_browser_open(&file_browser,
                    EDITOR_FILE_BROWSER_DIRECTORY, startup_directory, &font);
            }
        }
        rohr_graphics_layer_set(EDITOR_GRAPHICS_LAYER_MODAL);
        if(file_browser.active) {
            EditorFileBrowserResult browser_result = editor_file_browser_draw(
                &file_browser, &file_browser_field,
                &save_label,
                workspace_browser_action ==
                        EDITOR_WORKSPACE_BROWSER_ADD_ANIMATION_FRAME ?
                    &load_frame_label : &open_label,
                &create_project_label, &cancel_label,
                editor_window_width, EDITOR_VIEWPORT_BOTTOM);
            if(browser_result.submitted) {
                EditorResult load_result = editor_result_value(true);
                bool opened;
                EditorWorkspaceCommand command = {0};
                if(workspace_browser_action ==
                        EDITOR_WORKSPACE_BROWSER_ADD_ANIMATION_FRAME) {
                    const char *sprite_path = editor_project_relative_path_get(
                        workspace.directory, browser_result.path);
                    EditorCommand add_command = {.type = EDITOR_COMMAND_ANIMATION_FRAME_ADD,
                        .data.animation_frame_add = {.object = sprite_browser_object,
                            .sprite = animation_browser_sprite,
                            .size = {64.0f, 64.0f}}};
                    EditorCommandResult added;

                    opened = sprite_browser_object != EDITOR_OBJECT_INVALID &&
                        animation_browser_sprite != 0;
                    if(opened && strlen(sprite_path) >=
                            sizeof(add_command.data.animation_frame_add.path)) {
                        load_result = editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                            "Animation sprite path is too long: %s", browser_result.path);
                        opened = false;
                    } else if(opened) {
                        EditorObject *frame_object = editor_project_selected_get(&project);
                        EditorAnimatedSprite *animation = frame_object == NULL ||
                            frame_object->id != sprite_browser_object ? NULL :
                            editor_project_animated_sprite_get(frame_object,
                                animation_browser_sprite);
                        snprintf(add_command.data.animation_frame_add.name,
                            sizeof(add_command.data.animation_frame_add.name), "sprite_%zu",
                            animation == NULL ? 1 : animation->frame_count + 1);
                        snprintf(add_command.data.animation_frame_add.path,
                            sizeof(add_command.data.animation_frame_add.path), "%s",
                            sprite_path);
                        added = editor_command_execute(&project, &add_command);
                        opened = added.kind == ERROR_RESULT_VALUE;
                        if(!opened) {
                            load_result.kind = ERROR_RESULT_ERROR;
                            load_result.result.error = added.result.error;
                        }
                    }
                    sprite_browser_object = 0;
                    animation_browser_sprite = 0;
                } else if(workspace_browser_action == EDITOR_WORKSPACE_BROWSER_ADD_SPRITE) {
                    const char *sprite_path = editor_project_relative_path_get(
                        workspace.directory, browser_result.path);
                    EditorCommand add_command = {.type = EDITOR_COMMAND_SPRITE_ADD,
                        .data.sprite_add = {.object = sprite_browser_object,
                            .size = {64.0f, 64.0f}}};
                    EditorCommandResult added;

                    opened = sprite_browser_object != EDITOR_OBJECT_INVALID;
                    if(opened && strlen(sprite_path) >=
                            sizeof(add_command.data.sprite_add.path)) {
                        load_result = editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                            "Sprite path is too long: %s", browser_result.path);
                        opened = false;
                    } else if(opened) {
                        snprintf(add_command.data.sprite_add.name,
                            sizeof(add_command.data.sprite_add.name), "sprite_%u",
                            project.next_sprite_id);
                        snprintf(add_command.data.sprite_add.path,
                            sizeof(add_command.data.sprite_add.path), "%s",
                            sprite_path);
                        added = editor_command_execute(&project, &add_command);
                        opened = added.kind == ERROR_RESULT_VALUE;
                        if(!opened) {
                            load_result.kind = ERROR_RESULT_ERROR;
                            load_result.result.error = added.result.error;
                        }
                        if(opened) {
                            viewport_state.selection = EDITOR_SELECTION_SPRITE;
                            viewport_state.selected_sprite = added.result.object;
                        }
                    }
                    sprite_browser_object = 0;
                } else if(strlen(browser_result.path) >= sizeof(command.directory)) {
                    load_result = editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                        "Project directory path is too long: %s", browser_result.path);
                    opened = false;
                } else if(workspace_browser_action == EDITOR_WORKSPACE_BROWSER_NEW) {
                    memcpy(command.directory, browser_result.path,
                        strlen(browser_result.path) + 1);
                    command.type = EDITOR_WORKSPACE_COMMAND_CREATE;
                    load_result = editor_workspace_operation_execute(
                        &workspace, &project, &command);
                    opened = !editor_result_check(load_result);
                } else {
                    memcpy(command.directory, browser_result.path,
                        strlen(browser_result.path) + 1);
                    command.type = EDITOR_WORKSPACE_COMMAND_LOAD;
                    load_result = editor_workspace_operation_execute(
                        &workspace, &project, &command);
                    opened = !editor_result_check(load_result);
                }
                if(opened) {
                    if(workspace_browser_action != EDITOR_WORKSPACE_BROWSER_ADD_SPRITE &&
                            workspace_browser_action !=
                                EDITOR_WORKSPACE_BROWSER_ADD_ANIMATION_FRAME) {
                        editor_history_reset(&history);
                        saved_project_hash = editor_project_hash_get(&project);
                        editor_viewport_state_init(&viewport_state);
                        editor_navigation_state_apply(
                            &project, &viewport_state, &project.navigation);
                        panel_scroll_offset = 0.0f;
                        (void)editor_terminal_panel_project_open(
                            &terminal_panel, workspace.directory);
                        if(terminal_editor_operations &&
                                command.type == EDITOR_WORKSPACE_COMMAND_CREATE) {
                            char cli_command[3072];
                            if(!editor_result_check(editor_workspace_command_cli_write(
                                    &command, cli_command, sizeof(cli_command))))
                                editor_terminal_panel_operation_write(
                                    &terminal_panel, cli_command);
                        }
                    }
                } else {
                    editor_result_stderr_print(load_result);
                    file_browser.active = true;
                }
                if(opened) workspace_browser_action = EDITOR_WORKSPACE_BROWSER_NONE;
            } else if(browser_result.cancelled) {
                workspace_browser_action = EDITOR_WORKSPACE_BROWSER_NONE;
                sprite_browser_object = 0;
                animation_browser_sprite = 0;
            }
        }
        rohr_graphics_layer_set(EDITOR_GRAPHICS_LAYER_TOP_MENU);
        (void)rohr_graphics_screen_rect_draw(0.0f, EDITOR_MENU_HEIGHT - 1.0f,
            editor_window_width, 1.0f, (Color){75, 84, 100, 255});
        {
            Position pointer = rohr_graphics_mouse_screen_position_get();
            if(auto_shape_picker_open &&
                    mouse.button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_PRESSED) {
                UIRect button_bounds = viewport_state.mode == EDITOR_VIEWPORT_HITBOX ?
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 78.0f,
                        EDITOR_TOOLS_WIDTH - 20.0f, 28.0f} :
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 360.0f,
                        EDITOR_TOOLS_WIDTH - 20.0f, 30.0f};
                UIRect picker_bounds = viewport_state.mode == EDITOR_VIEWPORT_HITBOX ?
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 110.0f,
                        EDITOR_TOOLS_WIDTH - 20.0f, 62.0f} :
                    (UIRect){EDITOR_VIEWPORT_WIDTH + 10.0f, 394.0f,
                        EDITOR_TOOLS_WIDTH - 20.0f, 62.0f};
                if(!editor_point_in_rect(pointer, button_bounds) &&
                        !editor_point_in_rect(pointer, picker_bounds))
                    auto_shape_picker_open = false;
            }
            bool ui_consumed = !workspace.open || file_browser.active ||
                close_action != EDITOR_CLOSE_NONE ||
                viewport_context_open || color_picker.open ||
                rohr_ui_pointer_consumed_get() ||
                pointer.y < EDITOR_MENU_HEIGHT ||
                pointer.y >= EDITOR_VIEWPORT_BOTTOM;
            bool viewport_consumed;
            bool group_transform_before = viewport_state.group_dragging ||
                viewport_state.group_rotating ||
                (viewport_state.selected_item_count >= 2 &&
                    (viewport_state.rotated_body ||
                        viewport_state.rotated_soft_body));
            bool pan_modifier =
                rohr_controller_key_down_get(&keyboard, SDLK_LCTRL) ||
                rohr_controller_key_down_get(&keyboard, SDLK_RCTRL);
            if(viewport_state.marquee_active) {
                viewport_consumed = true;
                if(mouse.button_states[MOUSE_BUTTON_LEFT] ==
                        MOUSE_BUTTON_STATE_RELEASED) {
                    pointer_selection_handled = editor_viewport_marquee_finish(
                        &viewport_state, &project, pointer);
                }
                else editor_viewport_marquee_update(&viewport_state, pointer);
            } else if(viewport_state.mode == EDITOR_VIEWPORT_AUTO_SHAPE) {
                viewport_consumed = editor_viewport_auto_shape_update(
                    &viewport_state, &project, &auto_shape_config, pointer,
                    mouse.button_states[MOUSE_BUTTON_LEFT],
                    mouse.button_states[MOUSE_BUTTON_MIDDLE], pan_modifier,
                    viewport_wheel_y, ui_consumed);
            } else {
                bool control_select_press = pan_modifier &&
                    mouse.button_states[MOUSE_BUTTON_LEFT] ==
                        MOUSE_BUTTON_STATE_PRESSED;
                viewport_state.selection_modifier = control_select_press;
                viewport_consumed = editor_viewport_update(
                    &viewport_state, &project, pointer,
                    mouse.button_states[MOUSE_BUTTON_LEFT],
                    mouse.button_states[MOUSE_BUTTON_MIDDLE],
                    control_select_press ? false : pan_modifier,
                    viewport_wheel_y, ui_consumed);
                if(control_select_press && !viewport_consumed)
                    viewport_consumed = editor_viewport_update(
                        &viewport_state, &project, pointer,
                        mouse.button_states[MOUSE_BUTTON_LEFT],
                        mouse.button_states[MOUSE_BUTTON_MIDDLE], true,
                        viewport_wheel_y, ui_consumed);
                viewport_state.selection_modifier = false;
            }
            {
                bool group_transform_after = viewport_state.group_dragging ||
                    viewport_state.group_rotating ||
                    (viewport_state.selected_item_count >= 2 &&
                        (viewport_state.rotated_body ||
                            viewport_state.rotated_soft_body));
                if(!group_transform_before && group_transform_after) {
                    if(!editor_history_transaction_begin(&history)) {
                        viewport_state.group_dragging = false;
                        viewport_state.group_rotating = false;
                        viewport_state.rotated_body = false;
                        viewport_state.rotated_soft_body = false;
                    } else for(size_t i = 0;
                            i < viewport_state.selected_item_count; i += 1)
                        if(!editor_history_transaction_object_track(&history,
                                viewport_state.selected_items[i].object)) {
                            editor_history_transaction_cancel(&history);
                            viewport_state.group_dragging = false;
                            viewport_state.group_rotating = false;
                            viewport_state.rotated_body = false;
                            viewport_state.rotated_soft_body = false;
                            break;
                        }
                } else if(group_transform_before && !group_transform_after) {
                    (void)editor_history_transaction_end(&history);
                }
            }

            if(mouse.button_states[MOUSE_BUTTON_LEFT] ==
                        MOUSE_BUTTON_STATE_PRESSED &&
                    !ui_consumed && viewport_consumed &&
                    !viewport_state.camera_panning &&
                    !viewport_state.group_dragging &&
                    !viewport_state.group_rotating &&
                    !viewport_state.rotated_body &&
                    !viewport_state.rotated_soft_body &&
                    viewport_state.mode != EDITOR_VIEWPORT_AUTO_SHAPE) {
                EditorSelectionRef selection;
                if(editor_viewport_selection_ref_get(
                        &project, &viewport_state, &selection)) {
                    if(pan_modifier && viewport_state.selected_item_count == 0 &&
                            prior_pointer_selection_valid)
                        (void)editor_viewport_selection_set(&project, &viewport_state,
                            prior_pointer_selection, false);
                    (void)editor_viewport_selection_set(&project, &viewport_state,
                        selection, pan_modifier);
                    pointer_selection_handled = true;
                }
            }

            if(mouse.button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_PRESSED &&
                    !ui_consumed && !viewport_consumed && !panel_resizing) {
                editor_navigation_current_selection_clear(&project, &viewport_state);
                editor_viewport_marquee_begin(&viewport_state, pointer);
            }
        }
        if(workspace.open) {
            EditorNavigationState navigation_after = editor_navigation_state_get(
                &project, &viewport_state);
            bool selection_changed = navigation_before.selection !=
                    navigation_after.selection ||
                navigation_before.object != navigation_after.object ||
                navigation_before.rigid_body != navigation_after.rigid_body ||
                navigation_before.hitbox != navigation_after.hitbox ||
                navigation_before.joint != navigation_after.joint ||
                navigation_before.anchor != navigation_after.anchor ||
                navigation_before.soft_body != navigation_after.soft_body ||
                navigation_before.soft_node != navigation_after.soft_node ||
                navigation_before.soft_beam != navigation_after.soft_beam ||
                navigation_before.selected_line != navigation_after.selected_line ||
                navigation_before.selected_vertex != navigation_after.selected_vertex;
            if(selection_changed && !pointer_selection_handled) {
                EditorSelectionRef selection;
                bool additive = mouse.button_states[MOUSE_BUTTON_LEFT] ==
                        MOUSE_BUTTON_STATE_PRESSED &&
                    (
                    rohr_controller_key_down_get(&keyboard, SDLK_LCTRL) ||
                    rohr_controller_key_down_get(&keyboard, SDLK_RCTRL));
                if(editor_viewport_selection_ref_get(
                        &project, &viewport_state, &selection)) {
                    if(additive && viewport_state.selected_item_count == 0 &&
                            prior_pointer_selection_valid)
                        (void)editor_viewport_selection_set(&project, &viewport_state,
                            prior_pointer_selection, false);
                    (void)editor_viewport_selection_set(
                        &project, &viewport_state, selection, additive);
                    navigation_after = editor_navigation_state_get(
                        &project, &viewport_state);
                }
            }
            if(memcmp(&navigation_before, &navigation_after,
                    sizeof(navigation_after)) != 0) {
                EditorCommand command = {.type = EDITOR_COMMAND_NAVIGATION_SET,
                    .data.navigation = navigation_after};
                (void)editor_command_execute(&project, &command);
            }
        }
        editor_history_continuous_set(&history, field_editing ||
            mouse.button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_PRESSED ||
            mouse.button_states[MOUSE_BUTTON_LEFT] == MOUSE_BUTTON_STATE_DOWN);
        rohr_ui_frame_end();
        if(!file_browser.active) {
            (void)rohr_graphics_screen_rect_draw(
                EDITOR_VIEWPORT_WIDTH - 1.0f, EDITOR_MENU_HEIGHT, 3.0f,
                EDITOR_WINDOW_HEIGHT - EDITOR_MENU_HEIGHT,
                (Color){75, 84, 100, 255});
        }
        rohr_graphics_show();
    }

    editor_command_executed_callback_set(NULL, NULL);
    editor_command_executing_callback_set(NULL, NULL);
    editor_command_finished_callback_set(NULL, NULL);
    editor_operation_history = NULL;
    editor_viewport_state_destroy(&viewport_state);
    editor_viewport_assets_destroy();
    editor_history_destroy(&history);
    editor_project_destroy(&project);
    if(hidden_build_process != NULL) SDL_DestroyProcess(hidden_build_process);
    editor_terminal_panel_destroy(&terminal_panel);
    editor_build_settings_panel_destroy(&build_settings_panel);
    editor_visual_settings_panel_destroy(&visual_settings_panel);
    editor_notification_panel_destroy(&notification_panel);
    editor_bulk_panel_destroy(&bulk_panel);
    editor_origin_panel_destroy(&origin_panel);
    rohr_graphics_text_destroy(&add_sprite_label);
    rohr_graphics_text_destroy(&add_animated_sprite_label);
    rohr_graphics_text_destroy(&add_frame_label);
    rohr_graphics_text_destroy(&path_label);
    rohr_graphics_text_destroy(&scale_x_label);
    rohr_graphics_text_destroy(&scale_y_label);
    rohr_graphics_text_destroy(&ticks_per_frame_label);
    rohr_graphics_text_destroy(&time_per_frame_label);
    rohr_graphics_text_destroy(&starting_frame_label);
    rohr_graphics_text_destroy(&direction_label);
    rohr_graphics_text_destroy(&follow_rotation_label);
    rohr_graphics_text_destroy(&left_label);
    rohr_graphics_text_destroy(&right_label);
    rohr_graphics_text_destroy(&delete_sprite_label);
    rohr_graphics_text_destroy(&delete_animated_sprite_label);
    rohr_graphics_text_destroy(&path_field);
    for(size_t i = 0; i < 64; i += 1)
        rohr_graphics_text_destroy(&sprite_labels[i]);
    for(size_t i = 0; i < 32; i += 1)
        rohr_graphics_text_destroy(&animated_sprite_labels[i]);
    rohr_graphics_text_destroy(&stiffness_label);
    rohr_graphics_text_destroy(&rotation_global_label);
    rohr_graphics_text_destroy(&rotation_body_label);
    rohr_graphics_text_destroy(&position_global_label);
    rohr_graphics_text_destroy(&position_body_label);
    rohr_graphics_text_destroy(&rotation_label);
    rohr_graphics_text_destroy(&mass_label);
    rohr_graphics_text_destroy(&radius_label);
    rohr_graphics_text_destroy(&border_color_label);
    rohr_graphics_text_destroy(&surface_color_label);
    rohr_graphics_text_destroy(&node_color_label);
    rohr_graphics_text_destroy(&beam_color_label);
    rohr_graphics_text_destroy(&area_color_label);
    rohr_graphics_text_destroy(&inherit_label);
    rohr_graphics_text_destroy(&color_picker_hex_field);
    rohr_graphics_text_destroy(&color_picker_opacity_field);
    rohr_graphics_text_destroy(&opacity_label);
    rohr_graphics_text_destroy(&gravity_label);
    rohr_graphics_text_destroy(&friction_label);
    rohr_graphics_text_destroy(&restitution_label);
    rohr_graphics_text_destroy(&rest_length_label);
    rohr_graphics_text_destroy(&damping_label);
    rohr_graphics_text_destroy(&visual_size_label);
    rohr_graphics_text_destroy(&dynamic_label);
    rohr_graphics_text_destroy(&static_label);
    rohr_graphics_text_destroy(&rotation_locked_label);
    rohr_graphics_text_destroy(&rotation_unlocked_label);
    rohr_graphics_text_destroy(&node_b_label);
    rohr_graphics_text_destroy(&node_a_label);
    rohr_graphics_text_destroy(&body_b_label);
    rohr_graphics_text_destroy(&body_a_label);
    rohr_graphics_text_destroy(&rigid_body_label);
    rohr_graphics_text_destroy(&spring_label);
    rohr_graphics_text_destroy(&weld_label);
    rohr_graphics_text_destroy(&revolute_label);
    rohr_graphics_text_destroy(&visibility_visible_label);
    rohr_graphics_text_destroy(&visibility_hidden_label);
    rohr_graphics_text_destroy(&add_beam_label);
    rohr_graphics_text_destroy(&add_node_label);
    rohr_graphics_text_destroy(&add_soft_body_label);
    rohr_graphics_text_destroy(&add_joint_label);
    rohr_graphics_text_destroy(&add_anchor_label);
    rohr_graphics_text_destroy(&add_rigid_body_label);
    rohr_graphics_text_destroy(&delete_beam_label);
    rohr_graphics_text_destroy(&delete_node_label);
    rohr_graphics_text_destroy(&delete_soft_body_label);
    rohr_graphics_text_destroy(&delete_joint_label);
    rohr_graphics_text_destroy(&delete_anchor_label);
    rohr_graphics_text_destroy(&delete_rigid_body_label);
    for(size_t i = 0; i < EDITOR_RIGID_BODY_MAX; i += 1) {
        rohr_graphics_text_destroy(&rigid_body_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_BODY_HITBOX_MAX; i += 1) {
        rohr_graphics_text_destroy(&body_hitbox_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_JOINT_MAX; i += 1) {
        rohr_graphics_text_destroy(&joint_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_ANCHOR_MAX; i += 1) {
        rohr_graphics_text_destroy(&anchor_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_BODY_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_body_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_NODE_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_node_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_BEAM_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_beam_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_AREA_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_area_labels[i]);
    }
    rohr_graphics_text_destroy(&vertices_label);
    rohr_graphics_text_destroy(&length_field);
    rohr_graphics_text_destroy(&y_field);
    rohr_graphics_text_destroy(&x_field);
    rohr_graphics_text_destroy(&length_label);
    rohr_graphics_text_destroy(&object_name_label);
    rohr_graphics_text_destroy(&name_label);
    rohr_graphics_text_destroy(&origin_label);
    rohr_graphics_text_destroy(&y_label);
    rohr_graphics_text_destroy(&x_label);
    rohr_graphics_text_destroy(&constrained_label);
    rohr_graphics_text_destroy(&delete_object_label);
    rohr_graphics_text_destroy(&delete_line_label);
    rohr_graphics_text_destroy(&delete_vertex_label);
    rohr_graphics_text_destroy(&delete_hitbox_label);
    rohr_graphics_text_destroy(&add_vertex_label);
    rohr_graphics_text_destroy(&unlock_label);
    rohr_graphics_text_destroy(&lock_label);
    for(uint32_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
        rohr_graphics_text_destroy(&line_labels[i]);
        rohr_graphics_text_destroy(&vertex_labels[i]);
    }
    rohr_graphics_text_destroy(&lines_label);
    rohr_graphics_text_destroy(&hitbox_label);
    for(size_t i = 0; i < EDITOR_OBJECT_MAX; i += 1) {
        rohr_graphics_text_destroy(&object_name_labels[i]);
    }
    rohr_graphics_text_destroy(&add_hitbox_label);
    rohr_graphics_text_destroy(&add_object_label);
    rohr_graphics_text_destroy(&none_label);
    rohr_graphics_text_destroy(&apex_offset_label);
    rohr_graphics_text_destroy(&height_label);
    rohr_graphics_text_destroy(&width_label);
    rohr_graphics_text_destroy(&scalene_label);
    rohr_graphics_text_destroy(&isosceles_label);
    rohr_graphics_text_destroy(&equilateral_label);
    rohr_graphics_text_destroy(&circle_label);
    rohr_graphics_text_destroy(&rectangle_label);
    rohr_graphics_text_destroy(&triangle_label);
    rohr_graphics_text_destroy(&auto_shape_label);
    rohr_graphics_text_destroy(&hitbox_editor_label);
    rohr_graphics_text_destroy(&preferences_label);
    rohr_graphics_text_destroy(&grid_label);
    rohr_graphics_text_destroy(&terminal_label);
    rohr_graphics_text_destroy(&terminal_menu_label);
    rohr_graphics_text_destroy(&terminal_visible_label);
    rohr_graphics_text_destroy(&terminal_editor_operations_label);
    rohr_graphics_text_destroy(&terminal_generated_code_label);
    rohr_graphics_text_destroy(&terminal_build_operations_label);
    rohr_graphics_text_destroy(&reset_view_label);
    rohr_graphics_text_destroy(&save_label);
    rohr_graphics_text_destroy(&cancel_label);
    rohr_graphics_text_destroy(&dont_save_label);
    rohr_graphics_text_destroy(&unsaved_changes_label);
    rohr_graphics_text_destroy(&close_label);
    rohr_graphics_text_destroy(&exit_label);
    rohr_graphics_text_destroy(&open_label);
    rohr_graphics_text_destroy(&load_frame_label);
    rohr_graphics_text_destroy(&create_project_label);
    rohr_graphics_text_destroy(&new_label);
    rohr_graphics_text_destroy(&settings_label);
    rohr_graphics_text_destroy(&view_label);
    rohr_graphics_text_destroy(&generate_c_label);
    rohr_graphics_text_destroy(&compile_label);
    rohr_graphics_text_destroy(&build_project_label);
    rohr_graphics_text_destroy(&build_label);
    rohr_graphics_text_destroy(&local_view_label);
    rohr_graphics_text_destroy(&world_view_label);
    rohr_graphics_text_destroy(&collision_mask_name_field);
    rohr_graphics_text_destroy(&add_label);
    rohr_graphics_text_destroy(&collision_category_label);
    rohr_graphics_text_destroy(&collide_with_label);
    rohr_graphics_text_destroy(&collision_label);
    rohr_graphics_text_destroy(&particle_label);
    rohr_graphics_text_destroy(&particle_ring_color_label);
    rohr_graphics_text_destroy(&particle_fill_color_label);
    rohr_graphics_text_destroy(&auto_fit_label);
    rohr_graphics_text_destroy(&context_action_one_label);
    rohr_graphics_text_destroy(&context_action_two_label);
    rohr_graphics_text_destroy(&context_action_three_label);
    for(size_t i = 0; i < EDITOR_COLLISION_MASK_MAX; i += 1) {
        rohr_graphics_text_destroy(&collision_mask_labels[i]);
    }
    rohr_graphics_text_destroy(&file_label);
    rohr_graphics_text_destroy(&redo_label);
    rohr_graphics_text_destroy(&undo_label);
    rohr_graphics_text_destroy(&edit_label);
    rohr_graphics_text_destroy(&file_browser_field);
    editor_file_browser_destroy(&file_browser);
    rohr_graphics_font_destroy(&notification_font);
    rohr_graphics_font_destroy(&font);
    if(viewport != 0) (void)rohr_viewport_destroy(viewport);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 0;

fail:
    editor_viewport_assets_destroy();
    editor_command_executed_callback_set(NULL, NULL);
    editor_command_executing_callback_set(NULL, NULL);
    editor_command_finished_callback_set(NULL, NULL);
    editor_operation_history = NULL;
    editor_viewport_state_destroy(&viewport_state);
    editor_history_destroy(&history);
    editor_project_destroy(&project);
    if(hidden_build_process != NULL) SDL_DestroyProcess(hidden_build_process);
    editor_terminal_panel_destroy(&terminal_panel);
    editor_build_settings_panel_destroy(&build_settings_panel);
    editor_visual_settings_panel_destroy(&visual_settings_panel);
    editor_notification_panel_destroy(&notification_panel);
    editor_bulk_panel_destroy(&bulk_panel);
    editor_origin_panel_destroy(&origin_panel);
    rohr_graphics_text_destroy(&add_sprite_label);
    rohr_graphics_text_destroy(&add_animated_sprite_label);
    rohr_graphics_text_destroy(&add_frame_label);
    rohr_graphics_text_destroy(&path_label);
    rohr_graphics_text_destroy(&scale_x_label);
    rohr_graphics_text_destroy(&scale_y_label);
    rohr_graphics_text_destroy(&ticks_per_frame_label);
    rohr_graphics_text_destroy(&time_per_frame_label);
    rohr_graphics_text_destroy(&starting_frame_label);
    rohr_graphics_text_destroy(&direction_label);
    rohr_graphics_text_destroy(&follow_rotation_label);
    rohr_graphics_text_destroy(&left_label);
    rohr_graphics_text_destroy(&right_label);
    rohr_graphics_text_destroy(&delete_sprite_label);
    rohr_graphics_text_destroy(&delete_animated_sprite_label);
    rohr_graphics_text_destroy(&path_field);
    for(size_t i = 0; i < 64; i += 1)
        rohr_graphics_text_destroy(&sprite_labels[i]);
    for(size_t i = 0; i < 32; i += 1)
        rohr_graphics_text_destroy(&animated_sprite_labels[i]);
    rohr_graphics_text_destroy(&stiffness_label);
    rohr_graphics_text_destroy(&rotation_global_label);
    rohr_graphics_text_destroy(&rotation_body_label);
    rohr_graphics_text_destroy(&position_global_label);
    rohr_graphics_text_destroy(&position_body_label);
    rohr_graphics_text_destroy(&rotation_label);
    rohr_graphics_text_destroy(&mass_label);
    rohr_graphics_text_destroy(&radius_label);
    rohr_graphics_text_destroy(&border_color_label);
    rohr_graphics_text_destroy(&surface_color_label);
    rohr_graphics_text_destroy(&node_color_label);
    rohr_graphics_text_destroy(&beam_color_label);
    rohr_graphics_text_destroy(&area_color_label);
    rohr_graphics_text_destroy(&inherit_label);
    rohr_graphics_text_destroy(&color_picker_hex_field);
    rohr_graphics_text_destroy(&color_picker_opacity_field);
    rohr_graphics_text_destroy(&opacity_label);
    rohr_graphics_text_destroy(&gravity_label);
    rohr_graphics_text_destroy(&friction_label);
    rohr_graphics_text_destroy(&restitution_label);
    rohr_graphics_text_destroy(&rest_length_label);
    rohr_graphics_text_destroy(&damping_label);
    rohr_graphics_text_destroy(&visual_size_label);
    rohr_graphics_text_destroy(&dynamic_label);
    rohr_graphics_text_destroy(&static_label);
    rohr_graphics_text_destroy(&rotation_locked_label);
    rohr_graphics_text_destroy(&rotation_unlocked_label);
    rohr_graphics_text_destroy(&node_b_label);
    rohr_graphics_text_destroy(&node_a_label);
    rohr_graphics_text_destroy(&body_b_label);
    rohr_graphics_text_destroy(&body_a_label);
    rohr_graphics_text_destroy(&rigid_body_label);
    rohr_graphics_text_destroy(&spring_label);
    rohr_graphics_text_destroy(&weld_label);
    rohr_graphics_text_destroy(&revolute_label);
    rohr_graphics_text_destroy(&visibility_visible_label);
    rohr_graphics_text_destroy(&visibility_hidden_label);
    rohr_graphics_text_destroy(&add_beam_label);
    rohr_graphics_text_destroy(&add_node_label);
    rohr_graphics_text_destroy(&add_soft_body_label);
    rohr_graphics_text_destroy(&add_joint_label);
    rohr_graphics_text_destroy(&add_anchor_label);
    rohr_graphics_text_destroy(&add_rigid_body_label);
    rohr_graphics_text_destroy(&delete_beam_label);
    rohr_graphics_text_destroy(&delete_node_label);
    rohr_graphics_text_destroy(&delete_soft_body_label);
    rohr_graphics_text_destroy(&delete_joint_label);
    rohr_graphics_text_destroy(&delete_anchor_label);
    rohr_graphics_text_destroy(&delete_rigid_body_label);
    for(size_t i = 0; i < EDITOR_RIGID_BODY_MAX; i += 1) {
        rohr_graphics_text_destroy(&rigid_body_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_BODY_HITBOX_MAX; i += 1) {
        rohr_graphics_text_destroy(&body_hitbox_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_JOINT_MAX; i += 1) {
        rohr_graphics_text_destroy(&joint_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_ANCHOR_MAX; i += 1) {
        rohr_graphics_text_destroy(&anchor_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_BODY_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_body_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_NODE_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_node_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_BEAM_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_beam_labels[i]);
    }
    for(size_t i = 0; i < EDITOR_SOFT_AREA_MAX; i += 1) {
        rohr_graphics_text_destroy(&soft_area_labels[i]);
    }
    rohr_graphics_text_destroy(&vertices_label);
    rohr_graphics_text_destroy(&length_field);
    rohr_graphics_text_destroy(&y_field);
    rohr_graphics_text_destroy(&x_field);
    rohr_graphics_text_destroy(&length_label);
    rohr_graphics_text_destroy(&object_name_label);
    rohr_graphics_text_destroy(&name_label);
    rohr_graphics_text_destroy(&origin_label);
    rohr_graphics_text_destroy(&y_label);
    rohr_graphics_text_destroy(&x_label);
    rohr_graphics_text_destroy(&constrained_label);
    rohr_graphics_text_destroy(&delete_object_label);
    rohr_graphics_text_destroy(&delete_line_label);
    rohr_graphics_text_destroy(&delete_vertex_label);
    rohr_graphics_text_destroy(&delete_hitbox_label);
    rohr_graphics_text_destroy(&add_vertex_label);
    rohr_graphics_text_destroy(&unlock_label);
    rohr_graphics_text_destroy(&lock_label);
    for(uint32_t i = 0; i < EDITOR_HITBOX_VERTEX_MAX; i += 1) {
        rohr_graphics_text_destroy(&line_labels[i]);
        rohr_graphics_text_destroy(&vertex_labels[i]);
    }
    rohr_graphics_text_destroy(&lines_label);
    rohr_graphics_text_destroy(&hitbox_label);
    for(size_t i = 0; i < EDITOR_OBJECT_MAX; i += 1) {
        rohr_graphics_text_destroy(&object_name_labels[i]);
    }
    rohr_graphics_text_destroy(&add_hitbox_label);
    rohr_graphics_text_destroy(&add_object_label);
    rohr_graphics_text_destroy(&none_label);
    rohr_graphics_text_destroy(&apex_offset_label);
    rohr_graphics_text_destroy(&height_label);
    rohr_graphics_text_destroy(&width_label);
    rohr_graphics_text_destroy(&scalene_label);
    rohr_graphics_text_destroy(&isosceles_label);
    rohr_graphics_text_destroy(&equilateral_label);
    rohr_graphics_text_destroy(&circle_label);
    rohr_graphics_text_destroy(&rectangle_label);
    rohr_graphics_text_destroy(&triangle_label);
    rohr_graphics_text_destroy(&auto_shape_label);
    rohr_graphics_text_destroy(&hitbox_editor_label);
    rohr_graphics_text_destroy(&preferences_label);
    rohr_graphics_text_destroy(&grid_label);
    rohr_graphics_text_destroy(&terminal_label);
    rohr_graphics_text_destroy(&terminal_menu_label);
    rohr_graphics_text_destroy(&terminal_visible_label);
    rohr_graphics_text_destroy(&terminal_editor_operations_label);
    rohr_graphics_text_destroy(&terminal_generated_code_label);
    rohr_graphics_text_destroy(&terminal_build_operations_label);
    rohr_graphics_text_destroy(&reset_view_label);
    rohr_graphics_text_destroy(&save_label);
    rohr_graphics_text_destroy(&cancel_label);
    rohr_graphics_text_destroy(&dont_save_label);
    rohr_graphics_text_destroy(&unsaved_changes_label);
    rohr_graphics_text_destroy(&close_label);
    rohr_graphics_text_destroy(&exit_label);
    rohr_graphics_text_destroy(&open_label);
    rohr_graphics_text_destroy(&load_frame_label);
    rohr_graphics_text_destroy(&create_project_label);
    rohr_graphics_text_destroy(&new_label);
    rohr_graphics_text_destroy(&settings_label);
    rohr_graphics_text_destroy(&view_label);
    rohr_graphics_text_destroy(&generate_c_label);
    rohr_graphics_text_destroy(&compile_label);
    rohr_graphics_text_destroy(&build_project_label);
    rohr_graphics_text_destroy(&build_label);
    rohr_graphics_text_destroy(&local_view_label);
    rohr_graphics_text_destroy(&world_view_label);
    rohr_graphics_text_destroy(&collision_mask_name_field);
    rohr_graphics_text_destroy(&add_label);
    rohr_graphics_text_destroy(&collision_category_label);
    rohr_graphics_text_destroy(&collide_with_label);
    rohr_graphics_text_destroy(&collision_label);
    rohr_graphics_text_destroy(&particle_label);
    rohr_graphics_text_destroy(&particle_ring_color_label);
    rohr_graphics_text_destroy(&particle_fill_color_label);
    rohr_graphics_text_destroy(&auto_fit_label);
    rohr_graphics_text_destroy(&context_action_one_label);
    rohr_graphics_text_destroy(&context_action_two_label);
    rohr_graphics_text_destroy(&context_action_three_label);
    for(size_t i = 0; i < EDITOR_COLLISION_MASK_MAX; i += 1) {
        rohr_graphics_text_destroy(&collision_mask_labels[i]);
    }
    rohr_graphics_text_destroy(&file_label);
    rohr_graphics_text_destroy(&redo_label);
    rohr_graphics_text_destroy(&undo_label);
    rohr_graphics_text_destroy(&edit_label);
    rohr_graphics_text_destroy(&file_browser_field);
    editor_file_browser_destroy(&file_browser);
    rohr_graphics_font_destroy(&notification_font);
    rohr_graphics_font_destroy(&font);
    if(viewport != 0) (void)rohr_viewport_destroy(viewport);
    rohr_graphics_end();
    rohr_engine_shutdown();
    return 1;
}
