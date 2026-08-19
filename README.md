# Rohr Engine

Rohr Engine is an experimental 2D game engine written in C with SDL3. It is
designed to stay small, explicit, and hackable: systems, data ownership, and
generated code remain visible instead of being hidden behind a large framework.

The project is under active development. APIs and editor project formats may
change before a stable release.

## Demos

### Soft bodies

![Soft Body example](docs/assets/soft_body.gif)

[Higher-quality MP4](docs/assets/soft_body.mp4)

### Particles in a pit

![Flies in Pit example](docs/assets/flies_in_pit.gif)

[Higher-quality MP4](docs/assets/flies_in_pit.mp4)

## Current features

- Stable, generation-checked entity handles with dynamically growing component
  pools and explicit component masks.
- Generated game tags and typed sparse-set data components.
- Rigid-body motion with mass, velocity, acceleration, forces, torque, friction,
  restitution, gravity, static/dynamic/kinematic-driven behavior, and transform
  locks.
- Simple convex and concave polygon hitboxes. Concave shapes are decomposed
  internally; applications continue to work with one polygon.
- Dynamic AABB-tree broadphase, SAT narrow phase, multi-point contact manifolds,
  accumulated impulses, collision filtering, and overlap/contact transitions.
- Particle circles stored independently from rigid polygon hitboxes. Particle
  pairs use circles; mixed particle/rigid pairs use their polygons.
- Pin/revolute, weld, and spring-joint tooling with reusable anchors.
- Node/beam soft bodies, boundary contacts, damping, friction, and filled areas.
- Cameras, multiple viewports, presentation modes, logical resolution/aspect
  configuration, and deferred signed graphics layers.
- Sprites, animated sprites, text, primitive drawing, and PNG/JPEG-style image
  loading through SDL3_image.
- Primitive-component UI with buttons, fields, sliders, dropdowns, clipping,
  scrolling, keyboard navigation, and debug panels.
- Keyboard, mouse, and controller input.
- JSON runtime state and generated C game components.
- Visual object editor plus a matching command-line editor. Projects store
  editable JSON and generate replaceable C under `src/generated/`.
- Embedded terminal emulator, build notifications, Lua build configuration,
  and Linux/Nix/Windows SDK packaging workflows.
- Optional MP4 recording through `ffmpeg`.

Current limitations include no audio, swept collision/CCD, adaptive physics
substeps, polygon holes/self-intersections, or stable release-compatible editor
schema.

## Quick contributor build

On Linux with Nix installed:

```sh
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
./dev.sh build
./dev.sh test
```

Run the tools from:

```sh
./build/tools/rohr-gui/rohr-gui
./build/tools/rohr-cli/rohr-cli --help
```

Build one of the examples, then run it from the repository root:

```sh
./build/examples/pong
./build/examples/joints
./build/examples/soft_body
```

Windows contributors can use `dev.bat build` and `dev.bat test` from a Visual
Studio 2022 Developer PowerShell.

See [Building and SDK usage](docs/building.md) for direct CMake commands,
presets, dependencies, SDK production, and the intended future release-SDK
workflow.

## Minimal public API example

Applications include the facade rather than internal module headers:

```c
#include "rohr.h"

#include <stdio.h>

int main(void) {
    EngineResult result = rohr_engine_init();
    if(rohr_error_check(result)) {
        fprintf(stderr, "error %d: %s\n",
            result.result.error,
            rohr_error_message_get(result));
        return 1;
    }

    EntityResult entity = rohr_entity_add();
    if(rohr_error_check(entity)) {
        fprintf(stderr, "error %d: %s\n",
            entity.result.error,
            rohr_error_message_get(entity));
        rohr_engine_shutdown();
        return 1;
    }

    rohr_engine_shutdown();
    return 0;
}
```

Repository targets link `rohr_engine`. Installed SDK consumers use:

```cmake
find_package(Rohr CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE Rohr::Engine)
```

## Repository layout

```text
rohr-engine/
├── include/                 public engine and tooling headers
├── src/                     engine implementation grouped by domain
├── tools/
│   ├── editor_core/         editor commands and shared document operations
│   ├── cli/                 rohr-cli
│   ├── editor/              rohr-gui
│   └── terminal/            reusable terminal emulator
├── examples/                independent example projects
├── tests/                   engine and installed-SDK tests
├── docs/                    guides and generated API Markdown
├── packaging/               generic Linux SDK container
├── cmake/                   package configuration and toolchains
├── lib/                     vendored dependencies
├── dev.sh / dev.bat         development workflow entry points
├── build/                   generated development output
└── dist/                    generated SDK output
```

See [Architecture](docs/architecture.md) for ownership and module boundaries.

## Editor workflow

The editor authors objects containing rigid bodies, hitboxes, particle circles,
anchors, joints, soft bodies, sprites, and animations. Saving writes editor JSON;
**Build > Generate C** writes only `src/generated/`. Developer-owned gameplay
code remains in `src/main.c`.

The GUI and CLI share the same editor document operations. Typical installed-SDK
commands are:

```sh
rohr-cli --project /path/to/game generate-c
rohr-cli --project /path/to/game build
```

See [Editor guide](docs/editor.md) and
[Editor architecture](docs/editor_architecture.md).

## Documentation

- [Documentation overview](docs/README.md)
- [Architecture](docs/architecture.md)
- [Physics](docs/physics.md)
- [Building and SDK usage](docs/building.md)
- [Editor guide](docs/editor.md)
- [Editor architecture](docs/editor_architecture.md)
- [Entity handles](docs/entity_ids.md)
- [Error handling](docs/errors.md)
- [JSON game state](docs/game_state.md)
- [Engine API reference](docs/engine_api.md)
- [Tools API reference](docs/tools_api.md)

Generate HTML and refresh public API Markdown after configuring the Linux preset:

```sh
nix develop
cmake --preset linux
cmake --build build/cmake/linux --target docs
```

Generated HTML is written to `build/docs/html/index.html` and is not committed.

## Examples

- `flies-in-pit`: particles, collision, animation, and recording.
- `flies-around-ball`: attraction-style movement, particles, and animation.
- `fly-to-finish`: input, obstacles, animation, and collision.
- `game-state`: JSON-authored runtime state.
- `joints`: pin, weld, and spring constraints.
- `soft-body`: node/beam wheels, boundary contacts, particles, and vehicle input.
- `user-interface`: UI primitives and interactions.
- `view-port`: camera/viewport and sprite movement.
- `pong`: generated game components, collision, input, and scoring.

## Contributing

Pull requests for focused fixes, tests, examples, documentation, and features are
welcome. Preserve the project direction: plain C, explicit ownership, minimal
dependencies, independent systems, warnings treated as problems, and Linux plus
Windows build support.

## License status

The project does not yet have a finalized license. Until a `LICENSE` file is
added, treat redistribution and released-project use as undecided.
