# Rohr Engine

Rohr Engine is a small 2D game engine written in C with SDL3.

This project grew out of a passion for systems development, which gradually led me toward exploring engine design. It is a hands-on side project where I can experiment with low-level concepts, build each component from the ground up, and refine the design as it develops.

The aim is not to compete with large, established engines. Instead, the goal is to create a small, approachable, and hackable C engine whose core systems remain visible, understandable, and useful for experimentation.

Pull requests are welcome. Contributions such as new features, improved examples, bug fixes, documentation updates, and suggestions for cleaner design or structure are all appreciated.

## Example Demos

These GIFs are generated from the example programs in `examples/`.

### Flies in Pit Example

![Flies in Pit example](docs/assets/flies_in_pit.gif)

[Higher-quality MP4](docs/assets/flies_in_pit.mp4)

### Flies Around Ball Example

![Flies Around Ball example](docs/assets/flies_around_ball.gif)

[Higher-quality MP4](docs/assets/flies_around_ball.mp4)

## What It Does

Rohr Engine currently focuses on data-oriented 2D simulation:

* Entity ids are stable handles, separate from table indices.
* Component data is stored in indexed tables for cache-friendly systems.
* Object pools back engine tables and grow as entities are added.
* Entity table indices are still preserved for compute-heavy loops.
* Systems use explicit component masks instead of hidden object behavior.
* Physics supports convex polygon and circle-style shape data.
* Collision uses SAT for convex polygon overlap checks.
* Rigid bodies support mass, velocity, acceleration, angular velocity, restitution, and friction.
* Constraints include joints, axis locks, angle locks, and transform locks.
* Sprite animation is driven through SDL3 textures.
* Screen recording can write MP4 output through ffmpeg.
* A simple stdout console exists for engine logs and debugging.
* Doxygen comments are used for API documentation.

The engine is still early and in development so your mileage may vary.
I look forward to releasing a game with Rohr Engine in the soon future.

## Public API

* [Engine API reference](docs/engine_api.md)
* [Editor API reference](docs/editor_api.md)
Application code should include the public Rohr API facade:

```c
#include "rohr.h"
```

The public layer uses `rohr_` prefixes, for example:

```c
rohr_console_init();

EngineResult result = rohr_engine_init();
if(rohr_error_check(result)) {
    rohr_console_write(LOG_ENGINE, rohr_error_default_message(result.result.error));
    return 1;
}

EntityResult entity_result = rohr_entity_add();
if(rohr_error_check(entity_result)) {
    rohr_console_write(LOG_ENGINE, rohr_error_default_message(entity_result.result.error));
    return 1;
}
```

Internal modules still exist under `include/` and `src/`, but examples are intended to use the public API where possible.

## Project Structure

```text
engine/
├── include/               # Engine and editor public facades
├── src/                   # Engine and editor implementations
├── docs/                  # Doxygen config and documentation source
├── docs/assets/           # README and documentation media
├── examples/
│   ├── engine-core/       # Games using only librohr_engine
│   └── editor/            # Programs using librohr_editor
├── lib/                   # Generated engine and editor static libraries
└── build/                 # Generated objects, binaries, and docs
```

## Building

### Dependencies

* C compiler: `clang` or `gcc`
* C standard library
* CMake 3.20 or newer
* `pkg-config`
* `sdl3`
* `sdl3-image`
* `yyjson` is vendored under `lib/` (version 0.12.0, MIT)
* `ffmpeg` for recording or converting demo media
* Math library: `libm` / `-lm`

Enter the development environment before configuring:

```sh
nix develop
```

### CMake cheat sheet

List the available configuration, build, and test presets:

```sh
cmake --list-presets
cmake --list-presets=build
cmake --list-presets=test
```

Configure and build everything in debug mode:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
```

Configure and build everything in release mode:

```sh
cmake --preset release
cmake --build --preset release --parallel
```

Build only the core or editor examples:

```sh
cmake --build --preset core-examples --parallel
cmake --build --preset editor-examples --parallel
```

Build one example by its CMake target name:

```sh
cmake --build build/debug --target pong
cmake --build build/debug --target basic_editor
```

List every available target:

```sh
cmake --build build/debug --target help
```

Build and run the tests:

```sh
cmake --build --preset tests --parallel
ctest --preset debug
```

Clean compiled debug outputs while preserving the CMake configuration:

```sh
cmake --build --preset debug --target clean
```

Recreate the debug configuration when the cache needs to be reset:

```sh
cmake --preset debug --fresh
```

Examples and tests can be disabled for library-only builds with
`-DROHR_BUILD_EXAMPLES=OFF` and `-DROHR_BUILD_TESTS=OFF`.

Games include `rohr.h` and link `librohr_engine.a`. Editor applications include
`rohr_editor.h`, use the `RE_` API, and link `librohr_editor.a` followed by
`librohr_engine.a`.

## Nix

If you use Nix, the development shell includes the C toolchain, SDL dependencies, ffmpeg, and Doxygen:

```sh
nix develop
```

Then use the CMake commands above. For example:

```sh
cmake --preset debug
cmake --build --preset debug --parallel
```

## Documentation

Readable Markdown docs are committed in the repo:

* [Engine API reference](docs/engine_api.md)
* [Editor API reference](docs/editor_api.md)
* [Documentation overview](docs/README.md)
* [Architecture notes](docs/architecture.md)
* [Entity ids](docs/entity_ids.md)
* [JSON game state](docs/game_state.md)
* [Error handling](docs/errors.md)

Generate the Doxygen docs with:

```sh
cmake --build --preset debug --target docs
```

The generated HTML is written to:

```text
build/docs/html/index.html
```

Documentation source lives in `docs/`, and API comments live mainly in `include/`. Generated HTML under `build/docs/` is not committed.

For a movable standalone README preview, run:

```sh
cmake --build --preset debug --target static_readme
```

That writes `build/static/readme.html` and stages linked video assets under
`build/static/docs/assets/`.

## Examples

Current examples:

* `flies-in-pit`: physics, particles, animated sprites, collisions, grid drawing, and recording.
* `flies-around-ball`: joints, attraction-style motion, particles, and animated sprites.
* `view-port`: basic sprite movement and input handling.
* `pong` (editor): generated app-owned fire component, rotated JSON-authored arena,
  two-player WASD/arrow controls, collision paddles, and scoring.

Build output is separated the same way:

```text
build/debug/examples/
├── engine-core/
└── editor/
```

## Contributing

This project is very open to pull requests.

Useful contributions include:

* Bug fixes
* More examples
* Better docs
* Safer public API wrappers
* Physics fixes or focused improvements
* Rendering improvements
* Input, audio, camera, UI, or scene features
* Tests or reproducible demo cases
* Cleanup that keeps the engine simple and explicit

Please keep the style of the project in mind: plain C, explicit ownership, minimal dependencies, and focused changes.

## License Status

The license is not finalized yet.

I am currently deciding between `Apache-2.0` and `LGPL-3.0-or-later`. Until a
proper `LICENSE` file is added, please treat the licensing as undecided and be
careful about depending on the project for released work.

The goal is still for Rohr Engine to be usable as a library in other projects,
including closed-source games or applications. The main decision is whether
engine changes should be required to stay open when distributed.

## Roadmap

Things I would like to keep improving:

* Stable public API coverage
* Better camera and viewport tools
* Audio
* Scene serialization
* More complete input system
* More rendering helpers
* UI framework
* Better examples
* Tests and validation tools
* More complete documentation

Rohr Engine is an active personal project in the early phases of development.
Please keep this in mind if integrating into a project.
