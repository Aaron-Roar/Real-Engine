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
- [Editor API reference](editor_api.md)

## Local Generation

From the project root:

```sh
nix develop
cmake --preset debug
cmake --build --preset debug --target docs
```

The generated HTML is written to:

```text
build/docs/html/index.html
```

Generated documentation is not committed. The committed documentation source is
the Doxygen comments in `include/`, this `docs/` directory, and the Doxygen
configuration.

## Useful Pages

- [Engine API reference](engine_api.md)
- [Editor API reference](editor_api.md)
- [Architecture](architecture.md)
- [Entity ids](entity_ids.md)
- [Error handling](errors.md)
