#ifndef ROHR_EDITOR_H
#define ROHR_EDITOR_H

#include "rohr.h"

/**
 * @file rohr_editor.h
 * @brief Public Real Engine editor API facade.
 */

#include <stddef.h>

/** Maximum number of combined generated game tags and data components. */
#define RE_GAME_COMPONENT_LIMIT 64

/** Definition for a game component that has associated typed data. */
typedef struct RE_ComponentDefinition {
    /** Stable C identifier used for generated component symbols. */
    const char *name;
    /** C type emitted for the component's generated storage. */
    const char *type_name;
    /** Complete C declaration emitted into the generated header. */
    const char *type_declaration;
    /** Optional C initializer expression, or NULL when no default is required. */
    const char *default_value;
} RE_ComponentDefinition;

/** Editor-owned, fixed-capacity registry used to generate game components. */
typedef struct RE_ComponentRegistry {
    /** Non-owning tag names in generated bit order. */
    const char *tags[RE_GAME_COMPONENT_LIMIT];
    /** Non-owning component definitions in generated bit order. */
    RE_ComponentDefinition components[RE_GAME_COMPONENT_LIMIT];
    /** Number of registered tags. */
    size_t tag_count;
    /** Number of registered data components. */
    size_t component_count;
} RE_ComponentRegistry;

/**
 * Initialize an empty component registry without allocating memory.
 *
 * @param registry Registry to initialize.
 */
void RE_component_registry_init(RE_ComponentRegistry *registry);

/**
 * Register a tag by name, or succeed without duplication if it already exists.
 *
 * @param registry Registry that owns the definition list.
 * @param name Stable C identifier retained by the caller.
 * @return true when the tag exists after the call, false for invalid input,
 * conflicts, or exhausted capacity.
 */
bool RE_component_registry_add_tag(RE_ComponentRegistry *registry, const char *name);

/**
 * Register a typed data component.
 *
 * @param registry Registry that owns the definition list.
 * @param definition Non-owning definition retained by the caller.
 * @return true when the component exists after the call, false for invalid
 * input, conflicts, or exhausted capacity.
 */
bool RE_component_registry_add_component(
    RE_ComponentRegistry *registry,
    RE_ComponentDefinition definition
);

/**
 * Generate game_components.h and game_components.c from a registry.
 *
 * Existing files at the exact output paths are replaced. The caller owns the
 * output directory and all strings referenced by the registry.
 *
 * @param registry Registry to generate.
 * @param header_path Header output filepath.
 * @param source_path Source output filepath.
 * @return true when both files were written successfully.
 */
bool RE_component_registry_generate(
    const RE_ComponentRegistry *registry,
    const char *header_path,
    const char *source_path
);

/**
 * Initialize editor-owned state.
 *
 * The engine must be initialized before this function is called.
 *
 * @return EngineResult describing success or failure.
 */
EngineResult RE_init(void);

/**
 * Process one editor update.
 *
 * @return EngineResult describing success or failure.
 */
EngineResult RE_update(void);

#endif
