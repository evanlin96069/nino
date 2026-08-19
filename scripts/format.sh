#!/bin/sh
set -eu

if ! command -v clang-format >/dev/null 2>&1; then
    echo "Error: clang-format not found in PATH." >&2
    exit 1
fi

SCRIPT_DIR=$(
    cd "$(dirname "$0")" || exit 1
    pwd -P
)
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")
SRC_DIR="$PROJECT_ROOT/src"

find "$SRC_DIR" -type f \( -name "*.c" -o -name "*.h" \) -exec clang-format -i {} +
