/* Copyright 2026 Aaron Rohrer
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#ifndef ROHR_TOOLS_H
#define ROHR_TOOLS_H

/**
 * @file rohr_tools.h
 * @brief Public build-time tooling API for Rohr Engine projects.
 */

#include <stdbool.h>
#include <stddef.h>

/** Maximum number of combined generated game tags and data components. */
#define ROHR_TOOLS_GAME_COMPONENT_LIMIT 64

/** Definition for a generated game component with associated typed data. */
typedef struct RohrToolsComponentDefinition {
    /** Stable C identifier used for generated component symbols. */
    const char *name;
    /** C type emitted for the component's generated storage. */
    const char *type_name;
    /** Complete C declaration emitted into the generated header. */
    const char *type_declaration;
    /** Optional C initializer expression, or NULL when no default is required. */
    const char *default_value;
} RohrToolsComponentDefinition;

/** Caller-owned, fixed-capacity registry used to generate game components. */
typedef struct RohrToolsComponentRegistry {
    /** Non-owning tag names in generated bit order. */
    const char *tags[ROHR_TOOLS_GAME_COMPONENT_LIMIT];
    /** Non-owning component definitions in generated bit order. */
    RohrToolsComponentDefinition components[ROHR_TOOLS_GAME_COMPONENT_LIMIT];
    /** Number of registered tags. */
    size_t tag_count;
    /** Number of registered data components. */
    size_t component_count;
} RohrToolsComponentRegistry;

/** Initialize an empty component registry without allocating memory. */
void rohr_tools_component_registry_init(RohrToolsComponentRegistry *registry);

/** Register a non-owning generated tag name. */
bool rohr_tools_component_registry_tag_add(
    RohrToolsComponentRegistry *registry,
    const char *name
);

/** Register a non-owning generated data-component definition. */
bool rohr_tools_component_registry_component_add(
    RohrToolsComponentRegistry *registry,
    RohrToolsComponentDefinition definition
);

/** Generate game_components.h and game_components.c at exact output paths. */
bool rohr_tools_component_registry_generate(
    const RohrToolsComponentRegistry *registry,
    const char *header_path,
    const char *source_path
);

#endif
