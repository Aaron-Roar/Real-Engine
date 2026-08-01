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
