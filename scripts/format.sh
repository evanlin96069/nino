#!/bin/sh
set -eu

SCRIPT_DIR=$(
    cd "$(dirname "$0")" || exit 1
    pwd -P
)
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")
SRC_DIR="$PROJECT_ROOT/src"

FILES=$(find "$SRC_DIR" -type f \( -name "*.c" -o -name "*.h" \))

if ! command -v clang-format >/dev/null 2>&1; then
    echo "Error: clang-format not found in PATH." >&2
    exit 1
fi

echo "Formatting source files..."
for f in $FILES; do
    echo "  $f"
    clang-format -i "$f"
done

echo "Done formatting."
