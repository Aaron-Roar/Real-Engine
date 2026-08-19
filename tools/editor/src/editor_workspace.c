#include "editor_workspace.h"

#include "editor_config.h"

#include "yyjson.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const EditorSprite *editor_workspace_sprite_get(const EditorObject *object,
        EditorSpriteId id) {
    if(object == NULL || id == 0) return NULL;
    for(size_t i = 0; i < object->sprite_count; i += 1)
        if(object->sprites[i].id == id) return &object->sprites[i];
    return NULL;
}

static void editor_workspace_c_string_write(FILE *file, const char *value) {
    fputc('"', file);
    for(const unsigned char *at = (const unsigned char *)value;
            at != NULL && *at != '\0'; at += 1) {
        if(*at == '\\' || *at == '"') fputc('\\', file);
        if(*at == '\n') fputs("\\n", file);
        else if(*at == '\r') fputs("\\r", file);
        else if(*at == '\t') fputs("\\t", file);
        else fputc(*at, file);
    }
    fputc('"', file);
}

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

static EditorResult editor_workspace_manifest_load(EditorWorkspace *workspace,
    const char *directory, bool *recognized);

static EditorResult editor_workspace_root_find(char *root, size_t capacity,
        const char *selection, EditorWorkspace *workspace) {
    SDL_PathInfo info;
    size_t length;
    EditorResult result;
    bool recognized;

    if(root == NULL || capacity == 0 || selection == NULL || selection[0] == '\0' ||
            workspace == NULL ||
            snprintf(root, capacity, "%s", selection) >= (int)capacity)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Workspace root search received an invalid or oversized path");
    if(SDL_GetPathInfo(root, &info) && info.type == SDL_PATHTYPE_FILE) {
        char *separator = strrchr(root, '/');
#if defined(_WIN32)
        char *backslash = strrchr(root, '\\');
        if(backslash != NULL && (separator == NULL || backslash > separator))
            separator = backslash;
#endif
        if(separator == NULL) return editor_result_error(
            EDITOR_ERROR_PROJECT_ROOT_NOT_FOUND,
            "Could not determine the parent directory of: %s", selection);
        *separator = '\0';
    }
    for(;;) {
        recognized = false;
        result = editor_workspace_manifest_load(workspace, root, &recognized);
        if(recognized || !editor_result_check(result)) return result;
        length = strlen(root);
        while(length > 1 && (root[length - 1] == '/' || root[length - 1] == '\\'))
            root[--length] = '\0';
        while(length > 0 && root[length - 1] != '/' && root[length - 1] != '\\')
            length -= 1;
        if(length == 0) break;
        if(length == 1) {
            if(root[0] == '/') break;
            root[0] = '\0';
            break;
        }
        root[length - 1] = '\0';
    }
    return editor_result_error(EDITOR_ERROR_PROJECT_ROOT_NOT_FOUND,
        "Could not find a valid project manifest at or above: %s", selection);
}

static const EditorRigidBody *editor_workspace_body_get(
    const EditorObject *object, EditorRigidBodyId id) {
    if(object == NULL || id == 0) return NULL;
    for(size_t i = 0; i < object->rigid_body_count; i += 1)
        if(object->rigid_bodies[i].id == id) return &object->rigid_bodies[i];
    return NULL;
}

static const EditorAnchor *editor_workspace_anchor_get(
    const EditorObject *object, EditorAnchorId id) {
    if(object == NULL || id == 0) return NULL;
    for(size_t i = 0; i < object->anchor_count; i += 1)
        if(object->anchors[i].id == id) return &object->anchors[i];
    return NULL;
}

