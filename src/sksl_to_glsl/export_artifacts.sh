#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SKSLC_BIN=""
BUILD_SCRIPT="$ROOT_DIR/src/sksl_to_glsl/build.sh"
TMP_DIR=""
OUTPUT_DIR=""
BASE_NAME=""
declare -a INPUT_PATHS=()

cleanup() {
    if [[ -n "$TMP_DIR" && -d "$TMP_DIR" ]]; then
        rm -rf "$TMP_DIR"
    fi
}

trap cleanup EXIT

usage() {
    cat >&2 <<'EOF'
usage:
  export_sksl_artifacts.sh <input> <output_dir> [base_name]
  export_sksl_artifacts.sh <input1> [<input2> ...] <output_dir>
  export_sksl_artifacts.sh --output-dir <output_dir> [--base-name <base_name>] <input1> [<input2> ...]

notes:
  - [base_name] is only valid when exporting a single input.
  - when inputs come from tests/shaders/*.sksl, they are copied to temporary .rts files
    automatically so they compile as runtime shaders.
EOF
}

canonicalize_path() {
    local input="$1"
    local dir
    dir="$(cd "$(dirname "$input")" && pwd -P)"
    printf '%s/%s\n' "$dir" "$(basename "$input")"
}

find_working_skslc() {
    local candidate
    for candidate in \
        "$ROOT_DIR/skia/out/SkSL/skslc" \
        "$ROOT_DIR/skia/out/SkSL/gcc_like_host/skslc" \
        "$ROOT_DIR/skia/out/stog/skslc" \
        "$ROOT_DIR/skia/out/stog/gcc_like_host/skslc"; do
        if [[ -x "$candidate" && -f "$(dirname "$candidate")/sksl_shared.sksl" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

parse_args() {
    if [[ $# -eq 0 ]]; then
        usage
        exit 2
    fi

    if [[ "$1" == -* ]]; then
        while [[ $# -gt 0 ]]; do
            case "$1" in
                -o|--output-dir)
                    if [[ $# -lt 2 ]]; then
                        echo "missing value for $1" >&2
                        exit 2
                    fi
                    OUTPUT_DIR="$2"
                    shift 2
                    ;;
                -b|--base-name)
                    if [[ $# -lt 2 ]]; then
                        echo "missing value for $1" >&2
                        exit 2
                    fi
                    BASE_NAME="$2"
                    shift 2
                    ;;
                --)
                    shift
                    break
                    ;;
                -*)
                    echo "unknown option: $1" >&2
                    usage
                    exit 2
                    ;;
                *)
                    break
                    ;;
            esac
        done

        if [[ -z "$OUTPUT_DIR" ]]; then
            echo "missing --output-dir" >&2
            usage
            exit 2
        fi
        if [[ $# -lt 1 ]]; then
            echo "missing input file(s)" >&2
            usage
            exit 2
        fi
        INPUT_PATHS=("$@")
        return 0
    fi

    if [[ $# -eq 2 ]]; then
        INPUT_PATHS=("$1")
        OUTPUT_DIR="$2"
        return 0
    fi

    if [[ $# -eq 3 && ! -f "$2" ]]; then
        INPUT_PATHS=("$1")
        OUTPUT_DIR="$2"
        BASE_NAME="$3"
        return 0
    fi

    local last_arg="${!#}"
    local file_count=$(($# - 1))
    local i
    for ((i = 1; i <= file_count; ++i)); do
        if [[ ! -f "${!i}" ]]; then
            echo "ambiguous arguments near: ${!i}" >&2
            usage
            exit 2
        fi
    done

    INPUT_PATHS=("${@:1:file_count}")
    OUTPUT_DIR="$last_arg"
}

ensure_unique_output_names() {
    declare -A seen=()
    local input_path
    local base_name

    if [[ ${#INPUT_PATHS[@]} -le 1 ]]; then
        return 0
    fi

    for input_path in "${INPUT_PATHS[@]}"; do
        base_name="$(basename "${input_path%.*}")"
        if [[ -n "${seen[$base_name]:-}" ]]; then
            echo "duplicate output base name '$base_name' from multi-file inputs" >&2
            echo "use distinct source basenames or export files separately" >&2
            exit 1
        fi
        seen[$base_name]=1
    done
}

make_runtime_shader_copy_if_needed() {
    local input_path="$1"
    local base_name="$2"

    if [[ "$input_path" != "$ROOT_DIR"/tests/shaders/*.sksl ]]; then
        EFFECTIVE_INPUT_PATH="$input_path"
        return 0
    fi

    if [[ -z "$TMP_DIR" ]]; then
        TMP_DIR="$(mktemp -d)"
    fi

    EFFECTIVE_INPUT_PATH="$(mktemp "$TMP_DIR/${base_name}.XXXXXX.rts")"
    cp "$input_path" "$EFFECTIVE_INPUT_PATH"
    echo "info: treating test shader as runtime shader: $input_path -> $EFFECTIVE_INPUT_PATH" >&2
}

parse_args "$@"

if [[ -n "$BASE_NAME" && ${#INPUT_PATHS[@]} -ne 1 ]]; then
    echo "--base-name / [base_name] is only valid for a single input" >&2
    exit 2
fi

SKSLC_BIN="$(find_working_skslc || true)"

if [[ -z "$SKSLC_BIN" ]]; then
    echo "missing usable skslc binary under skia/out/{SkSL,stog}" >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
CUSTOM_GLSL_BIN="$("$BUILD_SCRIPT")"
ensure_unique_output_names

for INPUT_PATH in "${INPUT_PATHS[@]}"; do
    if [[ ! -f "$INPUT_PATH" ]]; then
        echo "missing input: $INPUT_PATH" >&2
        exit 1
    fi
done

for INPUT_PATH in "${INPUT_PATHS[@]}"; do
    INPUT_PATH="$(canonicalize_path "$INPUT_PATH")"

    CURRENT_BASE_NAME="$BASE_NAME"
    if [[ -z "$CURRENT_BASE_NAME" ]]; then
        CURRENT_BASE_NAME="$(basename "${INPUT_PATH%.*}")"
    fi

    make_runtime_shader_copy_if_needed "$INPUT_PATH" "$CURRENT_BASE_NAME"
    AST_PATH="$OUTPUT_DIR/$CURRENT_BASE_NAME.ast"
    GLSL_PATH="$OUTPUT_DIR/$CURRENT_BASE_NAME.glsl"

    "$SKSLC_BIN" --settings "$EFFECTIVE_INPUT_PATH" "$AST_PATH"
    "$CUSTOM_GLSL_BIN" "$EFFECTIVE_INPUT_PATH" "$GLSL_PATH"

    echo "AST:  $AST_PATH"
    echo "GLSL: $GLSL_PATH"
done
