#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${TEXTFX_BUILD_DIR:-$ROOT/build/dev}"
PACKAGE_BUILD_DIR="${TEXTFX_PACKAGE_BUILD_DIR:-$ROOT/build/release}"

run_cmake_configure() {
  local build_dir="$1"
  cmake -S "$ROOT" -B "$build_dir" -G Ninja -DBUILD_TESTING=ON "${@:2}"
}

case "${1:-run}" in
  run)
    run_cmake_configure "$BUILD_DIR"
    cmake --build "$BUILD_DIR" --target textfx
    "$BUILD_DIR/textfx"
    ;;
  test)
    run_cmake_configure "$BUILD_DIR"
    cmake --build "$BUILD_DIR" --target check
    ;;
  package)
    cmake --preset release
    cmake --build "$PACKAGE_BUILD_DIR" --target package
    ;;
  clean)
    rm -rf "$BUILD_DIR"
    ;;
  *)
    cat <<USAGE
Usage: ./run.sh [run|test|package|clean]

Environment overrides:
  TEXTFX_BUILD_DIR   Build directory. Default: ./build/dev
  TEXTFX_PACKAGE_BUILD_DIR  Package build directory. Default: ./build/release
USAGE
    exit 2
    ;;
esac
