#include "editor_workspace.h"

#include "yyjson.h"

#include <stdio.h>
#include <string.h>

static bool editor_workspace_path_join(char *output, size_t capacity,
    const char *directory, const char *relative) {
    size_t length;

    if(output == NULL || capacity == 0 || directory == NULL || relative == NULL) {
        return false;
    }
    length = strlen(directory);
    return snprintf(output, capacity, "%s%s%s", directory,
        length > 0 && directory[length - 1] != '/' ? "/" : "", relative) <
        (int)capacity;
}

static const char *editor_workspace_basename(const char *path) {
    const char *name = path;

    if(path == NULL) return "Game";
    for(const char *cursor = path; *cursor != '\0'; cursor += 1) {
        if(*cursor == '/' || *cursor == '\\') name = cursor + 1;
    }
    return name[0] == '\0' ? "Game" : name;
}

static bool editor_workspace_file_write(const char *path, const char *contents) {
    FILE *file;
    size_t length;

    if(path == NULL || contents == NULL) return false;
    file = fopen(path, "wb");
    if(file == NULL) return false;
    length = strlen(contents);
    if(fwrite(contents, 1, length, file) != length) {
        fclose(file);
        return false;
    }
    return fclose(file) == 0;
}

static bool editor_workspace_directory_create(const char *root, const char *relative) {
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];

    return editor_workspace_path_join(path, sizeof(path), root, relative) &&
        SDL_CreateDirectory(path);
}

static SDL_EnumerationResult SDLCALL editor_workspace_not_empty(void *userdata,
    const char *dirname, const char *filename) {
    bool *empty = userdata;

    (void)dirname;
    (void)filename;
    if(empty != NULL) *empty = false;
    return SDL_ENUM_SUCCESS;
}

static bool editor_workspace_directory_empty(const char *directory) {
    bool empty = true;

    return directory != NULL &&
        SDL_EnumerateDirectory(directory, editor_workspace_not_empty, &empty) && empty;
}

EditorWorkspaceConfig editor_workspace_config_default_get(void) {
    EditorWorkspaceConfig config = {
        .format_version = EDITOR_WORKSPACE_FORMAT_VERSION
    };

    snprintf(config.name, sizeof(config.name), "Game");
    snprintf(config.source_directory, sizeof(config.source_directory), "src");
    snprintf(config.generated_directory, sizeof(config.generated_directory),
        "src/generated");
    snprintf(config.asset_directory, sizeof(config.asset_directory), "assets");
    snprintf(config.object_directory, sizeof(config.object_directory), "objects");
    snprintf(config.editor_state_file, sizeof(config.editor_state_file),
        "objects/project.rohr.json");
    return config;
}

