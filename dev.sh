#!/usr/bin/env sh
# Copyright 2026 Aaron Rohrer
# SPDX-License-Identifier: LGPL-3.0-only

set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_directory="$project_root/build"
operation=${1:-build}

needs_dev_shell=true
case "$operation" in
    sdk|sdk-linux|sdk-nix)
        needs_dev_shell=false
        ;;
esac

if [ "$needs_dev_shell" = true ] && [ -z "${ROHR_DEV_SHELL:-}" ]; then
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
        -DROHR_BUILD_SDK_CONSUMER_TESTS=ON \
        -DROHR_ENABLE_DOCUMENTATION=OFF \
        -DROHR_PORTABLE_SDK=ON \
        -DROHR_SDK_INSTALL_PREFIX="$sdk_directory" \
        "$@"
    cmake --build "$sdk_build_directory" --parallel
    cmake --install "$sdk_build_directory" --prefix "$sdk_directory"
    ctest --test-dir "$sdk_build_directory" \
        --output-on-failure -R "^installed_sdk_consumer_${platform}$"
    echo "Rohr $platform SDK: $sdk_directory"
}

sdk_linux_generic() {
    if command -v docker >/dev/null 2>&1; then
        container_runtime=docker
        set -- --user "$(id -u):$(id -g)"
    elif command -v podman >/dev/null 2>&1; then
        container_runtime=podman
        set --
    else
        echo "Error: Docker or Podman is required for sdk-linux." >&2
        exit 1
    fi

    mkdir -p "$project_root/dist"
    "$container_runtime" build \
        -f "$project_root/packaging/linux/Dockerfile" \
        -t rohr-linux-sdk-builder "$project_root"
    "$container_runtime" run --rm \
        "$@" \
        -v "$project_root:/workspace:ro" \
        -v "$project_root/dist:/dist" \
        rohr-linux-sdk-builder
}

sdk_nix() {
    if ! command -v nix >/dev/null 2>&1; then
        echo "Error: Nix is required for sdk-nix." >&2
        exit 1
    fi
    mkdir -p "$project_root/dist"
    nix build "$project_root#sdk" --out-link "$project_root/dist/nix"
    echo "Rohr Nix SDK: $project_root/dist/nix"
}

sdk_windows_cross() {
    if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        echo "Error: x86_64-w64-mingw32-gcc is required for sdk-windows." >&2
        exit 1
    fi
    sdk_build windows \
        -DCMAKE_TOOLCHAIN_FILE="$project_root/cmake/toolchains/mingw_w64.cmake" \
        -DROHR_HOST_C_COMPILER="$(command -v cc)"
}

sdk_all() {
    sdk_nix
    sdk_linux_generic
    sdk_windows_cross
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
        sdk_all
        ;;
    sdk-linux)
        sdk_linux_generic
        ;;
    sdk-nix)
        sdk_nix
        ;;
    sdk-windows)
        sdk_windows_cross
        ;;
    clean)
        cmake -E remove_directory "$build_directory"
        cmake -E remove_directory "$project_root/dist"
        ;;
    *)
        echo "usage: ./dev.sh [build|test|sdk|sdk-linux|sdk-nix|sdk-windows|clean]" >&2
        exit 1
        ;;
esac
