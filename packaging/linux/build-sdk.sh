#!/usr/bin/env sh
set -eu

build_directory=/tmp/rohr-linux-sdk-build
sdk_directory=/dist/linux
archive_path=/dist/rohr-sdk-linux-x86_64.tar.gz

cmake -E remove_directory "$build_directory"
cmake -E remove_directory "$sdk_directory"
cmake -E remove -f "$archive_path"

cmake -S /workspace -B "$build_directory" \
    -DCMAKE_BUILD_TYPE=Release \
    -DROHR_BUILD_EXAMPLES=OFF \
    -DROHR_BUILD_TESTS=OFF \
    -DROHR_BUILD_SDK_CONSUMER_TESTS=ON \
    -DROHR_ENABLE_DOCUMENTATION=OFF \
    -DROHR_PORTABLE_SDK=ON \
    -DROHR_SDK_INSTALL_PREFIX="$sdk_directory"
cmake --build "$build_directory" --parallel
cmake --install "$build_directory" --prefix "$sdk_directory"
ctest --test-dir "$build_directory" --output-on-failure \
    -R '^installed_sdk_consumer_linux$'

cd /dist
cmake -E tar cfvz "$archive_path" --format=gnutar linux

echo "Rohr generic Linux SDK: $sdk_directory"
echo "Rohr generic Linux archive: $archive_path"
