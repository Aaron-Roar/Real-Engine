/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "editor_command.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLI_MAX 128
#define CLI_TEXT 1024

static bool cli_selector_flag(const char *value) {
    static const char *flags[] = {"--object", "--object-id", "--body", "--body-id",
        "--hitbox", "--hitbox-id", "--joint", "--joint-id", "--anchor",
        "--anchor-id", "--soft-body", "--soft-body-id", "--node", "--node-id",
        "--beam", "--beam-id", "--area", "--area-id", "--vertex", "--vertex-id", "--line",
        "--line-index", "--node-a", "--node-a-id", "--node-b", "--node-b-id",
        "--collision-mask", "--sprite", "--sprite-id", "--animated-sprite",
        "--animated-sprite-id", "--frame-index"};
    for(size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i += 1)
        if(strcmp(value, flags[i]) == 0) return true;
    return false;
}

static bool cli_text_add(char *output, size_t capacity, size_t *used,
        const char *value) {
    size_t length = strlen(value);
    if(*used >= capacity || length >= capacity - *used) return false;
    memcpy(output + *used, value, length + 1); *used += length; return true;
}

static bool cli_safe(const char *value) {
    if(value == NULL || value[0] == '\0') return false;
    for(const unsigned char *at = (const unsigned char *)value; *at; at += 1)
        if(!( (*at >= 'a' && *at <= 'z') || (*at >= 'A' && *at <= 'Z') ||
                (*at >= '0' && *at <= '9') || strchr("_-./:", *at))) return false;
    return true;
}

static bool cli_token_add(char *output, size_t capacity, size_t *used,
        const char *value) {
    if(*used && !cli_text_add(output, capacity, used, " ")) return false;
    if(cli_safe(value)) return cli_text_add(output, capacity, used, value);
    if(!cli_text_add(output, capacity, used, "'")) return false;
    for(const char *at = value; *at; at += 1) {
        char character[2] = {*at, '\0'};
        if(!cli_text_add(output, capacity, used,
                *at == '\'' ? "'\\''" : character)) return false;
    }
    return cli_text_add(output, capacity, used, "'");
}

static size_t cli_tokens(const char *text, char tokens[][CLI_TEXT]) {
    size_t count = 0;
    while(*text && count < CLI_MAX) {
        size_t used = 0; bool quoted = false;
        while(*text == ' ') text += 1;
        if(!*text) break;
        while(*text && (quoted || *text != ' ')) {
            if(*text == '\'') { quoted = !quoted; text += 1; continue; }
            if(*text == '\\' && !quoted && text[1]) text += 1;
            if(used + 1 < CLI_TEXT) tokens[count][used++] = *text;
            text += 1;
        }
        tokens[count][used] = '\0'; count += 1;
    }
    return count;
}

static const char *cli_item_flag(EditorItemKind kind) {
    static const char *flags[] = {"--object", "--body", "--hitbox", "--joint",
        "--anchor", "--soft-body", "--node", "--beam", "--vertex", "--line"};
    if(kind == EDITOR_ITEM_SOFT_AREA) return "--area";
    return kind <= EDITOR_ITEM_LINE ? flags[kind] : NULL;
}

static const char *cli_property(const char *domain, const char *action) {
    if(strcmp(action, "visibility") == 0) return "visibility";
    if(strcmp(action, "position") == 0) return "position";
    if(strcmp(action, "transform") == 0) return "transform";
    if(strcmp(action, "origin") == 0) return "origin";
    if(strcmp(action, "auto-shape") == 0) return "auto-shape";
    if(strcmp(action, "camera") == 0) return "camera";
    if(strcmp(action, "coordinates") == 0) return "coordinates";
    if(strcmp(domain, "navigation") == 0) return "navigation";
    return NULL;
}

static const EditorObject *cli_object_get(const EditorProject *project,
        EditorObjectId id) {
    if(project != NULL) for(size_t i = 0; i < project->object_count; i += 1)
        if(project->objects[i].id == id) return &project->objects[i];
    return NULL;
}

static bool cli_named_selector_add(char *output, size_t capacity, size_t *used,
        const char *name_flag, const char *id_flag, const char *name, uint32_t id,
        size_t matches) {
    char number[16];
    if(name != NULL && name[0] != '\0' && matches == 1)
        return cli_token_add(output, capacity, used, name_flag) &&
            cli_token_add(output, capacity, used, name);
    snprintf(number, sizeof(number), "%u", id);
    return cli_token_add(output, capacity, used, id_flag) &&
        cli_token_add(output, capacity, used, number);
}

