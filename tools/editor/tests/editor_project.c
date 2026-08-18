#include "editor_project.h"
#include "editor_workspace.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool position_equal(Position a, Position b) {
    return fabsf(a.x - b.x) < 0.001f && fabsf(a.y - b.y) < 0.001f;
}

static void workspace_fixture_remove(const char *root) {
    char path[2048];
    static const char *files[] = {
        "project.rohr.json", "objects/project.rohr.json", "src/main.c",
        "src/generated/project_objects.c", "src/generated/project_objects.h",
        "CMakeLists.txt", ".gitignore", "editor.lua"
    };
    static const char *directories[] = {
        "src/generated", "src", "assets", "objects"
    };

    for(size_t i = 0; i < sizeof(files) / sizeof(files[0]); i += 1) {
        snprintf(path, sizeof(path), "%s/%s", root, files[i]);
        (void)SDL_RemovePath(path);
    }
    for(size_t i = 0; i < sizeof(directories) / sizeof(directories[0]); i += 1) {
        snprintf(path, sizeof(path), "%s/%s", root, directories[i]);
        (void)SDL_RemovePath(path);
    }
    (void)SDL_RemovePath(root);
}

static bool file_contains(const char *path, const char *text) {
    FILE *file;
    char contents[16384];
    size_t length;

    if(path == NULL || text == NULL) return false;
    file = fopen(path, "rb");
    if(file == NULL) return false;
    length = fread(contents, 1, sizeof(contents) - 1, file);
    if(ferror(file)) {
        fclose(file);
        return false;
    }
    contents[length] = '\0';
    fclose(file);
    return strstr(contents, text) != NULL;
}

static bool file_replace(const char *path, const char *text) {
    FILE *file;
    size_t length;
    bool written;

    if(path == NULL || text == NULL) return false;
    file = fopen(path, "wb");
    if(file == NULL) return false;
    length = strlen(text);
    written = fwrite(text, 1, length, file) == length;
    return fclose(file) == 0 && written;
}

