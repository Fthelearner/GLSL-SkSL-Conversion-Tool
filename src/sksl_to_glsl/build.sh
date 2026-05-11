#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SKIA_OUT_DIR="$ROOT_DIR/skia/out/SkSL"
NINJA_FILE="$SKIA_OUT_DIR/obj/skslc.ninja"
SRC_FILE="$ROOT_DIR/src/sksl_to_glsl/sksl_export_glsl_custom.cpp"
BUILD_DIR="$ROOT_DIR/build/sksl_export_glsl_custom"
OBJ_FILE="$BUILD_DIR/sksl_export_glsl_custom.o"
RSP_FILE="$BUILD_DIR/sksl_export_glsl_custom.rsp"
BIN_FILE="$SKIA_OUT_DIR/sksl_export_glsl_custom"

if [[ ! -f "$NINJA_FILE" ]]; then
    echo "missing $NINJA_FILE" >&2
    exit 1
fi

if [[ ! -f "$SRC_FILE" ]]; then
    echo "missing $SRC_FILE" >&2
    exit 1
fi

mkdir -p "$BUILD_DIR"

read_ninja_var() {
    local key="$1"
    sed -n "s/^${key} = //p" "$NINJA_FILE"
}

DEFINES="$(read_ninja_var defines)"
INCLUDE_DIRS="$(read_ninja_var include_dirs)"
CFLAGS="$(read_ninja_var cflags)"
CFLAGS_CC="$(read_ninja_var cflags_cc)"
LINK_LINE="$(sed -n 's/^build \.\/skslc: link //p' "$NINJA_FILE" | sed 's/ | .*//')"

if [[ -z "$LINK_LINE" ]]; then
    echo "failed to extract skslc link inputs from $NINJA_FILE" >&2
    exit 1
fi

pushd "$SKIA_OUT_DIR" >/dev/null

printf '%s\n' $LINK_LINE | grep -v '^obj/tools/skslc/skslc.Main.o$' > "$RSP_FILE"

c++ \
    $DEFINES \
    $INCLUDE_DIRS \
    $CFLAGS \
    $CFLAGS_CC \
    -c "$SRC_FILE" \
    -o "$OBJ_FILE"

c++ \
    -rdynamic \
    -Wl,-rpath,'$ORIGIN' \
    -Wl,--gc-sections \
    "$OBJ_FILE" \
    -Wl,--start-group \
    @"$RSP_FILE" \
    -Wl,--end-group \
    -ldl \
    -lpthread \
    libtint_combined.a \
    -o "$BIN_FILE"

popd >/dev/null

echo "$BIN_FILE"