static bool cli_sprite_selector_add(const EditorObject *object, EditorSpriteId id,
        char *output, size_t capacity, size_t *used) {
    const char *name = NULL; size_t matches = 0;
    for(size_t i = 0; object != NULL && i < object->sprite_count; i += 1)
        if(object->sprites[i].id == id) name = object->sprites[i].name;
    if(name != NULL) for(size_t i = 0; i < object->sprite_count; i += 1)
        if(strcmp(object->sprites[i].name, name) == 0) matches += 1;
    return cli_named_selector_add(output, capacity, used, "--sprite", "--sprite-id",
        name, id, matches);
}

static bool cli_object_selector_add(const EditorProject *project, EditorObjectId id,
        char *output, size_t capacity, size_t *used) {
    const char *name = NULL; size_t matches = 0;
    for(size_t i = 0; i < project->object_count; i += 1)
        if(project->objects[i].id == id) name = project->objects[i].name;
    if(name != NULL) for(size_t i = 0; i < project->object_count; i += 1)
        if(strcmp(project->objects[i].name, name) == 0) matches += 1;
    return cli_named_selector_add(output, capacity, used, "--object", "--object-id",
        name, id, matches);
}

static bool cli_animated_selector_add(const EditorObject *object,
        EditorAnimatedSpriteId id, char *output, size_t capacity, size_t *used) {
    const char *name = NULL; size_t matches = 0;
    if(object != NULL) for(size_t i = 0; i < object->animated_sprite_count; i += 1)
        if(object->animated_sprite_items[i].id == id)
            name = object->animated_sprite_items[i].name;
    if(name != NULL) for(size_t i = 0; i < object->animated_sprite_count; i += 1)
        if(strcmp(object->animated_sprite_items[i].name, name) == 0) matches += 1;
    return cli_named_selector_add(output, capacity, used, "--animated-sprite",
        "--animated-sprite-id", name, id, matches);
}