static bool editor_workspace_manifest_save(const EditorWorkspace *workspace) {
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    bool success;

    if(workspace == NULL || !workspace->open ||
            !editor_workspace_path_join(path, sizeof(path), workspace->directory,
                "project.rohr.json")) return false;
    document = yyjson_mut_doc_new(NULL);
    if(document == NULL) return false;
    root = yyjson_mut_obj(document);
    yyjson_mut_doc_set_root(document, root);
    yyjson_mut_obj_add_uint(document, root, "format_version",
        workspace->config.format_version);
    yyjson_mut_obj_add_strcpy(document, root, "name", workspace->config.name);
    yyjson_mut_obj_add_strcpy(document, root, "source_directory",
        workspace->config.source_directory);
    yyjson_mut_obj_add_strcpy(document, root, "generated_directory",
        workspace->config.generated_directory);
    yyjson_mut_obj_add_strcpy(document, root, "asset_directory",
        workspace->config.asset_directory);
    yyjson_mut_obj_add_strcpy(document, root, "object_directory",
        workspace->config.object_directory);
    yyjson_mut_obj_add_strcpy(document, root, "editor_state_file",
        workspace->config.editor_state_file);
    yyjson_mut_obj_add_strcpy(document, root, "engine_root",
        workspace->config.engine_root);
    success = yyjson_mut_write_file(path, document, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(document);
    return success;
}

static bool editor_workspace_json_string(yyjson_val *root, const char *key,
    char *output, size_t capacity) {
    yyjson_val *value = yyjson_obj_get(root, key);
    size_t length;

    if(!yyjson_is_str(value) || output == NULL || capacity == 0) return false;
    length = yyjson_get_len(value);
    if(length == 0 || length >= capacity) return false;
    memcpy(output, yyjson_get_str(value), length + 1);
    return true;
}

static bool editor_workspace_manifest_load(EditorWorkspace *workspace,
    const char *directory) {
    yyjson_doc *document;
    yyjson_val *root;
    yyjson_val *version;
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    bool success = false;

    if(workspace == NULL || directory == NULL ||
            !editor_workspace_path_join(path, sizeof(path), directory,
                "project.rohr.json")) return false;
    document = yyjson_read_file(path, 0, NULL, NULL);
    if(document == NULL) return false;
    root = yyjson_doc_get_root(document);
    version = yyjson_obj_get(root, "format_version");
    if(!yyjson_is_obj(root) || !yyjson_is_uint(version) ||
            yyjson_get_uint(version) != EDITOR_WORKSPACE_FORMAT_VERSION) goto done;
    workspace->config = editor_workspace_config_default_get();
    workspace->config.format_version = (uint32_t)yyjson_get_uint(version);
    if(!editor_workspace_json_string(root, "name", workspace->config.name,
                sizeof(workspace->config.name)) ||
            !editor_workspace_json_string(root, "source_directory",
                workspace->config.source_directory,
                sizeof(workspace->config.source_directory)) ||
            !editor_workspace_json_string(root, "generated_directory",
                workspace->config.generated_directory,
                sizeof(workspace->config.generated_directory)) ||
            !editor_workspace_json_string(root, "asset_directory",
                workspace->config.asset_directory,
                sizeof(workspace->config.asset_directory)) ||
            !editor_workspace_json_string(root, "object_directory",
                workspace->config.object_directory,
                sizeof(workspace->config.object_directory)) ||
            !editor_workspace_json_string(root, "editor_state_file",
                workspace->config.editor_state_file,
                sizeof(workspace->config.editor_state_file)) ||
            !editor_workspace_json_string(root, "engine_root",
                workspace->config.engine_root,
                sizeof(workspace->config.engine_root)) ||
            strlen(directory) >= sizeof(workspace->directory)) goto done;
    snprintf(workspace->directory, sizeof(workspace->directory), "%s", directory);
    workspace->open = true;
    success = true;
done:
    yyjson_doc_free(document);
    return success;
}

static bool editor_workspace_scaffold_write(const EditorWorkspace *workspace) {
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    char cmake[EDITOR_WORKSPACE_PATH_MAX * 2 + 1024];
    static const char *main_source =
        "#include \"rohr.h\"\n\n"
        "#include <stdio.h>\n\n"
        "static bool ok(EngineResult result) {\n"
        "    if(!rohr_error_check(result)) return true;\n"
        "    rohr_error_stderr_print(result.result.error);\n"
        "    return false;\n"
        "}\n\n"
        "static Entity body_create(Position position, Vec2D size, bool dynamic) {\n"
        "    EntityResult added = rohr_entity_add();\n"
        "    Entity body;\n"
        "    if(rohr_error_check(added)) return ENTITY_INVALID;\n"
        "    body = added.result.value;\n"
        "    if(!ok(rohr_physics_position_set(body, position)) ||\n"
        "            !ok(rohr_physics_orientation_set(body, 0.0f)) ||\n"
        "            !ok(rohr_physics_hitbox_set(body,\n"
        "                rohr_math_square_create(size.x, size.y))) ||\n"
        "            !ok(rohr_physics_collision_with_all_set(body))) return ENTITY_INVALID;\n"
        "    if(!dynamic) return ok(rohr_physics_static_set(body)) ? body : ENTITY_INVALID;\n"
        "    if(!ok(rohr_physics_mass_set(body, 1.0f)) ||\n"
        "            !ok(rohr_physics_velocity_set(body, (Velocity){0})) ||\n"
        "            !ok(rohr_physics_dynamic_set(body)) ||\n"
        "            !ok(rohr_physics_gravity_enable(body))) return ENTITY_INVALID;\n"
        "    return body;\n"
        "}\n\n"
        "int main(void) {\n"
        "    KeyboardState keyboard = {0};\n"
        "    Entity floor;\n"
        "    Entity box;\n"
        "    if(!ok(rohr_engine_init()) || !ok(rohr_graphics_start()) ||\n"
        "            !ok(rohr_physics_gravity_set((Acceleration){0.0f, 900.0f}))) goto fail;\n"
        "    floor = body_create((Position){0.0f, 200.0f}, (Vec2D){600.0f, 40.0f}, false);\n"
        "    box = body_create((Position){0.0f, -120.0f}, (Vec2D){50.0f, 50.0f}, true);\n"
        "    if(floor == ENTITY_INVALID || box == ENTITY_INVALID) goto fail;\n"
        "    while(true) {\n"
        "        SDL_Event event;\n"
        "        rohr_controller_key_states_update(&keyboard);\n"
        "        while((event = rohr_engine_event_poll()).type != 0) {\n"
        "            rohr_controller_key_event_add(&keyboard,\n"
        "                rohr_controller_keyboard_event_capture(&event));\n"
        "            if(event.type == SDL_EVENT_QUIT) goto done;\n"
        "        }\n"
        "        if(rohr_controller_key_pressed_get(&keyboard, SDLK_ESCAPE)) break;\n"
        "        rohr_physics_update(rohr_system_tick_update());\n"
        "        rohr_graphics_background_draw((Color){18, 22, 30, 255});\n"
        "        rohr_graphics_hit_box_colored_draw(floor, GRAPHICS_FILLED,\n"
        "            (Color){90, 100, 115, 255});\n"
        "        rohr_graphics_hit_box_colored_draw(box, GRAPHICS_FILLED,\n"
        "            (Color){70, 170, 255, 255});\n"
        "        rohr_graphics_show();\n"
        "    }\n"
        "done:\n"
        "    rohr_graphics_end();\n"
        "    rohr_engine_shutdown();\n"
        "    return 0;\n"
        "fail:\n"
        "    fprintf(stderr, \"Game initialization failed\\n\");\n"
        "    rohr_graphics_end();\n"
        "    rohr_engine_shutdown();\n"
        "    return 1;\n"
        "}\n";
    static const char *gitignore = "build/\n";

    snprintf(cmake, sizeof(cmake),
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(%s LANGUAGES C)\n\n"
        "set(CMAKE_C_STANDARD 99)\n"
        "set(CMAKE_C_STANDARD_REQUIRED ON)\n"
        "set(ROHR_ENGINE_ROOT \"%s\" CACHE PATH \"Rohr Engine source directory\")\n"
        "set(ROHR_BUILD_EXAMPLES OFF CACHE BOOL \"\" FORCE)\n"
        "set(ROHR_BUILD_TESTS OFF CACHE BOOL \"\" FORCE)\n"
        "set(ROHR_BUILD_EDITOR OFF CACHE BOOL \"\" FORCE)\n"
        "add_subdirectory(\"${ROHR_ENGINE_ROOT}\" rohr-engine)\n\n"
        "file(GLOB ROHR_GENERATED_SOURCES CONFIGURE_DEPENDS src/generated/*.c)\n"
        "add_executable(%s src/main.c ${ROHR_GENERATED_SOURCES})\n"
        "target_include_directories(%s PRIVATE src/generated)\n"
        "target_link_libraries(%s PRIVATE rohr_engine)\n",
        workspace->config.name, workspace->config.engine_root,
        workspace->config.name, workspace->config.name, workspace->config.name);
    return editor_workspace_path_join(path, sizeof(path), workspace->directory,
            "CMakeLists.txt") && editor_workspace_file_write(path, cmake) &&
        editor_workspace_path_join(path, sizeof(path), workspace->directory,
            "src/main.c") && editor_workspace_file_write(path, main_source) &&
        editor_workspace_path_join(path, sizeof(path), workspace->directory,
            ".gitignore") && editor_workspace_file_write(path, gitignore);
}

bool editor_workspace_save(const EditorWorkspace *workspace,
    const EditorProject *project) {
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];

    return workspace != NULL && workspace->open && project != NULL &&
        editor_workspace_manifest_save(workspace) &&
        editor_workspace_path_join(path, sizeof(path), workspace->directory,
            workspace->config.editor_state_file) && editor_project_save(project, path);
}

