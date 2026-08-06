# Tools API Reference

This page is the GitHub-readable reference for the public Rohr Engine build-time tooling C API.
The source of truth is [`include/rohr_tools.h`](../include/rohr_tools.h), which uses Doxygen comments for the generated HTML documentation.

Application code should include the public facade:

```c
#include "rohr_tools.h"
```

## Contents

- <a href="#tools">Tools</a>

## Tools

### `rohr_tools_component_registry_init`

```c
void rohr_tools_component_registry_init(RohrToolsComponentRegistry *registry);
```

 Initialize an empty component registry without allocating memory.

### `rohr_tools_component_registry_tag_add`

```c
bool rohr_tools_component_registry_tag_add( RohrToolsComponentRegistry *registry, const char *name );
```

 Register a non-owning generated tag name.

### `rohr_tools_component_registry_component_add`

```c
bool rohr_tools_component_registry_component_add( RohrToolsComponentRegistry *registry, RohrToolsComponentDefinition definition );
```

 Register a non-owning generated data-component definition.

### `rohr_tools_component_registry_generate`

```c
bool rohr_tools_component_registry_generate( const RohrToolsComponentRegistry *registry, const char *header_path, const char *source_path );
```

 Generate game_components.h and game_components.c at exact output paths.
