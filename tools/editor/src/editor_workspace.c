#include "editor_workspace.h"

#include "yyjson.h"

#include <math.h>
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
                uint32_t triangles[EDITOR_SOFT_AREA_NODE_MAX - 2][3];
                size_t triangle_count = editor_project_soft_area_triangulate(
                    body, area, triangles, EDITOR_SOFT_AREA_NODE_MAX - 2);
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
        fprintf(file, "        %s_draw(&%s);\n", variable, variable);
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
        "add_executable(${PROJECT_NAME} src/main.c ${ROHR_GENERATED_SOURCES})\n"
        "target_include_directories(${PROJECT_NAME} PRIVATE src/generated)\n"
        "target_link_libraries(${PROJECT_NAME} PRIVATE rohr_engine)\n",
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
        editor_workspace_generated_objects_write(workspace, project);
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
            !editor_workspace_generated_objects_write(&created, project) ||
            !editor_workspace_main_write(&created, project)) return false;
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