static EditorResult cli_sprite_command_write(const EditorProject *project,
        const EditorCommand *command, const char *path, char *output, size_t capacity,
        bool *handled) {
    size_t used = 0; char number[64]; EditorObjectId object_id = 0;
    EditorAnimatedSpriteId animated_id = 0; const EditorObject *object;
    *handled = command->type >= EDITOR_COMMAND_SPRITE_ADD &&
        command->type <= EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET;
    if(!*handled) return editor_result_value(true);
    output[0] = '\0';
#define ADD(value) do { if(!cli_token_add(output, capacity, &used, (value))) goto full; } while(0)
    ADD("rohr-cli"); ADD("--project"); ADD(path);
    if(command->type >= EDITOR_COMMAND_SPRITE_ADD &&
            command->type <= EDITOR_COMMAND_SPRITE_VISIBILITY_SET) {
        object_id = command->type == EDITOR_COMMAND_SPRITE_ADD ?
            command->data.sprite_add.object : command->type == EDITOR_COMMAND_SPRITE_REMOVE ?
            command->data.sprite_remove.object : command->type == EDITOR_COMMAND_SPRITE_RENAME ?
            command->data.sprite_rename.object : command->type == EDITOR_COMMAND_SPRITE_PATH_SET ?
            command->data.sprite_path_set.object :
            command->type == EDITOR_COMMAND_SPRITE_POSITION_SET ?
                command->data.sprite_position_set.object :
            command->type == EDITOR_COMMAND_SPRITE_ROTATION_SET ?
                command->data.sprite_rotation_set.object :
            command->type == EDITOR_COMMAND_SPRITE_SIZE_SET ?
                command->data.sprite_size_set.object :
            command->type == EDITOR_COMMAND_SPRITE_BODY_SET ?
                command->data.sprite_body_set.object :
            command->type == EDITOR_COMMAND_SPRITE_FOLLOW_ROTATION_SET ?
                command->data.sprite_boolean_set.object :
                command->data.sprite_visibility_set.object;
        object = cli_object_get(project, object_id);
        if(!cli_object_selector_add(project, object_id, output, capacity, &used)) goto full;
        EditorSpriteId id = command->type == EDITOR_COMMAND_SPRITE_REMOVE ?
            command->data.sprite_remove.sprite : command->type == EDITOR_COMMAND_SPRITE_RENAME ?
            command->data.sprite_rename.sprite : command->type == EDITOR_COMMAND_SPRITE_PATH_SET ?
            command->data.sprite_path_set.sprite :
            command->type == EDITOR_COMMAND_SPRITE_POSITION_SET ?
                command->data.sprite_position_set.sprite :
            command->type == EDITOR_COMMAND_SPRITE_ROTATION_SET ?
                command->data.sprite_rotation_set.sprite :
            command->type == EDITOR_COMMAND_SPRITE_SIZE_SET ?
                command->data.sprite_size_set.sprite :
            command->type == EDITOR_COMMAND_SPRITE_BODY_SET ?
                command->data.sprite_body_set.sprite :
            command->type == EDITOR_COMMAND_SPRITE_FOLLOW_ROTATION_SET ?
                command->data.sprite_boolean_set.sprite :
                command->data.sprite_visibility_set.sprite;
        if(command->type == EDITOR_COMMAND_SPRITE_ADD) {
            ADD("--sprite"); ADD(command->data.sprite_add.name); ADD("add");
            ADD(command->data.sprite_add.path);
            snprintf(number, sizeof(number), "%.9g", command->data.sprite_add.size.x); ADD(number);
            snprintf(number, sizeof(number), "%.9g", command->data.sprite_add.size.y); ADD(number);
        } else {
            if(!cli_sprite_selector_add(object, id, output, capacity, &used)) goto full;
            if(command->type == EDITOR_COMMAND_SPRITE_REMOVE) ADD("delete");
            else if(command->type == EDITOR_COMMAND_SPRITE_RENAME) {
                ADD("rename"); ADD(command->data.sprite_rename.name);
            } else {
                ADD("--property");
                if(command->type == EDITOR_COMMAND_SPRITE_PATH_SET) {
                    ADD("path"); ADD(command->data.sprite_path_set.path);
                } else if(command->type == EDITOR_COMMAND_SPRITE_POSITION_SET) {
                    ADD("position");
                    snprintf(number, sizeof(number), "%.9g",
                        command->data.sprite_position_set.position.x); ADD(number);
                    snprintf(number, sizeof(number), "%.9g",
                        command->data.sprite_position_set.position.y); ADD(number);
                } else if(command->type == EDITOR_COMMAND_SPRITE_ROTATION_SET) {
                    ADD("rotation");
                    snprintf(number, sizeof(number), "%.9g",
                        command->data.sprite_rotation_set.rotation); ADD(number);
                } else if(command->type == EDITOR_COMMAND_SPRITE_SIZE_SET) {
                    ADD("size");
                    snprintf(number, sizeof(number), "%.9g", command->data.sprite_size_set.size.x); ADD(number);
                    snprintf(number, sizeof(number), "%.9g", command->data.sprite_size_set.size.y); ADD(number);
                } else if(command->type == EDITOR_COMMAND_SPRITE_BODY_SET) {
                    ADD("body");
                    if(command->data.sprite_body_set.body == 0) ADD("none");
                    else {
                        const EditorRigidBody *body = editor_project_rigid_body_get(
                            (EditorObject *)object, command->data.sprite_body_set.body);
                        ADD(body != NULL ? body->name : "none");
                    }
                } else if(command->type == EDITOR_COMMAND_SPRITE_FOLLOW_ROTATION_SET) {
                    ADD("follow-body-rotation");
                    ADD(command->data.sprite_boolean_set.enabled ? "true" : "false");
                } else {
                    ADD("visibility");
                    ADD(command->data.sprite_visibility_set.visible ? "true" : "false");
                }
            }
        }
        return editor_result_value(true);
    }
    object_id = command->data.animated_sprite_remove.object;
    animated_id = command->data.animated_sprite_remove.sprite;
    if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_ADD)
        object_id = command->data.animated_sprite_add.object;
    object = cli_object_get(project, object_id);
    if(!cli_object_selector_add(project, object_id, output, capacity, &used)) goto full;
    if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_ADD) {
        ADD("--animated-sprite"); ADD(command->data.animated_sprite_add.name); ADD("add");
        return editor_result_value(true);
    }
    if(!cli_animated_selector_add(object, animated_id, output, capacity, &used)) goto full;
    if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_REMOVE) ADD("delete");
    else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_RENAME) {
        ADD("rename"); ADD(command->data.animated_sprite_rename.name);
    } else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_ADD) {
        ADD("frame-add");
        ADD(command->data.animation_frame_add.name);
        ADD(command->data.animation_frame_add.path);
        snprintf(number, sizeof(number), "%.9g", command->data.animation_frame_add.size.x);
        ADD(number);
        snprintf(number, sizeof(number), "%.9g", command->data.animation_frame_add.size.y);
        ADD(number);
    } else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_REMOVE) {
        ADD("--frame-index");
        snprintf(number, sizeof(number), "%zu", command->data.animation_frame_remove.index); ADD(number);
        ADD("frame-delete");
    } else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_RENAME ||
            command->type == EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET ||
            command->type == EDITOR_COMMAND_ANIMATION_FRAME_SIZE_SET) {
        size_t index = command->type == EDITOR_COMMAND_ANIMATION_FRAME_RENAME ?
            command->data.animation_frame_rename.index :
            command->type == EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET ?
                command->data.animation_frame_path_set.index :
                command->data.animation_frame_size_set.index;
        ADD("--frame-index");
        snprintf(number, sizeof(number), "%zu", index); ADD(number);
        if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_RENAME) {
            ADD("frame-rename"); ADD(command->data.animation_frame_rename.name);
        } else if(command->type == EDITOR_COMMAND_ANIMATION_FRAME_PATH_SET) {
            ADD("frame-path-set"); ADD(command->data.animation_frame_path_set.path);
        } else {
            ADD("frame-size-set");
            snprintf(number, sizeof(number), "%.9g",
                command->data.animation_frame_size_set.size.x); ADD(number);
            snprintf(number, sizeof(number), "%.9g",
                command->data.animation_frame_size_set.size.y); ADD(number);
        }
    } else {
        ADD("--property");
        if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_BODY_SET) {
            ADD("body");
            if(command->data.animated_sprite_body_set.body == 0) ADD("none");
            else {
                const EditorRigidBody *body = editor_project_rigid_body_get(
                    (EditorObject *)object, command->data.animated_sprite_body_set.body);
                ADD(body != NULL ? body->name : "none");
            }
        } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_POSITION_SET) {
            ADD("position");
            snprintf(number, sizeof(number), "%.9g",
                command->data.animated_sprite_position_set.position.x); ADD(number);
            snprintf(number, sizeof(number), "%.9g",
                command->data.animated_sprite_position_set.position.y); ADD(number);
        } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_ROTATION_SET) {
            ADD("rotation");
            snprintf(number, sizeof(number), "%.9g",
                command->data.animated_sprite_rotation_set.rotation); ADD(number);
        } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_SCALE_SET) {
            ADD("scale");
            snprintf(number, sizeof(number), "%.9g", command->data.animated_sprite_scale_set.scale.x); ADD(number);
            snprintf(number, sizeof(number), "%.9g", command->data.animated_sprite_scale_set.scale.y); ADD(number);
        } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_TIMING_SET) {
            ADD("timing");
            snprintf(number, sizeof(number), "%llu", (unsigned long long)command->data.animated_sprite_timing_set.ticks); ADD(number);
            snprintf(number, sizeof(number), "%.17g", (double)command->data.animated_sprite_timing_set.time); ADD(number);
        } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_STARTING_FRAME_SET) {
            ADD("starting-frame"); snprintf(number, sizeof(number), "%u", command->data.animated_sprite_starting_frame_set.frame); ADD(number);
        } else if(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_DIRECTION_SET) {
            ADD("direction"); ADD(command->data.animated_sprite_direction_set.direction == DIRECTION_LEFT ? "left" : "right");
        } else {
            ADD(command->type == EDITOR_COMMAND_ANIMATED_SPRITE_VISIBILITY_SET ?
                "visibility" : command->type ==
                    EDITOR_COMMAND_ANIMATED_SPRITE_PLAYING_SET ?
                    "playing" : "follow-body-rotation");
            ADD(command->data.animated_sprite_boolean_set.enabled ? "true" : "false");
        }
    }
    return editor_result_value(true);