bool editor_workspace_create(EditorWorkspace *workspace, EditorProject *project,
    const char *directory, const char *engine_root) {
    EditorWorkspace created = {0};

    if(workspace == NULL || project == NULL || directory == NULL || directory[0] == '\0' ||
            engine_root == NULL || engine_root[0] == '\0' ||
            strlen(directory) >= sizeof(created.directory) ||
            strlen(engine_root) >= sizeof(created.config.engine_root) ||
            !SDL_CreateDirectory(directory) ||
            !editor_workspace_directory_empty(directory)) return false;
    created.config = editor_workspace_config_default_get();
    editor_project_object_name_format(created.config.name,
        sizeof(created.config.name), editor_workspace_basename(directory));
    snprintf(created.config.engine_root, sizeof(created.config.engine_root), "%s",
        engine_root);
    snprintf(created.directory, sizeof(created.directory), "%s", directory);
    created.open = true;
    if(!editor_workspace_directory_create(directory, created.config.source_directory) ||
            !editor_workspace_directory_create(directory,
                created.config.generated_directory) ||
            !editor_workspace_directory_create(directory, created.config.asset_directory) ||
            !editor_workspace_directory_create(directory, created.config.object_directory)) {
        return false;
    }
    editor_project_init(project);
    if(!editor_workspace_save(&created, project) ||
            !editor_workspace_scaffold_write(&created)) return false;
    *workspace = created;
    return true;
}

bool editor_workspace_load(EditorWorkspace *workspace, EditorProject *project,
    const char *directory) {
    EditorWorkspace loaded = {0};
    static EditorProject loaded_project;
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];

    if(workspace == NULL || project == NULL ||
            !editor_workspace_manifest_load(&loaded, directory) ||
            !editor_workspace_path_join(path, sizeof(path), directory,
                loaded.config.editor_state_file) ||
            !editor_project_load(&loaded_project, path)) return false;
    *workspace = loaded;
    *project = loaded_project;
    return true;
}

void editor_workspace_close(EditorWorkspace *workspace, EditorProject *project) {
    if(workspace != NULL) *workspace = (EditorWorkspace){0};
    editor_project_init(project);
}
