#!/bin/sh
set -eu

: "${EDITOR_NAME:=nino}"
: "${EDITOR_VERSION:=0.0.6}"
: "${OUTPUT:=$EDITOR_NAME}"
: "${HOST_CC:=cc}"
: "${CC:=cc}"
: "${CFLAGS:=-std=c11 -Wall -Wextra -pedantic}"

# Add command line arguments to CFLAGS
for arg in "$@"; do
    CFLAGS="$CFLAGS $arg"
done

SCRIPT_DIR=$(
    cd "$(dirname "$0")" || exit 1
    pwd -P
)
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")
RESOURCE_DIR="$PROJECT_ROOT/resources"
SRC_DIR="$PROJECT_ROOT/src"
BUILD_DIR="$PROJECT_ROOT/build"

mkdir -p "$BUILD_DIR"

printf '%s\n' "[1/3] Building bundler..."
"$HOST_CC" $CFLAGS "$RESOURCE_DIR/bundler.c" -o "$BUILD_DIR/bundler"

printf '%s\n' "[2/3] Generating bundle.h..."
SYNTAX_FILES=""
for f in "$RESOURCE_DIR"/syntax/*.json; do
    [ -f "$f" ] && SYNTAX_FILES="$SYNTAX_FILES $f"
done

if [ -z "$SYNTAX_FILES" ]; then
    printf '%s\n' "Error: no syntax JSON files found in $RESOURCE_DIR/syntax" >&2
    exit 1
fi

"$BUILD_DIR/bundler" "$RESOURCE_DIR/bundle.h" $SYNTAX_FILES

SOURCES=""
for f in "$SRC_DIR"/*.c; do
    [ -f "$f" ] || continue
    case "$(basename "$f")" in
    os_win32.c) continue ;;
    esac
    SOURCES="$SOURCES $f"

done

printf '%s\n' "[3/3] Building $OUTPUT..."
$CC $CFLAGS \
    -include "$SRC_DIR/common.h" \
    -DEDITOR_NAME="\"$EDITOR_NAME\"" \
    -DEDITOR_VERSION="\"$EDITOR_VERSION\"" \
    $SOURCES \
    -o "$BUILD_DIR/$OUTPUT"

printf '%s\n' "Done: $BUILD_DIR/$OUTPUT"