full:
    return editor_result_error(EDITOR_ERROR_CAPACITY,
        "Selector-first sprite command exceeds output capacity");
#undef ADD
}

EditorResult editor_command_cli_standard_write(const EditorProject *project,
        const EditorCommand *command, const EditorCommandResult *result,
        const char *path, char *output, size_t capacity) {
    char legacy[4096], token[CLI_MAX][CLI_TEXT];
    size_t count, at = 4, used = 0;
    const char *property;
    bool handled = false;
    EditorResult special = cli_sprite_command_write(project, command, path,
        output, capacity, &handled);
    if(handled) return special;
    EditorResult serialized = editor_command_cli_named_write(project, command,
        path, legacy, sizeof(legacy));
    if(editor_result_check(serialized)) return serialized;
    count = cli_tokens(legacy, token);
    if(count < 4) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "Could not serialize selector-first command");
    output[0] = '\0';
#define ADD(value) do { if(!cli_token_add(output, capacity, &used, (value))) goto full; } while(0)
    ADD("rohr-cli"); ADD("--project"); ADD(token[3]);
    if(command->type == EDITOR_COMMAND_NAVIGATION_SET) {
        if(count != 17) goto invalid;
        for(size_t i = 7; i + 1 < count; i += 2) { ADD(token[i]); ADD(token[i + 1]); }
        ADD("--property"); ADD("navigation"); ADD(token[4]); ADD(token[5]); ADD(token[6]);
        return editor_result_value(true);
    }
    if(command->type == EDITOR_COMMAND_OBJECT_ADD) {
        ADD("--object"); ADD(command->data.object_add.name); at = 5;
    } else if(command->type == EDITOR_COMMAND_COLLISION_MASK_ADD) {
        ADD("--collision-mask"); ADD(command->data.collision_mask_add.name); at = 5;
    } else while(at + 1 < count && cli_selector_flag(token[at])) {
        ADD(token[at]); ADD(token[at + 1]); at += 2;
    }
    if(command->type == EDITOR_COMMAND_ITEM_ADD && result != NULL &&
            result->created.valid) {
        const char *flag = cli_item_flag(result->created.kind);
        if(flag == NULL || result->created.name[0] == '\0') goto invalid;
        ADD(flag); ADD(result->created.name);
    }
    if(strcmp(token[2], "add") == 0 || strcmp(token[2], "delete") == 0 ||
            strcmp(token[2], "rename") == 0) ADD(token[2]);
    else {
        ADD("--property");
        if(strcmp(token[2], "set") == 0 || strcmp(token[2], "filter") == 0 ||
                strcmp(token[2], "connect") == 0) {
            if(at >= count) goto invalid;
            ADD(token[at++]);
        } else {
            property = cli_property(token[1], token[2]);
            if(property == NULL) goto invalid;
            ADD(property);
        }
    }
    for(; at < count; at += 1) ADD(token[at]);
