# Building and using Rohr

## Game developers should use an SDK

If you are making a game with Rohr, use the SDK built for your platform. You do
not need the engine repository, its Git history, or a separate SDL installation.
The SDK keeps the engine, editor, command-line tool, headers, libraries, CMake
package, templates, and license notices together.

Keep the unpacked SDK directory intact. `rohr-gui` and `rohr-cli` locate SDK
resources relative to their own executable, so the directory can be moved as a
unit without installing Rohr system-wide.

Rohr has three SDK targets:

| Target | Intended user | Output from this repository |
| --- | --- | --- |
| Generic Linux | x86_64 Linux users who do not use Nix | `dist/linux/` and `dist/rohr-sdk-linux-x86_64.tar.gz` |
| Nix Linux | x86_64 or AArch64 Linux users with Nix | `dist/nix`, a link to the Nix store package |
| Windows | 64-bit Windows users | `dist/windows/` |

These packages are portable within their target environment, not universally
interchangeable. Use the Linux SDK on Linux, the Windows SDK on Windows, and the
Nix SDK through Nix.

### Generic Linux compatibility

The generic Linux SDK is built inside the older Rocky Linux 8 baseline in
`packaging/linux/Dockerfile`. Building against an older glibc baseline avoids
accidentally requiring a newer development machine's glibc and improves
compatibility with newer x86_64 glibc-based distributions.

It still expects normal operating-system facilities such as glibc, Wayland or
X11, and working GPU/OpenGL or Vulkan drivers. It is not intended for musl-only
systems or non-Linux operating systems.

Unpack a supplied archive and launch the editor:

```sh
tar -xzf rohr-sdk-linux-x86_64.tar.gz
cd linux
./bin/rohr-gui
```

You may move or rename the `linux/` directory afterward, provided its contents
stay together.

### Nix SDK

The Nix SDK supports the `x86_64-linux` and `aarch64-linux` systems declared by
`flake.nix`. Nix supplies and wraps the required runtime libraries, making this
the most reproducible option for NixOS and other Linux systems with Nix.

The output is a Nix store package rather than a standalone tar archive. From a
source checkout:

```sh
./dev.sh sdk-nix
./dist/nix/bin/rohr-gui
```

Do not copy only the binary out of the package: its wrapped runtime dependencies
are resolved through the Nix store.

### Windows SDK

Use a supplied Windows SDK on Windows and keep its `bin`, `include`, `lib`, and
`share` directories together. The package includes Rohr and statically compiled
SDL libraries, so users do not install SDL separately.

SDKs built with MinGW and MSVC belong to different compiler/runtime families.
Use the SDK matching the toolchain used to build the game. Release packages
should identify that toolchain clearly.

## Creating and building a game

Start `bin/rohr-gui`, create a project, save it, and choose **Build > Build
Project**. This generates game C under `src/generated/`, configures the project,
and compiles it. Generated game output is not LGPL-covered Rohr source and may
be used, modified, and distributed without restriction.

The equivalent terminal workflow is:

```sh
/path/to/rohr-sdk/bin/rohr-cli --project /path/to/game generate-c
/path/to/rohr-sdk/bin/rohr-cli --project /path/to/game build
```

When the terminal is already at the project root, `--project` may be omitted:

```sh
cd /path/to/game
/path/to/rohr-sdk/bin/rohr-cli build
```

`generate-c` only replaces `src/generated/`. `compile` configures when needed
and invokes the configured compile command. `build` generates C and then
compiles.

### Hand-written CMake game

The SDK exports `Rohr::Engine`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_game C)

find_package(Rohr CONFIG REQUIRED)
add_executable(my_game src/main.c)
target_link_libraries(my_game PRIVATE Rohr::Engine)
```

If the SDK is not installed in a standard CMake location, point CMake at its
root:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/rohr-sdk
cmake --build build
```

## Building SDKs from the repository

Most game developers should download a supplied SDK instead. Build the SDKs
yourself when developing Rohr, testing unreleased changes, or preparing a
release.

If Git is unfamiliar, `git clone` downloads a working copy and `cd` enters it:

```sh
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
```

Build only the SDK you need:

```sh
./dev.sh sdk-linux    # generic x86_64 Linux archive; requires Docker or Podman
./dev.sh sdk-nix      # Nix package; requires Nix
./dev.sh sdk-windows  # Windows cross-build; requires MinGW-w64 and a host C compiler
```

To attempt all three distributions:

```sh
./dev.sh sdk
```

That command requires all three platform builders to be available. Failure of
one target does not mean the other SDK formats are unusable; during normal work,
run the one platform command you need.

On native Windows, open a Visual Studio 2022 Developer PowerShell and run:

```powershell
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
dev.bat sdk
```

Every SDK workflow installs the exported CMake package and builds a separate
consumer project from `tests/installed_sdk_consumer`. It also verifies required
Rohr, SDL, Lua, yyjson, FreeType, and JetBrains Mono license notices.

## Building and testing Rohr itself

This workflow is for engine contributors, not ordinary game projects. On Linux,
install Nix and run:

```sh
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
./dev.sh build
./dev.sh test
```

`dev.sh` enters `nix develop` automatically. Development output is organized as:

```text
build/
├── examples/             example executables
├── tests/                test executables
└── tools/
    ├── rohr-cli/rohr-cli
    └── rohr-gui/rohr-gui
```

Run the editor or an example from the repository root:

```sh
./build/tools/rohr-gui/rohr-gui
./build/examples/pong
./build/examples/joints
```

Clean generated development and SDK output with:

```sh
./dev.sh clean
```

### Direct CMake and presets

Contributors who need direct CMake control can use:

```sh
nix develop
cmake --preset linux
cmake --build --preset linux --parallel
ctest --preset linux
```

Preset CMake state is stored under `build/cmake/linux`, while runnable artifacts
remain under `build/`. Do not run `cmake --build build` unless that exact
directory was configured by `./dev.sh build` or `cmake -S . -B build`.

Useful targets include:

```sh
cmake --build --preset linux-examples --parallel
cmake --build --preset linux-tests --parallel
cmake --build build/cmake/linux --target pong
cmake --build build/cmake/linux --target docs
cmake --build build/cmake/linux --target help
```

Build options include `ROHR_BUILD_EXAMPLES`, `ROHR_BUILD_TESTS`,
`ROHR_BUILD_SDK_CONSUMER_TESTS`, and `ROHR_ENABLE_DOCUMENTATION`.

### Native Windows contributor build

Install Visual Studio 2022 with **Desktop development with C++**, then use a
Developer PowerShell:

```powershell
dev.bat build
dev.bat test
```

Or use the native CMake presets:

```powershell
cmake --preset windows
cmake --build --preset windows-debug --parallel
ctest --preset windows
```

## Optional tools

`ffmpeg` is only required for recording or video-generation workflows. It is
not required to use the editor, build a normal game, or link against Rohr.
