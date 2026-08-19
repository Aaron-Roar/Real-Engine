# Building and SDK usage

CMake is the authoritative build system. Repository scripts provide memorable
defaults but do not replace CMake.

## Contributor build from the repository

The supported Linux development environment is Nix:

```sh
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
./dev.sh build
./dev.sh test
```

`dev.sh` enters `nix develop` automatically unless it is already inside the
development shell. Its direct development build uses `build/`:

```text
build/
├── examples/             example executables
├── tests/                test executables
└── tools/
    ├── rohr-cli/rohr-cli
    └── rohr-gui/rohr-gui
```

Useful script operations:

```sh
./dev.sh build
./dev.sh test
./dev.sh sdk-nix
./dev.sh sdk-linux
./dev.sh sdk-windows
./dev.sh sdk          # attempts every SDK distribution
./dev.sh clean        # removes build/ and dist/
```

`sdk-linux` requires Docker or Podman. `sdk-windows` on Linux requires an
`x86_64-w64-mingw32` toolchain. `sdk` therefore requires every platform
builder; use the platform-specific operation during ordinary development.

### Direct CMake and presets

Inside `nix develop`, contributors may use the Linux presets instead:

```sh
nix develop
cmake --preset linux
cmake --build --preset linux --parallel
ctest --preset linux
```

Preset output uses `build/cmake/linux` for CMake state while placing runnable
artifacts under `build/`. Do not run `cmake --build build` unless `build/` was
configured by `dev.sh build` or `cmake -S . -B build`.

Common targets:

```sh
cmake --build --preset linux-examples --parallel
cmake --build --preset linux-tests --parallel
cmake --build build/cmake/linux --target pong
cmake --build build/cmake/linux --target docs
cmake --build build/cmake/linux --target help
```

Build options include `ROHR_BUILD_EXAMPLES`, `ROHR_BUILD_TESTS`,
`ROHR_BUILD_SDK_CONSUMER_TESTS`, and `ROHR_ENABLE_DOCUMENTATION`.

### Windows contributor build

Install Visual Studio 2022 with **Desktop development with C++**, then use a
Developer PowerShell:

```powershell
dev.bat build
dev.bat test
```

Or use the native presets:

```powershell
cmake --preset windows
cmake --build --preset windows-debug --parallel
ctest --preset windows
```

`dev.bat sdk` creates and consumer-tests the native Windows SDK.

## SDK production

The repository currently supports three distribution paths:

- `dist/linux`: generic Linux SDK built in the baseline container, plus
  `dist/rohr-sdk-linux-x86_64.tar.gz`;
- `dist/nix`: Nix output link produced by `nix build .#sdk`;
- `dist/windows`: native or MinGW Windows SDK depending on the host workflow.

Every SDK build installs the exported `Rohr` CMake package and builds a clean
consumer project from `tests/installed_sdk_consumer`. Release publication and
stable archive hosting are future work; local `dist/` outputs are development
artifacts today.

## Using a supplied SDK

Future release archives are intended to be unpacked as one self-contained SDK:

```text
rohr-sdk/
├── bin/                  rohr-cli and rohr-gui
├── include/              Rohr and SDL public headers
├── lib/ or lib64/        static libraries and CMake package files
└── share/rohr/           templates, Lua defaults, licenses
```

Keep this layout intact because the tools locate SDK resources relative to
their executable.

### Create and build an editor project

Launch `bin/rohr-gui`, create a project, save it, and choose **Build > Build
Project**. From a terminal, the equivalent workflow is:

```sh
/path/to/rohr-sdk/bin/rohr-cli --project /path/to/game generate-c
/path/to/rohr-sdk/bin/rohr-cli --project /path/to/game build
```

When the current working directory is the project root, `--project` may be
omitted. `generate-c` only replaces `src/generated/`. `compile` configures when
needed and invokes the configured compile command; `build` generates C and then
compiles.

### Build a hand-written CMake application

The SDK exports `Rohr::Engine`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_game C)

find_package(Rohr CONFIG REQUIRED)
add_executable(my_game src/main.c)
target_link_libraries(my_game PRIVATE Rohr::Engine)
```

Configure with the SDK prefix when it is not installed in a standard CMake
location:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/rohr-sdk
cmake --build build
```

Windows uses the same CMake package workflow. Use a Windows SDK built for the
same compiler/runtime family as the application.

## Runtime dependencies

Repository builds compile vendored SDL3, SDL3_image, SDL3_ttf, yyjson, Lua, and
support libraries as configured by CMake. A generic Linux SDK still relies on
ordinary platform facilities such as glibc, Wayland or X11, and GPU/OpenGL or
Vulkan drivers. The Nix package wraps its GUI runtime libraries.
Windows SDK output is intended to run without a separate SDL installation.

`ffmpeg` is optional and is only required for recording/video workflows.