static const EditorSoftNode *editor_workspace_soft_node_get(
    const EditorSoftBody *body, EditorSoftNodeId id) {
    if(body == NULL || id == 0) return NULL;
    for(size_t i = 0; i < body->node_count; i += 1)
        if(body->nodes[i].id == id) return &body->nodes[i];
    return NULL;
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
    EditorObject *starter;
    EditorRigidBody *floor_body;
    EditorRigidBody *box_body;
    EditorRigidBody *chassis_body;
    EditorRigidBody *wheel_body;
    EditorRigidBody *welded_body;
    EditorRigidBody *spring_body;
    EditorRigidBody *particle_body;
    EditorAnchor *chassis_pin;
    EditorAnchor *wheel_pin;
    EditorAnchor *chassis_weld;
    EditorAnchor *welded_anchor;
    EditorAnchor *chassis_spring;
    EditorAnchor *spring_anchor;
    EditorJoint *joint;
    EditorSoftBody *soft_body;
    EditorSoftNode *soft_nodes[4];
    EditorSprite *sprite;
    EditorAnimatedSprite *animation;
    EditorRigidBodyId box_id;
    EditorRigidBodyId chassis_id;
    EditorRigidBodyId wheel_id;
    EditorRigidBodyId welded_id;
    EditorRigidBodyId spring_id;
    EditorRigidBodyId particle_id;
    EditorAnchorId chassis_pin_id;
    EditorAnchorId wheel_pin_id;
    EditorAnchorId chassis_weld_id;
    EditorAnchorId welded_anchor_id;
    EditorAnchorId chassis_spring_id;
    EditorAnchorId spring_anchor_id;

    if(project == NULL) return false;
    editor_project_init(project);
    starter = editor_project_object_add(project, (Position){0});
    if(starter == NULL) return false;
    snprintf(starter->name, sizeof(starter->name), "Starter");
    floor_body = editor_project_rigid_body_add(project, starter);
    if(floor_body == NULL) return false;
    snprintf(floor_body->name, sizeof(floor_body->name), "floor");
    floor_body->position = (Position){0.0f, -200.0f};
    floor_body->static_body = true;
    floor_body->friction = 0.5f;
    floor_body->restitution = 0.0f;
    editor_workspace_hitbox_rectangle_set(project, &floor_body->hitboxes[0],
        600.0f, 40.0f);

    box_body = editor_project_rigid_body_add(project, starter);
    if(box_body == NULL) return false;
    snprintf(box_body->name, sizeof(box_body->name), "box");
    box_body->position = (Position){0.0f, 120.0f};
    box_body->mass_value = 5.0f;
    box_body->friction = 0.5f;
    box_body->restitution = 0.0f;
    box_body->gravity_enabled = true;
    editor_workspace_hitbox_rectangle_set(project, &box_body->hitboxes[0],
        50.0f, 50.0f);
    box_id = box_body->id;

    chassis_body = editor_project_rigid_body_add(project, starter);
    if(chassis_body == NULL) return false;
    chassis_id = chassis_body->id;
    wheel_body = editor_project_rigid_body_add(project, starter);
    if(wheel_body == NULL) return false;
    wheel_id = wheel_body->id;
    welded_body = editor_project_rigid_body_add(project, starter);
    if(welded_body == NULL) return false;
    welded_id = welded_body->id;
    spring_body = editor_project_rigid_body_add(project, starter);
    if(spring_body == NULL) return false;
    spring_id = spring_body->id;
    particle_body = editor_project_rigid_body_add(project, starter);
    if(particle_body == NULL) return false;
    particle_id = particle_body->id;
    chassis_body = editor_project_rigid_body_get(starter, chassis_id);
    wheel_body = editor_project_rigid_body_get(starter, wheel_id);
    welded_body = editor_project_rigid_body_get(starter, welded_id);
    spring_body = editor_project_rigid_body_get(starter, spring_id);
    particle_body = editor_project_rigid_body_get(starter, particle_id);
    snprintf(chassis_body->name, sizeof(chassis_body->name), "joint_chassis");
    chassis_body->position = (Position){-220.0f, 20.0f};
    editor_workspace_hitbox_rectangle_set(project, &chassis_body->hitboxes[0],
        120.0f, 30.0f);
    snprintf(wheel_body->name, sizeof(wheel_body->name), "revolute_wheel");
    wheel_body->position = (Position){-160.0f, 20.0f};
    editor_workspace_hitbox_rectangle_set(project, &wheel_body->hitboxes[0],
        42.0f, 42.0f);
    snprintf(welded_body->name, sizeof(welded_body->name), "welded_body");
    welded_body->position = (Position){-280.0f, 20.0f};
    editor_workspace_hitbox_rectangle_set(project, &welded_body->hitboxes[0],
        38.0f, 50.0f);
    snprintf(spring_body->name, sizeof(spring_body->name), "spring_body");
    spring_body->position = (Position){-220.0f, 125.0f};
    editor_workspace_hitbox_rectangle_set(project, &spring_body->hitboxes[0],
        42.0f, 42.0f);
    snprintf(particle_body->name, sizeof(particle_body->name), "particle_body");
    particle_body->position = (Position){100.0f, 20.0f};
    particle_body->particle = true;
    particle_body->particle_auto_fit = false;
    particle_body->particle_radius = 28.0f;

    chassis_pin = editor_project_anchor_add(project, starter,
        (Position){60.0f, 0.0f}, chassis_body->id);
    if(chassis_pin == NULL) return false;
    chassis_pin_id = chassis_pin->id;
    wheel_pin = editor_project_anchor_add(project, starter,
        (Position){0.0f, 0.0f}, wheel_body->id);
    if(wheel_pin == NULL) return false;
    wheel_pin_id = wheel_pin->id;
    chassis_weld = editor_project_anchor_add(project, starter,
        (Position){-60.0f, 0.0f}, chassis_body->id);
    if(chassis_weld == NULL) return false;
    chassis_weld_id = chassis_weld->id;
    welded_anchor = editor_project_anchor_add(project, starter,
        (Position){0.0f, 0.0f}, welded_body->id);
    if(welded_anchor == NULL) return false;
    welded_anchor_id = welded_anchor->id;
    chassis_spring = editor_project_anchor_add(project, starter,
        (Position){0.0f, 0.0f}, chassis_body->id);
    if(chassis_spring == NULL) return false;
    chassis_spring_id = chassis_spring->id;
    spring_anchor = editor_project_anchor_add(project, starter,
        (Position){0.0f, 0.0f}, spring_body->id);
    if(spring_anchor == NULL) return false;
    spring_anchor_id = spring_anchor->id;
    if(editor_project_anchor_add(project, starter,
            (Position){300.0f, 190.0f}, 0) == NULL) return false;
    chassis_pin = editor_project_anchor_get(starter, chassis_pin_id);
    wheel_pin = editor_project_anchor_get(starter, wheel_pin_id);
    chassis_weld = editor_project_anchor_get(starter, chassis_weld_id);
    welded_anchor = editor_project_anchor_get(starter, welded_anchor_id);
    chassis_spring = editor_project_anchor_get(starter, chassis_spring_id);
    spring_anchor = editor_project_anchor_get(starter, spring_anchor_id);
    snprintf(chassis_pin->name, sizeof(chassis_pin->name), "chassis_pin");
    snprintf(wheel_pin->name, sizeof(wheel_pin->name), "wheel_pin");
    snprintf(chassis_weld->name, sizeof(chassis_weld->name), "chassis_weld");
    snprintf(welded_anchor->name, sizeof(welded_anchor->name), "welded_anchor");
    snprintf(chassis_spring->name, sizeof(chassis_spring->name), "chassis_spring");
    snprintf(spring_anchor->name, sizeof(spring_anchor->name), "spring_anchor");
    snprintf(starter->anchors[starter->anchor_count - 1].name,
        sizeof(starter->anchors[starter->anchor_count - 1].name), "world_anchor");

    joint = editor_project_joint_add(project, starter, EDITOR_JOINT_REVOLUTE);
    if(joint == NULL) return false;
    snprintf(joint->name, sizeof(joint->name), "revolute_joint");
    joint->anchor_a = chassis_pin_id;
    joint->anchor_b = wheel_pin_id;
    joint = editor_project_joint_add(project, starter, EDITOR_JOINT_WELD);
    if(joint == NULL) return false;
    snprintf(joint->name, sizeof(joint->name), "weld_joint");
    joint->anchor_a = chassis_weld_id;
    joint->anchor_b = welded_anchor_id;
    joint = editor_project_joint_add(project, starter, EDITOR_JOINT_SPRING);
    if(joint == NULL) return false;
    snprintf(joint->name, sizeof(joint->name), "spring_joint");
    joint->anchor_a = chassis_spring_id;
    joint->anchor_b = spring_anchor_id;
    joint->rest_length = 105.0f;

    soft_body = editor_project_soft_body_add(project, starter);
    if(soft_body == NULL) return false;
    snprintf(soft_body->name, sizeof(soft_body->name), "soft_body");
    soft_body->position = (Position){220.0f, 80.0f};
    soft_nodes[0] = editor_project_soft_node_add(project, soft_body,
        (Position){-45.0f, -45.0f});
    soft_nodes[1] = editor_project_soft_node_add(project, soft_body,
        (Position){45.0f, -45.0f});
    soft_nodes[2] = editor_project_soft_node_add(project, soft_body,
        (Position){45.0f, 45.0f});
    soft_nodes[3] = editor_project_soft_node_add(project, soft_body,
        (Position){-45.0f, 45.0f});
    for(size_t i = 0; i < 4; i += 1) if(soft_nodes[i] == NULL) return false;
    for(size_t i = 0; i < 4; i += 1)
        if(editor_project_soft_beam_add(project, soft_body, soft_nodes[i]->id,
                soft_nodes[(i + 1) % 4]->id) == NULL) return false;
    if(editor_project_soft_beam_add(project, soft_body,
            soft_nodes[0]->id, soft_nodes[2]->id) == NULL) return false;

    sprite = editor_project_sprite_add(project, starter, "standalone_sprite",
        "assets/tutorial_frame_1.png");
    if(sprite == NULL) return false;
    sprite->position = (Position){220.0f, -100.0f};
    sprite->size = (Scale){64.0f, 64.0f};

    animation = editor_project_animated_sprite_add(project, starter);
    if(animation == NULL) return false;
    snprintf(animation->name, sizeof(animation->name), "attached_animation");
    animation->rigid_body = box_id;
    animation->time_per_frame = 0.2;
    animation->playing = true;
    if(!editor_project_animation_frame_add(project, animation, "frame_1",
            "assets/tutorial_frame_1.png", (Scale){64.0f, 64.0f}) ||
            !editor_project_animation_frame_add(project, animation, "frame_2",
                "assets/tutorial_frame_2.png", (Scale){64.0f, 64.0f})) return false;
    animation = editor_project_animated_sprite_add(project, starter);
    if(animation == NULL) return false;
    snprintf(animation->name, sizeof(animation->name), "free_animation");
    animation->editor_position = (Position){340.0f, 80.0f};
    animation->time_per_frame = 0.2;
    animation->playing = true;
    if(!editor_project_animation_frame_add(project, animation, "frame_1",
            "assets/tutorial_frame_1.png", (Scale){64.0f, 64.0f}) ||
            !editor_project_animation_frame_add(project, animation, "frame_2",
                "assets/tutorial_frame_2.png", (Scale){64.0f, 64.0f})) return false;
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

static EditorResult editor_workspace_manifest_load(EditorWorkspace *workspace,
        const char *directory, bool *recognized) {
    yyjson_doc *document;
    yyjson_val *root;
    yyjson_val *version;
    yyjson_read_err read_error = {0};
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    EditorResult result;

    if(recognized != NULL) *recognized = false;
    if(workspace == NULL || directory == NULL || recognized == NULL ||
            !editor_workspace_path_join(path, sizeof(path), directory,
                "project.rohr.json")) return editor_result_error(
                    EDITOR_ERROR_INVALID_ARGUMENT,
                    "Workspace manifest load received an invalid path");
    document = yyjson_read_file(path, 0, NULL, &read_error);
    if(document == NULL) return editor_result_error(
        read_error.code == YYJSON_READ_ERROR_FILE_OPEN ||
                read_error.code == YYJSON_READ_ERROR_FILE_READ ?
            EDITOR_ERROR_FILE_IO : EDITOR_ERROR_JSON_PARSE,
        "Could not parse workspace manifest '%s': %s at byte %zu", path,
        read_error.msg == NULL ? "unknown JSON error" : read_error.msg,
        read_error.pos);
    root = yyjson_doc_get_root(document);
    version = yyjson_obj_get(root, "format_version");
    *recognized = yyjson_is_obj(root) &&
        yyjson_obj_get(root, "editor_state_file") != NULL;
    result = editor_result_error(EDITOR_ERROR_SCHEMA_INVALID,
        "Workspace manifest '%s' does not match the current schema", path);
    if(!yyjson_is_obj(root)) goto done;
    if(!yyjson_is_uint(version)) {
        result = editor_result_error(EDITOR_ERROR_SCHEMA_VERSION,
            "Workspace manifest '%s' is missing integer format_version; expected %u",
            path, EDITOR_WORKSPACE_FORMAT_VERSION);
        goto done;
    }
    if(yyjson_get_uint(version) != EDITOR_WORKSPACE_FORMAT_VERSION) {
        result = editor_result_error(EDITOR_ERROR_SCHEMA_VERSION,
            "Workspace manifest '%s' uses format_version %llu; this editor requires %u",
            path, (unsigned long long)yyjson_get_uint(version),
            EDITOR_WORKSPACE_FORMAT_VERSION);
        goto done;
    }
    workspace->config = editor_workspace_config_default_get();
    workspace->config.format_version = (uint32_t)yyjson_get_uint(version);
#define EDITOR_MANIFEST_STRING_READ(Field) \
    if(!editor_workspace_json_string(root, #Field, workspace->config.Field, \
            sizeof(workspace->config.Field))) { \
        result = editor_result_error(EDITOR_ERROR_SCHEMA_INVALID, \
            "Workspace manifest '%s' has missing, empty, or oversized field '%s'", \
            path, #Field); \
        goto done; \
    }
    EDITOR_MANIFEST_STRING_READ(name);
    EDITOR_MANIFEST_STRING_READ(source_directory);
    EDITOR_MANIFEST_STRING_READ(generated_directory);
    EDITOR_MANIFEST_STRING_READ(asset_directory);
    EDITOR_MANIFEST_STRING_READ(object_directory);
    EDITOR_MANIFEST_STRING_READ(editor_state_file);
#undef EDITOR_MANIFEST_STRING_READ
    if(strlen(directory) >= sizeof(workspace->directory)) {
        result = editor_result_error(EDITOR_ERROR_FILE_IO,
            "Workspace directory path is too long: %s", directory);
        goto done;
    }
    snprintf(workspace->directory, sizeof(workspace->directory), "%s", directory);
    workspace->open = true;
    result = editor_result_value(true);
done:
    yyjson_doc_free(document);
    return result;
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
        "    bool gravity_enabled, bool collision_enabled, bool particle,\n"
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
        "        if(particle) GENERATED_APPLY(rohr_entity_components_add(*output, ROHR_PARTICLE));\n"
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
    fprintf(source,
        "static EngineResult generated_sprite_create(Entity *output, Position position,\n"
        "    const char *path, Scale size, bool visible) {\n"
        "    EntityResult added = rohr_entity_add();\n"
        "    AnimationAssetResult loaded;\n"
        "    AnimatedSprite sprite;\n"
        "    EngineResult result;\n"
        "    if(rohr_error_check(added)) return rohr_error_result_error(added.result.error);\n"
        "    *output = added.result.value;\n"
        "    result = rohr_physics_position_set(*output, position);\n"
        "    if(rohr_error_check(result)) goto fail;\n"
        "    loaded = rohr_graphics_animation_load((AnimationDescriptor){\n"
        "        .amount_of_descriptors = 1,\n"
        "        .texture_descriptors = {{.file = path, .size = size}}});\n"
        "    if(rohr_error_check(loaded)) {\n"
        "        result = rohr_error_result_error(loaded.result.error);\n"
        "        goto fail;\n"
        "    }\n"
        "    sprite = rohr_graphics_animated_sprite_create(loaded.result.value,\n"
        "        (Scale){1.0f, 1.0f});\n"
        "    sprite.visible = visible;\n"
        "    result = rohr_graphics_animated_sprite_add(*output, sprite);\n"
        "    if(rohr_error_check(result)) goto fail;\n"
        "    return rohr_error_result_value(true);\n"
        "fail:\n"
        "    (void)rohr_entity_delete(*output);\n"
        "    *output = ENTITY_INVALID;\n"
        "    return result;\n"
        "}\n\n");
    fprintf(source,
        "static EngineResult generated_world_anchor_create(Entity *owner,\n"
        "    JointAnchorId *anchor, Position position) {\n"
        "    EntityResult added = rohr_entity_add();\n"
        "    JointAnchorIdResult created;\n"
        "    EngineResult result;\n"
        "    if(rohr_error_check(added)) return rohr_error_result_error(added.result.error);\n"
        "    *owner = added.result.value;\n"
        "    result = rohr_physics_position_set(*owner, position);\n"
        "    if(rohr_error_check(result)) goto fail;\n"
        "    result = rohr_physics_static_set(*owner);\n"
        "    if(rohr_error_check(result)) goto fail;\n"
        "    created = rohr_physics_joint_anchor_create(*owner, (Vec2D){0});\n"
        "    if(rohr_error_check(created)) {\n"
        "        result = rohr_error_result_error(created.result.error);\n"
        "        goto fail;\n"
        "    }\n"
        "    *anchor = created.result.value;\n"
        "    return rohr_error_result_value(true);\n"
        "fail:\n"
        "    (void)rohr_entity_delete(*owner);\n"
        "    *owner = ENTITY_INVALID;\n"
        "    *anchor = JOINT_ANCHOR_INVALID;\n"
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
        for(size_t anchor_index = 0; anchor_index < object->anchor_count; anchor_index += 1) {
            const EditorAnchor *anchor = &object->anchors[anchor_index];
            fprintf(header, "    JointAnchorId anchor_%s;\n", anchor->name);
            if(editor_workspace_body_get(object, anchor->rigid_body) == NULL ||
                    !anchor->position_follows_body)
                fprintf(header, "    Entity anchor_%s_owner;\n", anchor->name);
        }
        for(size_t joint_index = 0; joint_index < object->joint_count; joint_index += 1)
            fprintf(header, "    Entity joint_%s;\n", object->joint_items[joint_index].name);
        for(size_t soft_body_index = 0; soft_body_index < object->soft_body_count;
                soft_body_index += 1) {
            const EditorSoftBody *body = &object->soft_body_items[soft_body_index];
            fprintf(header, "    Entity %s;\n", body->name);
            for(size_t node_index = 0; node_index < body->node_count; node_index += 1)
                fprintf(header, "    Entity %s;\n", body->nodes[node_index].name);
            for(size_t beam_index = 0; beam_index < body->beam_count; beam_index += 1)
                fprintf(header, "    Entity %s;\n", body->beams[beam_index].name);
            for(size_t area_index = 0; area_index < body->area_count; area_index += 1)
                fprintf(header, "    Entity %s;\n", body->areas[area_index].name);
        }
        for(size_t sprite_index = 0; sprite_index < object->sprite_count;
                sprite_index += 1)
            fprintf(header, "    Entity sprite_%s;\n",
                object->sprites[sprite_index].name);
        for(size_t sprite_index = 0; sprite_index < object->animated_sprite_count;
                sprite_index += 1)
            if(editor_workspace_body_get(object,
                    object->animated_sprite_items[sprite_index].rigid_body) == NULL)
                fprintf(header, "    Entity animation_%s;\n",
                    object->animated_sprite_items[sprite_index].name);
        fprintf(header,
            "} %s;\n\n"
            "EngineResult %s_create(%s *object, Position position);\n"
            "void %s_draw(const %s *object);\n"
            "void %s_destroy(%s *object);\n\n",
            object->name, function_name, object->name, function_name, object->name,
            function_name, object->name);
        fprintf(source, "EngineResult %s_create(%s *object, Position position) {\n"
            "    EngineResult result;\n"
            "    if(object == NULL) return rohr_error_result_error("
            "ERROR_MEMORY_POOL_NULL_POINTER);\n"
            "    *object = (%s){0};\n", function_name, object->name, object->name);
        for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
            const EditorRigidBody *body = &object->rigid_bodies[body_index];
            const EditorHitbox *hitbox = body->hitbox_count > 0 ? &body->hitboxes[0] : NULL;
            Position particle_center = editor_project_particle_center_get(body);
            float particle_radius = body->particle_auto_fit ?
                editor_project_particle_auto_radius_get(body) : body->particle_radius;

            fprintf(source,
                "    result = generated_body_create(&object->%s, "
                "(Position){position.x + %#.9gf, position.y + %#.9gf}, %#.9gf, "
                "(Shape){.amount_of_vertices = %u, .vertices = {",
                body->name, body->position.x, body->position.y, body->rotation,
                body->particle ? 4 : hitbox == NULL ? 0 : hitbox->vertex_count);
            if(body->particle) {
                fprintf(source, "{%#.9gf, %#.9gf}, {%#.9gf, %#.9gf}, "
                    "{%#.9gf, %#.9gf}, {%#.9gf, %#.9gf}",
                    particle_center.x + particle_radius, particle_center.y,
                    particle_center.x, particle_center.y + particle_radius,
                    particle_center.x - particle_radius, particle_center.y,
                    particle_center.x, particle_center.y - particle_radius);
            } else if(hitbox != NULL) {
                for(uint32_t vertex = 0; vertex < hitbox->vertex_count; vertex += 1) {
                    fprintf(source, "%s{%#.9gf, %#.9gf}", vertex == 0 ? "" : ", ",
                        hitbox->vertices[vertex].position.x,
                        hitbox->vertices[vertex].position.y);
                }
            }
            fprintf(source,
                "}}, %#.9gf, %#.9gf, %#.9gf, %s, %s, %s, %s, %s, "
                "UINT64_C(%llu), UINT64_C(%llu));\n"
                "    if(rohr_error_check(result)) goto fail;\n",
                body->mass_value, body->friction, body->restitution,
                body->static_body ? "true" : "false",
                body->rotation_locked ? "true" : "false",
                body->gravity_enabled ? "true" : "false",
                body->collision_enabled ? "true" : "false",
                body->particle ? "true" : "false",
                (unsigned long long)body->collision_category,
                (unsigned long long)body->collision_with);
        }
        for(size_t sprite_index = 0; sprite_index < object->sprite_count;
                sprite_index += 1) {
            const EditorSprite *sprite = &object->sprites[sprite_index];
            fprintf(source,
                "    result = generated_sprite_create(&object->sprite_%s, "
                "(Position){position.x + %#.9gf, position.y + %#.9gf}, ",
                sprite->name, sprite->position.x, sprite->position.y);
            editor_workspace_c_string_write(source, sprite->path);
            fprintf(source, ", (Scale){%#.9gf, %#.9gf}, %s);\n"
                "    if(rohr_error_check(result)) goto fail;\n",
                sprite->size.x, sprite->size.y,
                sprite->visible ? "true" : "false");
        }
        for(size_t sprite_index = 0; sprite_index < object->animated_sprite_count;
                sprite_index += 1) {
            const EditorAnimatedSprite *sprite = &object->animated_sprite_items[sprite_index];
            const EditorRigidBody *body = editor_workspace_body_get(object,
                sprite->rigid_body);
            char target[EDITOR_OBJECT_NAME_MAX + 24];
            if(sprite->frame_count == 0 ||
                    sprite->frame_count > MAX_ANIMATIONS_FRAMES) continue;
            if(body == NULL) {
                snprintf(target, sizeof(target), "animation_%s", sprite->name);
                fprintf(source,
                    "    { EntityResult added = rohr_entity_add();\n"
                    "      if(rohr_error_check(added)) { result = rohr_error_result_error("
                    "added.result.error); goto fail; }\n"
                    "      object->%s = added.result.value;\n"
                    "      result = rohr_physics_position_set(object->%s, "
                    "(Position){position.x + %#.9gf, position.y + %#.9gf});\n"
                    "      if(rohr_error_check(result)) goto fail; }\n",
                    target, target, sprite->editor_position.x,
                    sprite->editor_position.y);
            } else snprintf(target, sizeof(target), "%s", body->name);
            fprintf(source,
                "    { AnimationDescriptor descriptor = {.amount_of_descriptors = %zu, "
                ".ticks_per_frame = UINT64_C(%llu), .time_per_frame = %.17g, "
                ".texture_descriptors = {",
                sprite->frame_count, (unsigned long long)sprite->ticks_per_frame,
                sprite->time_per_frame);
            for(size_t frame = 0; frame < sprite->frame_count; frame += 1) {
                const EditorAnimationFrame *asset = &sprite->frames[frame];
                fprintf(source, "%s{.file = ", frame == 0 ? "" : ", ");
                editor_workspace_c_string_write(source, asset->path);
                fprintf(source, ", .size = {%#.9gf, %#.9gf}}",
                    asset->size.x, asset->size.y);
            }
            fprintf(source,
                "}};\n"
                "      AnimationAssetResult loaded = rohr_graphics_animation_load(descriptor);\n"
                "      AnimatedSprite animated;\n"
                "      if(rohr_error_check(loaded)) { result = rohr_error_result_error("
                "loaded.result.error); goto fail; }\n"
                "      animated = rohr_graphics_animated_sprite_create(loaded.result.value, "
                "(Scale){%#.9gf, %#.9gf});\n"
                "      animated.animation_frame = %u;\n"
                "      animated.direction = %s;\n"
                "      animated.follow_entity_rotation = %s;\n"
                "      animated.visible = %s;\n"
                "      result = rohr_graphics_animated_sprite_add(object->%s, animated);\n"
                "      if(rohr_error_check(result)) goto fail; }\n",
                sprite->scale.x, sprite->scale.y, sprite->starting_frame,
                sprite->direction == DIRECTION_LEFT ? "DIRECTION_LEFT" :
                    "DIRECTION_RIGHT",
                sprite->follow_body_rotation ? "true" : "false",
                sprite->visible ? "true" : "false", target);
        }
        for(size_t anchor_index = 0; anchor_index < object->anchor_count; anchor_index += 1) {
            const EditorAnchor *anchor = &object->anchors[anchor_index];
            const EditorRigidBody *body = editor_workspace_body_get(
                object, anchor->rigid_body);
            if(body != NULL && anchor->position_follows_body) {
                fprintf(source,
                    "    { JointAnchorIdResult created = rohr_physics_joint_anchor_create("
                    "object->%s, (Vec2D){%#.9gf, %#.9gf});\n"
                    "      if(rohr_error_check(created)) { result = rohr_error_result_error("
                    "created.result.error); goto fail; }\n"
                    "      object->anchor_%s = created.result.value; }\n",
                    body->name, anchor->position.x, anchor->position.y, anchor->name);
            } else {
                fprintf(source,
                    "    result = generated_world_anchor_create(&object->anchor_%s_owner, "
                    "&object->anchor_%s, (Position){position.x + %#.9gf, "
                    "position.y + %#.9gf});\n"
                    "    if(rohr_error_check(result)) goto fail;\n",
                    anchor->name, anchor->name, anchor->position.x, anchor->position.y);
            }
        }
        for(size_t joint_index = 0; joint_index < object->joint_count; joint_index += 1) {
            const EditorJoint *joint = &object->joint_items[joint_index];
            const EditorAnchor *anchor_a = editor_workspace_anchor_get(object, joint->anchor_a);
            const EditorAnchor *anchor_b = editor_workspace_anchor_get(object, joint->anchor_b);
            if(anchor_a == NULL || anchor_b == NULL) continue;
            fprintf(source,
                "    { EntityResult added = rohr_entity_add();\n"
                "      if(rohr_error_check(added)) { result = rohr_error_result_error("
                "added.result.error); goto fail; }\n"
                "      object->joint_%s = added.result.value; }\n",
                joint->name);
            if(joint->kind == EDITOR_JOINT_REVOLUTE) {
                fprintf(source,
                    "    result = rohr_physics_joint_pin_set(object->joint_%s, "
                    "object->anchor_%s, object->anchor_%s);\n",
                    joint->name, anchor_a->name, anchor_b->name);
            } else if(joint->kind == EDITOR_JOINT_WELD) {
                fprintf(source,
                    "    result = rohr_physics_joint_weld_set(object->joint_%s, "
                    "object->anchor_%s, object->anchor_%s);\n",
                    joint->name, anchor_a->name, anchor_b->name);
            } else {
                fprintf(source,
                    "    result = rohr_physics_joint_spring_set(object->joint_%s, "
                    "object->anchor_%s, object->anchor_%s, %#.9gf, %#.9gf, %#.9gf);\n",
                    joint->name, anchor_a->name, anchor_b->name,
                    joint->rest_length, joint->stiffness, joint->damping);
            }
            fprintf(source, "    if(rohr_error_check(result)) goto fail;\n");
        }
        for(size_t soft_body_index = 0; soft_body_index < object->soft_body_count;
                soft_body_index += 1) {
            const EditorSoftBody *body = &object->soft_body_items[soft_body_index];
            fprintf(source,
                "    { EntityResult created = rohr_physics_soft_body_create();\n"
                "      if(rohr_error_check(created)) { result = rohr_error_result_error("
                "created.result.error); goto fail; }\n"
                "      object->%s = created.result.value; }\n",
                body->name);
            for(size_t node_index = 0; node_index < body->node_count; node_index += 1) {
                const EditorSoftNode *node = &body->nodes[node_index];
                float cosine = cosf(body->rotation);
                float sine = sinf(body->rotation);
                Position transformed = {
                    body->position.x + node->position.x * cosine - node->position.y * sine,
                    body->position.y + node->position.x * sine + node->position.y * cosine
                };
                fprintf(source,
                    "    { EntityResult created = rohr_physics_soft_body_node_create("
                    "object->%s, (Position){position.x + %#.9gf, "
                    "position.y + %#.9gf}, %#.9gf, %#.9gf);\n"
                    "      if(rohr_error_check(created)) { result = rohr_error_result_error("
                    "created.result.error); goto fail; }\n"
                    "      object->%s = created.result.value; }\n",
                    body->name, transformed.x, transformed.y,
                    node->node_mass, node->radius, node->name);
                if(node->gravity_enabled) {
                    fprintf(source,
                        "    result = rohr_physics_gravity_enable(object->%s);\n"
                        "    if(rohr_error_check(result)) goto fail;\n",
                        node->name);
                }
                fprintf(source,
                    "    result = rohr_physics_friction_set(object->%s, %#.9gf);\n"
                    "    if(rohr_error_check(result)) goto fail;\n"
                    "    result = rohr_physics_restitution_set(object->%s, %#.9gf);\n"
                    "    if(rohr_error_check(result)) goto fail;\n",
                    node->name, node->friction, node->name, node->restitution);
                fprintf(source,
                    "    result = rohr_physics_soft_body_node_collision_filter_set("
                    "object->%s, UINT64_C(%llu), UINT64_C(%llu));\n"
                    "    if(rohr_error_check(result)) goto fail;\n",
                    node->name,
                    (unsigned long long)(node->collision_enabled ?
                        node->collision_category : ROHR_COLLISION_CATEGORY_NONE),
                    (unsigned long long)(node->collision_enabled ?
                        node->collision_with : ROHR_COLLISION_CATEGORY_NONE));
            }
            for(size_t beam_index = 0; beam_index < body->beam_count; beam_index += 1) {
                const EditorSoftBeam *beam = &body->beams[beam_index];
                const EditorSoftNode *node_a = editor_workspace_soft_node_get(
                    body, beam->node_a);
                const EditorSoftNode *node_b = editor_workspace_soft_node_get(
                    body, beam->node_b);
                if(node_a == NULL || node_b == NULL) continue;
                fprintf(source,
                    "    { EntityResult created = rohr_physics_soft_body_beam_create("
                    "object->%s, object->%s, "
                    "object->%s, %#.9gf, %#.9gf);\n"
                    "      if(rohr_error_check(created)) { result = rohr_error_result_error("
                    "created.result.error); goto fail; }\n"
                    "      object->%s = created.result.value; }\n",
                    body->name, node_a->name, node_b->name, beam->stiffness,
                    beam->damping, beam->name);
                if(beam->color_overridden) {
                    fprintf(source,
                        "    result = rohr_graphics_soft_body_beam_color_set(object->%s, "
                        "object->%s, object->%s, rohr_graphics_color_hex_create("
                        "UINT32_C(0x%08x)));\n"
                        "    if(rohr_error_check(result)) goto fail;\n",
                        body->name, node_a->name, node_b->name, beam->color);
                }
            }
            for(size_t area_index = 0; area_index < body->area_count; area_index += 1) {
                const EditorSoftArea *area = &body->areas[area_index];
                if(area->node_count < 3) continue;
                uint32_t triangles[area->node_count - 2][3];
                size_t triangle_count = editor_project_soft_area_triangulate(
                    body, area, triangles, area->node_count - 2);
                for(size_t triangle = 0; triangle < triangle_count; triangle += 1) {
                    const EditorSoftNode *node_a = editor_workspace_soft_node_get(
                        body, area->nodes[triangles[triangle][0]]);
                    const EditorSoftNode *node_b = editor_workspace_soft_node_get(
                        body, area->nodes[triangles[triangle][1]]);
                    const EditorSoftNode *node_c = editor_workspace_soft_node_get(
                        body, area->nodes[triangles[triangle][2]]);
                    if(node_a == NULL || node_b == NULL || node_c == NULL) continue;
                    fprintf(source,
                        "    { EntityResult created = rohr_physics_soft_body_triangle_create("
                        "object->%s, object->%s, object->%s, object->%s);\n"
                        "      if(rohr_error_check(created)) { result = rohr_error_result_error("
                        "created.result.error); goto fail; }\n",
                        body->name, node_a->name, node_b->name, node_c->name);
                    if(triangle == 0) fprintf(source,
                        "      object->%s = created.result.value; }\n", area->name);
                    else fprintf(source, "    }\n");
                    if(area->color_overridden) {
                        fprintf(source,
                            "    result = rohr_graphics_soft_body_area_color_set(object->%s, "
                            "object->%s, object->%s, object->%s, "
                            "rohr_graphics_color_hex_create(UINT32_C(0x%08x)));\n"
                            "    if(rohr_error_check(result)) goto fail;\n",
                            body->name, node_a->name, node_b->name, node_c->name,
                            area->color);
                    }
                }
            }
            for(size_t node_index = 0; node_index < body->node_count; node_index += 1) {
                const EditorSoftNode *node = &body->nodes[node_index];
                if(!node->color_overridden) continue;
                fprintf(source,
                    "    result = rohr_graphics_soft_body_node_color_set(object->%s, "
                    "object->%s, rohr_graphics_color_hex_create(UINT32_C(0x%08x)));\n"
                    "    if(rohr_error_check(result)) goto fail;\n",
                    body->name, node->name, node->color);
            }
        }
        fprintf(source,
            "    return rohr_error_result_value(true);\n"
            "fail:\n"
            "    %s_destroy(object);\n"
            "    return result;\n"
            "}\n\n"
            "void %s_draw(const %s *object) {\n"
            "    if(object == NULL) return;\n",
            function_name, function_name, object->name);
        for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
            const EditorRigidBody *body = &object->rigid_bodies[body_index];
            fprintf(source,
                "    (void)rohr_graphics_hit_box_colored_draw(object->%s, GRAPHICS_FILLED, "
                "rohr_graphics_color_hex_create(UINT32_C(0x%08x)));\n"
                "    (void)rohr_graphics_hit_box_colored_draw(object->%s, GRAPHICS_OUTLINE, "
                "rohr_graphics_color_hex_create(UINT32_C(0x%08x)));\n",
                body->name, body->surface_color, body->name, body->border_color);
        }
        for(size_t soft_body_index = 0; soft_body_index < object->soft_body_count;
                soft_body_index += 1) {
            const EditorSoftBody *body = &object->soft_body_items[soft_body_index];
            fprintf(source,
                "    (void)rohr_graphics_soft_body_draw(object->%s, "
                "rohr_graphics_color_hex_create(UINT32_C(0x%08x)), "
                "rohr_graphics_color_hex_create(UINT32_C(0x%08x)), "
                "rohr_graphics_color_hex_create(UINT32_C(0x%08x)));\n",
                body->name, body->area_color, body->beam_color, body->node_color);
        }
        fprintf(source,
            "}\n\n"
            "void %s_destroy(%s *object) {\n"
            "    if(object == NULL) return;\n",
            function_name, object->name);
        for(size_t joint_index = 0; joint_index < object->joint_count; joint_index += 1) {
            const EditorJoint *joint = &object->joint_items[joint_index];
            fprintf(source,
                "    if(object->joint_%s != ENTITY_INVALID) "
                "(void)rohr_entity_delete(object->joint_%s);\n",
                joint->name, joint->name);
        }
        for(size_t soft_body_index = 0; soft_body_index < object->soft_body_count;
                soft_body_index += 1) {
            const EditorSoftBody *body = &object->soft_body_items[soft_body_index];
            fprintf(source,
                "    if(object->%s != ENTITY_INVALID) "
                "(void)rohr_entity_delete(object->%s);\n",
                body->name, body->name);
        }
        for(size_t sprite_index = 0; sprite_index < object->sprite_count;
                sprite_index += 1) {
            const EditorSprite *sprite = &object->sprites[sprite_index];
            fprintf(source,
                "    if(object->sprite_%s != ENTITY_INVALID) "
                "(void)rohr_entity_delete(object->sprite_%s);\n",
                sprite->name, sprite->name);
        }
        for(size_t sprite_index = 0; sprite_index < object->animated_sprite_count;
                sprite_index += 1) {
            const EditorAnimatedSprite *sprite = &object->animated_sprite_items[sprite_index];
            if(editor_workspace_body_get(object, sprite->rigid_body) != NULL) continue;
            fprintf(source,
                "    if(object->animation_%s != ENTITY_INVALID) "
                "(void)rohr_entity_delete(object->animation_%s);\n",
                sprite->name, sprite->name);
        }
        for(size_t anchor_index = 0; anchor_index < object->anchor_count; anchor_index += 1) {
            const EditorAnchor *anchor = &object->anchors[anchor_index];
            if(editor_workspace_body_get(object, anchor->rigid_body) == NULL ||
                    !anchor->position_follows_body)
                fprintf(source,
                    "    if(object->anchor_%s_owner != ENTITY_INVALID) "
                    "(void)rohr_entity_delete(object->anchor_%s_owner);\n",
                    anchor->name, anchor->name);
        }
        for(size_t body_index = 0; body_index < object->rigid_body_count; body_index += 1) {
            const EditorRigidBody *body = &object->rigid_bodies[body_index];
            fprintf(source,
                "    if(object->%s != ENTITY_INVALID) "
                "(void)rohr_entity_delete(object->%s);\n",
                body->name, body->name);
        }
        fprintf(source, "    *object = (%s){0};\n}\n\n", object->name);
    }
    fprintf(header, "typedef struct ProjectObjects {\n");
    for(size_t object_index = 0; object_index < project->object_count; object_index += 1) {
        char variable[EDITOR_OBJECT_NAME_MAX];
        editor_project_property_name_format(variable, sizeof(variable),
            project->objects[object_index].name);
        fprintf(header, "    %s %s;\n", project->objects[object_index].name, variable);
    }
    fprintf(header,
        "    bool created;\n"
        "} ProjectObjects;\n\n"
        "EngineResult project_objects_create_all(ProjectObjects *objects);\n"
        "void project_objects_draw_all(const ProjectObjects *objects);\n"
        "void project_objects_destroy_all(ProjectObjects *objects);\n\n");
    fprintf(source,
        "EngineResult project_objects_create_all(ProjectObjects *objects) {\n"
        "    EngineResult result;\n"
        "    if(objects == NULL) return rohr_error_result_error("
            "ERROR_MEMORY_POOL_NULL_POINTER);\n"
        "    *objects = (ProjectObjects){0};\n");
    for(size_t object_index = 0; object_index < project->object_count; object_index += 1) {
        const EditorObject *object = &project->objects[object_index];
        char variable[EDITOR_OBJECT_NAME_MAX];
        editor_project_property_name_format(variable, sizeof(variable), object->name);
        fprintf(source,
            "    result = %s_create(&objects->%s, (Position){%#.9gf, %#.9gf});\n"
            "    if(rohr_error_check(result)) goto fail;\n",
            variable, variable, object->position.x, object->position.y);
    }
    fprintf(source,
        "    objects->created = true;\n"
        "    return rohr_error_result_value(true);\n"
        "fail:\n"
        "    project_objects_destroy_all(objects);\n"
        "    return result;\n"
        "}\n\n"
        "void project_objects_draw_all(const ProjectObjects *objects) {\n"
        "    if(objects == NULL) return;\n");
    for(size_t object_index = 0; object_index < project->object_count; object_index += 1) {
        char variable[EDITOR_OBJECT_NAME_MAX];
        editor_project_property_name_format(variable, sizeof(variable),
            project->objects[object_index].name);
        fprintf(source, "    %s_draw(&objects->%s);\n", variable, variable);
    }
    fprintf(source,
        "}\n\n"
        "void project_objects_destroy_all(ProjectObjects *objects) {\n"
        "    if(objects == NULL) return;\n");
    for(size_t object_index = project->object_count; object_index > 0; object_index -= 1) {
        char variable[EDITOR_OBJECT_NAME_MAX];
        editor_project_property_name_format(variable, sizeof(variable),
            project->objects[object_index - 1].name);
        fprintf(source, "    %s_destroy(&objects->%s);\n", variable, variable);
    }
    fprintf(source, "    *objects = (ProjectObjects){0};\n}\n\n");
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
        "    fprintf(stderr, \"error %%d: %%s\\n\", (int)result.result.error,\n"
        "        rohr_error_message_get(result));\n"
        "    return false;\n"
        "}\n\n"
        "int main(void) {\n"
        "    KeyboardState keyboard = {0};\n");
    fprintf(file, "    ProjectObjects objects = {0};\n");
    fprintf(file,
        "    if(!ok(rohr_engine_init()) || !ok(rohr_graphics_start()) ||\n"
        "            !ok(rohr_physics_gravity_set((Acceleration){0.0f, -900.0f}))) goto fail;\n"
        );
    fprintf(file, "    if(!ok(project_objects_create_all(&objects))) goto fail;\n");
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
        "        rohr_graphics_sprite_frames_update(rohr_engine_tick_get(), "
        "rohr_engine_time_get());\n"
        "        rohr_graphics_layer_set(-100);\n"
        "        rohr_graphics_background_draw((Color){18, 22, 30, 255});\n"
        "        rohr_graphics_layer_set(0);\n");
    fprintf(file, "        project_objects_draw_all(&objects);\n");
    fprintf(file, "        rohr_graphics_animated_sprites_draw();\n");
    fprintf(file,
        "        rohr_graphics_show();\n"
        "    }\n"
        "done:\n"
        "    project_objects_destroy_all(&objects);\n"
        "    rohr_graphics_end();\n"
        "    rohr_engine_shutdown();\n"
        "    return 0;\n"
        "fail:\n"
        "    fprintf(stderr, \"Game initialization failed\\n\");\n"
        "    project_objects_destroy_all(&objects);\n"
        "    rohr_graphics_end();\n"
        "    rohr_engine_shutdown();\n"
        "    return 1;\n"
        "}\n");
    return fclose(file) == 0;
}

