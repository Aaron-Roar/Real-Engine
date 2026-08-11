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

static void editor_workspace_hitbox_rectangle_set(EditorProject *project,
    EditorHitbox *hitbox, float width, float height) {
    Position vertices[4] = {
        {width * 0.5f, height * 0.5f},
        {width * 0.5f, height * -0.5f},
        {width * -0.5f, height * -0.5f},
        {width * -0.5f, height * 0.5f}
    };

    if(project == NULL || hitbox == NULL) return;
    if(hitbox->vertex_count < 4) {
        hitbox->vertices[3].id = project->next_vertex_id++;
    }
    hitbox->vertex_count = 4;
    for(uint32_t i = 0; i < 4; i += 1) {
        hitbox->vertices[i].position = vertices[i];
        snprintf(hitbox->vertices[i].name, sizeof(hitbox->vertices[i].name),
            "vertex_%u", i + 1);
        snprintf(hitbox->line_names[i], sizeof(hitbox->line_names[i]),
            "line_%u", i + 1);
    }
}

static bool editor_workspace_starter_project_init(EditorProject *project) {
    EditorObject *floor;
    EditorObject *box;
    EditorRigidBody *floor_body;
    EditorRigidBody *box_body;

    if(project == NULL) return false;
    editor_project_init(project);
    floor = editor_project_object_add(project, (Position){0.0f, -200.0f});
    if(floor == NULL) return false;
    snprintf(floor->name, sizeof(floor->name), "Floor");
    floor_body = editor_project_rigid_body_add(project, floor);
    if(floor_body == NULL) return false;
    snprintf(floor_body->name, sizeof(floor_body->name), "floor");
    floor_body->static_body = true;
    floor_body->friction = 0.5f;
    floor_body->restitution = 0.0f;
    editor_workspace_hitbox_rectangle_set(project, &floor_body->hitboxes[0],
        600.0f, 40.0f);

    box = editor_project_object_add(project, (Position){0.0f, 120.0f});
    if(box == NULL) return false;
    snprintf(box->name, sizeof(box->name), "Box");
    box_body = editor_project_rigid_body_add(project, box);
    if(box_body == NULL) return false;
    snprintf(box_body->name, sizeof(box_body->name), "box");
    box_body->mass_value = 5.0f;
    box_body->friction = 0.5f;
    box_body->restitution = 0.0f;
    box_body->gravity_enabled = true;
    editor_workspace_hitbox_rectangle_set(project, &box_body->hitboxes[0],
        50.0f, 50.0f);
    editor_project_selection_clear(project);
    return true;
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

static bool editor_workspace_generated_objects_write(const EditorWorkspace *workspace,
    const EditorProject *project) {
    char header_path[EDITOR_WORKSPACE_PATH_MAX * 2];
    char source_path[EDITOR_WORKSPACE_PATH_MAX * 2];
    FILE *header;
    FILE *source;

    if(workspace == NULL || project == NULL ||
            !editor_workspace_path_join(header_path, sizeof(header_path),
                workspace->directory, "src/generated/project_objects.h") ||
            !editor_workspace_path_join(source_path, sizeof(source_path),
                workspace->directory, "src/generated/project_objects.c")) return false;
    header = fopen(header_path, "wb");
    if(header == NULL) return false;
    source = fopen(source_path, "wb");
    if(source == NULL) {
        fclose(header);
        return false;
    }
    fprintf(header,
        "#ifndef ROHR_GENERATED_PROJECT_OBJECTS_H\n"
        "#define ROHR_GENERATED_PROJECT_OBJECTS_H\n\n"
        "#include \"rohr.h\"\n\n");
    fprintf(source,
        "#include \"project_objects.h\"\n\n"
        "static EngineResult generated_body_create(Entity *output, Position position,\n"
        "    float rotation, Shape hitbox, float mass_value, float friction,\n"
        "    float restitution, bool static_body, bool rotation_locked,\n"
        "    bool gravity_enabled, bool collision_enabled,\n"
        "    RohrCollisionCategoryMask collision_category,\n"
        "    RohrCollisionCategoryMask collision_with) {\n"
        "    EntityResult added = rohr_entity_add();\n"
        "    EngineResult result;\n"
        "    if(rohr_error_check(added)) return rohr_error_result_error(added.result.error);\n"
        "    *output = added.result.value;\n"
        "#define GENERATED_APPLY(call) do { result = (call); if(rohr_error_check(result)) "
        "goto fail; } while(0)\n"
        "    GENERATED_APPLY(rohr_physics_position_set(*output, position));\n"
        "    GENERATED_APPLY(rohr_physics_orientation_set(*output, rotation));\n"
        "    GENERATED_APPLY(rohr_physics_hitbox_set(*output, hitbox));\n"
        "    GENERATED_APPLY(rohr_physics_collision_category_set(*output,\n"
        "        collision_enabled ? collision_category : ROHR_COLLISION_CATEGORY_NONE));\n"
        "    GENERATED_APPLY(rohr_physics_collision_with_set(*output,\n"
        "        collision_enabled ? collision_with : ROHR_COLLISION_CATEGORY_NONE));\n"
        "    if(collision_enabled) {\n"
        "        GENERATED_APPLY(rohr_entity_components_add(*output, ROHR_COLLISION));\n"
        "    }\n"
        "    GENERATED_APPLY(rohr_physics_friction_set(*output, friction));\n"
        "    GENERATED_APPLY(rohr_physics_restitution_set(*output, restitution));\n"
        "    if(static_body) GENERATED_APPLY(rohr_physics_static_set(*output));\n"
        "    else {\n"
        "        GENERATED_APPLY(rohr_physics_mass_set(*output, mass_value));\n"
        "        GENERATED_APPLY(rohr_physics_velocity_set(*output, (Velocity){0}));\n"
        "        GENERATED_APPLY(rohr_physics_angular_velocity_set(*output, 0.0f));\n"
        "        GENERATED_APPLY(rohr_physics_acceleration_set(*output, "
        "(Acceleration){0}));\n"
        "        GENERATED_APPLY(rohr_physics_dynamic_set(*output));\n"
        "        if(rotation_locked) GENERATED_APPLY(rohr_physics_angle_lock_set(*output, "
        "rotation, rotation));\n"
        "        if(gravity_enabled) GENERATED_APPLY(rohr_physics_gravity_enable(*output));\n"
        "    }\n"
        "#undef GENERATED_APPLY\n"
        "    return rohr_error_result_value(true);\n"
        "fail:\n"
        "#undef GENERATED_APPLY\n"
        "    (void)rohr_entity_delete(*output);\n"
        "    *output = ENTITY_INVALID;\n"
        "    return result;\n"
        "}\n\n");
    for(size_t object_index = 0; object_index < project->object_count; object_index += 1) {
        const EditorObject *object = &project->objects[object_index];
        char function_name[EDITOR_OBJECT_NAME_MAX];

        editor_project_property_name_format(function_name, sizeof(function_name),
            object->name);
        fprintf(header, "typedef struct %s {\n", object->name);
        for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
            fprintf(header, "    Entity %s;\n", object->rigid_bodies[body_index].name);
        }
        fprintf(header,
            "} %s;\n\n"
            "EngineResult %s_create(%s *object, Position position);\n"
            "void %s_destroy(%s *object);\n\n",
            object->name, function_name, object->name, function_name, object->name);
        fprintf(source, "EngineResult %s_create(%s *object, Position position) {\n"
            "    EngineResult result;\n"
            "    if(object == NULL) return rohr_error_result_error("
            "ERROR_MEMORY_POOL_NULL_POINTER);\n"
            "    *object = (%s){0};\n", function_name, object->name, object->name);
        for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
            const EditorRigidBody *body = &object->rigid_bodies[body_index];
            const EditorHitbox *hitbox = body->hitbox_count > 0 ? &body->hitboxes[0] : NULL;

            fprintf(source,
                "    result = generated_body_create(&object->%s, "
                "(Position){position.x + %#.9gf, position.y + %#.9gf}, %#.9gf, "
                "(Shape){.amount_of_vertices = %u, .vertices = {",
                body->name, body->position.x, body->position.y, body->rotation,
                hitbox == NULL ? 0 : hitbox->vertex_count);
            if(hitbox != NULL) {
                for(uint32_t vertex = 0; vertex < hitbox->vertex_count; vertex += 1) {
                    fprintf(source, "%s{%#.9gf, %#.9gf}", vertex == 0 ? "" : ", ",
                        hitbox->vertices[vertex].position.x,
                        hitbox->vertices[vertex].position.y);
                }
            }
            fprintf(source,
                "}}, %#.9gf, %#.9gf, %#.9gf, %s, %s, %s, %s, "
                "UINT64_C(%llu), UINT64_C(%llu));\n"
                "    if(rohr_error_check(result)) goto fail;\n",
                body->mass_value, body->friction, body->restitution,
                body->static_body ? "true" : "false",
                body->rotation_locked ? "true" : "false",
                body->gravity_enabled ? "true" : "false",
                body->collision_enabled ? "true" : "false",
                (unsigned long long)body->collision_category,
                (unsigned long long)body->collision_with);
        }
        fprintf(source,
            "    return rohr_error_result_value(true);\n"
            "fail:\n"
            "    %s_destroy(object);\n"
            "    return result;\n"
            "}\n\n"
            "void %s_destroy(%s *object) {\n"
            "    if(object == NULL) return;\n",
            function_name, function_name, object->name);
        for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
            const EditorRigidBody *body = &object->rigid_bodies[body_index];
            fprintf(source,
                "    if(object->%s != ENTITY_INVALID) "
                "(void)rohr_entity_delete(object->%s);\n",
                body->name, body->name);
        }
        fprintf(source, "    *object = (%s){0};\n}\n\n", object->name);
    }
    fprintf(header, "#endif\n");
    {
        bool header_closed = fclose(header) == 0;
        bool source_closed = fclose(source) == 0;
        return header_closed && source_closed;
    }
}

