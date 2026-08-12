# Rohr Engine Documentation

Rohr Engine is a lightweight C and SDL3 game engine built around explicit
engine systems, stable entity ids, component tables, object pools, and a small
error/result API.

Application code should prefer the public facade:

```c
#include "rohr.h"
```

The facade exposes `rohr_`-prefixed functions while the engine keeps its
smaller internal module headers and implementations.

## Public API

- [Engine API reference](engine_api.md)
- [Tools API reference](tools_api.md)

## Editor

- [Using the editor](editor.md)
- [Editor architecture](editor_architecture.md)

## Local Generation

From the project root:

```sh
nix develop
cmake --preset linux
cmake --build build --target docs
```

The generated HTML is written to:

```text
build/docs/html/index.html
```

The `docs` target also refreshes the committed Markdown API references. To
regenerate only those references, run:

```sh
cmake --build build --target generate_public_api
```

Generated HTML is not committed. The Markdown API references are committed and
are generated from the Doxygen comments in the public headers under `include/`.

## Useful Pages

- [Engine API reference](engine_api.md)
- [Tools API reference](tools_api.md)
- [Architecture](architecture.md)
- [Using the editor](editor.md)
- [Editor architecture](editor_architecture.md)
- [Entity ids](entity_ids.md)
- [Error handling](errors.md)
