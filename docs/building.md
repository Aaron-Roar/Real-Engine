# Building and using Rohr

## Build an SDK first

Game developers should build and use an SDK. The normal workflow is:

```text
clone Rohr -> build the SDK for your system -> run rohr-gui -> make your game
```

The SDK contains the editor, CLI, headers, libraries, CMake package, project
templates, configuration, and license notices. You do not need to install SDL
separately.

If Git is new to you, `git clone` downloads the repository and `cd` enters the
downloaded directory:

```sh
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
```

Then follow exactly one of the platform sections below.

### Generic Linux SDK

Use this on an x86_64 Linux system when you do not want the Nix package. Docker
or Podman must be installed and running.

```sh
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
./dev.sh sdk-linux
./dist/linux/bin/rohr-gui
```

The result is available in both forms:

```text
dist/linux/
dist/rohr-sdk-linux-x86_64.tar.gz
```

The SDK is built inside the older Rocky Linux 8 baseline. Building against an
older glibc avoids accidentally requiring the newer glibc from the developer's
computer and improves compatibility with newer x86_64 glibc-based Linux
distributions.

The unpacked SDK is relocatable as one directory. It still requires normal
Linux facilities such as glibc, Wayland or X11, and working GPU/OpenGL or Vulkan
drivers. It is not intended for musl-only systems.

To test the archive itself:

```sh
mkdir rohr-sdk-test
tar -xzf dist/rohr-sdk-linux-x86_64.tar.gz -C rohr-sdk-test
./rohr-sdk-test/linux/bin/rohr-gui
```

### Nix SDK on Linux

Use this on NixOS or another Linux system with Nix installed. It supports
`x86_64-linux` and `aarch64-linux`.

```sh
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
./dev.sh sdk-nix
./dist/nix/bin/rohr-gui
```

The result is:

```text
dist/nix -> /nix/store/...-rohr-sdk-...
```

Nix supplies and wraps the runtime libraries, making this the most reproducible
Linux option. `dist/nix` is a link to the Nix store, not an independent archive.
Do not copy only `rohr-gui` out of it; run it through the package so its wrapped
dependencies remain available.

### Windows SDK

On Windows, install Git, CMake, and Visual Studio 2022 with **Desktop development
with C++**. Open a Visual Studio Developer PowerShell and run:

```powershell
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
dev.bat sdk
dist\windows\bin\rohr-gui.exe
```

The result is:

```text
dist\windows\
```

Keep its `bin`, `include`, `lib`, and `share` directories together. Rohr and SDL
are included, so SDL does not need to be installed separately.

Linux developers preparing a MinGW Windows SDK can instead run:

```sh
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
./dev.sh sdk-windows
```

That requires `x86_64-w64-mingw32-gcc` and a native host C compiler. MinGW and
MSVC SDKs use different compiler/runtime families, so games should use the SDK
matching their compiler.

### Build every SDK

On a Linux release machine with Nix, Docker or Podman, MinGW-w64, and a host C
compiler installed:

```sh
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
./dev.sh sdk
```

This attempts the generic Linux, Nix Linux, and Windows distributions. Most
developers only need the single SDK command for their system.

Every SDK build installs the exported CMake package, installs the license
notices, and builds a separate consumer project to verify that an external game
can link against the finished SDK.

## Make a game with the SDK

Start the `rohr-gui` binary inside the SDK, create a project, save it, and choose
**Build > Build Project**. This generates game C under `src/generated/`,
configures the project, and compiles it.

Generated game output is not LGPL-covered Rohr source. It may be used, modified,
and distributed without restriction.

The CLI equivalent is:

```sh
/path/to/sdk/bin/rohr-cli --project /path/to/game generate-c
/path/to/sdk/bin/rohr-cli --project /path/to/game build
```

If the terminal is already inside the game project:

```sh
cd /path/to/game
/path/to/sdk/bin/rohr-cli build
```

`generate-c` replaces only `src/generated/`. `compile` configures when needed
and invokes the configured compiler command. `build` generates C and then
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

Point CMake at the SDK directory:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk
cmake --build build
```

## Build and test the engine repository

The commands below are for people changing Rohr itself. Game developers should
use one of the SDK workflows above.

### Linux contributor build

Install Git and Nix, then run:

```sh
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
./dev.sh build
./dev.sh test
```

`dev.sh` enters `nix develop` automatically. The output is organized as:

```text
build/
├── examples/
├── tests/
└── tools/
    ├── rohr-cli/rohr-cli
    └── rohr-gui/rohr-gui
```

Run development binaries from the repository root:

```sh
./build/tools/rohr-gui/rohr-gui
./build/examples/pong
./build/examples/joints
./build/examples/soft_body
```

Remove generated development and SDK output with:

```sh
./dev.sh clean
```

### Windows contributor build

Open a Visual Studio 2022 Developer PowerShell:

```powershell
git clone https://github.com/Aaron-Roar/rohr-engine.git
cd rohr-engine
dev.bat build
dev.bat test
```

### Direct CMake workflow

Contributors who need direct control can use the Linux presets:

```sh
nix develop
cmake --preset linux
cmake --build --preset linux --parallel
ctest --preset linux
```

Preset CMake state is under `build/cmake/linux`, while runnable artifacts remain
under `build/`.

Useful focused commands are:

```sh
cmake --build --preset linux-examples --parallel
cmake --build --preset linux-tests --parallel
cmake --build build/cmake/linux --target pong
cmake --build build/cmake/linux --target docs
cmake --build build/cmake/linux --target help
```

Do not run `cmake --build build` unless `build/` was configured by
`./dev.sh build` or `cmake -S . -B build`.

On native Windows, the equivalent preset workflow is:

```powershell
cmake --preset windows
cmake --build --preset windows-debug --parallel
ctest --preset windows
```

## Optional tools

`ffmpeg` is only required for recording and video-generation workflows. It is
not required to run the editor, build a normal game, or link against Rohr.
