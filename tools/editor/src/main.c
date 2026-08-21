/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include "rohr.h"
#include "editor_project.h"
#include "editor_workspace.h"
#include "browser/editor_file_browser.h"
#include "editor_history.h"
#include "editor_shortcuts.h"
#include "editor_viewport.h"
#include "editor_layout.h"
#include "editor_navigation.h"
#include "editor_command.h"
#include "editor_object_commands.h"
#include "editors/geometry/editor_origin_panel.h"
#include "editors/multi/editor_bulk_panel.h"
#include "panels/editor_build_settings_panel.h"
#include "panels/editor_build_notifications.h"
#include "panels/editor_generation_report.h"
#include "panels/editor_notification_panel.h"
#include "panels/editor_terminal_panel.h"
#include "panels/editor_visual_settings_panel.h"
#include "panels/editor_error_notifications.h"
#include "states/editor_app_state.h"
#include "states/editor_project_launcher_state.h"
#include "editors/physics/editor_particle.h"
#include "editors/physics/editor_anchor.h"
#include "editors/physics/editor_rigid_body.h"
#include "editors/physics/editor_joint.h"
#include "editors/geometry/editor_vertex.h"
#include "editors/geometry/editor_line.h"
#include "editors/geometry/editor_auto_shape_editor.h"
#include "editors/geometry/editor_hitbox.h"
#include "editors/render/editor_animation_frame.h"
#include "editors/render/editor_sprite.h"
#include "editors/render/editor_animated_sprite.h"
#include "editors/object/editor_object.h"
#include "editors/object/editor_hierarchy.h"
#include "editors/soft_body/editor_soft_beam.h"
#include "editors/soft_body/editor_soft_node.h"
#include "editors/soft_body/editor_soft_area.h"
#include "editors/soft_body/editor_soft_body.h"
#include "viewport/controls/editor_coordinate_toggle.h"
#include "viewport/menus/editor_viewport_context_menu.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef ROHR_DEVELOPMENT_SOURCE_DIR
#define ROHR_DEVELOPMENT_SOURCE_DIR ""
#endif

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
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

typedef struct EditorModeColorContext {
    EditorColorPicker *picker;
    EditorProject *project;
} EditorModeColorContext;

typedef struct EditorModeDeleteContext {
    EditorProject *project;
    EditorViewportState *viewport;
} EditorModeDeleteContext;

typedef struct EditorHierarchyDragState EditorHierarchyDragState;

typedef struct EditorModeHierarchyContext {
    EditorHierarchyDragState *drag;
    Position pointer;
    MouseButtonState primary;
    float scroll_offset;
} EditorModeHierarchyContext;

typedef struct EditorCollisionMenuContext {
    FontAsset *font;
    TextAsset *labels;
    char (*caches)[EDITOR_OBJECT_NAME_MAX];
    char *name;
    size_t name_capacity;
    TextAsset *name_field;
    const TextAsset *add_label;
} EditorCollisionMenuContext;

typedef struct EditorAnimationBrowserContext {
    EditorFileBrowser *browser;
    EditorWorkspace *workspace;
    FontAsset *font;
    EditorObjectId *object;
    EditorAnimatedSpriteId *sprite;
    EditorWorkspaceBrowserAction *action;
} EditorAnimationBrowserContext;

float editor_viewport_width = WINDOW_WIDTH * 0.8f;
float editor_window_width = WINDOW_WIDTH;
float editor_window_height = WINDOW_HEIGHT;
float editor_viewport_bottom = WINDOW_HEIGHT;

static EditorTerminalPanel *editor_operation_terminal;
static const EditorWorkspace *editor_operation_workspace;
static const EditorProject *editor_operation_project;
static bool *editor_operation_enabled;
static EditorHistory *editor_operation_history;

static bool editor_open_item_delete(EditorProject *project,
    EditorViewportState *viewport_state);

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
        .animation_frame = state->selected_animation_frame,
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
    state->selected_animation_frame = navigation->animation_frame;
    state->selected_origin_kind = (EditorOriginKind)navigation->origin_kind;
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

