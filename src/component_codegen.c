#include "rohr_editor.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool RE_identifier_check(const char *value) {
    size_t i;

    if(value == NULL || value[0] == '\0') {
        return false;
    }
    if(value[0] != '_' && !isalpha((unsigned char)value[0])) {
        return false;
    }
    for(i = 1; value[i] != '\0'; i += 1) {
        if(value[i] != '_' && !isalnum((unsigned char)value[i])) {
            return false;
        }
    }
    return true;
}

static bool RE_registry_name_check(
        const RE_ComponentRegistry *registry,
        const char *name
        ) {
    size_t i;

    for(i = 0; i < registry->tag_count; i += 1) {
        if(strcmp(registry->tags[i], name) == 0) {
            return true;
        }
    }
    for(i = 0; i < registry->component_count; i += 1) {
        if(strcmp(registry->components[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static size_t RE_registry_count(const RE_ComponentRegistry *registry) {
    return registry->tag_count + registry->component_count;
}

void RE_component_registry_init(RE_ComponentRegistry *registry) {
    if(registry == NULL) {
        return;
    }
    memset(registry, 0, sizeof(*registry));
}

bool RE_component_registry_tag_add(RE_ComponentRegistry *registry, const char *name) {
    size_t i;

    if(registry == NULL || !RE_identifier_check(name)) {
        return false;
    }
    for(i = 0; i < registry->tag_count; i += 1) {
        if(strcmp(registry->tags[i], name) == 0) {
            return true;
        }
    }
    if(RE_registry_name_check(registry, name)) {
        return false;
    }
    if(RE_registry_count(registry) >= RE_GAME_COMPONENT_LIMIT) {
        return false;
    }
    registry->tags[registry->tag_count] = name;
    registry->tag_count += 1;
    return true;
}

bool RE_component_registry_component_add(
        RE_ComponentRegistry *registry,
        RE_ComponentDefinition definition
        ) {
    size_t i;

    if(registry == NULL || !RE_identifier_check(definition.name) ||
            !RE_identifier_check(definition.type_name) ||
            definition.type_declaration == NULL ||
            definition.type_declaration[0] == '\0') {
        return false;
    }
    for(i = 0; i < registry->component_count; i += 1) {
        if(strcmp(registry->components[i].name, definition.name) == 0) {
            return strcmp(registry->components[i].type_name, definition.type_name) == 0 &&
                strcmp(
                    registry->components[i].type_declaration,
                    definition.type_declaration
                ) == 0;
        }
    }
    if(RE_registry_name_check(registry, definition.name)) {
        return false;
    }
    if(RE_registry_count(registry) >= RE_GAME_COMPONENT_LIMIT) {
        return false;
    }
    registry->components[registry->component_count] = definition;
    registry->component_count += 1;
    return true;
}

static bool RE_identifier_lower(const char *name, char *output, size_t size) {
    size_t i;
    size_t length;

    length = strlen(name);
    if(length + 1 > size) {
        return false;
    }
    for(i = 0; i < length; i += 1) {
        output[i] = (char)tolower((unsigned char)name[i]);
    }
    output[length] = '\0';
    return true;
}

static const char *RE_path_basename(const char *path) {
    const char *basename = path;
    const char *cursor;

    for(cursor = path; *cursor != '\0'; cursor += 1) {
        if(*cursor == '/' || *cursor == '\\') {
            basename = cursor + 1;
        }
    }
    return basename;
}

static bool RE_header_generate(
        const RE_ComponentRegistry *registry,
        FILE *header
        ) {
    size_t i;
    size_t bit = 0;
    char lower_name[128];

    if(fprintf(header,
            "#ifndef GAME_COMPONENTS_H\n"
            "#define GAME_COMPONENTS_H\n\n"
            "#include <stdbool.h>\n"
            "#include <stdint.h>\n"
            "#include \"rohr.h\"\n\n"
            "typedef uint64_t GameComponentMask;\n\n") < 0) {
        return false;
    }

    for(i = 0; i < registry->tag_count; i += 1, bit += 1) {
        if(fprintf(header,
                "#define GAME_TAG_%s ((GameComponentMask)UINT64_C(1) << %zu)\n",
                registry->tags[i], bit) < 0) {
            return false;
        }
    }
    for(i = 0; i < registry->component_count; i += 1, bit += 1) {
        if(fprintf(header,
                "#define GAME_COMPONENT_%s ((GameComponentMask)UINT64_C(1) << %zu)\n",
                registry->components[i].name, bit) < 0) {
            return false;
        }
    }

    if(fprintf(header,
            "\nbool game_components_init(void);\n"
            "void game_components_shutdown(void);\n"
            "void game_components_clear(Entity entity);\n\n") < 0) {
        return false;
    }

    for(i = 0; i < registry->tag_count; i += 1) {
        if(!RE_identifier_lower(registry->tags[i], lower_name, sizeof(lower_name))) {
            return false;
        }
        if(fprintf(header,
                "bool game_%s_set(Entity entity);\n"
                "bool game_%s_check(Entity entity);\n"
                "void game_%s_remove(Entity entity);\n\n",
                lower_name, lower_name, lower_name) < 0) {
            return false;
        }
    }

    for(i = 0; i < registry->component_count; i += 1) {
        const RE_ComponentDefinition *component = &registry->components[i];
        if(!RE_identifier_lower(component->name, lower_name, sizeof(lower_name))) {
            return false;
        }
        if(fprintf(header,
                "%s\n\n"
                "ERROR_DECLARE_RESULT_TYPE(Game%sResult, %s);\n"
                "typedef void (*Game%sDestroyHook)(Entity entity, %s *value);\n"
                "void game_%s_destroy_hook_set(Game%sDestroyHook hook);\n"
                "bool game_%s_set(Entity entity, %s value);\n"
                "Game%sResult game_%s_get(Entity entity);\n"
                "%s *game_%s_addr_get(Entity entity);\n"
                "bool game_%s_check(Entity entity);\n"
                "void game_%s_remove(Entity entity);\n\n",
                component->type_declaration,
                component->type_name, component->type_name,
                component->type_name, component->type_name,
                lower_name, component->type_name,
                lower_name, component->type_name,
                component->type_name, lower_name,
                component->type_name, lower_name,
                lower_name,
                lower_name) < 0) {
            return false;
        }
    }

    return fprintf(header, "#endif\n") >= 0;
}

static bool RE_source_prelude_generate(FILE *source, const char *header_name) {
    return fprintf(source,
        "#include \"%s\"\n\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n\n"
        "typedef struct GameEntityStatePool {\n"
        "    Entity *entities;\n"
        "    GameComponentMask *masks;\n"
        "    uint32_t *sparse;\n"
        "    size_t count;\n"
        "    size_t capacity;\n"
        "    size_t sparse_capacity;\n"
        "} GameEntityStatePool;\n\n"
        "static GameEntityStatePool game_entity_states = {0};\n\n"
        "static bool game_reserve(void **items, size_t item_size, size_t *capacity, size_t required) {\n"
        "    size_t next_capacity;\n"
        "    void *next_items;\n\n"
        "    if(required <= *capacity) {\n"
        "        return true;\n"
        "    }\n"
        "    next_capacity = *capacity == 0 ? 8 : *capacity;\n"
        "    while(next_capacity < required) {\n"
        "        if(next_capacity > SIZE_MAX / 2) {\n"
        "            return false;\n"
        "        }\n"
        "        next_capacity *= 2;\n"
        "    }\n"
        "    if(next_capacity > SIZE_MAX / item_size) {\n"
        "        return false;\n"
        "    }\n"
        "    next_items = realloc(*items, next_capacity * item_size);\n"
        "    if(next_items == NULL) {\n"
        "        return false;\n"
        "    }\n"
        "    *items = next_items;\n"
        "    *capacity = next_capacity;\n"
        "    return true;\n"
        "}\n\n"
        "static bool game_sparse_reserve(uint32_t **sparse, size_t *capacity, size_t required) {\n"
        "    size_t previous_capacity = *capacity;\n"
        "    if(!game_reserve((void **)sparse, sizeof(**sparse), capacity, required)) {\n"
        "        return false;\n"
        "    }\n"
        "    if(*capacity > previous_capacity) {\n"
        "        memset(*sparse + previous_capacity, 0, (*capacity - previous_capacity) * sizeof(**sparse));\n"
        "    }\n"
        "    return true;\n"
        "}\n\n"
        "static bool game_entity_index(Entity entity, EntityIndex *entity_index) {\n"
        "    EntityIndexResult result = rohr_entity_index_get(entity);\n"
        "    if(rohr_error_check(result)) {\n"
        "        return false;\n"
        "    }\n"
        "    *entity_index = result.result.value;\n"
        "    return true;\n"
        "}\n\n"
        "static bool game_entity_state_find(Entity entity, EntityIndex *entity_index, size_t *dense_index) {\n"
        "    uint32_t encoded;\n"
        "    if(!game_entity_index(entity, entity_index) || *entity_index >= game_entity_states.sparse_capacity) {\n"
        "        return false;\n"
        "    }\n"
        "    encoded = game_entity_states.sparse[*entity_index];\n"
        "    if(encoded == 0 || encoded - 1 >= game_entity_states.count) {\n"
        "        return false;\n"
        "    }\n"
        "    *dense_index = encoded - 1;\n"
        "    return game_entity_states.entities[*dense_index] == entity;\n"
        "}\n\n"
        "static bool game_entity_state_bit_set(Entity entity, GameComponentMask bit) {\n"
        "    EntityIndex entity_index;\n"
        "    size_t dense_index;\n"
        "    size_t entity_capacity;\n"
        "    size_t mask_capacity;\n"
        "    if(game_entity_state_find(entity, &entity_index, &dense_index)) {\n"
        "        game_entity_states.masks[dense_index] |= bit;\n"
        "        return true;\n"
        "    }\n"
        "    if(!game_entity_index(entity, &entity_index)) {\n"
        "        return false;\n"
        "    }\n"
        "    entity_capacity = game_entity_states.capacity;\n"
        "    mask_capacity = game_entity_states.capacity;\n"
        "    if(!game_reserve((void **)&game_entity_states.entities, sizeof(*game_entity_states.entities), &entity_capacity, game_entity_states.count + 1) ||\n"
        "            !game_reserve((void **)&game_entity_states.masks, sizeof(*game_entity_states.masks), &mask_capacity, game_entity_states.count + 1) ||\n"
        "            !game_sparse_reserve(&game_entity_states.sparse, &game_entity_states.sparse_capacity, (size_t)entity_index + 1)) {\n"
        "        return false;\n"
        "    }\n"
        "    game_entity_states.capacity = entity_capacity < mask_capacity ? entity_capacity : mask_capacity;\n"
        "    dense_index = game_entity_states.count;\n"
        "    game_entity_states.entities[dense_index] = entity;\n"
        "    game_entity_states.masks[dense_index] = bit;\n"
        "    game_entity_states.sparse[entity_index] = (uint32_t)dense_index + 1;\n"
        "    game_entity_states.count += 1;\n"
        "    return true;\n"
        "}\n\n"
        "static bool game_entity_state_bit_check(Entity entity, GameComponentMask bit) {\n"
        "    EntityIndex entity_index;\n"
        "    size_t dense_index;\n"
        "    return game_entity_state_find(entity, &entity_index, &dense_index) &&\n"
        "        (game_entity_states.masks[dense_index] & bit) == bit;\n"
        "}\n\n"
        "static void game_entity_state_bit_clear(Entity entity, GameComponentMask bit) {\n"
        "    EntityIndex entity_index;\n"
        "    EntityIndex moved_index;\n"
        "    size_t dense_index;\n"
        "    size_t last_index;\n"
        "    if(!game_entity_state_find(entity, &entity_index, &dense_index)) {\n"
        "        return;\n"
        "    }\n"
        "    game_entity_states.masks[dense_index] &= ~bit;\n"
        "    if(game_entity_states.masks[dense_index] != 0) {\n"
        "        return;\n"
        "    }\n"
        "    last_index = game_entity_states.count - 1;\n"
        "    game_entity_states.sparse[entity_index] = 0;\n"
        "    if(dense_index != last_index) {\n"
        "        game_entity_states.entities[dense_index] = game_entity_states.entities[last_index];\n"
        "        game_entity_states.masks[dense_index] = game_entity_states.masks[last_index];\n"
        "        if(game_entity_index(game_entity_states.entities[dense_index], &moved_index)) {\n"
        "            game_entity_states.sparse[moved_index] = (uint32_t)dense_index + 1;\n"
        "        }\n"
        "    }\n"
        "    game_entity_states.count -= 1;\n"
        "}\n\n",
        header_name) >= 0;
}

static bool RE_tag_source_generate(FILE *source, const char *name) {
    char lower_name[128];
    if(!RE_identifier_lower(name, lower_name, sizeof(lower_name))) {
        return false;
    }
    return fprintf(source,
        "bool game_%s_set(Entity entity) {\n"
        "    return game_entity_state_bit_set(entity, GAME_TAG_%s);\n"
        "}\n\n"
        "bool game_%s_check(Entity entity) {\n"
        "    return game_entity_state_bit_check(entity, GAME_TAG_%s);\n"
        "}\n\n"
        "void game_%s_remove(Entity entity) {\n"
        "    game_entity_state_bit_clear(entity, GAME_TAG_%s);\n"
        "}\n\n",
        lower_name, name,
        lower_name, name,
        lower_name, name) >= 0;
}

static bool RE_component_source_generate(
        FILE *source,
        const RE_ComponentDefinition *component
        ) {
    char name[128];
    const char *type = component->type_name;
    const char *symbol = component->name;

    if(!RE_identifier_lower(symbol, name, sizeof(name))) {
        return false;
    }
    if(fprintf(source,
            "typedef struct Game_%sPool {\n"
            "    Entity *entities;\n"
            "    %s *values;\n"
            "    uint32_t *sparse;\n"
            "    size_t count;\n"
            "    size_t capacity;\n"
            "    size_t sparse_capacity;\n"
            "} Game_%sPool;\n\n"
            "static Game_%sPool game_%s_pool = {0};\n"
            "static Game%sDestroyHook game_%s_destroy_hook = NULL;\n\n"
            "void game_%s_destroy_hook_set(Game%sDestroyHook hook) {\n"
            "    game_%s_destroy_hook = hook;\n"
            "}\n\n",
            type, type, type, type, name,
            type, name, name, type, name) < 0) {
        return false;
    }
    if(fprintf(source,
            "static bool game_%s_find(Entity entity, EntityIndex *entity_index, size_t *dense_index) {\n"
            "    uint32_t encoded;\n"
            "    if(!game_entity_index(entity, entity_index) || *entity_index >= game_%s_pool.sparse_capacity) {\n"
            "        return false;\n"
            "    }\n"
            "    encoded = game_%s_pool.sparse[*entity_index];\n"
            "    if(encoded == 0 || encoded - 1 >= game_%s_pool.count) {\n"
            "        return false;\n"
            "    }\n"
            "    *dense_index = encoded - 1;\n"
            "    return game_%s_pool.entities[*dense_index] == entity;\n"
            "}\n\n",
            name, name, name, name, name) < 0) {
        return false;
    }
    if(fprintf(source,
            "bool game_%s_set(Entity entity, %s value) {\n"
            "    EntityIndex entity_index;\n"
            "    size_t dense_index;\n"
            "    size_t entity_capacity;\n"
            "    size_t value_capacity;\n"
            "    if(game_%s_find(entity, &entity_index, &dense_index)) {\n"
            "        if(game_%s_destroy_hook != NULL) {\n"
            "            game_%s_destroy_hook(entity, &game_%s_pool.values[dense_index]);\n"
            "        }\n"
            "        game_%s_pool.values[dense_index] = value;\n"
            "        return game_entity_state_bit_set(entity, GAME_COMPONENT_%s);\n"
            "    }\n"
            "    if(!game_entity_index(entity, &entity_index)) {\n"
            "        return false;\n"
            "    }\n"
            "    entity_capacity = game_%s_pool.capacity;\n"
            "    value_capacity = game_%s_pool.capacity;\n",
            name, type, name,
            name, name, name,
            name, symbol, name, name) < 0) {
        return false;
    }
    if(fprintf(source,
            "    if(!game_reserve((void **)&game_%s_pool.entities, sizeof(*game_%s_pool.entities), &entity_capacity, game_%s_pool.count + 1) ||\n"
            "            !game_reserve((void **)&game_%s_pool.values, sizeof(*game_%s_pool.values), &value_capacity, game_%s_pool.count + 1) ||\n"
            "            !game_sparse_reserve(&game_%s_pool.sparse, &game_%s_pool.sparse_capacity, (size_t)entity_index + 1)) {\n"
            "        return false;\n"
            "    }\n"
            "    game_%s_pool.capacity = entity_capacity < value_capacity ? entity_capacity : value_capacity;\n"
            "    dense_index = game_%s_pool.count;\n"
            "    game_%s_pool.entities[dense_index] = entity;\n"
            "    game_%s_pool.values[dense_index] = value;\n"
            "    game_%s_pool.sparse[entity_index] = (uint32_t)dense_index + 1;\n"
            "    game_%s_pool.count += 1;\n",
            name, name, name, name, name, name,
            name, name, name, name, name, name,
            name, name) < 0) {
        return false;
    }
    if(fprintf(source,
            "    if(!game_entity_state_bit_set(entity, GAME_COMPONENT_%s)) {\n"
            "        game_%s_pool.count -= 1;\n"
            "        game_%s_pool.sparse[entity_index] = 0;\n"
            "        return false;\n"
            "    }\n"
            "    return true;\n"
            "}\n\n",
            symbol, name, name) < 0) {
        return false;
    }
    if(fprintf(source,
            "Game%sResult game_%s_get(Entity entity) {\n"
            "    EntityIndex entity_index;\n"
            "    size_t dense_index;\n"
            "    if(!game_%s_find(entity, &entity_index, &dense_index)) {\n"
            "        return ERROR_RESULT_MAKE_ERROR(Game%sResult, ERROR_ENGINE_COMPONENT_MISSING);\n"
            "    }\n"
            "    return ERROR_RESULT_MAKE_VALUE(Game%sResult, game_%s_pool.values[dense_index]);\n"
            "}\n\n",
            type, name, name, type, type, name) < 0) {
        return false;
    }
    if(fprintf(source,
            "%s *game_%s_addr_get(Entity entity) {\n"
            "    EntityIndex entity_index;\n"
            "    size_t dense_index;\n"
            "    if(!game_%s_find(entity, &entity_index, &dense_index)) {\n"
            "        return NULL;\n"
            "    }\n"
            "    return &game_%s_pool.values[dense_index];\n"
            "}\n\n"
            "bool game_%s_check(Entity entity) {\n"
            "    EntityIndex entity_index;\n"
            "    size_t dense_index;\n"
            "    return game_%s_find(entity, &entity_index, &dense_index);\n"
            "}\n\n",
            type, name, name, name, name, name) < 0) {
        return false;
    }
    return fprintf(source,
        "void game_%s_remove(Entity entity) {\n"
        "    EntityIndex entity_index;\n"
        "    EntityIndex moved_index;\n"
        "    size_t dense_index;\n"
        "    size_t last_index;\n"
        "    if(!game_%s_find(entity, &entity_index, &dense_index)) {\n"
        "        return;\n"
        "    }\n"
        "    last_index = game_%s_pool.count - 1;\n"
        "    if(game_%s_destroy_hook != NULL) {\n"
        "        game_%s_destroy_hook(entity, &game_%s_pool.values[dense_index]);\n"
        "    }\n"
        "    game_%s_pool.sparse[entity_index] = 0;\n"
        "    if(dense_index != last_index) {\n"
        "        game_%s_pool.entities[dense_index] = game_%s_pool.entities[last_index];\n"
        "        game_%s_pool.values[dense_index] = game_%s_pool.values[last_index];\n"
        "        if(game_entity_index(game_%s_pool.entities[dense_index], &moved_index)) {\n"
        "            game_%s_pool.sparse[moved_index] = (uint32_t)dense_index + 1;\n"
        "        }\n"
        "    }\n"
        "    game_%s_pool.count -= 1;\n"
        "    game_entity_state_bit_clear(entity, GAME_COMPONENT_%s);\n"
        "}\n\n",
        name, name, name,
        name, name, name,
        name, name, name, name, name, name, name, name,
        symbol) >= 0;
}

static bool RE_source_epilogue_generate(
        const RE_ComponentRegistry *registry,
        FILE *source
        ) {
    size_t i;
    char lower_name[128];

    if(fprintf(source,
            "bool game_components_init(void) {\n"
            "    game_components_shutdown();\n"
            "    return true;\n"
            "}\n\n"
            "void game_components_clear(Entity entity) {\n") < 0) {
        return false;
    }
    for(i = 0; i < registry->component_count; i += 1) {
        if(!RE_identifier_lower(registry->components[i].name, lower_name, sizeof(lower_name)) ||
                fprintf(source, "    game_%s_remove(entity);\n", lower_name) < 0) {
            return false;
        }
    }
    for(i = 0; i < registry->tag_count; i += 1) {
        if(!RE_identifier_lower(registry->tags[i], lower_name, sizeof(lower_name)) ||
                fprintf(source, "    game_%s_remove(entity);\n", lower_name) < 0) {
            return false;
        }
    }
    if(fprintf(source, "}\n\nvoid game_components_shutdown(void) {\n") < 0) {
        return false;
    }
    for(i = 0; i < registry->component_count; i += 1) {
        if(!RE_identifier_lower(registry->components[i].name, lower_name, sizeof(lower_name)) ||
                fprintf(source,
                    "    if(game_%s_destroy_hook != NULL) {\n"
                    "        size_t i;\n"
                    "        for(i = 0; i < game_%s_pool.count; i += 1) {\n"
                    "            game_%s_destroy_hook(game_%s_pool.entities[i], &game_%s_pool.values[i]);\n"
                    "        }\n"
                    "    }\n"
                    "    free(game_%s_pool.entities);\n"
                    "    free(game_%s_pool.values);\n"
                    "    free(game_%s_pool.sparse);\n"
                    "    memset(&game_%s_pool, 0, sizeof(game_%s_pool));\n",
                    lower_name, lower_name,
                    lower_name, lower_name, lower_name,
                    lower_name, lower_name, lower_name, lower_name, lower_name) < 0 ||
                fprintf(source,
                    "    game_%s_destroy_hook = NULL;\n",
                    lower_name) < 0) {
            return false;
        }
    }
    return fprintf(source,
        "    free(game_entity_states.entities);\n"
        "    free(game_entity_states.masks);\n"
        "    free(game_entity_states.sparse);\n"
        "    memset(&game_entity_states, 0, sizeof(game_entity_states));\n"
        "}\n") >= 0;
}

bool RE_component_registry_generate(
        const RE_ComponentRegistry *registry,
        const char *header_path,
        const char *source_path
        ) {
    FILE *header;
    FILE *source;
    size_t i;
    bool success = true;

    if(registry == NULL || header_path == NULL || source_path == NULL ||
            header_path[0] == '\0' || source_path[0] == '\0') {
        return false;
    }
    header = fopen(header_path, "w");
    if(header == NULL) {
        return false;
    }
    source = fopen(source_path, "w");
    if(source == NULL) {
        fclose(header);
        return false;
    }

    success = RE_header_generate(registry, header) &&
        RE_source_prelude_generate(source, RE_path_basename(header_path));
    for(i = 0; success && i < registry->tag_count; i += 1) {
        success = RE_tag_source_generate(source, registry->tags[i]);
    }
    for(i = 0; success && i < registry->component_count; i += 1) {
        success = RE_component_source_generate(source, &registry->components[i]);
    }
    if(success) {
        success = RE_source_epilogue_generate(registry, source);
    }
    if(fclose(header) != 0 || fclose(source) != 0) {
        success = false;
    }
    return success;
}
