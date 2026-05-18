#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
VALIDATOR="$ROOT_DIR/build/StandAlone/glslangValidator"
OUTPUT_DIR="${1:-$SCRIPT_DIR/generated}"

if [ ! -x "$VALIDATOR" ]; then
    echo "missing validator: $VALIDATOR" >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR/spv" "$OUTPUT_DIR/ast"

for shader in "$SCRIPT_DIR"/*.frag; do
    name=$(basename -- "${shader%.frag}")
    echo "[compile] $name"
    "$VALIDATOR" -V "$shader" -o "$OUTPUT_DIR/spv/$name.spv"
    "$VALIDATOR" -V -i "$shader" > "$OUTPUT_DIR/ast/$name.ast"
done

echo "generated SPIR-V in $OUTPUT_DIR/spv"
echo "generated AST in $OUTPUT_DIR/ast"