#undef ADD
    return editor_result_value(true);
invalid:
    return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "Command cannot be represented by selector-first grammar");
full:
    return editor_result_error(EDITOR_ERROR_CAPACITY,
        "Selector-first command exceeds output capacity");
}

typedef struct CliInput {
    char *selectors[CLI_MAX];
    size_t selector_count;
    const char *path;
    int terminal;
    const char *domain;
    const char *target_flag;
    const char *target_name;
    int rank;
} CliInput;

static void cli_target_consider(CliInput *input, const char *flag, const char *name) {
    int rank; const char *domain;
    if(strstr(flag, "frame-index")) return;
    if(strstr(flag, "animated-sprite")) { rank = 2; domain = "animated-sprite"; }
    else if(strstr(flag, "sprite")) { rank = 1; domain = "sprite"; }
    else if(strstr(flag, "vertex")) { rank = 3; domain = "vertex"; }
    else if(strstr(flag, "line")) { rank = 3; domain = "line"; }
    else if(strstr(flag, "hitbox")) { rank = 2; domain = "hitbox"; }
    else if(strstr(flag, "collision-mask")) { rank = 1; domain = "collision-mask"; }
    else if(strstr(flag, "node-a") || strstr(flag, "node-b")) return;
    else if(strstr(flag, "node")) { rank = 2; domain = "soft-node"; }
    else if(strstr(flag, "beam")) { rank = 2; domain = "soft-beam"; }
    else if(strstr(flag, "area")) { rank = 2; domain = "soft-area"; }
    else if(strstr(flag, "soft-body")) { rank = 1; domain = "soft-body"; }
    else if(strstr(flag, "body")) { rank = 1; domain = "rigid-body"; }
    else if(strstr(flag, "joint")) { rank = 1; domain = "joint"; }
    else if(strstr(flag, "anchor")) { rank = 1; domain = "anchor"; }
    else { rank = 0; domain = "object"; }
    if(rank >= input->rank) {
        input->rank = rank; input->domain = domain;
        input->target_flag = flag; input->target_name = name;
    }
}