static bool editor_workspace_main_write(const EditorWorkspace *workspace,
    const EditorProject *project) {
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    FILE *file;

    if(workspace == NULL || project == NULL ||
            !editor_workspace_path_join(path, sizeof(path), workspace->directory,
                "src/main.c")) return false;
    file = fopen(path, "wb");
    if(file == NULL) return false;
    fprintf(file,
        "#include \"project_objects.h\"\n\n"
        "#include <stdio.h>\n\n"
        "static bool ok(EngineResult result) {\n"
        "    if(!rohr_error_check(result)) return true;\n"
        "    rohr_error_stderr_print(result.result.error);\n"
        "    return false;\n"
        "}\n\n"
        "int main(void) {\n"
        "    KeyboardState keyboard = {0};\n");
    for(size_t i = 0; i < project->object_count; i += 1) {
        char variable[EDITOR_OBJECT_NAME_MAX];
        editor_project_property_name_format(variable, sizeof(variable),
            project->objects[i].name);
        fprintf(file, "    %s %s = {0};\n", project->objects[i].name, variable);
    }
    fprintf(file,
        "    if(!ok(rohr_engine_init()) || !ok(rohr_graphics_start()) ||\n"
        "            !ok(rohr_physics_gravity_set((Acceleration){0.0f, -900.0f}))) goto fail;\n"
        );
    for(size_t i = 0; i < project->object_count; i += 1) {
        char variable[EDITOR_OBJECT_NAME_MAX];
        editor_project_property_name_format(variable, sizeof(variable),
            project->objects[i].name);
        fprintf(file, "    if(!ok(%s_create(&%s, (Position){%#.9gf, %#.9gf}))) goto fail;\n",
            variable, variable, project->objects[i].position.x,
            project->objects[i].position.y);
    }
    fprintf(file,
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
        "        rohr_graphics_background_draw((Color){18, 22, 30, 255});\n");
    for(size_t object_index = 0; object_index < project->object_count; object_index += 1) {
        const EditorObject *object = &project->objects[object_index];
        char variable[EDITOR_OBJECT_NAME_MAX];
        editor_project_property_name_format(variable, sizeof(variable), object->name);
        for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
            fprintf(file,
                "        rohr_graphics_hit_box_colored_draw(%s.%s, GRAPHICS_FILLED, "
                "(Color){%s});\n",
                variable, object->rigid_bodies[body_index].name,
                object_index == 0 ? "90, 100, 115, 255" : "70, 170, 255, 255");
        }
    }
    fprintf(file,
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
        "}\n");
    return fclose(file) == 0;
}

static bool editor_workspace_scaffold_write(const EditorWorkspace *workspace) {
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    char cmake[EDITOR_WORKSPACE_PATH_MAX * 2 + 1024];
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
        "add_executable(mygame src/main.c ${ROHR_GENERATED_SOURCES})\n"
        "target_include_directories(mygame PRIVATE src/generated)\n"
        "target_link_libraries(mygame PRIVATE rohr_engine)\n",
        workspace->config.name, workspace->config.engine_root);
    return editor_workspace_path_join(path, sizeof(path), workspace->directory,
            "CMakeLists.txt") && editor_workspace_file_write(path, cmake) &&
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

bool editor_workspace_c_generate(const EditorWorkspace *workspace,
    const EditorProject *project) {
    return workspace != NULL && workspace->open && project != NULL &&
        editor_workspace_generated_objects_write(workspace, project) &&
        editor_workspace_main_write(workspace, project);
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
    if(!editor_workspace_starter_project_init(project) ||
            !editor_workspace_save(&created, project) ||
            !editor_workspace_scaffold_write(&created) ||
            !editor_workspace_c_generate(&created, project)) return false;
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
