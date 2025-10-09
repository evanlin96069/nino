#!/usr/bin/env bash
set -e

EDITOR_NAME="nino"
EDITOR_VERSION="0.0.6"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

RESOURCE_DIR="$PROJECT_ROOT/resources"
SRC_DIR="$PROJECT_ROOT/src"

BUILD_DIR="$PROJECT_ROOT/build"
mkdir -p "$BUILD_DIR"

CORE_SOURCES=$(find "$SRC_DIR" -type f \( -name "*.c" ! -name "os_win32.c" \))

echo "[1/3] Building bundler..."
gcc -std=c11 -Wall -Wextra -pedantic \
    "$RESOURCE_DIR/bundler.c" \
    -o "$BUILD_DIR/bundler"

echo "[2/3] Generating bundle.h..."
"$BUILD_DIR/bundler" "$RESOURCE_DIR/bundle.h" "$RESOURCE_DIR"/syntax/*.json

echo "[3/3] Building $EDITOR_NAME..."
gcc -std=c11 -Wall -Wextra -pedantic \
    -include "$SRC_DIR/common.h" \
    -DEDITOR_NAME="\"$EDITOR_NAME\"" \
    -DEDITOR_VERSION="\"$EDITOR_VERSION\"" \
    $CORE_SOURCES \
    -o "$BUILD_DIR/$EDITOR_NAME"

echo "Build complete: $BUILD_DIR/$EDITOR_NAME"