static void editor_startup_failure_report(const char *stage) {
    char message[512];
    FILE *log;
    if(stage == NULL || stage[0] == '\0') stage = "unknown startup stage";
    (void)snprintf(message, sizeof(message),
        "Rohr GUI could not start during: %s\n"
        "See rohr_gui_error.txt beside rohr-gui for details.", stage);
    fprintf(stderr, "%s\n", message);
    fflush(stderr);
    log = fopen("rohr_gui_error.txt", "wb");
    if(log != NULL) {
        fprintf(log, "%s\n", message);
        fclose(log);
    }
#if defined(_WIN32)
    (void)MessageBoxA(NULL, message, "Rohr GUI startup failed",
        MB_OK | MB_ICONERROR | MB_TASKMODAL);
#endif
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
            object->joint_count + object->soft_body_count + object->sprite_count +
            object->animated_sprite_count) * 30.0f);
    }
    if(state->mode == EDITOR_VIEWPORT_RIGID_BODY) {
        for(size_t i = 0; i < object->rigid_body_count; i += 1) {
            if(object->rigid_bodies[i].id == state->selected_rigid_body) {
                return fmaxf(height, 674.0f +
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
    if(state->mode == EDITOR_VIEWPORT_ANIMATED_SPRITE) {
        for(size_t i = 0; i < object->animated_sprite_count; i += 1)
            if(object->animated_sprite_items[i].id == state->selected_animated_sprite)
                return fmaxf(height, 614.0f +
                    (float)object->animated_sprite_items[i].frame_count * 30.0f);
    }
    if(state->mode == EDITOR_VIEWPORT_HITBOX) {
        for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
            const EditorRigidBody *body = &object->rigid_bodies[body_index];
            if(body->id != state->selected_rigid_body) continue;
            for(size_t hitbox_index = 0; hitbox_index < body->hitbox_count; hitbox_index += 1) {
                if(body->hitboxes[hitbox_index].id == state->selected_hitbox) {
                    return fmaxf(height, 249.0f +
                        (float)body->hitboxes[hitbox_index].vertex_count * 54.0f);
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

static void editor_mode_color_picker_open(void *context,
        uint32_t *color,
        EditorItemKind kind,
        EditorObjectId object,
        uint32_t parent,
        uint32_t item,
        EditorPropertyKind property) {
    EditorModeColorContext *mode = context;
    if(mode == NULL) return;
    editor_color_picker_open(mode->picker, color, mode->project, kind,
        object, parent, item, property);
}

static void editor_mode_local_color_picker_open(void *context, uint32_t *color) {
    EditorModeColorContext *mode = context;
    if(mode == NULL) return;
    editor_color_picker_open(mode->picker, color, NULL, EDITOR_ITEM_OBJECT,
        0, 0, 0, EDITOR_PROPERTY_COLOR);
}

static float editor_mode_delete_y_get(void *context) {
    EditorModeDeleteContext *mode = context;
    return mode == NULL ? 0.0f :
        editor_panel_delete_y_get(mode->project, mode->viewport);
}

static bool editor_mode_open_item_delete(void *context) {
    EditorModeDeleteContext *mode = context;
    return mode != NULL && editor_open_item_delete(
        mode->project, mode->viewport);
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

static bool editor_rigid_body_collision_menu_draw(void *opaque,
        const char *id_prefix, EditorProject *project, uint64_t *active_masks,
        EditorObjectId object, EditorRigidBodyId body,
        EditorCollisionFilterKind filter, float x, float y, float width,
        bool *field_active, size_t *row_count) {
    EditorCollisionMenuContext *context = opaque;
    if(context == NULL) return false;
    return editor_collision_mask_menu_draw(id_prefix, project, active_masks,
        EDITOR_ITEM_RIGID_BODY, object, 0, body, filter, context->font,
        context->labels, context->caches, context->name,
        context->name_capacity, context->name_field, context->add_label,
        x, y, width, field_active, row_count);
}

static bool editor_soft_node_collision_menu_draw(void *opaque,
        const char *id_prefix, EditorProject *project, uint64_t *active_masks,
        EditorObjectId object, EditorSoftBodyId body, EditorSoftNodeId node,
        EditorCollisionFilterKind filter, float x, float y, float width,
        bool *field_active, size_t *row_count) {
    EditorCollisionMenuContext *context = opaque;
    if(context == NULL) return false;
    return editor_collision_mask_menu_draw(id_prefix, project, active_masks,
        EDITOR_ITEM_SOFT_NODE, object, body, node, filter, context->font,
        context->labels, context->caches, context->name,
        context->name_capacity, context->name_field, context->add_label,
        x, y, width, field_active, row_count);
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

static void editor_rigid_body_dropdown_preview_set(EditorViewportState *state,
        const EditorObject *object, UIDropdownResult result,
        EditorRigidBodyId selected) {
    if(state == NULL || object == NULL) return;
    if(result.hovered_index >= 0) {
        size_t option = (size_t)result.hovered_index;
        state->preview_rigid_body = option == 0 || option > object->rigid_body_count ?
            0 : object->rigid_bodies[option - 1].id;
    } else if(result.button_hovered) {
        state->preview_rigid_body = selected;
    }
}

static void editor_mode_rigid_body_preview(void *context, EditorObject *object,
        UIDropdownResult result, EditorRigidBodyId selected) {
    editor_rigid_body_dropdown_preview_set(context, object, result, selected);
}

static void editor_mode_animation_frame_browser_open(void *opaque,
        EditorObjectId object, EditorAnimatedSpriteId sprite) {
    EditorAnimationBrowserContext *context = opaque;
    if(context == NULL) return;
    *context->object = object;
    *context->sprite = sprite;
    *context->action = EDITOR_WORKSPACE_BROWSER_ADD_ANIMATION_FRAME;
    if(!editor_file_browser_open(context->browser,
            EDITOR_FILE_BROWSER_OPEN_PNG_MULTI,
            context->workspace->directory, context->font)) {
        *context->object = 0;
        *context->sprite = 0;
        *context->action = EDITOR_WORKSPACE_BROWSER_NONE;
    }
}

static void editor_mode_sprite_browser_open(void *opaque, EditorObjectId object) {
    EditorAnimationBrowserContext *context = opaque;
    if(context == NULL) return;
    *context->object = object;
    *context->action = EDITOR_WORKSPACE_BROWSER_ADD_SPRITE;
    if(!editor_file_browser_open(context->browser, EDITOR_FILE_BROWSER_OPEN_PNG,
            context->workspace->directory, context->font)) {
        *context->object = 0;
        *context->action = EDITOR_WORKSPACE_BROWSER_NONE;
    }
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
    if(viewport_state->selection == EDITOR_SELECTION_ANIMATION_FRAME) {
        EditorAnimatedSprite *animation = editor_project_animated_sprite_get(selected,
            viewport_state->selected_animated_sprite);
        if(animation == NULL) return false;
        for(size_t i = 0; i < animation->frame_count; i += 1) {
            if(animation->frames[i].id != viewport_state->selected_animation_frame)
                continue;
            EditorCommand command = {.type = EDITOR_COMMAND_ANIMATION_FRAME_REMOVE,
                .data.animation_frame_remove = {.object = selected->id,
                    .sprite = animation->id, .index = i}};
            if(editor_command_execute(project, &command).kind == ERROR_RESULT_ERROR)
                return false;
            viewport_state->mode = EDITOR_VIEWPORT_ANIMATED_SPRITE;
            viewport_state->selection = EDITOR_SELECTION_ANIMATED_SPRITE;
            viewport_state->selected_animation_frame = 0;
            return true;
        }
        return false;
    }
    if(viewport_state->selection == EDITOR_SELECTION_SPRITE) {
        EditorSprite *sprite = editor_project_sprite_get(selected,
            viewport_state->selected_sprite);
        EditorCommand command;
        if(sprite == NULL) return false;
        command = (EditorCommand){.type = EDITOR_COMMAND_SPRITE_REMOVE,
            .data.sprite_remove = {.object = selected->id,
                .sprite = sprite->id}};
        if(editor_command_execute(project, &command).kind == ERROR_RESULT_ERROR)
            return false;
        viewport_state->mode = EDITOR_VIEWPORT_OBJECT;
        viewport_state->selection = EDITOR_SELECTION_OBJECT;
        viewport_state->selected_sprite = 0;
        return true;
    }
    if(viewport_state->selection == EDITOR_SELECTION_ANIMATED_SPRITE) {
        EditorAnimatedSprite *sprite = editor_project_animated_sprite_get(selected,
            viewport_state->selected_animated_sprite);
        EditorCommand command;
        if(sprite == NULL) return false;
        command = (EditorCommand){.type = EDITOR_COMMAND_ANIMATED_SPRITE_REMOVE,
            .data.animated_sprite_remove = {.object = selected->id,
                .sprite = sprite->id}};
        if(editor_command_execute(project, &command).kind == ERROR_RESULT_ERROR)
            return false;
        viewport_state->mode = EDITOR_VIEWPORT_OBJECT;
        viewport_state->selection = EDITOR_SELECTION_OBJECT;
        viewport_state->selected_animated_sprite = 0;
        return true;
    }
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

static void editor_mode_hierarchy_row(void *opaque,
        EditorViewportState *viewport, EditorSelectionRef selection,
        UIRect bounds, UIButtonResult interaction, bool last) {
    EditorModeHierarchyContext *context = opaque;
    if(context == NULL) return;
    editor_hierarchy_drag_row(context->drag, viewport, selection, bounds,
        interaction, context->pointer, context->primary,
        context->scroll_offset, last);
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
    const char *startup_stage = "editor history initialization";
    KeyboardState keyboard = {0};
    MouseState mouse = {0};
    float viewport_wheel_y = 0.0f;
    FontAsset font = {0};
    FontAsset notification_font = {0};
    TextAsset file_label = {0};
    TextAsset edit_label = {0};
    TextAsset undo_label = {0};
    TextAsset redo_label = {0};
    TextAsset build_label = {0};
    TextAsset generate_c_label = {0};
    TextAsset compile_label = {0};
    TextAsset build_project_label = {0};
    TextAsset collision_label = {0};
    TextAsset particle_label = {0};
    TextAsset particle_ring_color_label = {0};
    TextAsset particle_fill_color_label = {0};
    TextAsset auto_fit_label = {0};
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
    TextAsset color_picker_hex_field = {0};
    TextAsset color_picker_opacity_field = {0};
    TextAsset opacity_label = {0};
    EditorColorPicker color_picker = {0};
    ViewportId viewport = 0;
    static EditorProject project;
    uint64_t saved_project_hash;
    EditorViewportState viewport_state = {0};
    EditorFileBrowser file_browser;
    EditorWorkspace workspace = {0};
    EditorAppStateMachine app_state;
    EditorProjectLauncherState project_launcher_state;
    EditorParticleEditor particle_editor = {0};
    EditorAnchorEditor anchor_editor = {0};
    EditorRigidBodyEditor rigid_body_editor = {0};
    EditorJointEditor joint_editor = {0};
    EditorVertexEditor vertex_editor = {0};
    EditorLineEditor line_editor = {0};
    EditorAutoShapeEditor auto_shape_editor = {0};
    EditorHitboxEditor hitbox_editor = {0};
    EditorAnimationFrameEditor animation_frame_editor = {0};
    EditorSpriteEditor sprite_editor = {0};
    EditorAnimatedSpriteEditor animated_sprite_editor = {0};
    EditorObjectEditor object_editor = {0};
    EditorHierarchyEditor hierarchy_editor = {0};
    EditorSoftBeamEditor soft_beam_editor = {0};
    EditorSoftNodeEditor soft_node_editor = {0};
    EditorSoftAreaEditor soft_area_editor = {0};
    EditorSoftBodyEditor soft_body_editor = {0};
    EditorCoordinateToggle coordinate_toggle = {0};
    EditorViewportContextMenu viewport_context_menu = {0};
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
    bool column_frame_multi_edit_open = false;
    EditorCloseAction close_action = EDITOR_CLOSE_NONE;
    float panel_scroll_offset = 0.0f;
    EditorViewportMode panel_scroll_mode = EDITOR_VIEWPORT_HIERARCHY;

    editor_operation_terminal = &terminal_panel;
    editor_operation_workspace = &workspace;
    editor_operation_project = &project;
    editor_operation_enabled = &terminal_editor_operations;
    editor_project_init(&project);
    editor_app_state_init(&app_state);
    if(!editor_history_init(&history, &project)) goto fail;
    editor_operation_history = &history;
    editor_command_executing_callback_set(editor_operation_command_executing, NULL);
    editor_command_executed_callback_set(editor_operation_command_write, NULL);
    editor_command_finished_callback_set(editor_operation_command_finished, NULL);
    saved_project_hash = editor_project_hash_get(&project);
    editor_viewport_state_init(&viewport_state);
    editor_navigation_state_apply(&project, &viewport_state, &project.navigation);
    editor_file_browser_init(&file_browser);
    startup_stage = "startup directory discovery";
    {
        char *directory = SDL_GetCurrentDirectory();
        if(directory == NULL || strlen(directory) >= sizeof(startup_directory)) {
            if(directory != NULL) SDL_free(directory);
            goto fail;
        }
        snprintf(startup_directory, sizeof(startup_directory), "%s", directory);
        SDL_free(directory);
    }

    startup_stage = "editor executable directory selection";
    if(!editor_use_executable_directory()) goto fail;
    startup_stage = "engine initialization";
    if(!editor_result_ok(rohr_engine_init())) goto fail;
    {
        startup_stage = "GUI state loading";
        EditorResult result = editor_gui_state_resolve(&gui_state,
            gui_state_path, sizeof(gui_state_path));
        if(editor_result_check(result)) {
            editor_result_stderr_print(result);
            goto fail;
        }
    }
    startup_stage = "graphics initialization";
    if(!editor_result_ok(rohr_graphics_start())) goto fail;
    startup_stage = "window presentation setup";
    if(!editor_gui_state_presentation_apply(&gui_state)) goto fail;
    startup_stage = "automatic aspect ratio setup";
    if(!rohr_graphics_aspect_ratio_auto_set(true)) goto fail;
    editor_window_layout_sync();
    {
        startup_stage = "editor viewport creation";
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
        startup_stage = "editor font loading";
        FontAssetResult result = rohr_graphics_font_load((FontDescriptor){
            .file = "assets/jetbrains_mono_bold_italic.ttf",
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
        startup_stage = "notification font loading";
        FontAssetResult result = rohr_graphics_font_load((FontDescriptor){
            .file = "assets/jetbrains_mono_bold_italic.ttf",
            .point_size = 9.0f
        });

        if(rohr_error_check(result)) {
            fprintf(stderr, "error %d: %s\n", (int)result.result.error,
                rohr_error_message_get(result));
            goto fail;
        }
        notification_font = result.result.value;
    }
    startup_stage = "editor controls creation";
    if(!editor_text_create(&font, "File", &file_label) ||
            !editor_text_create(&font, "Edit", &edit_label) ||
            !editor_text_create(&font, "Undo    Ctrl+Z", &undo_label) ||
            !editor_text_create(&font, "Redo    Ctrl+Y", &redo_label) ||
            !editor_text_create(&font, "Build", &build_label) ||
            !editor_text_create(&font, "Generate C", &generate_c_label) ||
            !editor_text_create(&font, "Compile", &compile_label) ||
            !editor_text_create(&font, "Build Project", &build_project_label) ||
            !editor_text_create(&font, "Collision", &collision_label) ||
            !editor_text_create(&font, "Particle", &particle_label) ||
            !editor_text_create(&font, "Ring Color", &particle_ring_color_label) ||
            !editor_text_create(&font, "Fill Color", &particle_fill_color_label) ||
            !editor_text_create(&font, "Auto Fit", &auto_fit_label) ||
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
            !editor_origin_panel_create(&origin_panel, &font) ||
            !editor_particle_editor_create(&particle_editor, &font) ||
            !editor_anchor_editor_create(&anchor_editor, &font) ||
            !editor_rigid_body_editor_create(&rigid_body_editor, &font) ||
            !editor_joint_editor_create(&joint_editor, &font) ||
            !editor_vertex_editor_create(&vertex_editor, &font) ||
            !editor_line_editor_create(&line_editor, &font) ||
            !editor_auto_shape_editor_create(&auto_shape_editor, &font) ||
            !editor_hitbox_editor_create(&hitbox_editor, &font) ||
            !editor_animation_frame_editor_create(&animation_frame_editor, &font) ||
            !editor_sprite_editor_create(&sprite_editor, &font) ||
            !editor_animated_sprite_editor_create(&animated_sprite_editor, &font) ||
            !editor_object_editor_create(&object_editor, &font) ||
            !editor_hierarchy_editor_create(&hierarchy_editor, &font) ||
            !editor_soft_beam_editor_create(&soft_beam_editor, &font) ||
            !editor_soft_node_editor_create(&soft_node_editor, &font) ||
            !editor_soft_area_editor_create(&soft_area_editor, &font) ||
            !editor_soft_body_editor_create(&soft_body_editor, &font) ||
            !editor_coordinate_toggle_create(&coordinate_toggle, &font) ||
            !editor_viewport_context_menu_create(&viewport_context_menu, &font) ||
            !editor_bulk_panel_create(&bulk_panel, &font) ||
            !editor_build_settings_panel_create(&build_settings_panel, &font) ||
            !editor_visual_settings_panel_create(&visual_settings_panel, &font) ||
            !editor_notification_panel_create(&notification_panel, &font,
                &notification_font) ||
            !editor_terminal_panel_create(&terminal_panel, &font)) goto fail;
    editor_project_launcher_state_init(
        &project_launcher_state, &new_label, &open_label);
    startup_stage = "visual settings initialization";
    if(!editor_visual_settings_panel_state_set(&visual_settings_panel,
            &gui_state, gui_state_path)) goto fail;
    terminal_panel.visible = true;
    if(!editor_terminal_panel_project_open(&terminal_panel, startup_directory)) {
        terminal_panel.visible = false;
        editor_notification_panel_push(&notification_panel, "Terminal - FAIL",
            "The embedded terminal could not start. The editor remains usable. "
            "Restart from a console for the platform error details.");
    }
    startup_stage = "color picker creation";
    if(!editor_text_create(&font, "#FFFFFFFF", &color_picker_hex_field) ||
            !editor_text_create(&font, "100.0", &color_picker_opacity_field) ||
            !editor_text_create(&font, "Opacity %", &opacity_label)) goto fail;

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
        editor_history_continuous_set(&history, field_editing);
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
        } else if((auto_shape_picker_open ||
                hitbox_editor.auto_shape_picker_open) &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            auto_shape_picker_open = false;
            hitbox_editor.auto_shape_picker_open = false;
        } else if((collision_category_open || collide_with_open) &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            collision_category_open = false;
            collide_with_open = false;
        } else if(color_picker.open &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            editor_color_picker_commit(&color_picker);
        } else if(column_frame_multi_edit_open &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            column_frame_multi_edit_open = false;
        } else if(!field_editing &&
                !editor_terminal_panel_focused_check(&terminal_panel) &&
                rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) {
            if(viewport_state.selected_item_count > 1) {
                editor_viewport_multi_selection_dismiss(&project, &viewport_state);
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
        bool frame_multi_selection = viewport_state.selected_item_count > 1;
        for(size_t i = 0; i < viewport_state.selected_item_count; i += 1)
            if(viewport_state.selected_items[i].kind !=
                    EDITOR_SELECTION_ANIMATION_FRAME) frame_multi_selection = false;
        if(!frame_multi_selection) column_frame_multi_edit_open = false;
        if(viewport_state.selected_item_count > 1 &&
                (!frame_multi_selection || column_frame_multi_edit_open)) {
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
            EditorModeColorContext color_context = {
                .picker = &color_picker, .project = &project};
            field_editing = editor_particle_editor_draw(&particle_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .color_open = editor_mode_color_picker_open,
                    .color_context = &color_context});
        } else if(viewport_state.mode == EDITOR_VIEWPORT_RIGID_BODY) {
            EditorModeColorContext color_context = {
                .picker = &color_picker, .project = &project};
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            EditorModeHierarchyContext hierarchy_context = {
                .drag = &hierarchy_drag, .pointer = hierarchy_pointer,
                .primary = hierarchy_primary,
                .scroll_offset = panel_scroll_offset};
            EditorCollisionMenuContext collision_context = {
                .font = &font, .labels = collision_mask_labels,
                .caches = collision_mask_cache, .name = collision_mask_name,
                .name_capacity = sizeof(collision_mask_name),
                .name_field = &collision_mask_name_field,
                .add_label = &add_label};
            field_editing = editor_rigid_body_editor_draw(&rigid_body_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .color_open = editor_mode_color_picker_open,
                    .color_context = &color_context,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_open_item = editor_mode_open_item_delete,
                    .delete_context = &delete_context,
                    .hierarchy_row = editor_mode_hierarchy_row,
                    .hierarchy_context = &hierarchy_context,
                    .primary_button = hierarchy_primary},
                editor_rigid_body_collision_menu_draw, &collision_context);
        } else if(viewport_state.mode == EDITOR_VIEWPORT_HITBOX) {
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            field_editing = editor_hitbox_editor_draw(&hitbox_editor,
                &auto_shape_editor, &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_open_item = editor_mode_open_item_delete,
                    .delete_context = &delete_context});
        } else if(viewport_state.mode == EDITOR_VIEWPORT_AUTO_SHAPE) {
            field_editing = editor_auto_shape_editor_draw(&auto_shape_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH});
        } else if(viewport_state.mode == EDITOR_VIEWPORT_VERTEX) {
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            field_editing = editor_vertex_editor_draw(&vertex_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_open_item = editor_mode_open_item_delete,
                    .delete_context = &delete_context});
        } else if(viewport_state.mode == EDITOR_VIEWPORT_LINE) {
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            field_editing = editor_line_editor_draw(&line_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_open_item = editor_mode_open_item_delete,
                    .delete_context = &delete_context});
        } else if(viewport_state.mode == EDITOR_VIEWPORT_JOINT) {
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            EditorModeHierarchyContext hierarchy_context = {
                .drag = &hierarchy_drag, .pointer = hierarchy_pointer,
                .primary = hierarchy_primary,
                .scroll_offset = panel_scroll_offset};
            field_editing = editor_joint_editor_draw(&joint_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_open_item = editor_mode_open_item_delete,
                    .delete_context = &delete_context,
                    .hierarchy_row = editor_mode_hierarchy_row,
                    .hierarchy_context = &hierarchy_context,
                    .primary_button = hierarchy_primary});
        } else if(viewport_state.mode == EDITOR_VIEWPORT_ANCHOR) {
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            field_editing = editor_anchor_editor_draw(&anchor_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_open_item = editor_mode_open_item_delete,
                    .delete_context = &delete_context});
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SOFT_BODY) {
            EditorModeColorContext color_context = {
                .picker = &color_picker, .project = &project};
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            EditorModeHierarchyContext hierarchy_context = {
                .drag = &hierarchy_drag, .pointer = hierarchy_pointer,
                .primary = hierarchy_primary,
                .scroll_offset = panel_scroll_offset};
            field_editing = editor_soft_body_editor_draw(&soft_body_editor,
                &auto_shape_editor, &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .color_open = editor_mode_color_picker_open,
                    .color_context = &color_context,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_open_item = editor_mode_open_item_delete,
                    .delete_context = &delete_context,
                    .hierarchy_row = editor_mode_hierarchy_row,
                    .hierarchy_context = &hierarchy_context,
                    .primary_button = hierarchy_primary});
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SOFT_NODE) {
            EditorModeColorContext color_context = {
                .picker = &color_picker, .project = &project};
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            EditorCollisionMenuContext collision_context = {
                .font = &font, .labels = collision_mask_labels,
                .caches = collision_mask_cache, .name = collision_mask_name,
                .name_capacity = sizeof(collision_mask_name),
                .name_field = &collision_mask_name_field,
                .add_label = &add_label};
            field_editing = editor_soft_node_editor_draw(&soft_node_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .color_open = editor_mode_color_picker_open,
                    .color_context = &color_context,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_open_item = editor_mode_open_item_delete,
                    .delete_context = &delete_context,
                    .primary_button = hierarchy_primary},
                editor_soft_node_collision_menu_draw, &collision_context);
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SOFT_BEAM) {
            EditorModeColorContext color_context = {
                .picker = &color_picker, .project = &project};
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            field_editing = editor_soft_beam_editor_draw(&soft_beam_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .color_open = editor_mode_color_picker_open,
                    .color_context = &color_context,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_open_item = editor_mode_open_item_delete,
                    .delete_context = &delete_context});
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SOFT_AREA) {
            EditorModeColorContext color_context = {
                .picker = &color_picker, .project = &project};
            field_editing = editor_soft_area_editor_draw(&soft_area_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .color_open = editor_mode_color_picker_open,
                    .local_color_open = editor_mode_local_color_picker_open,
                    .color_context = &color_context});
        } else if(viewport_state.mode == EDITOR_VIEWPORT_SPRITE) {
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            field_editing = editor_sprite_editor_draw(&sprite_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_open_item = editor_mode_open_item_delete,
                    .delete_context = &delete_context},
                editor_mode_rigid_body_preview, &viewport_state);
        } else if(viewport_state.mode == EDITOR_VIEWPORT_ANIMATION_FRAME) {
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            field_editing = editor_animation_frame_editor_draw(
                &animation_frame_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_context = &delete_context});
        } else if(viewport_state.mode == EDITOR_VIEWPORT_ANIMATED_SPRITE) {
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            EditorModeHierarchyContext hierarchy_context = {
                .drag = &hierarchy_drag, .pointer = hierarchy_pointer,
                .primary = hierarchy_primary,
                .scroll_offset = panel_scroll_offset};
            EditorAnimationBrowserContext browser_context = {
                .browser = &file_browser, .workspace = &workspace, .font = &font,
                .object = &sprite_browser_object,
                .sprite = &animation_browser_sprite,
                .action = &workspace_browser_action};
            bool additive_selection =
                rohr_controller_key_down_get(&keyboard, SDLK_LCTRL) ||
                rohr_controller_key_down_get(&keyboard, SDLK_RCTRL);
            field_editing = editor_animated_sprite_editor_draw(
                &animated_sprite_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_open_item = editor_mode_open_item_delete,
                    .delete_context = &delete_context,
                    .hierarchy_row = editor_mode_hierarchy_row,
                    .hierarchy_context = &hierarchy_context,
                    .primary_button = hierarchy_primary},
                editor_mode_rigid_body_preview, &viewport_state,
                editor_mode_animation_frame_browser_open, &browser_context,
                additive_selection, &column_frame_multi_edit_open);
        } else if(viewport_state.mode == EDITOR_VIEWPORT_OBJECT) {
            EditorModeDeleteContext delete_context = {
                .project = &project, .viewport = &viewport_state};
            EditorModeHierarchyContext hierarchy_context = {
                .drag = &hierarchy_drag, .pointer = hierarchy_pointer,
                .primary = hierarchy_primary,
                .scroll_offset = panel_scroll_offset};
            EditorAnimationBrowserContext browser_context = {
                .browser = &file_browser, .workspace = &workspace, .font = &font,
                .object = &sprite_browser_object,
                .sprite = &animation_browser_sprite,
                .action = &workspace_browser_action};
            bool additive_selection =
                rohr_controller_key_down_get(&keyboard, SDLK_LCTRL) ||
                rohr_controller_key_down_get(&keyboard, SDLK_RCTRL);
            field_editing = editor_object_editor_draw(&object_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .delete_y_get = editor_mode_delete_y_get,
                    .delete_open_item = editor_mode_open_item_delete,
                    .delete_context = &delete_context,
                    .hierarchy_row = editor_mode_hierarchy_row,
                    .hierarchy_context = &hierarchy_context,
                    .primary_button = hierarchy_primary},
                editor_mode_sprite_browser_open, &browser_context,
                additive_selection);
        } else {
            EditorModeHierarchyContext hierarchy_context = {
                .drag = &hierarchy_drag, .pointer = hierarchy_pointer,
                .primary = hierarchy_primary,
                .scroll_offset = panel_scroll_offset};
            editor_hierarchy_editor_draw(&hierarchy_editor,
                &(EditorModeContext){.project = &project,
                    .viewport = &viewport_state, .x = EDITOR_VIEWPORT_WIDTH,
                    .width = EDITOR_TOOLS_WIDTH,
                    .hierarchy_row = editor_mode_hierarchy_row,
                    .hierarchy_context = &hierarchy_context,
                    .primary_button = hierarchy_primary});
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
        editor_coordinate_toggle_draw(&coordinate_toggle, &project,
            &viewport_state, EDITOR_MENU_HEIGHT);
        editor_viewport_context_menu_draw(&viewport_context_menu, &mouse,
            EDITOR_VIEWPORT_WIDTH, EDITOR_MENU_HEIGHT, EDITOR_VIEWPORT_BOTTOM,
            EDITOR_WINDOW_HEIGHT);
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
                    editor_viewport_context_menu_close(&viewport_context_menu);
                    auto_shape_picker_open = false;
                    hitbox_editor.auto_shape_picker_open = false;
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
                if(workspace.open) {
                    EditorResult result = editor_workspace_operation_execute(
                        &workspace, &project, &command);
                    if(editor_result_check(result)) {
                        editor_result_stderr_print(result);
                        editor_error_notification_failure(&notification_panel,
                            "Save project", result);
                    } else {
                        saved_project_hash = editor_project_hash_get(&project);
                    }
                }
            } else if(file_menu.changed && file_menu.selected_index == 3) {
                if(editor_project_hash_get(&project) == saved_project_hash) {
                    editor_workspace_close(&workspace, &project);
                    editor_app_state_transition(&app_state,
                        EDITOR_APP_STATE_PROJECT_LAUNCHER);
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
                    if(editor_result_check(result)) {
                        editor_result_stderr_print(result);
                        editor_error_notification_failure(&notification_panel,
                            "Visual settings", result);
                    }
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
                if(editor_result_check(result)) {
                    editor_result_stderr_print(result);
                    editor_error_notification_failure(&notification_panel,
                        "Build settings", result);
                }
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
                EditorResult result;
                snprintf(command.directory, sizeof(command.directory), "%s",
                    workspace.directory);
                result = editor_workspace_operation_execute(
                    &workspace, &project, &command);
                if(!editor_result_check(result)) {
                    if(close_action == EDITOR_CLOSE_PROGRAM) {
                        running = false;
                    } else {
                        editor_workspace_close(&workspace, &project);
                        editor_app_state_transition(&app_state,
                            EDITOR_APP_STATE_PROJECT_LAUNCHER);
                        (void)editor_terminal_panel_project_open(
                            &terminal_panel, startup_directory);
                        editor_history_reset(&history);
                        saved_project_hash = editor_project_hash_get(&project);
                        editor_viewport_state_init(&viewport_state);
                        panel_scroll_offset = 0.0f;
                    }
                    close_action = EDITOR_CLOSE_NONE;
                } else {
                    editor_result_stderr_print(result);
                    editor_error_notification_failure(&notification_panel,
                        "Save project", result);
                }
            }
            if(rohr_ui_button("editor.close.dont_save", &dont_save_label,
                    (UIRect){dialog.x + 148.0f, dialog.y + 102.0f,
                        140.0f, 36.0f}, NULL).clicked) {
                if(close_action == EDITOR_CLOSE_PROGRAM) {
                    running = false;
                } else {
                    editor_workspace_close(&workspace, &project);
                    editor_app_state_transition(&app_state,
                        EDITOR_APP_STATE_PROJECT_LAUNCHER);
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
        if(editor_app_state_get(&app_state) ==
                EDITOR_APP_STATE_PROJECT_LAUNCHER && !file_browser.active) {
            EditorProjectLauncherRequest request =
                editor_project_launcher_state_draw(&project_launcher_state,
                    editor_window_width, EDITOR_VIEWPORT_BOTTOM);
            if(request == EDITOR_PROJECT_LAUNCHER_REQUEST_NEW_PROJECT) {
                workspace_browser_action = EDITOR_WORKSPACE_BROWSER_NEW;
                (void)editor_file_browser_open(&file_browser,
                    EDITOR_FILE_BROWSER_CREATE_DIRECTORY, startup_directory, &font);
            } else if(request == EDITOR_PROJECT_LAUNCHER_REQUEST_LOAD_PROJECT) {
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
                    opened = sprite_browser_object != EDITOR_OBJECT_INVALID &&
                        animation_browser_sprite != 0 &&
                        browser_result.selected_count > 0;
                    if(opened) {
                        EditorObject *frame_object = editor_project_selected_get(&project);
                        EditorAnimatedSprite *animation = frame_object == NULL ||
                            frame_object->id != sprite_browser_object ? NULL :
                            editor_project_animated_sprite_get(frame_object,
                                animation_browser_sprite);
                        size_t starting_count = animation == NULL ? 0 :
                            animation->frame_count;
                        if(animation == NULL || !editor_history_transaction_begin(&history) ||
                                !editor_history_transaction_object_track(&history,
                                    sprite_browser_object)) {
                            load_result = editor_result_error(EDITOR_ERROR_CAPACITY,
                                "Could not start animation frame import");
                            opened = false;
                        }
                        for(size_t i = 0; opened &&
                                i < browser_result.selected_count; i += 1) {
                            char selected_path[EDITOR_FILE_BROWSER_PATH_MAX +
                                EDITOR_FILE_BROWSER_NAME_MAX];
                            const char *relative_path;
                            EditorCommand add_command = {
                                .type = EDITOR_COMMAND_ANIMATION_FRAME_ADD,
                                .data.animation_frame_add = {
                                    .object = sprite_browser_object,
                                    .sprite = animation_browser_sprite,
                                    .size = {64.0f, 64.0f}}};
                            EditorCommandResult added;
                            if(!editor_file_browser_selected_path_get(&file_browser, i,
                                    selected_path, sizeof(selected_path))) {
                                load_result = editor_result_error(EDITOR_ERROR_NOT_FOUND,
                                    "Selected animation frame path was not found");
                                opened = false;
                                break;
                            }
                            relative_path = editor_project_relative_path_get(
                                workspace.directory, selected_path);
                            if(strlen(relative_path) >=
                                    sizeof(add_command.data.animation_frame_add.path)) {
                                load_result = editor_result_error(
                                    EDITOR_ERROR_INVALID_ARGUMENT,
                                    "Animation frame path is too long: %s", selected_path);
                                opened = false;
                                break;
                            }
                            snprintf(add_command.data.animation_frame_add.name,
                                sizeof(add_command.data.animation_frame_add.name),
                                "frame_%zu", starting_count + i + 1);
                            snprintf(add_command.data.animation_frame_add.path,
                                sizeof(add_command.data.animation_frame_add.path), "%s",
                                relative_path);
                            added = editor_command_execute(&project, &add_command);
                            if(added.kind == ERROR_RESULT_ERROR) {
                                load_result.kind = ERROR_RESULT_ERROR;
                                load_result.result.error = added.result.error;
                                opened = false;
                            }
                        }
                        if(opened) opened = editor_history_transaction_end(&history);
                        else editor_history_transaction_cancel(&history);
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
                        editor_app_state_transition(&app_state,
                            EDITOR_APP_STATE_WORKSPACE);
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
                    editor_error_notification_failure(&notification_panel,
                        workspace_browser_action == EDITOR_WORKSPACE_BROWSER_NEW ?
                            "Create project" :
                        workspace_browser_action ==
                                EDITOR_WORKSPACE_BROWSER_ADD_ANIMATION_FRAME ?
                            "Load frame" :
                        workspace_browser_action == EDITOR_WORKSPACE_BROWSER_ADD_SPRITE ?
                            "Load sprite" : "Load project",
                        load_result);
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
            if((auto_shape_picker_open || hitbox_editor.auto_shape_picker_open) &&
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
                    hitbox_editor.auto_shape_picker_open = false;
            }
            bool ui_consumed = !workspace.open || file_browser.active ||
                close_action != EDITOR_CLOSE_NONE ||
                editor_viewport_context_menu_open_check(&viewport_context_menu) ||
                color_picker.open ||
                rohr_ui_pointer_consumed_get() ||
                pointer.y < EDITOR_MENU_HEIGHT ||
                pointer.y >= EDITOR_VIEWPORT_BOTTOM;
            bool viewport_consumed;
            bool transform_before =
                editor_viewport_transform_active_check(&viewport_state);
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
                    &viewport_state, &project, &auto_shape_editor.config, pointer,
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
            (void)editor_navigation_viewport_transform_history_update(
                &project, &viewport_state, &history, transform_before);

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
                    if(selection.kind == EDITOR_SELECTION_SPRITE ||
                            selection.kind == EDITOR_SELECTION_ANIMATED_SPRITE)
                        (void)editor_navigation_selected_open(
                            &project, &viewport_state);
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
            bool selection_changed = navigation_before.mode !=
                    navigation_after.mode ||
                navigation_before.selection !=
                    navigation_after.selection ||
                navigation_before.object != navigation_after.object ||
                navigation_before.rigid_body != navigation_after.rigid_body ||
                navigation_before.hitbox != navigation_after.hitbox ||
                navigation_before.joint != navigation_after.joint ||
                navigation_before.anchor != navigation_after.anchor ||
                navigation_before.soft_body != navigation_after.soft_body ||
                navigation_before.soft_node != navigation_after.soft_node ||
                navigation_before.soft_beam != navigation_after.soft_beam ||
                navigation_before.sprite != navigation_after.sprite ||
                navigation_before.animated_sprite !=
                    navigation_after.animated_sprite ||
                navigation_before.animation_frame !=
                    navigation_after.animation_frame ||
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
        editor_history_continuous_set(&history, field_editing);
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
    editor_particle_editor_destroy(&particle_editor);
    editor_anchor_editor_destroy(&anchor_editor);
    editor_rigid_body_editor_destroy(&rigid_body_editor);
    editor_joint_editor_destroy(&joint_editor);
    editor_vertex_editor_destroy(&vertex_editor);
    editor_line_editor_destroy(&line_editor);
    editor_auto_shape_editor_destroy(&auto_shape_editor);
    editor_hitbox_editor_destroy(&hitbox_editor);
    editor_animation_frame_editor_destroy(&animation_frame_editor);
    editor_sprite_editor_destroy(&sprite_editor);
    editor_animated_sprite_editor_destroy(&animated_sprite_editor);
    editor_object_editor_destroy(&object_editor);
    editor_hierarchy_editor_destroy(&hierarchy_editor);
    editor_soft_beam_editor_destroy(&soft_beam_editor);
    editor_soft_node_editor_destroy(&soft_node_editor);
    editor_soft_area_editor_destroy(&soft_area_editor);
    editor_soft_body_editor_destroy(&soft_body_editor);
    editor_coordinate_toggle_destroy(&coordinate_toggle);
    editor_viewport_context_menu_destroy(&viewport_context_menu);
    rohr_graphics_text_destroy(&color_picker_hex_field);
    rohr_graphics_text_destroy(&color_picker_opacity_field);
    rohr_graphics_text_destroy(&opacity_label);
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
    rohr_graphics_text_destroy(&collision_mask_name_field);
    rohr_graphics_text_destroy(&add_label);
    rohr_graphics_text_destroy(&collision_category_label);
    rohr_graphics_text_destroy(&collide_with_label);
    rohr_graphics_text_destroy(&collision_label);
    rohr_graphics_text_destroy(&particle_label);
    rohr_graphics_text_destroy(&particle_ring_color_label);
    rohr_graphics_text_destroy(&particle_fill_color_label);
    rohr_graphics_text_destroy(&auto_fit_label);
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
    editor_startup_failure_report(startup_stage);
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
    editor_particle_editor_destroy(&particle_editor);
    editor_anchor_editor_destroy(&anchor_editor);
    editor_rigid_body_editor_destroy(&rigid_body_editor);
    editor_joint_editor_destroy(&joint_editor);
    editor_vertex_editor_destroy(&vertex_editor);
    editor_line_editor_destroy(&line_editor);
    editor_auto_shape_editor_destroy(&auto_shape_editor);
    editor_hitbox_editor_destroy(&hitbox_editor);
    editor_animation_frame_editor_destroy(&animation_frame_editor);
    editor_sprite_editor_destroy(&sprite_editor);
    editor_animated_sprite_editor_destroy(&animated_sprite_editor);
    editor_object_editor_destroy(&object_editor);
    editor_hierarchy_editor_destroy(&hierarchy_editor);
    editor_soft_beam_editor_destroy(&soft_beam_editor);
    editor_soft_node_editor_destroy(&soft_node_editor);
    editor_soft_area_editor_destroy(&soft_area_editor);
    editor_soft_body_editor_destroy(&soft_body_editor);
    editor_coordinate_toggle_destroy(&coordinate_toggle);
    editor_viewport_context_menu_destroy(&viewport_context_menu);
    rohr_graphics_text_destroy(&color_picker_hex_field);
    rohr_graphics_text_destroy(&color_picker_opacity_field);
    rohr_graphics_text_destroy(&opacity_label);
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
    rohr_graphics_text_destroy(&collision_mask_name_field);
    rohr_graphics_text_destroy(&add_label);
    rohr_graphics_text_destroy(&collision_category_label);
    rohr_graphics_text_destroy(&collide_with_label);
    rohr_graphics_text_destroy(&collision_label);
    rohr_graphics_text_destroy(&particle_label);
    rohr_graphics_text_destroy(&particle_ring_color_label);
    rohr_graphics_text_destroy(&particle_fill_color_label);
    rohr_graphics_text_destroy(&auto_fit_label);
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
