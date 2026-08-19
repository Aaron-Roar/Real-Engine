#!/usr/bin/env sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_directory="$project_root/build"
operation=${1:-build}

if [ -z "${ROHR_DEV_SHELL:-}" ]; then
    if ! command -v nix >/dev/null 2>&1; then
        echo "Error: Nix is required to enter the Rohr development environment." >&2
        echo "Install Nix, then run: $0 $*" >&2
        exit 1
    fi

    exec nix develop "$project_root" -c "$0" "$@"
fi

configure() {
    cmake -S "$project_root" -B "$build_directory"
}

build() {
    configure
    cmake --build "$build_directory"
}

sdk_build() {
    platform=$1
    sdk_build_directory="$project_root/build/sdk/$platform"
    sdk_directory="$project_root/dist/$platform"
    shift

    cmake -E remove_directory "$project_root/dist/rohr"
    cmake -E remove_directory "$sdk_directory"
    cmake -S "$project_root" -B "$sdk_build_directory" \
        -DCMAKE_BUILD_TYPE=Release \
        -DROHR_BUILD_EXAMPLES=OFF \
        -DROHR_BUILD_TESTS=OFF \
        -DROHR_ENABLE_DOCUMENTATION=OFF \
        -DROHR_PORTABLE_SDK=ON \
        "$@"
    cmake --build "$sdk_build_directory" --parallel
    cmake --install "$sdk_build_directory" --prefix "$sdk_directory"
    echo "Rohr $platform SDK: $sdk_directory"
}

sdk_native() {
    case "$(uname -s)" in
        Linux*)
            sdk_build linux
            ;;
        MINGW*|MSYS*|CYGWIN*)
            sdk_build windows
            ;;
        *)
            echo "Error: SDK packaging is not configured for $(uname -s)." >&2
            exit 1
            ;;
    esac
}

sdk_windows_cross() {
    if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        echo "Error: x86_64-w64-mingw32-gcc is required for sdk-windows." >&2
        exit 1
    fi
    sdk_build windows \
        -DCMAKE_TOOLCHAIN_FILE="$project_root/cmake/toolchains/mingw-w64.cmake" \
        -DROHR_HOST_C_COMPILER="$(command -v cc)"
}

case "$operation" in
    build)
        build
        ;;
    test)
        build
        ctest --test-dir "$build_directory" --output-on-failure
        ;;
    sdk)
        sdk_native
        ;;
    sdk-linux)
        sdk_build linux
        ;;
    sdk-windows)
        sdk_windows_cross
        ;;
    clean)
        cmake -E remove_directory "$build_directory"
        cmake -E remove_directory "$project_root/dist"
        ;;
    *)
        echo "usage: ./dev.sh [build|test|sdk|sdk-linux|sdk-windows|clean]" >&2
        exit 1
        ;;
esac