static bool editor_workspace_scaffold_write(const EditorWorkspace *workspace) {
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    char cmake[2048];
    static const char *gitignore = "build/\n";

    snprintf(cmake, sizeof(cmake),
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(%s LANGUAGES C)\n\n"
        "set(CMAKE_C_STANDARD 99)\n"
        "set(CMAKE_C_STANDARD_REQUIRED ON)\n"
        "if(DEFINED ROHR_ENGINE_SOURCE_ROOT AND "
            "EXISTS \"${ROHR_ENGINE_SOURCE_ROOT}/CMakeLists.txt\")\n"
        "    set(ROHR_BUILD_EXAMPLES OFF CACHE BOOL \"\" FORCE)\n"
        "    set(ROHR_BUILD_TESTS OFF CACHE BOOL \"\" FORCE)\n"
        "    set(ROHR_BUILD_EDITOR OFF CACHE BOOL \"\" FORCE)\n"
        "    set(ROHR_BUILD_CLI OFF CACHE BOOL \"\" FORCE)\n"
        "    add_subdirectory(\"${ROHR_ENGINE_SOURCE_ROOT}\" rohr-engine)\n"
        "    set(ROHR_ENGINE_TARGET rohr_engine)\n"
        "else()\n"
        "    if(DEFINED ROHR_ENGINE_SOURCE_ROOT)\n"
        "        list(PREPEND CMAKE_PREFIX_PATH \"${ROHR_ENGINE_SOURCE_ROOT}\")\n"
        "    endif()\n"
        "    find_package(Rohr CONFIG REQUIRED)\n"
        "    set(ROHR_ENGINE_TARGET Rohr::Engine)\n"
        "endif()\n\n"
        "file(GLOB ROHR_GENERATED_SOURCES CONFIGURE_DEPENDS src/generated/*.c)\n"
        "add_executable(${PROJECT_NAME} src/main.c ${ROHR_GENERATED_SOURCES})\n"
        "target_include_directories(${PROJECT_NAME} PRIVATE src/generated)\n"
        "target_link_libraries(${PROJECT_NAME} PRIVATE ${ROHR_ENGINE_TARGET})\n",
        workspace->config.name);
    return editor_workspace_path_join(path, sizeof(path), workspace->directory,
            "CMakeLists.txt") && editor_workspace_file_write(path, cmake) &&
        editor_workspace_path_join(path, sizeof(path), workspace->directory,
            ".gitignore") && editor_workspace_file_write(path, gitignore);
}

