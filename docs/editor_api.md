# Editor API Reference

This page is the GitHub-readable reference for the public Real Engine editor C API.
The source of truth is [`include/rohr_editor.h`](../include/rohr_editor.h), which uses Doxygen comments for the generated HTML documentation.

Application code should include the public facade:

```c
#include "rohr_editor.h"
```

## Contents

- <a href="#editor">Editor</a>

## Editor

### `RE_GAME_COMPONENT_LIMIT`

```c
#define RE_GAME_COMPONENT_LIMIT 64
```

 Maximum number of combined generated game tags and data components.

### `RE_ComponentDefinition`

```c
typedef struct RE_ComponentDefinition {
```

 Definition for a game component that has associated typed data.

### `RE_ComponentRegistry`

```c
typedef struct RE_ComponentRegistry {
```

 Editor-owned, fixed-capacity registry used to generate game components.

### `RE_GAME_COMPONENT_LIMIT`

```c
const char *tags[RE_GAME_COMPONENT_LIMIT];
```

 Non-owning tag names in generated bit order.

### `RE_ComponentDefinition`

```c
RE_ComponentDefinition components[RE_GAME_COMPONENT_LIMIT];
```

 Non-owning component definitions in generated bit order.

### `RE_component_registry_init`

```c
void RE_component_registry_init(RE_ComponentRegistry *registry);
```

Initialize an empty component registry without allocating memory.

| Parameter | Description |
| --- | --- |
| `registry` | Registry to initialize. |

### `RE_component_registry_add_tag`

```c
bool RE_component_registry_add_tag(RE_ComponentRegistry *registry, const char *name);
```

Register a tag by name, or succeed without duplication if it already exists.

| Parameter | Description |
| --- | --- |
| `registry` | Registry that owns the definition list. |
| `name` | Stable C identifier retained by the caller. |

**Returns:** true when the tag exists after the call, false for invalid input, conflicts, or exhausted capacity.

### `RE_component_registry_add_component`

```c
bool RE_component_registry_add_component( RE_ComponentRegistry *registry, RE_ComponentDefinition definition );
```

Register a typed data component.

| Parameter | Description |
| --- | --- |
| `registry` | Registry that owns the definition list. |
| `definition` | Non-owning definition retained by the caller. |

**Returns:** true when the component exists after the call, false for invalid input, conflicts, or exhausted capacity.

### `RE_component_registry_generate`

```c
bool RE_component_registry_generate( const RE_ComponentRegistry *registry, const char *header_path, const char *source_path );
```

Generate game_components.h and game_components.c from a registry.

Existing files at the exact output paths are replaced. The caller owns the

output directory and all strings referenced by the registry.

| Parameter | Description |
| --- | --- |
| `registry` | Registry to generate. |
| `header_path` | Header output filepath. |
| `source_path` | Source output filepath. |

**Returns:** true when both files were written successfully.

### `RE_entity_find_by_name`

```c
EntityResult RE_entity_find_by_name(const char *name);
```

Find a named game-state entity for editor-authored code.

| Parameter | Description |
| --- | --- |
| `name` | Entity name stored in the loaded game state. |

**Returns:** EntityResult containing the live entity handle or lookup failure.

### `RE_init`

```c
EngineResult RE_init(void);
```

Initialize editor-owned state.

The engine must be initialized before this function is called.

**Returns:** EngineResult describing success or failure.

### `RE_update`

```c
EngineResult RE_update(void);
```

Process one editor update.

**Returns:** EngineResult describing success or failure.
