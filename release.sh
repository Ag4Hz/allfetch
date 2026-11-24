#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
build="$root/build-release-linux"
cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release -DALLFETCH_PACKAGE_RELEASE=ON
cmake --build "$build" --config Release --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
cpack --config "$build/CPackConfig.cmake" -C Release -B "$root/dist"
