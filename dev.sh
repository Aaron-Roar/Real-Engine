#!/usr/bin/env sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_directory="$project_root/build"
sdk_directory="$project_root/dist/rohr"
operation=${1:-build}

if [ -z "${IN_NIX_SHELL:-}" ]; then
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

case "$operation" in
    build)
        build
        ;;
    test)
        build
        ctest --test-dir "$build_directory" --output-on-failure
        ;;
    sdk)
        build
        cmake --install "$build_directory" --prefix "$sdk_directory"
        ;;
    clean)
        cmake -E remove_directory "$build_directory"
        cmake -E remove_directory "$project_root/dist"
        ;;
    *)
        echo "usage: ./dev.sh [build|test|sdk|clean]" >&2
        exit 1
        ;;
esac