static bool editor_workspace_config_template_copy(const EditorWorkspace *workspace) {
    char source[EDITOR_WORKSPACE_PATH_MAX * 2];
    char destination[EDITOR_WORKSPACE_PATH_MAX * 2];
    size_t size = 0;
    void *contents;
    FILE *file;
    EditorResult result = editor_config_sdk_path_get(source, sizeof(source),
        "project-editor.lua", true);
    if(editor_result_check(result) || !editor_workspace_path_join(destination,
            sizeof(destination), workspace->directory, "editor.lua")) return false;
    contents = SDL_LoadFile(source, &size);
    if(contents == NULL) return false;
    file = fopen(destination, "wb");
    if(file == NULL) {
        SDL_free(contents);
        return false;
    }
    bool written = fwrite(contents, 1, size, file) == size;
    bool closed = fclose(file) == 0;
    SDL_free(contents);
    return written && closed;
}

static bool editor_workspace_starter_assets_copy(const EditorWorkspace *workspace) {
    static const char *names[] = {"tutorial_frame_1.png", "tutorial_frame_2.png"};
    static const char *development_names[] = {
        "examples/flies-around-ball/assets/elder-fly/flying/f1.png",
        "examples/flies-around-ball/assets/elder-fly/flying/f2.png"
    };
    char relative[EDITOR_WORKSPACE_PATH_MAX];
    char source[EDITOR_WORKSPACE_PATH_MAX * 2];
    char destination[EDITOR_WORKSPACE_PATH_MAX * 2];
    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i += 1) {
        EditorResult result;
        if(snprintf(relative, sizeof(relative), "starter-assets/%s", names[i]) >=
                (int)sizeof(relative)) return false;
        result = editor_config_sdk_path_get(source, sizeof(source), relative, false);
        if(editor_result_check(result) || source[0] == '\0') {
            if(snprintf(source, sizeof(source), "%s/%s", ROHR_DEVELOPMENT_SOURCE_DIR,
                    development_names[i]) >= (int)sizeof(source)) return false;
        }
        if(!editor_workspace_path_join(relative, sizeof(relative),
                workspace->config.asset_directory, names[i]) ||
                !editor_workspace_path_join(destination, sizeof(destination),
                    workspace->directory, relative) ||
                !SDL_CopyFile(source, destination)) return false;
    }
    return true;
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
        editor_workspace_generated_objects_write(workspace, project);
}

