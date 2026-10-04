#!/bin/sh
set -eu

: "${EDITOR_NAME:=nino}"
: "${EDITOR_VERSION:=0.2.1}"
: "${OUTPUT:=$EDITOR_NAME}"
: "${HOST_CC:=cc}"
: "${EMCC:=emcc}"
: "${CFLAGS:=-std=c11 -Wall -Wextra -pedantic -Wshadow -O2}"

# Usage: build-web.sh [demo_home [demo_workspace]]
# demo_home:      directory bundled as the user home (/home/web_user)
# demo_workspace: path relative to the home that the editor opens
DEMO_HOME=${1:-}
DEMO_WORKSPACE=${2:-}

SCRIPT_DIR=$(
    cd "$(dirname "$0")" || exit 1
    pwd -P
)
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")
RESOURCE_DIR="$PROJECT_ROOT/resources"
BUILD_DIR="$PROJECT_ROOT/build"
DIST_DIR="$PROJECT_ROOT/dist/web"

# Add include directories
CFLAGS="$CFLAGS -I$PROJECT_ROOT/src"

mkdir -p "$BUILD_DIR" "$DIST_DIR"

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

SOURCES="\
    src/editor/action.c
    src/editor/buildnum.c
    src/editor/config.c
    src/editor/console.c
    src/editor/editor.c
    src/editor/file_io.c
    src/editor/highlight.c
    src/editor/row.c
    src/editor/search.c
    src/editor/select.c
    src/editor/panels/edit.c
    src/editor/panels/explorer.c
    src/editor/panels/prompt.c
    src/editor/panels/welcome.c
    src/ui/compositor.c
    src/ui/layout.c
    src/ui/surface.c
    src/utils/json.c
    src/utils/unicode.c
    src/utils/utils.c
    src/backend/shared/event.c
    src/backend/shared/frame_differ.c
    src/backend/web/web.c
    src/backend/web/input.c
    src/backend/web/output.c
    src/utils/os_unix.c"

WEB_SOURCES="\
    src/backend/web/index.html
    src/backend/web/web.css
    src/backend/web/web.js"

DEMO_FLAGS=""
WEB_HOME="/home/web_user"
if [ -n "$DEMO_HOME" ]; then
    DEMO_FLAGS="--preload-file=$DEMO_HOME@$WEB_HOME"
fi
if [ -n "$DEMO_WORKSPACE" ]; then
    DEMO_FLAGS="$DEMO_FLAGS -DDEMO_WORKSPACE_PATH=\"$WEB_HOME/${DEMO_WORKSPACE#/}\""
fi

printf '%s\n' "[3/3] Building $OUTPUT..."
$EMCC $CFLAGS \
    -include "$PROJECT_ROOT/src/common.h" \
    -DEDITOR_NAME="\"$EDITOR_NAME\"" \
    -DEDITOR_VERSION="\"$EDITOR_VERSION\"" \
    $DEMO_FLAGS \
    $(for s in $SOURCES; do echo "$PROJECT_ROOT/$s"; done) \
    -o "$DIST_DIR/$OUTPUT.js"

printf '%s\n' "[3/3] Copying web files..."
cp $(for s in $WEB_SOURCES; do echo "$PROJECT_ROOT/$s"; done) "$DIST_DIR"

printf '%s\n' "Done: $DIST_DIR/$OUTPUT.wasm"