int main(void) {
    static EditorProject project;
    EditorObject *object;
    EditorRigidBody *chassis;
    EditorRigidBody *wheel;
    EditorHitbox *hitbox;
    EditorJoint *joint;
    EditorJoint *joint_two;
    EditorAnchor *manual_anchor;
    EditorAnchor *second_anchor;
    EditorAnchorId manual_anchor_id;
    EditorAnchorId second_anchor_id;
    EditorJointId joint_two_id;
    EditorSoftBody *soft_body;
    EditorSoftNode *node_a;
    EditorSoftNode *node_b;
    EditorSoftBeam *beam;
    Position first;
    Position second;
    char formatted[EDITOR_OBJECT_NAME_MAX];

    {
        static EditorProject workspace_project;
        static EditorProject loaded_project;
        EditorWorkspace workspace = {0};
        EditorWorkspace loaded_workspace = {0};
        EditorWorkspaceConfig defaults = editor_workspace_config_default_get();
        EditorWorkspaceCommand workspace_command = {0};
        const char *fixture = "/tmp/rohr_editor_workspace_test";
        SDL_PathInfo info;
        char path[2048];
        char cli_command[4096];

        workspace_fixture_remove(fixture);
        workspace_command.type = EDITOR_WORKSPACE_COMMAND_CREATE;
        snprintf(workspace_command.directory, sizeof(workspace_command.directory),
            "%s", fixture);
        if(editor_result_check(editor_workspace_command_execute(
                    &workspace, &workspace_project, &workspace_command)) ||
                editor_result_check(editor_workspace_command_cli_write(
                    &workspace_command, cli_command, sizeof(cli_command))) ||
                strstr(cli_command, "editor-cli --project ") != cli_command ||
                strstr(cli_command, " create") == NULL) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        workspace_command = (EditorWorkspaceCommand){
            .type = EDITOR_WORKSPACE_COMMAND_LOAD};
        snprintf(workspace_command.directory, sizeof(workspace_command.directory),
            "%s", fixture);
        if(editor_result_check(editor_workspace_command_execute(
                    &loaded_workspace, &loaded_project, &workspace_command))) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        if(defaults.format_version != EDITOR_WORKSPACE_FORMAT_VERSION ||
                strcmp(defaults.source_directory, "src") != 0 ||
                strcmp(defaults.generated_directory, "src/generated") != 0 ||
                strcmp(defaults.editor_state_file,
                    "objects/project.rohr.json") != 0 ||
                !loaded_workspace.open || strcmp(loaded_workspace.config.name,
                    "RohrEditorWorkspaceTest") != 0 ||
                loaded_project.object_count != 1 ||
                strcmp(loaded_project.objects[0].name, "Starter") != 0 ||
                !position_equal(loaded_project.objects[0].position,
                    (Position){0.0f, 0.0f}) ||
                loaded_project.objects[0].rigid_body_count != 2 ||
                !position_equal(loaded_project.objects[0].rigid_bodies[0].position,
                    (Position){0.0f, -200.0f}) ||
                !position_equal(loaded_project.objects[0].rigid_bodies[1].position,
                    (Position){0.0f, 120.0f}) ||
                !loaded_project.objects[0].rigid_bodies[0].static_body ||
                !loaded_project.objects[0].rigid_bodies[1].gravity_enabled ||
                fabsf(loaded_project.objects[0].rigid_bodies[1].mass_value - 5.0f) >
                    0.001f ||
                loaded_project.objects[0].rigid_bodies[0].hitboxes[0].vertex_count != 4 ||
                loaded_project.objects[0].rigid_bodies[1].hitboxes[0].vertex_count != 4) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        snprintf(path, sizeof(path), "%s/src/main.c", fixture);
        if(!SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE ||
                !file_contains(path,
                    "rohr_physics_gravity_set((Acceleration){0.0f, -900.0f})") ||
                !file_contains(path, "starter_create(&starter")) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        if(!file_replace(path, "/* developer-owned main */\n")) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        snprintf(path, sizeof(path), "%s/src/generated/project_objects.c", fixture);
        if(!SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE ||
                !file_contains(path, "EngineResult starter_create") ||
                !file_contains(path,
                    "}}, 5.00000000f, 0.500000000f, 0.00000000f, false") ||
                !file_contains(path, "rohr_physics_collision_category_set") ||
                !file_contains(path, "ROHR_COLLISION_CATEGORY_NONE") ||
                !file_contains(path,
                    "rohr_entity_components_add(*output, ROHR_COLLISION)")) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        loaded_project.objects[0].rigid_bodies[1].mass_value = 7.0f;
        loaded_project.objects[0].rigid_bodies[1].particle = true;
        {
            EditorObject *generated_object = &loaded_project.objects[0];
            EditorRigidBody *generated_body = &generated_object->rigid_bodies[1];
            EditorAnchor *body_anchor = editor_project_anchor_add(&loaded_project,
                generated_object, (Position){12.0f, 0.0f}, generated_body->id);
            EditorAnchor *world_anchor = editor_project_anchor_add(&loaded_project,
                generated_object, (Position){80.0f, 20.0f}, 0);
            EditorJoint *generated_joint = editor_project_joint_add(&loaded_project,
                generated_object, EDITOR_JOINT_SPRING);
            EditorSprite *generated_sprite = editor_project_sprite_add(&loaded_project,
                generated_object, "fly_frame", "assets/fly frame.png");
            EditorAnimatedSprite *generated_animation =
                editor_project_animated_sprite_add(&loaded_project, generated_object);
            EditorSoftBody *generated_soft_body = editor_project_soft_body_add(
                &loaded_project, generated_object);
            if(generated_soft_body != NULL) generated_soft_body->rotation = 0.5f;
            EditorSoftNode *generated_node_a = editor_project_soft_node_add(
                &loaded_project, generated_soft_body, (Position){0.0f, 40.0f});
            EditorSoftNode *generated_node_b = editor_project_soft_node_add(
                &loaded_project, generated_soft_body, (Position){20.0f, 40.0f});
            EditorSoftNode *generated_node_c = editor_project_soft_node_add(
                &loaded_project, generated_soft_body, (Position){10.0f, 60.0f});
            EditorSoftBeam *generated_beam = generated_node_a == NULL ||
                generated_node_b == NULL ? NULL : editor_project_soft_beam_add(
                    &loaded_project, generated_soft_body, generated_node_a->id,
                    generated_node_b->id);
            EditorSoftBeam *generated_beam_b = generated_node_b == NULL ||
                generated_node_c == NULL ? NULL : editor_project_soft_beam_add(
                    &loaded_project, generated_soft_body, generated_node_b->id,
                    generated_node_c->id);
            EditorSoftBeam *generated_beam_c = generated_node_c == NULL ||
                generated_node_a == NULL ? NULL : editor_project_soft_beam_add(
                    &loaded_project, generated_soft_body, generated_node_c->id,
                    generated_node_a->id);
            if(generated_node_a != NULL) {
                generated_node_a->radius = 6.5f;
                generated_node_a->friction = 0.6f;
                generated_node_a->restitution = 0.4f;
            }
            if(generated_sprite != NULL) generated_sprite->size = (Scale){32.0f, 24.0f};
            if(generated_animation != NULL && generated_sprite != NULL) {
                generated_animation->rigid_body = generated_body->id;
                generated_animation->scale = (Scale){2.0f, 3.0f};
                generated_animation->time_per_frame = 0.125;
                generated_animation->follow_body_rotation = false;
                (void)editor_project_animation_frame_add(&loaded_project,
                    generated_animation, "first_frame", "assets/box.png",
                    generated_sprite->size);
            }
            if(generated_beam != NULL) generated_beam->damping = 0.3f;
            if(generated_node_a != NULL) {
                generated_node_a->color = UINT32_C(0xff0000ff);
                generated_node_a->color_overridden = true;
            }
            if(generated_soft_body != NULL && generated_soft_body->area_count == 1) {
                generated_soft_body->areas[0].color = UINT32_C(0x00ff00ff);
                generated_soft_body->areas[0].color_overridden = true;
            }
            if(body_anchor == NULL || world_anchor == NULL || generated_joint == NULL ||
                    generated_soft_body == NULL || generated_node_a == NULL ||
                    generated_node_b == NULL || generated_node_c == NULL ||
                    generated_beam == NULL || generated_beam_b == NULL ||
                    generated_beam_c == NULL || generated_soft_body->area_count != 1 ||
                    generated_sprite == NULL || generated_animation == NULL ||
                    generated_animation->frame_count != 1 ||
                    !editor_project_joint_anchor_set(generated_object,
                        generated_joint, 0, body_anchor->id) ||
                    !editor_project_joint_anchor_set(generated_object,
                        generated_joint, 1, world_anchor->id)) {
                workspace_fixture_remove(fixture);
                return 1;
            }
        }
        workspace_command.type = EDITOR_WORKSPACE_COMMAND_SAVE;
        snprintf(workspace_command.directory, sizeof(workspace_command.directory),
            "%s", fixture);
        if(editor_result_check(editor_workspace_command_execute(
                    &loaded_workspace, &loaded_project, &workspace_command)) ||
                !file_contains(path, "5.00000000f") ||
                file_contains(path, "7.00000000f")) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        workspace_command.type = EDITOR_WORKSPACE_COMMAND_GENERATE_C;
        if(editor_result_check(editor_workspace_command_execute(
                    &loaded_workspace, &loaded_project, &workspace_command)) ||
                editor_result_check(editor_workspace_command_cli_write(
                    &workspace_command, cli_command, sizeof(cli_command))) ||
                strstr(cli_command, "editor-cli --project ") != cli_command ||
                strstr(cli_command, " generate-c") == NULL ||
                !file_contains(path, "7.00000000f") ||
                !file_contains(path, "ROHR_PARTICLE") ||
                !file_contains(path, "generated_world_anchor_create") ||
                !file_contains(path, "rohr_physics_joint_anchor_create") ||
                !file_contains(path, "rohr_physics_joint_spring_set") ||
                !file_contains(path, "rohr_physics_soft_body_create") ||
                !file_contains(path, "rohr_physics_soft_body_node_create") ||
                !file_contains(path, "6.50000000f") ||
                !file_contains(path, "rohr_physics_friction_set") ||
                !file_contains(path, "rohr_physics_restitution_set") ||
                !file_contains(path,
                    "rohr_physics_soft_body_node_collision_filter_set") ||
                !file_contains(path, "rohr_physics_soft_body_beam_create") ||
                !file_contains(path, "rohr_physics_soft_body_triangle_create") ||
                !file_contains(path, "rohr_graphics_soft_body_node_color_set") ||
                !file_contains(path, "rohr_graphics_soft_body_area_color_set") ||
                !file_contains(path, "rohr_graphics_animation_load") ||
                !file_contains(path, "assets/fly frame.png") ||
                !file_contains(path, "animated.follow_entity_rotation = false") ||
                !file_contains(path, "(Scale){2.00000000f, 3.00000000f}") ||
                !file_contains(path, "void starter_draw")) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        snprintf(path, sizeof(path), "%s/src/generated/project_objects.h", fixture);
        if(!file_contains(path, "Entity soft_body_1;") ||
                file_contains(path, "Entity soft_body_soft_body_1;")) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        snprintf(path, sizeof(path), "%s/src/main.c", fixture);
        if(!file_contains(path, "/* developer-owned main */")) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        snprintf(path, sizeof(path), "%s/objects", fixture);
        if(editor_result_check(editor_workspace_load(
                    &loaded_workspace, &loaded_project, path)) ||
                !loaded_workspace.open || loaded_project.object_count != 1) {
            fprintf(stderr, "nested workspace load failed: %s\n", path);
            workspace_fixture_remove(fixture);
            return 1;
        }
        snprintf(path, sizeof(path), "%s/project.rohr.json", fixture);
        if(editor_result_check(editor_workspace_load(
                    &loaded_workspace, &loaded_project, path)) ||
                !loaded_workspace.open || loaded_project.object_count != 1) {
            fprintf(stderr, "manifest workspace load failed: %s\n", path);
            workspace_fixture_remove(fixture);
            return 1;
        }
        snprintf(path, sizeof(path), "%s/CMakeLists.txt", fixture);
        if(!file_contains(path,
                "add_executable(${PROJECT_NAME} src/main.c") ||
                !file_contains(path,
                    "target_link_libraries(${PROJECT_NAME} PRIVATE ${ROHR_ENGINE_TARGET})")) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        editor_workspace_close(&loaded_workspace, &loaded_project);
        if(loaded_workspace.open || loaded_project.object_count != 0) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        workspace_fixture_remove(fixture);
    }

    editor_project_object_name_format(formatted, sizeof(formatted), "fast car");
    if(strcmp(formatted, "FastCar") != 0) return 1;
    editor_project_object_name_format(formatted, sizeof(formatted), "3d box");
    if(strcmp(formatted, "Object3dBox") != 0) return 1;
    editor_project_property_name_format(formatted, sizeof(formatted), "carBody");
    if(strcmp(formatted, "car_body") != 0) return 1;
    editor_project_property_name_format(formatted, sizeof(formatted), "HTTPServer");
    if(strcmp(formatted, "http_server") != 0) return 1;
    editor_project_property_name_format(formatted, sizeof(formatted), "struct");
    if(strcmp(formatted, "item_struct") != 0) return 1;

    editor_project_init(&project);
    object = editor_project_object_add(&project, (Position){10.0f, 20.0f});
    if(object == NULL || strcmp(object->name, "Object1") != 0 ||
            !object->visible || project.selected != object->id) return 1;
    chassis = editor_project_rigid_body_add(&project, object);
    wheel = editor_project_rigid_body_add(&project, object);
    if(chassis == NULL || wheel == NULL || chassis->id == wheel->id ||
            !chassis->visible || chassis->hitbox_count != 1 ||
            wheel->hitbox_count != 1 || fabsf(chassis->mass_value - 1.0f) > 0.001f ||
            fabsf(chassis->friction - 0.5f) > 0.001f ||
            fabsf(chassis->restitution) > 0.001f || chassis->static_body ||
            chassis->rotation_locked || chassis->gravity_enabled ||
            !chassis->particle_auto_fit) return 1;
    if(project.collision_mask_count != 1 ||
            strcmp(project.collision_masks[0].name, "default") != 0 ||
            !chassis->collision_enabled || chassis->collision_category != UINT64_C(1) ||
            chassis->collision_with != UINT64_C(1)) return 1;
    {
        size_t mask_index = SIZE_MAX;
        if(!editor_project_collision_mask_add(&project, "Enemy", &mask_index) ||
                mask_index != 1 || project.collision_mask_count != 2 ||
                strcmp(project.collision_masks[1].name, "enemy") != 0 ||
                editor_project_collision_mask_add(&project, "enemy", &mask_index) ||
                mask_index != 1 || project.collision_mask_count != 2) return 1;
    }
    chassis->collision_category |= UINT64_C(1) << 1;
    chassis->particle = true;
    editor_project_particle_auto_fit_update(&project);
    if(fabsf(chassis->particle_radius -
            editor_project_particle_auto_radius_get(chassis)) > 0.001f ||
            chassis->particle_radius <= 0.0f) return 1;
    chassis->particle_auto_fit = false;
    chassis->particle_radius = 42.0f;
    chassis->particle_ring_color = UINT32_C(0xff8800ff);
    chassis->particle_fill_color = UINT32_C(0x22446680);
    chassis->border_color = UINT32_C(0xabcdef12);
    chassis->surface_color = UINT32_C(0x12345678);
    hitbox = &chassis->hitboxes[0];
    if(hitbox == NULL || !hitbox->visible || hitbox->vertex_count != 3) return 1;
    editor_project_property_name_format(hitbox->vertices[0].name,
        sizeof(hitbox->vertices[0].name), "front Point");
    editor_project_property_name_format(hitbox->line_names[0],
        sizeof(hitbox->line_names[0]), "upperEdge");
    if(strcmp(hitbox->vertices[0].name, "front_point") != 0 ||
            strcmp(hitbox->line_names[0], "upper_edge") != 0) return 1;
    {
        Position local_before = hitbox->vertices[0].position;
        Position world_before = {chassis->position.x + local_before.x,
            chassis->position.y + local_before.y};
        if(!editor_project_rigid_body_origin_set(
                    object, chassis, (Position){5.0f, 7.0f}) ||
                !position_equal((Position){chassis->position.x +
                        hitbox->vertices[0].position.x,
                    chassis->position.y + hitbox->vertices[0].position.y},
                    world_before) || !editor_project_rigid_body_origin_set(
                        object, chassis, (Position){0.0f, 0.0f})) return 1;
    }
    first = hitbox->vertices[0].position;
    second = hitbox->vertices[1].position;
    if(!editor_project_hitbox_vertex_insert(&project, hitbox, 0) ||
            hitbox->vertex_count != 4 ||
            !position_equal(hitbox->vertices[0].position, first) ||
            !position_equal(hitbox->vertices[1].position, (Position){
                (first.x + second.x) * 0.5f, (first.y + second.y) * 0.5f}) ||
            !position_equal(hitbox->vertices[2].position, second) ||
            strcmp(hitbox->line_names[0], "upper_edge") != 0 ||
            strcmp(hitbox->vertices[1].name, "vertex_4") != 0 ||
            !editor_project_hitbox_line_remove(hitbox, 0) ||
            hitbox->vertex_count != 3 ||
            strcmp(hitbox->line_names[0], "upper_edge") != 0) return 1;

    joint = editor_project_joint_add(&project, object, EDITOR_JOINT_SPRING);
    if(joint == NULL || object->anchor_count != 0 || joint->anchor_a != 0 ||
            joint->anchor_b != 0 || fabsf(joint->rest_length) > 0.001f ||
            fabsf(joint->stiffness - 100.0f) > 0.001f ||
            fabsf(joint->damping - 10.0f) > 0.001f ||
            fabsf(joint->visual_size - 1.0f) > 0.001f) return 1;
    joint_two = editor_project_joint_add(&project, object, EDITOR_JOINT_WELD);
    joint_two_id = joint_two == NULL ? 0 : joint_two->id;
    if(joint_two == NULL || joint_two->anchor_a != 0 || joint_two->anchor_b != 0) return 1;
    manual_anchor = editor_project_anchor_add(&project, object,
        (Position){5.0f, 6.0f}, chassis->id);
    second_anchor = editor_project_anchor_add(&project, object,
        (Position){25.0f, 6.0f}, wheel->id);
    manual_anchor_id = manual_anchor == NULL ? 0 : manual_anchor->id;
    second_anchor_id = second_anchor == NULL ? 0 : second_anchor->id;
    if(manual_anchor == NULL || second_anchor == NULL ||
            !editor_project_joint_anchor_set(object, joint, 0, manual_anchor_id) ||
            !editor_project_joint_anchor_set(object, joint, 1, second_anchor_id) ||
            fabsf(joint->rest_length - 20.0f) > 0.001f ||
            !editor_project_joint_remove(object, joint->id) ||
            editor_project_anchor_get(object, manual_anchor_id) == NULL ||
            editor_project_anchor_get(object, second_anchor_id) == NULL) return 1;
    if(!editor_project_joint_anchor_set(object, joint_two, 0, manual_anchor_id) ||
            !editor_project_joint_anchor_set(object, joint_two, 1, second_anchor_id) ||
            !editor_project_joint_remove(object, joint_two_id) ||
            object->anchor_count != 2 ||
            editor_project_anchor_get(object, manual_anchor_id) == NULL ||
            editor_project_anchor_get(object, second_anchor_id) == NULL) return 1;
    manual_anchor = editor_project_anchor_get(object, manual_anchor_id);
    if(manual_anchor == NULL || !editor_project_anchor_position_lock_set(
            object, manual_anchor, false) || !editor_project_anchor_rotation_lock_set(
            object, manual_anchor, false)) return 1;
    chassis->position = (Position){10.0f, 0.0f};
    chassis->rotation = 1.57079632679f;
    if(!editor_project_anchor_position_lock_set(object, manual_anchor, true) ||
            !position_equal(manual_anchor->position, (Position){6.0f, 5.0f}) ||
            !editor_project_anchor_position_lock_set(object, manual_anchor, false) ||
            !position_equal(manual_anchor->position, (Position){5.0f, 6.0f}) ||
            !editor_project_anchor_rotation_lock_set(object, manual_anchor, true) ||
            fabsf(manual_anchor->rotation + 1.57079632679f) > 0.001f ||
            !editor_project_anchor_rotation_lock_set(object, manual_anchor, false) ||
            fabsf(manual_anchor->rotation) > 0.001f) return 1;
    joint = editor_project_joint_add(&project, object, EDITOR_JOINT_SPRING);
    if(joint == NULL || !editor_project_joint_anchor_set(
            object, joint, 0, manual_anchor_id) ||
            !editor_project_joint_anchor_set(object, joint, 1, second_anchor_id) ||
            !editor_project_rigid_body_remove(object, wheel->id) ||
            object->joint_count != 1 ||
            editor_project_anchor_get(object, second_anchor_id) == NULL ||
            editor_project_anchor_get(object, second_anchor_id)->rigid_body != 0 ||
            !editor_project_joint_remove(object, joint->id)) return 1;

    soft_body = editor_project_soft_body_add(&project, object);
    if(soft_body != NULL) {
        soft_body->position = (Position){3.0f, 4.0f};
        soft_body->rotation = 0.75f;
    }
    node_a = editor_project_soft_node_add(&project, soft_body, (Position){0});
    node_b = editor_project_soft_node_add(&project, soft_body, (Position){20.0f, 0.0f});
    if(soft_body == NULL || node_a == NULL || node_b == NULL) return 1;
    node_b->collision_category = UINT64_C(1) | (UINT64_C(1) << 1);
    node_b->collision_with = UINT64_C(1);
    node_b->radius = 7.25f;
    node_b->friction = 0.7f;
    node_b->restitution = 0.35f;
    {
        float cosine = cosf(soft_body->rotation);
        float sine = sinf(soft_body->rotation);
        Position world_before = {
            soft_body->position.x + node_b->position.x * cosine -
                node_b->position.y * sine,
            soft_body->position.y + node_b->position.x * sine +
                node_b->position.y * cosine
        };
        if(!editor_project_soft_body_origin_set(
                    soft_body, (Position){8.0f, 9.0f})) return 1;
        if(!position_equal((Position){soft_body->position.x +
                    node_b->position.x * cosine - node_b->position.y * sine,
                soft_body->position.y + node_b->position.x * sine +
                    node_b->position.y * cosine}, world_before)) return 1;
    }
    if(node_a->gravity_enabled || node_b->gravity_enabled) return 1;
    beam = editor_project_soft_beam_add(&project, soft_body, 0, 0);
    if(beam == NULL || !beam->visible || beam->node_a != 0 || beam->node_b != 0) return 1;
    beam->node_a = node_a->id;
    beam->node_b = node_b->id;
    if(!editor_project_soft_beam_remove(&project, soft_body, beam->id) ||
            soft_body->node_count != 2) return 1;
    beam = editor_project_soft_beam_add(&project, soft_body, node_a->id, node_b->id);
    if(beam != NULL) beam->damping = 0.45f;
    if(beam == NULL ||
            !editor_project_soft_node_remove(&project, soft_body, node_a->id) ||
            soft_body->beam_count != 1 || beam->node_a != 0 ||
            soft_body->node_count != 1 || beam->node_b != soft_body->nodes[0].id) return 1;

    {
        static EditorProject weld_project;
        EditorObject *weld_object;
        EditorRigidBody *body_a;
        EditorRigidBody *body_b;
        EditorAnchor *anchor_a;
        EditorAnchor *anchor_b;
        EditorJoint *weld;

        editor_project_init(&weld_project);
        weld_object = editor_project_object_add(&weld_project, (Position){0});
        body_a = editor_project_rigid_body_add(&weld_project, weld_object);
        body_b = editor_project_rigid_body_add(&weld_project, weld_object);
        if(body_a == NULL || body_b == NULL) return 1;
        body_a->rotation = 0.25f;
        body_b->rotation = 1.0f;
        anchor_a = editor_project_anchor_add(&weld_project, weld_object,
            (Position){0.0f, 0.0f}, body_a->id);
        anchor_b = editor_project_anchor_add(&weld_project, weld_object,
            (Position){20.0f, 0.0f}, body_b->id);
        weld = editor_project_joint_add(&weld_project, weld_object, EDITOR_JOINT_WELD);
        if(anchor_a == NULL || anchor_b == NULL || weld == NULL ||
                !editor_project_joint_anchor_set(
                    weld_object, weld, 0, anchor_a->id) ||
                !editor_project_joint_anchor_set(
                    weld_object, weld, 1, anchor_b->id) ||
                fabsf(weld->rest_angle - 0.75f) > 0.001f) return 1;
        body_a->rotation = 0.5f;
        editor_project_rigid_body_constraints_apply(weld_object, body_a->id);
        if(fabsf(body_b->rotation - 1.25f) > 0.001f) return 1;
    }

    {
        static EditorProject loaded;
        const char *path = "editor_project_round_trip.json";
        EditorObject *loaded_object;

        if(!editor_project_save(&project, path) ||
                editor_result_check(editor_project_load(&loaded, path))) return 1;
        (void)remove(path);
        loaded_object = editor_project_selected_get(&loaded);
        if(loaded_object == NULL || loaded.object_count != project.object_count ||
                loaded_object->id != object->id ||
                loaded_object->rigid_body_count != object->rigid_body_count ||
                loaded_object->anchor_count != object->anchor_count ||
                loaded_object->joint_count != object->joint_count ||
                loaded_object->soft_body_count != object->soft_body_count ||
                loaded_object->hierarchy_count != object->hierarchy_count ||
                memcmp(loaded_object->hierarchy, object->hierarchy,
                    object->hierarchy_count * sizeof(object->hierarchy[0])) != 0 ||
                loaded_object->soft_body_items[0].hierarchy_count !=
                    object->soft_body_items[0].hierarchy_count ||
                memcmp(loaded_object->soft_body_items[0].hierarchy,
                    object->soft_body_items[0].hierarchy,
                    object->soft_body_items[0].hierarchy_count *
                        sizeof(object->soft_body_items[0].hierarchy[0])) != 0 ||
                !position_equal(loaded_object->soft_body_items[0].position,
                    (Position){8.0f, 9.0f}) ||
                fabsf(loaded_object->soft_body_items[0].rotation - 0.75f) > 0.001f ||
                loaded_object->soft_body_items[0].nodes[0].collision_category !=
                    (UINT64_C(1) | (UINT64_C(1) << 1)) ||
                loaded_object->soft_body_items[0].nodes[0].collision_with != UINT64_C(1) ||
                fabsf(loaded_object->soft_body_items[0].nodes[0].radius - 7.25f) >
                    0.001f ||
                fabsf(loaded_object->soft_body_items[0].nodes[0].friction - 0.7f) >
                    0.001f ||
                fabsf(loaded_object->soft_body_items[0].nodes[0].restitution - 0.35f) >
                    0.001f ||
                fabsf(loaded_object->soft_body_items[0].beams[0].damping - 0.45f) >
                    0.001f ||
                strcmp(loaded_object->name, object->name) != 0 ||
                !position_equal(loaded_object->position, object->position) ||
                loaded.next_id != project.next_id ||
                loaded.next_vertex_id != project.next_vertex_id ||
                loaded.next_rigid_body_id != project.next_rigid_body_id ||
                loaded.next_anchor_id != project.next_anchor_id ||
                loaded.next_soft_node_id != project.next_soft_node_id ||
                loaded.next_soft_beam_id != project.next_soft_beam_id ||
                loaded.collision_mask_count != 2 ||
                strcmp(loaded.collision_masks[1].name, "enemy") != 0 ||
                loaded_object->rigid_bodies[0].collision_with != UINT64_C(1) ||
                loaded_object->rigid_bodies[0].collision_category !=
                    (UINT64_C(1) | (UINT64_C(1) << 1)) ||
                !loaded_object->rigid_bodies[0].particle ||
                loaded_object->rigid_bodies[0].particle_auto_fit ||
                fabsf(loaded_object->rigid_bodies[0].particle_radius - 42.0f) > 0.001f ||
                loaded_object->rigid_bodies[0].particle_ring_color !=
                    UINT32_C(0xff8800ff) ||
                loaded_object->rigid_bodies[0].particle_fill_color !=
                    UINT32_C(0x22446680) ||
                loaded_object->rigid_bodies[0].border_color != UINT32_C(0xabcdef12) ||
                loaded_object->rigid_bodies[0].surface_color != UINT32_C(0x12345678) ||
                strcmp(loaded_object->rigid_bodies[0].hitboxes[0].vertices[0].name,
                    "front_point") != 0 ||
                strcmp(loaded_object->rigid_bodies[0].hitboxes[0].line_names[0],
                    "upper_edge") != 0) return 1;
    }

    {
        static EditorProject topology_project;
        EditorObject *topology_object;
        EditorSoftBody *topology_body;
        EditorSoftNode *nodes[6];
        const Position node_positions[6] = {
            {20.0f, 0.0f}, {10.0f, 17.0f}, {-10.0f, 17.0f},
            {-20.0f, 0.0f}, {-10.0f, -17.0f}, {10.0f, -17.0f}
        };
        uint32_t triangles[EDITOR_SOFT_AREA_NODE_MAX - 2][3];
        editor_project_init(&topology_project);
        topology_object = editor_project_object_add(&topology_project, (Position){0});
        topology_body = editor_project_soft_body_add(&topology_project, topology_object);
        if(topology_object == NULL || topology_body == NULL) return 1;
        for(size_t i = 0; i < 6; i += 1) {
            nodes[i] = editor_project_soft_node_add(
                &topology_project, topology_body, node_positions[i]);
            if(nodes[i] == NULL) return 1;
        }
        for(size_t i = 0; i < 6; i += 1) {
            if(editor_project_soft_beam_add(&topology_project, topology_body,
                    nodes[i]->id, nodes[(i + 1) % 6]->id) == NULL) return 1;
        }
        if(topology_body->area_count != 1 || topology_body->areas[0].node_count != 6 ||
                editor_project_soft_area_triangulate(topology_body,
                    &topology_body->areas[0], triangles,
                    EDITOR_SOFT_AREA_NODE_MAX - 2) != 4) return 1;
        if(editor_project_soft_beam_add(&topology_project, topology_body,
                nodes[0]->id, nodes[3]->id) == NULL ||
                topology_body->area_count != 2 ||
                topology_body->areas[0].node_count != 4 ||
                topology_body->areas[1].node_count != 4) return 1;
    }

    {
        static EditorProject concave_project;
        EditorObject *concave_object;
        EditorSoftBody *concave_body;
        EditorSoftNode *nodes[5];
        const Position node_positions[5] = {
            {-30.0f, -20.0f}, {30.0f, -20.0f}, {5.0f, 0.0f},
            {30.0f, 20.0f}, {-30.0f, 20.0f}
        };
        uint32_t triangles[EDITOR_SOFT_AREA_NODE_MAX - 2][3];

        editor_project_init(&concave_project);
        concave_object = editor_project_object_add(&concave_project, (Position){0});
        concave_body = editor_project_soft_body_add(&concave_project, concave_object);
        if(concave_object == NULL || concave_body == NULL) return 1;
        for(size_t i = 0; i < 5; i += 1) {
            nodes[i] = editor_project_soft_node_add(
                &concave_project, concave_body, node_positions[i]);
            if(nodes[i] == NULL) return 1;
        }
        for(size_t i = 0; i < 5; i += 1) {
            if(editor_project_soft_beam_add(&concave_project, concave_body,
                    nodes[i]->id, nodes[(i + 1) % 5]->id) == NULL) return 1;
        }
        if(concave_body->area_count != 1 || concave_body->areas[0].node_count != 5 ||
                editor_project_soft_area_triangulate(concave_body,
                    &concave_body->areas[0], triangles,
                    EDITOR_SOFT_AREA_NODE_MAX - 2) != 3) return 1;
    }

    {
        static EditorProject disconnected_project;
        EditorObject *disconnected_object;
        EditorSoftBody *disconnected_body;
        EditorSoftNode *nodes[6];
        const Position node_positions[6] = {
            {-50.0f, -10.0f}, {-30.0f, -10.0f}, {-40.0f, 10.0f},
            {30.0f, -10.0f}, {50.0f, -10.0f}, {40.0f, 10.0f}
        };

        editor_project_init(&disconnected_project);
        disconnected_object = editor_project_object_add(
            &disconnected_project, (Position){0});
        disconnected_body = editor_project_soft_body_add(
            &disconnected_project, disconnected_object);
        if(disconnected_object == NULL || disconnected_body == NULL) return 1;
        for(size_t i = 0; i < 6; i += 1) {
            nodes[i] = editor_project_soft_node_add(
                &disconnected_project, disconnected_body, node_positions[i]);
            if(nodes[i] == NULL) return 1;
        }
        for(size_t triangle = 0; triangle < 2; triangle += 1) {
            size_t first_node = triangle * 3;
            for(size_t edge = 0; edge < 3; edge += 1) {
                if(editor_project_soft_beam_add(&disconnected_project,
                        disconnected_body, nodes[first_node + edge]->id,
                        nodes[first_node + (edge + 1) % 3]->id) == NULL) return 1;
            }
        }
        if(disconnected_body->area_count != 2 ||
                disconnected_body->areas[0].node_count != 3 ||
                disconnected_body->areas[1].node_count != 3) return 1;
    }

    {
        static EditorProject nested_project;
        EditorObject *nested_object;
        EditorSoftBody *nested_body;
        EditorSoftNode *nodes[8];
        const Position node_positions[8] = {
            {-40.0f, -40.0f}, {40.0f, -40.0f}, {40.0f, 40.0f}, {-40.0f, 40.0f},
            {-10.0f, -10.0f}, {10.0f, -10.0f}, {10.0f, 10.0f}, {-10.0f, 10.0f}
        };

        editor_project_init(&nested_project);
        nested_object = editor_project_object_add(&nested_project, (Position){0});
        nested_body = editor_project_soft_body_add(&nested_project, nested_object);
        if(nested_object == NULL || nested_body == NULL) return 1;
        for(size_t i = 0; i < 8; i += 1) {
            nodes[i] = editor_project_soft_node_add(
                &nested_project, nested_body, node_positions[i]);
            if(nodes[i] == NULL) return 1;
        }
        for(size_t loop = 0; loop < 2; loop += 1) {
            size_t first_node = loop * 4;
            for(size_t edge = 0; edge < 4; edge += 1) {
                if(editor_project_soft_beam_add(&nested_project, nested_body,
                        nodes[first_node + edge]->id,
                        nodes[first_node + (edge + 1) % 4]->id) == NULL) return 1;
            }
        }
        if(nested_body->area_count != 2 || nested_body->areas[0].node_count != 4 ||
                nested_body->areas[1].node_count != 4) return 1;
    }

    {
        static EditorProject invalid_version_project;
        const char *path = "editor_project_invalid_version.json";
        EditorResult result;

        if(!file_replace(path, "{\"format_version\":99}")) return 1;
        result = editor_project_load(&invalid_version_project, path);
        (void)remove(path);
        if(!editor_result_check(result) ||
                result.result.error.code != EDITOR_ERROR_SCHEMA_VERSION ||
                strstr(result.result.error.message, "format_version 99") == NULL ||
                strstr(result.result.error.message, "requires 1") == NULL) return 1;
    }

    {
        static EditorProject invalid_project;
        EditorObject *invalid_object;
        EditorSoftBody *invalid_body;
        EditorSoftNode *nodes[4];
        const Position node_positions[4] = {
            {-20.0f, 0.0f}, {0.0f, 20.0f}, {20.0f, 0.0f}, {0.0f, -20.0f}
        };

        editor_project_init(&invalid_project);
        invalid_object = editor_project_object_add(&invalid_project, (Position){0});
        invalid_body = editor_project_soft_body_add(&invalid_project, invalid_object);
        if(invalid_object == NULL || invalid_body == NULL) return 1;
        for(size_t i = 0; i < 4; i += 1) {
            nodes[i] = editor_project_soft_node_add(
                &invalid_project, invalid_body, node_positions[i]);
            if(nodes[i] == NULL) return 1;
        }
        for(size_t i = 0; i < 3; i += 1) {
            if(editor_project_soft_beam_add(&invalid_project, invalid_body,
                    nodes[i]->id, nodes[i + 1]->id) == NULL) return 1;
        }
        if(invalid_body->area_count != 0 ||
                editor_project_soft_beam_add(&invalid_project, invalid_body,
                    nodes[3]->id, 0) == NULL || invalid_body->area_count != 0)
            return 1;
    }

    {
        static EditorProject invalid_project;
        const char *path = "editor_project_invalid.json";
        EditorResult result;

        if(!file_replace(path, "{not valid json")) return 1;
        result = editor_project_load(&invalid_project, path);
        if(!editor_result_check(result) ||
                result.result.error.code != EDITOR_ERROR_JSON_PARSE ||
                strstr(result.result.error.message, "byte") == NULL) return 1;
        if(!file_replace(path, "{\"objects\":[]}")) return 1;
        result = editor_project_load(&invalid_project, path);
        (void)remove(path);
        if(!editor_result_check(result) ||
                result.result.error.code != EDITOR_ERROR_SCHEMA_VERSION ||
                strstr(result.result.error.message, "missing integer format_version") == NULL)
            return 1;
    }

    {
        static EditorProject reference_project;
        static EditorProject ignored_project;
        EditorObject *reference_object;
        EditorRigidBody *reference_body;
        const char *path = "editor_project_invalid_reference.json";
        EditorResult result;

        editor_project_init(&reference_project);
        reference_object = editor_project_object_add(
            &reference_project, (Position){0});
        if(reference_object == NULL) return 1;
        reference_body = editor_project_rigid_body_add(
            &reference_project, reference_object);
        if(reference_body == NULL) return 1;
        reference_body->collision_category = UINT64_C(1) << 4;
        if(!editor_project_save(&reference_project, path)) return 1;
        result = editor_project_load(&ignored_project, path);
        (void)remove(path);
        if(!editor_result_check(result) ||
                result.result.error.code != EDITOR_ERROR_REFERENCE_INVALID ||
                strstr(result.result.error.message, "collision-mask reference") == NULL)
            return 1;
    }

    {
        static EditorProject manifest_project;
        EditorWorkspace manifest_workspace = {0};
        const char *fixture = "/tmp/rohr_editor_manifest_error_test";
        char path[2048];
        EditorResult result;

        workspace_fixture_remove(fixture);
        if(!SDL_CreateDirectory(fixture)) return 1;
        snprintf(path, sizeof(path), "%s/project.rohr.json", fixture);
        if(!file_replace(path,
                "{\"format_version\":1,\"editor_state_file\":"
                "\"objects/project.rohr.json\"}")) return 1;
        result = editor_workspace_load(&manifest_workspace, &manifest_project, fixture);
        workspace_fixture_remove(fixture);
        if(!editor_result_check(result) ||
                result.result.error.code != EDITOR_ERROR_SCHEMA_INVALID ||
                strstr(result.result.error.message, "field 'name'") == NULL) return 1;
    }

    {
        static EditorProject creation_project;
        EditorWorkspace creation_workspace = {0};
        const char *fixture = "/tmp/rohr_editor_creation_error_test";
        char path[2048];
        EditorResult result;

        workspace_fixture_remove(fixture);
        if(!SDL_CreateDirectory(fixture)) return 1;
        snprintf(path, sizeof(path), "%s/project.rohr.json", fixture);
        if(!file_replace(path, "occupied")) return 1;
        result = editor_workspace_create(
            &creation_workspace, &creation_project, fixture);
        workspace_fixture_remove(fixture);
        if(!editor_result_check(result) || result.result.error.code != EDITOR_ERROR_FILE_IO ||
                strstr(result.result.error.message, "not empty") == NULL) return 1;
    }

    {
        static EditorProject creation_project;
        EditorWorkspace creation_workspace = {0};
        const char *fixture = "/tmp/rohr_editor_config_creation_test";
        char path[2048];
        SDL_PathInfo info;
        EditorResult result;

        workspace_fixture_remove(fixture);
        result = editor_workspace_create(&creation_workspace, &creation_project, fixture);
        snprintf(path, sizeof(path), "%s/editor.lua", fixture);
        if(editor_result_check(result) || !SDL_GetPathInfo(path, &info) ||
                info.type != SDL_PATHTYPE_FILE) {
            workspace_fixture_remove(fixture);
            return 1;
        }
        workspace_fixture_remove(fixture);
    }

    {
        EditorProject dynamic_project;
        EditorObject *dynamic_object;
        editor_project_init(&dynamic_project);
        dynamic_object = editor_project_object_add(&dynamic_project, (Position){0});
        if(dynamic_object == NULL) return 1;
        for(size_t i = 0; i < EDITOR_RIGID_BODY_MAX + 1; i += 1)
            if(editor_project_rigid_body_add(&dynamic_project,
                    dynamic_object) == NULL) return 1;
        for(size_t i = 0; i < EDITOR_SOFT_BODY_MAX + 1; i += 1)
            if(editor_project_soft_body_add(&dynamic_project,
                    dynamic_object) == NULL) return 1;
        for(size_t i = 0; i < EDITOR_SOFT_NODE_MAX + 1; i += 1)
            if(editor_project_soft_node_add(&dynamic_project,
                    &dynamic_object->soft_body_items[0],
                    (Position){(float)i, 0.0f}) == NULL) return 1;
        if(dynamic_object->rigid_body_count != EDITOR_RIGID_BODY_MAX + 1 ||
                dynamic_object->soft_body_count != EDITOR_SOFT_BODY_MAX + 1 ||
                dynamic_object->soft_body_items[0].node_count !=
                    EDITOR_SOFT_NODE_MAX + 1) return 1;
        editor_project_destroy(&dynamic_project);
    }

    editor_project_selection_clear(&project);
    if(editor_project_selected_get(&project) != NULL ||
            !editor_project_object_select(&project, object->id) ||
            !editor_project_object_remove(&project, object->id) ||
            project.object_count != 0) return 1;
    return 0;
}
