# Rohr Engine

Rohr Engine is a small 2D game engine written in C with SDL3.

This project grew out of a passion for systems development, which gradually led me toward exploring engine design. It is a hands-on side project where I can experiment with low-level concepts, build each component from the ground up, and refine the design as it develops.

The aim is not to compete with large, established engines. Instead, the goal is to create a small, approachable, and hackable C engine whose core systems remain visible, understandable, and useful for experimentation.

Pull requests are welcome. Contributions such as new features, improved examples, bug fixes, documentation updates, and suggestions for cleaner design or structure are all appreciated.

## Example Demos

These GIFs are generated from the example programs in `examples/`.


### Soft Body Example

![Soft Body example](docs/assets/soft_body.gif)

[Higher-quality MP4](docs/assets/soft_body.mp4)

### Flies in Pit Example

![Flies in Pit example](docs/assets/flies_in_pit.gif)

[Higher-quality MP4](docs/assets/flies_in_pit.mp4)
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
* [Tools API reference](docs/tools_api.md)
* [Editor guide](docs/editor.md)
* [Editor architecture](docs/editor_architecture.md)
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
├── include/               # Runtime and build-time tooling APIs
├── src/
│   ├── core/              # Lifecycle, facade, errors, and timing systems
│   ├── entity/            # Entity storage and entity-pair utilities
│   ├── physics/           # Pipeline and physics API implementation
│   │   ├── broadphase/
│   │   ├── collision/
│   │   ├── constraints/
│   │   ├── joints/
│   │   ├── particles/
│   │   ├── rigid_body/
│   │   └── soft_body/
│   ├── graphics/
│   ├── input/
│   ├── math/
│   ├── state/
│   └── tools/
├── docs/                  # Doxygen config and documentation source
├── docs/assets/           # README and documentation media
├── examples/
│   ├── view-port/         # Example games and demonstrations
│   └── ...
├── tools/editor/          # Visual object editor and C generator
├── lib/                   # Vendored third-party source dependencies
└── build/                 # Generated objects, binaries, and docs
```

## Building

The supported development workflows are available through thin wrappers around
CMake:

```sh
./dev.sh build    # Configure and build the engine, tools, tests, and examples
./dev.sh test     # Build and run the test suite
./dev.sh sdk      # Build the Nix SDK on Linux
./dev.sh sdk-linux # Build generic Linux in a Docker/Podman baseline container
./dev.sh sdk-nix   # Build the Nix package explicitly
./dev.sh sdk-windows # Cross-build; requires an x86_64-w64-mingw32 toolchain
./dev.sh clean    # Remove build/ and dist/
```

On Windows, use `dev.bat build`, `dev.bat test`, `dev.bat sdk`, or
`dev.bat clean`. CMake remains the authoritative build system; the wrappers only
provide memorable entry points.

SDK releases use three independently tested distribution paths:

```text
dist/
├── linux/             # Generic Linux SDK built against the release baseline
│   ├── bin/          # editor-cli and editor-gui
│   ├── include/      # Rohr and SDL public headers
│   ├── lib or lib64/ # Static libraries and CMake packages
│   └── share/        # Editor Lua defaults, project template, and licenses
├── nix -> /nix/store/... # Nix package output created by nix build
└── windows/          # Native MSVC Windows SDK
```

`./dev.sh sdk-linux` also creates
`dist/rohr-sdk-linux-x86_64.tar.gz`. The generic build runs inside the baseline
container under `packaging/linux/`; it intentionally contains no Nix store
paths. `./dev.sh sdk-nix` supports the Linux systems declared by `flake.nix`
and produces a Nix output link instead of a relocatable archive. Run
`dev.bat sdk` on Windows for the native, statically linked runtime package.

Release archives should contain the contents of the relevant platform directory.
The installed
tools find this SDK relative to their own executable, so project creation does
not require an engine source path.
Each SDK command also configures and links a separate consumer project against
the installed package. The test runs as `installed_sdk_consumer_linux` or
`installed_sdk_consumer_windows`, matching the target platform.
New projects copy the SDK's portable `project-editor.lua` template to a
top-level `editor.lua`. See [Editor documentation](docs/editor.md#lua-configuration)
for CLI configuration precedence and command placeholders.

### Dependencies

* C compiler: `clang` or `gcc`
* C standard library
* CMake 3.20 or newer
* `pkg-config`
* SDL3, SDL3_image, and SDL3_ttf are vendored under `lib/` and compiled
  statically with the engine (versions 3.4.10, 3.4.4, and 3.2.2; zlib license)
* FreeType source is vendored for self-contained Windows SDL3_ttf builds
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

Configure the engine development build:

```sh
cmake --preset linux
cmake --build --preset linux --parallel
```

Build all examples with development settings:

```sh
cmake --build build/cmake/linux --target examples --parallel
```

Build one example from the engine root:

```sh
cmake --build build/cmake/linux --target pong
```

From inside any example project, configure and build Debug or Release with local presets:

```sh
cmake --preset debug
cmake --build --preset debug --parallel