EditorResult editor_workspace_create(EditorWorkspace *workspace, EditorProject *project,
    const char *directory) {
    EditorWorkspace created = {0};

    if(workspace == NULL || project == NULL || directory == NULL || directory[0] == '\0' ||
            strlen(directory) >= sizeof(created.directory))
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Project creation received an invalid or oversized path");
    if(!SDL_CreateDirectory(directory)) return editor_result_error(
        EDITOR_ERROR_FILE_IO, "Could not create project directory: %s", directory);
    if(!editor_workspace_directory_empty(directory)) return editor_result_error(
        EDITOR_ERROR_FILE_IO, "Project directory is not empty: %s", directory);
    created.config = editor_workspace_config_default_get();
    editor_project_object_name_format(created.config.name,
        sizeof(created.config.name), editor_workspace_basename(directory));
    snprintf(created.directory, sizeof(created.directory), "%s", directory);
    created.open = true;
    if(!editor_workspace_directory_create(directory, created.config.source_directory) ||
            !editor_workspace_directory_create(directory,
                created.config.generated_directory) ||
            !editor_workspace_directory_create(directory, created.config.asset_directory) ||
            !editor_workspace_directory_create(directory, created.config.object_directory)) {
        return editor_result_error(EDITOR_ERROR_FILE_IO,
            "Could not create the project directory structure under: %s", directory);
    }
    if(!editor_workspace_starter_project_init(project)) return editor_result_error(
        EDITOR_ERROR_SCHEMA_INVALID, "Could not initialize the starter project");
    if(!editor_workspace_starter_assets_copy(&created)) return editor_result_error(
        EDITOR_ERROR_FILE_IO, "Could not copy starter assets under: %s", directory);
    if(!editor_workspace_save(&created, project)) return editor_result_error(
        EDITOR_ERROR_FILE_IO, "Could not save the initial project state under: %s", directory);
    if(!editor_workspace_scaffold_write(&created)) return editor_result_error(
        EDITOR_ERROR_FILE_IO, "Could not write the project build scaffold under: %s", directory);
    if(!editor_workspace_config_template_copy(&created)) return editor_result_error(
        EDITOR_ERROR_FILE_IO, "Could not copy editor.lua under: %s", directory);
    if(!editor_workspace_generated_objects_write(&created, project))
        return editor_result_error(EDITOR_ERROR_FILE_IO,
            "Could not write initial generated C under: %s", directory);
    if(!editor_workspace_main_write(&created, project)) return editor_result_error(
        EDITOR_ERROR_FILE_IO, "Could not write initial main.c under: %s", directory);
    *workspace = created;
    return editor_result_value(true);
}

