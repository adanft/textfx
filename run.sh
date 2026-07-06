#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${TEXTFX_BUILD_DIR:-$ROOT/build/dev}"
PACKAGE_BUILD_DIR="${TEXTFX_PACKAGE_BUILD_DIR:-$ROOT/build/release}"

canonicalize_path() {
  local path="$1"
  [[ "$path" == /* ]] || path="$ROOT/$path"

  if [[ -d "$path" ]]; then
    cd -P -- "$path" && pwd
    return
  fi

  local existing="$path"
  local -a suffix=()
  while [[ ! -e "$existing" && "$existing" != "/" ]]; do
    suffix=("$(basename -- "$existing")" "${suffix[@]}")
    existing="$(dirname -- "$existing")"
  done

  local resolved
  if [[ -d "$existing" ]]; then
    resolved="$(cd -P -- "$existing" && pwd)"
  else
    local parent
    parent="$(dirname -- "$existing")"
    resolved="$(cd -P -- "$parent" && pwd)/$(basename -- "$existing")"
  fi

  local part
  for part in "${suffix[@]}"; do
    case "$part" in
      ""|".") ;;
      "..") resolved="$(dirname -- "$resolved")" ;;
      *) resolved="$resolved/$part" ;;
    esac
  done

  printf '%s\n' "$resolved"
}

assert_clean_build_dir() {
  local root_build
  local target
  root_build="$(cd -P -- "$ROOT" && pwd)/build"
  target="$(canonicalize_path "$1")"

  case "$target" in
    "$root_build"/*) ;;
    *)
      printf 'Refusing to clean outside repository build directory: %s\n' "$target" >&2
      printf 'Set TEXTFX_BUILD_DIR under %s.\n' "$root_build" >&2
      exit 2
      ;;
  esac

  printf '%s\n' "$target"
}

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
    run_cmake_configure "$BUILD_DIR" -DTEXTFX_REQUIRE_TEST_DEPS=ON
    cmake --build "$BUILD_DIR" --target check
    ;;
  package)
    cmake --preset release
    cmake --build "$PACKAGE_BUILD_DIR" --target package
    ;;
  clean)
    CLEAN_DIR="$(assert_clean_build_dir "$BUILD_DIR")"
    rm -rf --one-file-system -- "$CLEAN_DIR"
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