static EditorResult cli_input_get(int count, char **arguments, CliInput *input) {
    *input = (CliInput){.path = "./objects/project.rohr.json", .terminal = -1, .rank = -1};
    for(int i = 1; i < count; i += 1) {
        if(strcmp(arguments[i], "--project") == 0) {
            if(++i >= count) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "--project requires a path");
            input->path = arguments[i];
        } else if(strcmp(arguments[i], "--property") == 0 || arguments[i][0] != '-') {
            input->terminal = i; break;
        } else if(cli_selector_flag(arguments[i])) {
            const char *flag = arguments[i];
            if(++i >= count) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                "%s requires a value", flag);
            input->selectors[input->selector_count++] = (char *)flag;
            input->selectors[input->selector_count++] = arguments[i];
            cli_target_consider(input, flag, arguments[i]);
        } else return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Unknown selector %s", arguments[i]);
    }
    if(input->terminal < 0)
        return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
            "Expected an operation");
    return editor_result_value(true);
}

static bool cli_selector_target_check(const CliInput *input, size_t index) {
    return input->selectors[index] == input->target_flag;
}

EditorResult editor_command_cli_standard_parse(const EditorProject *project,
        int count, char **arguments, const char **path, EditorCommand *command) {
    CliInput input;
    char *normalized[CLI_MAX];
    int n = 0;
    const char *operation;
    const char *property = NULL;
    const char *domain;
    EditorResult result = cli_input_get(count, arguments, &input);
    if(editor_result_check(result)) return result;
    operation = arguments[input.terminal];
    domain = input.domain;
    if(strcmp(operation, "--property") == 0) {
        if(input.terminal + 1 >= count) return editor_result_error(
            EDITOR_ERROR_INVALID_ARGUMENT, "--property requires a name");
        property = arguments[input.terminal + 1];
        if(strcmp(property, "navigation") == 0) domain = "navigation";
        else if(strcmp(property, "camera") == 0 || strcmp(property, "coordinates") == 0)
            domain = "viewport";
        else if(strcmp(property, "anchor-a") == 0 || strcmp(property, "anchor-b") == 0)
            domain = "joint";
        else if(strcmp(property, "rigid-body") == 0) domain = "anchor";
        else if(strcmp(property, "body") == 0 && domain == NULL)
            domain = "animated-sprite";
        else if(strcmp(property, "node-a") == 0 || strcmp(property, "node-b") == 0)
            domain = "soft-beam";
    }
    if(domain == NULL) return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
        "Operation requires a selected target");
    *path = input.path;
    if(strcmp(operation, "add") == 0 &&
            (strcmp(domain, "object") == 0 || strcmp(domain, "collision-mask") == 0)) {
        normalized[n++] = arguments[0]; normalized[n++] = "object";
        if(strcmp(domain, "collision-mask") == 0) normalized[1] = "collision-mask";
        normalized[n++] = "add"; normalized[n++] = (char *)input.path;
        normalized[n++] = (char *)input.target_name;
        for(int i = input.terminal + 1; i < count; i += 1) normalized[n++] = arguments[i];
        return editor_command_cli_parse(n, normalized, path, command);
    }
    normalized[n++] = arguments[0]; normalized[n++] = (char *)domain;
    normalized[n++] = (char *)(strcmp(operation, "--property") == 0 ?
        (strcmp(property, "visibility") == 0 ? "visibility" :
         strcmp(property, "position") == 0 &&
            (strcmp(domain, "rigid-body") == 0 || strcmp(domain, "soft-body") == 0 ||
             strcmp(domain, "anchor") == 0) ? "transform" :
         strcmp(property, "position") == 0 &&
            (strcmp(domain, "sprite") == 0 ||
             strcmp(domain, "animated-sprite") == 0) ? "set" :
         strcmp(property, "position") == 0 ? "position" :
         strcmp(property, "rotation") == 0 &&
            (strcmp(domain, "sprite") == 0 ||
             strcmp(domain, "animated-sprite") == 0) ? "set" :
         strcmp(property, "rotation") == 0 ? "transform" :
         strcmp(property, "transform") == 0 ? "transform" :
         strcmp(property, "origin") == 0 ? "origin" :
         strcmp(property, "auto-shape") == 0 ? "auto-shape" :
         strcmp(property, "camera") == 0 ? "camera" :
         strcmp(property, "coordinates") == 0 ? "coordinates" :
         strcmp(property, "navigation") == 0 ? "set" :
         strcmp(property, "category") == 0 || strcmp(property, "collide-with") == 0 ?
            "filter" :
         strcmp(property, "anchor-a") == 0 || strcmp(property, "anchor-b") == 0 ||
         strcmp(property, "rigid-body") == 0 || strcmp(property, "node-a") == 0 ||
         strcmp(property, "node-b") == 0 || strcmp(property, "body") == 0 ?
            "connect" : "set") : operation);
    normalized[n++] = (char *)input.path;
    for(size_t i = 0; i < input.selector_count; i += 2) {
        if(strcmp(operation, "add") == 0 && cli_selector_target_check(&input, i)) continue;
        normalized[n++] = input.selectors[i]; normalized[n++] = input.selectors[i + 1];
    }
    if(strcmp(operation, "add") == 0 &&
            (strcmp(domain, "animated-sprite") == 0 ||
                strcmp(domain, "sprite") == 0))
        normalized[n++] = (char *)input.target_name;
    if(strcmp(operation, "--property") == 0) {
        const char *action = normalized[2];
        if(strcmp(action, "set") == 0 || strcmp(action, "filter") == 0)
            normalized[n++] = (char *)property;
        else if(strcmp(action, "connect") == 0) normalized[n++] = (char *)property;
        if(strcmp(property, "rotation") == 0 &&
                strcmp(action, "transform") == 0) {
            if(input.terminal + 2 >= count || input.terminal + 3 != count)
                return editor_result_error(EDITOR_ERROR_INVALID_ARGUMENT,
                    "rotation requires one value");
            normalized[n++] = "0"; normalized[n++] = "0";
            normalized[n++] = arguments[input.terminal + 2];
        } else {
            for(int i = input.terminal + 2; i < count; i += 1)
                normalized[n++] = arguments[i];
            if(strcmp(property, "position") == 0 && strcmp(action, "transform") == 0)
                normalized[n++] = "0";
        }
    } else for(int i = input.terminal + 1; i < count; i += 1)
        normalized[n++] = arguments[i];
    result = editor_command_cli_named_parse(project, n, normalized, path, command);
    if(!editor_result_check(result) && strcmp(operation, "--property") == 0 &&
            (strcmp(property, "position") == 0 || strcmp(property, "rotation") == 0) &&
            (strcmp(domain, "rigid-body") == 0 || strcmp(domain, "soft-body") == 0 ||
             strcmp(domain, "anchor") == 0)) {
        EditorObject *object = NULL;
        for(size_t i = 0; i < project->object_count; i += 1) {
            EditorObjectId id = strcmp(domain, "rigid-body") == 0 ?
                command->data.rigid_body_transform.object :
                strcmp(domain, "soft-body") == 0 ? command->data.soft_body_transform.object :
                command->data.anchor_transform.object;
            if(project->objects[i].id == id) object = (EditorObject *)&project->objects[i];
        }
        if(strcmp(domain, "rigid-body") == 0) {
            EditorRigidBody *body = editor_project_rigid_body_get(object,
                command->data.rigid_body_transform.body);
            if(body != NULL) {
                if(strcmp(property, "position") == 0)
                    command->data.rigid_body_transform.rotation = body->rotation;
                else command->data.rigid_body_transform.position = body->position;
            }
        } else if(strcmp(domain, "soft-body") == 0 && object != NULL) {
            EditorSoftBody *body = NULL;
            for(size_t i = 0; i < object->soft_body_count; i += 1)
                if(object->soft_body_items[i].id == command->data.soft_body_transform.body)
                    body = &object->soft_body_items[i];
            if(body != NULL) {
                if(strcmp(property, "position") == 0)
                    command->data.soft_body_transform.rotation = body->rotation;
                else command->data.soft_body_transform.position = body->position;
            }
        } else if(strcmp(domain, "anchor") == 0) {
            EditorAnchor *anchor = editor_project_anchor_get(object,
                command->data.anchor_transform.anchor);
            if(anchor != NULL) {
                if(strcmp(property, "position") == 0)
                    command->data.anchor_transform.rotation = anchor->rotation;
                else command->data.anchor_transform.position = anchor->position;
            }
        }
    }
    if(!editor_result_check(result) && strcmp(operation, "add") == 0)
        snprintf(command->data.item_add.name, sizeof(command->data.item_add.name),
            "%s", input.target_name);
    return result;
}