EditorResult editor_workspace_load(EditorWorkspace *workspace, EditorProject *project,
    const char *directory) {
    EditorWorkspace loaded = {0};
    static EditorProject loaded_project;
    char path[EDITOR_WORKSPACE_PATH_MAX * 2];
    char root[EDITOR_WORKSPACE_PATH_MAX];
    EditorResult result;

    if(workspace == NULL || project == NULL || directory == NULL)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Workspace load received an invalid argument");
    result = editor_workspace_root_find(root, sizeof(root), directory, &loaded);
    if(editor_result_check(result)) return result;
    if(!editor_workspace_path_join(path, sizeof(path), root,
            loaded.config.editor_state_file))
        return editor_result_error(EDITOR_ERROR_FILE_IO,
            "Project editor-state path is too long under: %s", root);
    result = editor_project_load(&loaded_project, path);
    if(editor_result_check(result)) return result;
    *workspace = loaded;
    editor_project_destroy(project);
    *project = loaded_project;
    loaded_project = (EditorProject){0};
    return editor_result_value(true);
}

void editor_workspace_close(EditorWorkspace *workspace, EditorProject *project) {
    if(workspace != NULL) *workspace = (EditorWorkspace){0};
    editor_project_destroy(project);
    editor_project_init(project);
}

EditorResult editor_workspace_command_execute(EditorWorkspace *workspace,
        EditorProject *project, const EditorWorkspaceCommand *command) {
    if(workspace == NULL || project == NULL || command == NULL)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Workspace command requires a workspace, project, and command");
    switch(command->type) {
        case EDITOR_WORKSPACE_COMMAND_CREATE:
            return editor_workspace_create(workspace, project, command->directory);
        case EDITOR_WORKSPACE_COMMAND_LOAD:
            return editor_workspace_load(workspace, project, command->directory);
        case EDITOR_WORKSPACE_COMMAND_SAVE:
            if(!editor_workspace_save(workspace, project))
                return editor_result_error(EDITOR_ERROR_FILE_IO,
                    "Could not save project workspace: %s", workspace->directory);
            return editor_result_value(true);
        case EDITOR_WORKSPACE_COMMAND_GENERATE_C:
            if(!editor_workspace_c_generate(workspace, project))
                return editor_result_error(EDITOR_ERROR_FILE_IO,
                    "Could not generate project C source: %s", workspace->directory);
            return editor_result_value(true);
    }
    return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "Unknown workspace command");
}