cmake --preset release
cmake --build --preset release --parallel
```

List every available target:

```sh
cmake --build build/cmake/linux --target help
```

Build and run the tests:

```sh
cmake --build --preset linux-tests --parallel
ctest --preset linux
```

Clean compiled engine-development outputs while preserving configuration:

```sh
cmake --build build/cmake/linux --target clean
```

Recreate the engine-development configuration when the cache needs resetting:

```sh
cmake --preset linux --fresh
```

Examples and tests can be disabled for library-only builds with
`-DROHR_BUILD_EXAMPLES=OFF` and `-DROHR_BUILD_TESTS=OFF`.

## Editor

Build and run the editor from the configured engine build:

```sh
cmake --build build --target rohr_editor
./build/editor/rohr_editor
```

The editor saves authored objects as JSON and generates replaceable C under a
game project's `src/generated/` directory. It does not overwrite developer-owned
game source when regenerating. See the [editor guide](docs/editor.md) for the
workflow and [editor architecture](docs/editor_architecture.md) for its internal
design.

### Windows 10/11

Install Visual Studio 2022 with the **Desktop development with C++** workload,
then run from a Developer PowerShell:

```powershell
cmake --preset windows
cmake --build --preset windows-debug --parallel
ctest --preset windows
```

Use `windows-release` for an optimized build or `windows-examples` to build
only the examples. SDL3, SDL3_image, SDL3_ttf, and FreeType are compiled from
the vendored sources; no separate SDL installation or vcpkg setup is needed.
The editor terminal uses the Windows ConPTY API and requires Windows 10 version
1809 or newer.

Linux hosts with a MinGW-w64 toolchain can configure the Windows compile check
with `cmake --preset windows-cross` and build it with
`cmake --build --preset windows-cross --parallel`. Cross-built test executables
must still be run on Windows; CTest cannot execute them directly on Linux.
Component generators are built as host tools during the cross-build, so generated
C files do not need to be bootstrapped manually. Set `ROHR_HOST_C_COMPILER` if
CMake cannot locate the build host's C compiler automatically.

Games include `rohr.h` and link the `rohr_engine` CMake target. Build-time
component generators include `rohr_tools.h` and link the `rohr_tools` target.

## Nix

If you use Nix, the development shell includes the C toolchain, SDL dependencies, ffmpeg, and Doxygen:

```sh
nix develop
```

Then use the CMake commands above. For example:

```sh
cmake --preset linux
cmake --build --preset linux --parallel
```

## Documentation

Readable Markdown docs are committed in the repo:

* [Engine API reference](docs/engine_api.md)
* [Tools API reference](docs/tools_api.md)
* [Documentation overview](docs/README.md)
* [Architecture notes](docs/architecture.md)
* [Entity ids](docs/entity_ids.md)
* [JSON game state](docs/game_state.md)
* [Error handling](docs/errors.md)

Generate the Doxygen docs with:

```sh
cmake --build build/cmake/linux --target docs
```

The generated HTML is written to:

```text
build/docs/html/index.html
```

Documentation source lives in `docs/`, and API comments live mainly in `include/`. Generated HTML under `build/docs/` is not committed.

For a movable standalone README preview, run:

```sh
cmake --build build/cmake/linux --target static_readme
```

That writes `build/static/readme.html` and stages linked video assets under
`build/static/docs/assets/`.

## Examples

Current examples:

* `flies-in-pit`: physics, particles, animated sprites, collisions, grid drawing, and recording.
* `flies-around-ball`: joints, attraction-style motion, particles, and animated sprites.
* `joints`: pin, weld, and spring assemblies periodically thrown around an enclosed room.
* `soft-body`: controllable rigid chassis with two node-beam soft wheels,
  welded ramp geometry, a particle pit, and collision-triggered slow-motion
  camera zoom.
* `view-port`: basic sprite movement and input handling.
* `pong`: generated app-owned fire component, rotated JSON-authored arena,
  two-player WASD/arrow controls, collision paddles, and scoring.

Build output is separated the same way:

```text
build/examples/
├── pong
├── joints
├── soft_body
├── fly_to_finish
├── assets/<example>/
└── ...
```

Each example is a small project with its own `CMakeLists.txt`, `src/`, and
optional `assets/` directory. Run an example from its output directory so its
staged assets are the current working directory:

```sh
./build/examples/pong
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