static bool editor_workspace_command_text_append(char *output, size_t capacity,
        size_t *used, const char *text) {
    size_t length = strlen(text);
    if(*used >= capacity || length >= capacity - *used) return false;
    memcpy(output + *used, text, length);
    *used += length;
    output[*used] = '\0';
    return true;
}

static bool editor_workspace_command_shell_append(char *output, size_t capacity,
        size_t *used, const char *text) {
    if(!editor_workspace_command_text_append(output, capacity, used, "'")) return false;
    for(const char *at = text; *at != '\0'; at += 1) {
        char character[2] = {*at, '\0'};
        if(!editor_workspace_command_text_append(output, capacity, used,
                *at == '\'' ? "'\\''" : character)) return false;
    }
    return editor_workspace_command_text_append(output, capacity, used, "'");
}

EditorResult editor_workspace_command_cli_write(const EditorWorkspaceCommand *command,
        char *output, size_t output_capacity) {
    const char *action;
    size_t used = 0;
    if(command == NULL || output == NULL || output_capacity == 0 ||
            command->directory[0] == '\0')
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Workspace command serialization received an invalid argument");
    if(command->type == EDITOR_WORKSPACE_COMMAND_CREATE) action = "create";
    else if(command->type == EDITOR_WORKSPACE_COMMAND_GENERATE_C) action = "generate-c";
    else return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "Unknown workspace command");
    output[0] = '\0';
    if(!editor_workspace_command_text_append(output, output_capacity, &used,
            "editor-cli --project ") ||
            !editor_workspace_command_shell_append(output, output_capacity, &used,
                command->directory)) goto capacity_error;
    if(!editor_workspace_command_text_append(output, output_capacity, &used, " ") ||
            !editor_workspace_command_text_append(output, output_capacity, &used,
                action)) goto capacity_error;
    return editor_result_value(true);
capacity_error:
    return editor_result_error(EDITOR_ERROR_CAPACITY,
        "Workspace CLI command output buffer is too small");
}
