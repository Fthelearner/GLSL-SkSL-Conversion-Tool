#!/usr/bin/env bash
set -uo pipefail

# ============================================================================
# Bidirectional SKSL <-> GLSL Round-Trip Test Orchestrator
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# --- tool paths ---
SKSLC_CUSTOM="$PROJECT_ROOT/skia/out/SkSL/sksl_export_glsl_custom"
SKSLC="$PROJECT_ROOT/skia/out/stog/skslc"
GLSLANG="$PROJECT_ROOT/glslang/build/StandAlone/glslang"
GLSLANG_TO_SKSL="$PROJECT_ROOT/glslang/build/glslang_demo/glslang-to-sksl"
RENDER_GLSL="$PROJECT_ROOT/glslang/glslang_demo/render_glsl"
RENDER_GLSL_LIVE="$PROJECT_ROOT/glslang/glslang_demo/render_glsl_live"
RENDER_SKSL="$SCRIPT_DIR/render_sksl.py"
RENDER_SKSL_LIVE="$PROJECT_ROOT/src/renderer/shader_live_preview.py"
COMPARE_PY="$SCRIPT_DIR/compare_images.py"
PARAMS_SH="$SCRIPT_DIR/params.sh"
SHADERS_DIR="$PROJECT_ROOT/tests/shaders"

# --- result directories ---
RESULTS_DIR="$PROJECT_ROOT/results/t5"
V1="$RESULTS_DIR/v1"
V12="$RESULTS_DIR/v12"
REPORTS="$RESULTS_DIR/reports"
TMP="$RESULTS_DIR/tmp"

# --- configuration ---
SHADERS=()
RUN_DIR1=true
RUN_DIR2=true

# --- source params ---
source "$PARAMS_SH"

# --- helpers ---

usage() {
    cat <<'EOF'
Usage: runner.sh <command> [options]

Commands:
  test             Run bidirectional SKSL↔GLSL round-trip tests (default)
  pipeline <in>    Flexible single-direction conversion + render + animate
  fulltest <in>    Bidirectional v1+v2+roundtrip verification (one shader)
  live <shader>    Open real-time GUI preview window (SDL2, wall-clock time)
  compare <a> <b>  Compare two PNG images pixel-by-pixel (PSNR/MSE)
  help             Show this help

────────────────────────────────────────────────────────────────────────────
test — Bidirectional round-trip tests
────────────────────────────────────────────────────────────────────────────
  runner.sh test [--shader NAME] [--dir1 | --dir2] [--all]

  --shader NAME     Run only the named shader (repeatable, default: all)
  --dir1            Run only Direction 1 (SKSL → GLSL → SKSL)
  --dir2            Run only Direction 2 (GLSL → SKSL → GLSL)
  --all             Run all shaders, both directions (default)

  Examples:
    runner.sh test --shader passthrough
    runner.sh test --dir1 --shader water_ripple

────────────────────────────────────────────────────────────────────────────
fulltest — Bidirectional v1+v2+roundtrip verification (one shader)
────────────────────────────────────────────────────────────────────────────
  runner.sh fulltest <input> --output <dir> [--stage v1|v2|report]

  <input>            .sksl or .glsl/.frag file
  --output DIR       Output root (required)

  --stage v1|v2|report  Run single stage (default: all three — v1→v2→roundtrip)
  --gpu              Render SKSL via GPU path (sksl→GLSL→render_glsl), matching
                      the GLSL backend. Essential for raymarched / numerically
                      sensitive shaders (e.g. purple_cloud, curve).
  --force            Re-render even if outputs exist
  --help             Show full options

  Directory layout (inside <output>/<name>/):
    v1/{before,code,after,report}/  — original→convert→render→compare
    v2/{before,code,after,report}/  — v1/code→reverse→render→compare
    report/                         — roundtrip: v1/before vs v2/after

  Examples:
    runner.sh fulltest tests/shaders/water_ripple.sksl --output results/sksltoglsl/test2
    runner.sh fulltest tests/frag/curve.frag --output results/glsltosksl/test2
    runner.sh fulltest tests/frag/curve.frag --output results/glsltosksl/test2 --stage v1

────────────────────────────────────────────────────────────────────────────
pipeline — Single-direction conversion + render (+ animate)
────────────────────────────────────────────────────────────────────────────
  runner.sh pipeline <input> [options]

  <input>           .sksl, .glsl, or .frag file (or directory)

  --config FILE     Explicit config file (default: auto-detect <name>.params.json)
  --width W         Override output width
  --height H        Override output height
  --fps N           Animation frames per second (default: 30)
  --duration SEC    Animation duration (overrides config)
  --direction DIR   "sksl_to_glsl", "glsl_to_sksl", or "bidirectional"
  --no-animate      Force single-frame even if animation detected
  --no-ascii        Disable terminal ASCII animation preview
  --output DIR      Override output root (default: results/)
  --force           Re-render even if outputs exist

  Config file (<name>.params.json, auto-detected alongside input):
    {
      "dimensions": {"width": 1280, "height": 720},
      "textures": {"image": "path/to/tex.png", "map": {"path": "p.png", "raw": true}},
      "uniforms": {"strength": 1.2, "center": [0.5, 0.7]},
      "animation": {"enabled": true, "uniform": "iTime", "start": 0.0, "end": 5.0, "fps": 30},
      "direction": "sksl_to_glsl"
    }

  Examples:
    runner.sh pipeline tests/shaders/passthrough.sksl
    runner.sh pipeline tests/frag/spread.frag --direction glsl_to_sksl
    runner.sh pipeline my_shader.sksl --fps 60 --duration 10

────────────────────────────────────────────────────────────────────────────
live — Real-time GUI preview window (SDL2, wall-clock time driven)
────────────────────────────────────────────────────────────────────────────
  runner.sh live <shader> [options]

  <shader>          .sksl or .glsl/.frag file

  --width W         Window width (default: 1280)
  --height H        Window height (default: 720)
  --fps N           Target FPS limit (0=unlimited, default: 0)
  --texture N=P     Child texture: name=path (repeatable)
  --uniform N=V     Static uniform: name=value (repeatable, comma-sep for vec)
  --raw NAME        Child name to use nearest-neighbor (repeatable)
  --title TITLE     Window title

  SKSL shaders use skia-python RuntimeEffect (CPU).
  GLSL shaders use OpenGL via render_glsl_live (GPU, higher FPS).

  Examples:
    runner.sh live tests/shaders/water_ripple.sksl --texture image=tests/assets/input.png
    runner.sh live tests/frag/spread.frag --width 640 --height 360
    runner.sh live glslang/glslang_demo/result/v7/spread.sksl --width 640 --height 360

────────────────────────────────────────────────────────────────────────────
compare — Pixel-level image comparison
────────────────────────────────────────────────────────────────────────────
  runner.sh compare <image_a> <image_b> [options]

  <image_a>         First PNG path (baseline)
  <image_b>         Second PNG path (test image)

  --threshold-psnr N      Minimum PSNR to pass (default: 30.0 dB)
  --threshold-diff-pct N  Maximum pixel diff % to pass (default: 5.0%)
  --output-json FILE      Write JSON report to file
  --json                  Print JSON report to stdout

  Examples:
    runner.sh compare before.png after.png
    runner.sh compare a.png b.png --output-json results/report.json
    runner.sh compare a.png b.png --threshold-psnr 35 --threshold-diff-pct 2
EOF
}

log_step() {
    local shader="$1" step="$2" status="$3" detail="${4:-}"
    local ts
    ts=$(date +%H:%M:%S)
    local tag
    case "$status" in
        START)  tag="▶" ;;
        OK)     tag="✓" ;;
        FAIL)   tag="✗" ;;
        SKIP)   tag="○" ;;
        *)      tag=" " ;;
    esac
    printf "[%s] %s %s/%-45s %s\n" "$ts" "$tag" "$shader" "$step" "$detail"
    if [ "$status" = "FAIL" ]; then
        FAILURES+=("$shader/$step: $detail")
    fi
}

run_step() {
    # run_step <shader> <step_name> <log_suffix> -- cmd args...
    local shader="$1" step="$2" log_suffix="$3"; shift 3
    local log="$TMP/${shader}_${log_suffix}.log"
    mkdir -p "$(dirname "$log")"

    local exit_code=0
    "$@" >"$log" 2>&1 || exit_code=$?

    if [ "$exit_code" -ne 0 ]; then
        # Show last line(s) of log for quick diagnosis
        local detail="exit=$exit_code"
        if [ -s "$log" ]; then
            local last_line
            last_line=$(tail -1 "$log" | cut -c1-80)
            detail="$detail  $last_line"
        fi
        log_step "$shader" "$step" FAIL "$detail  log=$log"
        return 1
    fi
    log_step "$shader" "$step" OK
    return 0
}

png_to_raw_rgba() {
    # png_to_raw_rgba <png_path> <raw_path>
    # Converts PNG to raw RGBA binary: [4B width][4B height][RGBA data]
    local png="$1" raw="$2"
    if [ ! -f "$png" ]; then
        echo "ERROR: missing PNG: $png" >&2
        return 1
    fi
    uv run --project "$PROJECT_ROOT" --no-sync python3 -c "
import struct, sys
from PIL import Image
img = Image.open('$png').convert('RGBA')
w, h = img.size
data = img.tobytes()
with open('$raw', 'wb') as f:
    f.write(struct.pack('<ii', w, h))
    f.write(data)
" 2>/dev/null
    return $?
}

fix_premultiplied_alpha() {
    # Skia RuntimeEffect's .eval() returns premultiplied alpha.
    # texture() in GLSL returns non-premultiplied (straight) alpha.
    #
    # Two-pass approach:
    #   1. Inline one-line helper functions that wrap texture() calls
    #      (e.g. vec4 sampleImage(coord) { return texture(img, expr); })
    #   2. Add VAR.rgb *= VAR.a after every vec4 VAR = texture(...);
    local glsl="$1"
    python3 -c "
import re
with open('$glsl', 'r') as f:
    content = f.read()

# Pass 1: Inline simple texture-wrapper helpers.
# Match: vec4 NAME(vec2 PARAM) { return texture(SAMPLER, BODY); }
helper_re = re.compile(
    r'vec4\s+(\w+)\s*\(\s*vec2\s+(\w+)\s*\)\s*\{\s*return\s+(texture\([^;]+\))\s*;\s*\}'
)
for m in helper_re.finditer(content):
    name, param, tex_call = m.group(1), m.group(2), m.group(3)
    # Remove the function definition FIRST to avoid matching its parameter list
    content = content.replace(m.group(0), '')
    # Then replace remaining call sites: name(ARG) → texture(..., ARG)
    def make_sub(_tc=tex_call, _p=param):
        def replacer(call_m):
            arg = call_m.group(1)
            return _tc.replace(_p, arg)
        return replacer
    content = re.sub(
        rf'\b{name}\(([^)]+)\)',
        make_sub(),
        content
    )

# Pass 2: Add premultiplied-alpha fix after texture() assignments.
content = re.sub(
    r'    vec4 (\w+) = texture\([^;]+\);',
    r'\g<0>\n    \1.rgb *= \1.a;',
    content
)
with open('$glsl', 'w') as f:
    f.write(content)
" 2>/dev/null
    return $?
}

# ── shared utility helpers ──────────────────────────────────────────────

detect_shader_type() {
    local path="$1"
    case "$path" in
        *.sksl) echo "sksl" ;;
        *.frag|*.glsl) echo "glsl" ;;
        *) echo "unknown" ;;
    esac
}

extract_shader_name() {
    local path="$1"
    local base
    base=$(basename "$path")
    echo "${base%.*}"
}

find_config_file() {
    local input="$1"
    if [ -f "$input" ]; then
        local dir; dir=$(dirname "$input")
        local base; base=$(basename "$input" | sed 's/\.[^.]*$//')
        local cfg="$dir/${base}.params.json"
        [ -f "$cfg" ] && echo "$cfg" || echo ""
    elif [ -d "$input" ]; then
        local cfg="$input/params.json"
        [ -f "$cfg" ] && echo "$cfg" || echo ""
    fi
}

detect_animation_from_source() {
    local src="$1"
    [ ! -f "$src" ] && { echo "false"; return; }
    if grep -qE '\b(iTime|iTimeDelta|iFrame|time)\b' "$src" 2>/dev/null; then
        echo "true"
    else
        echo "false"
    fi
}

load_and_validate_config() {
    local config_file="$1" shader_name="$2" shader_type="$3"
    if [ -z "$config_file" ] || [ ! -f "$config_file" ]; then
        python3 -c "import json; print(json.dumps({
            'name': '$shader_name',
            'dimensions': {'width': 1280, 'height': 720},
            'textures': {},
            'uniforms': {},
            'animation': {'enabled': False},
            'direction': '${shader_type}_to_glsl' if '$shader_type' == 'sksl' else 'glsl_to_sksl',
            'shader_type': '$shader_type'
        }))"
        return
    fi
    python3 -c "
import json, os
with open('$config_file') as f:
    cfg = json.load(f)
cfg.setdefault('name', '$shader_name')
cfg.setdefault('dimensions', {'width': 1280, 'height': 720})
cfg['dimensions'].setdefault('width', 1280)
cfg['dimensions'].setdefault('height', 720)
cfg.setdefault('textures', {})
cfg.setdefault('uniforms', {})
anim = cfg.setdefault('animation', {})
anim.setdefault('enabled', False)
anim.setdefault('uniform', 'iTime')
anim.setdefault('start', 0.0)
anim.setdefault('end', 5.0)
anim.setdefault('fps', 30)
ascii_cfg = anim.setdefault('ascii', {})
ascii_cfg.setdefault('enabled', True)
ascii_cfg.setdefault('fps', 10)
cfg.setdefault('direction', '${shader_type}_to_glsl' if '$shader_type' == 'sksl' else 'glsl_to_sksl')
cfg.setdefault('shader_type', '$shader_type')
# Resolve relative texture paths
base = os.path.join('$PROJECT_ROOT', 'tests')
for name, val in cfg.get('textures', {}).items():
    if isinstance(val, str):
        if not os.path.isabs(val):
            cfg['textures'][name] = os.path.join(base, val)
    elif isinstance(val, dict):
        p = val.get('path', '')
        if p and not os.path.isabs(p):
            val['path'] = os.path.join(base, p)
print(json.dumps(cfg))
"
}

ensure_output_dirs() {
    local name="$1" root="${2:-$PIPELINE_OUTPUT_ROOT}"
    mkdir -p "$root/$name/before" "$root/$name/code" "$root/$name/after"
    echo "$root/$name"
}

# ── live GUI preview ─────────────────────────────────────────────────────

run_live_preview() {
    local input="$1"; shift
    local shader_type
    shader_type=$(detect_shader_type "$input")

    if [ "$shader_type" = "sksl" ]; then
        echo "=== Live SKSL Preview: $input ===" >&2
        uv run python3 "$RENDER_SKSL_LIVE" --sksl "$input" "$@"
    elif [ "$shader_type" = "glsl" ]; then
        if [ ! -x "$RENDER_GLSL_LIVE" ]; then
            echo "ERROR: render_glsl_live not found at $RENDER_GLSL_LIVE" >&2
            echo "Build: cd glslang/glslang_demo && gcc -O2 -o render_glsl_live render_glsl_live.c \$(pkg-config --cflags --libs sdl2 epoxy) -lGL -lm" >&2
            return 1
        fi
        echo "=== Live GLSL Preview: $input ===" >&2
        "$RENDER_GLSL_LIVE" "$input" "$@"
    else
        echo "ERROR: unknown shader type for '$input'" >&2
        return 1
    fi
}

# ── image comparison ───────────────────────────────────────────────────────

run_compare() {
    local image_a="" image_b="" output_json="" threshold_psnr="" threshold_diff_pct=""
    local json_flag="" diffmap=""

    while [ $# -gt 0 ]; do
        case "$1" in
            --output-json) output_json="$2"; shift 2 ;;
            --threshold-psnr) threshold_psnr="$2"; shift 2 ;;
            --threshold-diff-pct) threshold_diff_pct="$2"; shift 2 ;;
            --diffmap) diffmap="$2"; shift 2 ;;
            --json) json_flag="--json"; shift ;;
            -*)
                echo "Unknown option: $1" >&2
                return 2
                ;;
            *)
                if [ -z "$image_a" ]; then image_a="$1"
                elif [ -z "$image_b" ]; then image_b="$1"
                else
                    echo "ERROR: unexpected arg: $1" >&2
                    return 2
                fi
                shift
                ;;
        esac
    done

    if [ -z "$image_a" ] || [ -z "$image_b" ]; then
        echo "ERROR: two image paths required" >&2
        echo "Usage: runner.sh compare <image_a> <image_b> [--output-json FILE] [--threshold-psnr N] [--threshold-diff-pct N] [--diffmap FILE] [--json]" >&2
        return 2
    fi

    local cmd=("python3" "$COMPARE_PY" "$image_a" "$image_b")
    [ -n "$threshold_psnr" ] && cmd+=("--threshold-psnr" "$threshold_psnr")
    [ -n "$threshold_diff_pct" ] && cmd+=("--threshold-diff-percent" "$threshold_diff_pct")
    [ -n "$output_json" ] && cmd+=("--output-json" "$output_json")
    [ -n "$diffmap" ] && cmd+=("--diffmap" "$diffmap")
    [ -n "$json_flag" ] && cmd+=("$json_flag")

    "${cmd[@]}"
    return $?
}

check_tools() {
    local missing=0
    for tool in "$SKSLC_CUSTOM" "$SKSLC" "$GLSLANG" "$GLSLANG_TO_SKSL" "$RENDER_GLSL"; do
        if [ ! -x "$tool" ]; then
            echo "ERROR: missing tool: $tool" >&2
            missing=1
        fi
    done
    if [ ! -f "$RENDER_SKSL" ]; then
        echo "ERROR: missing script: $RENDER_SKSL" >&2
        missing=1
    fi
    return $missing
}

build_glsl_render_cmd() {
    # Builds the render_glsl command line for a shader.
    # Uses the per-shader texture/uniform functions.
    local shader="$1" frag_path="$2" out_ppm="$3"
    local width="$4" height="$5"

    # Fix premultiplied alpha: Skia RuntimeEffect .eval() returns premultiplied,
    # but GLSL texture() returns straight alpha. Premultiply intermediate results.
    local fixed_glsl="$TMP/${shader}_fixed.glsl"
    cp "$frag_path" "$fixed_glsl"
    fix_premultiplied_alpha "$fixed_glsl"

    local cmd=("$RENDER_GLSL" "$fixed_glsl" "$out_ppm" "$width" "$height")

    # textures (use raw RGBA to preserve alpha channel)
    local tex_func="${shader}_textures"
    local tex_output
    tex_output="$($tex_func 2>/dev/null)" || true
    if [ -n "$tex_output" ]; then
        while IFS=' ' read -r tname tpath rest; do
            [ -z "$tname" ] && continue
            local raw="$TMP/${shader}_${tname}.raw"
            png_to_raw_rgba "$tpath" "$raw" || continue
            cmd+=("--rgatex" "$tname" "$raw")
        done <<< "$tex_output"
    fi

    # raw textures (nearest-neighbor filtering)
    local raw_func="${shader}_raw_children"
    local raw_output
    raw_output="$($raw_func 2>/dev/null)" || true
    if [ -n "$raw_output" ]; then
        for rname in $raw_output; do
            cmd+=("--rawtex" "$rname")
        done
    fi

    # uniforms
    local uni_func="${shader}_uniforms"
    local uni_output
    uni_output="$($uni_func 2>/dev/null)" || true
    if [ -n "$uni_output" ]; then
        while IFS= read -r line; do
            [ -z "$line" ] && continue
            read -r uname uvals <<< "$line"
            [ -z "$uname" ] && continue
            cmd+=("--uniform" "$uname" $uvals)
        done <<< "$uni_output"
    fi

    echo "${cmd[@]}"
}

build_sksl_render_cmd() {
    # Builds the render_sksl_to_png.py command line for a shader.
    local shader="$1" sksl_path="$2" out_png="$3"
    local width="$4" height="$5"

    local cmd=("uv" "run" "--project" "$PROJECT_ROOT" "python3" "$RENDER_SKSL"
        "--sksl" "$sksl_path"
        "--output" "$out_png"
        "--width" "$width" "--height" "$height")

    # textures
    local tex_func="${shader}_textures"
    local tex_output
    tex_output="$($tex_func 2>/dev/null)" || true
    if [ -n "$tex_output" ]; then
        while IFS=' ' read -r tname tpath rest; do
            [ -z "$tname" ] && continue
            cmd+=("--texture" "${tname}=${tpath}")
        done <<< "$tex_output"
    fi

    # uniforms
    local uni_func="${shader}_uniforms"
    local uni_output
    uni_output="$($uni_func 2>/dev/null)" || true
    if [ -n "$uni_output" ]; then
        while IFS= read -r line; do
            [ -z "$line" ] && continue
            read -r uname uvals <<< "$line"
            [ -z "$uname" ] && continue
            # collapse values with commas for vec types
            local collapsed
            collapsed=$(echo "$uvals" | tr ' ' ',')
            cmd+=("--uniform" "${uname}=${collapsed}")
        done <<< "$uni_output"
    fi

    # raw children
    local raw_func="${shader}_raw_children"
    local raw_output
    raw_output="$($raw_func 2>/dev/null)" || true
    if [ -n "$raw_output" ]; then
        for r in $raw_output; do
            cmd+=("--raw" "$r")
        done
    fi

    echo "${cmd[@]}"
}

render_original_skia() {
    # Render original SKSL using renderer's app.py
    local shader="$1" out_png="$2"
    local skip_args_func="${shader}_skip_demo_args"
    local extra_args
    extra_args="$($skip_args_func 2>/dev/null)" || extra_args=""

    mkdir -p "$(dirname "$out_png")"
    cd "$PROJECT_ROOT" || return 1
    uv run python src/renderer/main.py \
        --effect "$shader" \
        --output "$out_png" \
        $extra_args
}

# ============================================================================
# Direction 1: SKSL -> GLSL -> SKSL
# ============================================================================
run_direction1() {
    local shader="$1"
    local width=1280 height=720

    local sksl_src="$SHADERS_DIR/${shader}.sksl"

    # Step 1: Render original SKSL
    local d1_step1_out="$V1/sksl/${shader}.png"
    mkdir -p "$V1/sksl"
    if [ -f "$d1_step1_out" ]; then
        log_step "$shader" "D1.S1_render_orig_SKSL" SKIP "already exists"
    else
        if run_step "$shader" "D1.S1_render_orig_SKSL" "d1_s1_render" \
            render_original_skia "$shader" "$d1_step1_out"; then
            :
        else
            log_step "$shader" "D1" FAIL "Cannot render original SKSL — skipping Direction 1"
            return 1
        fi
    fi

    # Step 2: SKSL -> GLSL + provenance
    local d1_step2_dir="$V1/sksl_to_glsl/$shader"
    local d1_glsl="$d1_step2_dir/${shader}.glsl"
    local d1_glsl_prov="$d1_step2_dir/${shader}.glsl.provenance"
    mkdir -p "$d1_step2_dir"
    if [ -f "$d1_glsl" ]; then
        log_step "$shader" "D1.S2_SKSL_to_GLSL" SKIP "already exists"
    else
        # sksl_export_glsl_custom needs .rts for runtime shaders (uniform shader etc.)
        local sksl_as_rts="$TMP/${shader}_d1.rts"
        cp "$sksl_src" "$sksl_as_rts"
        run_step "$shader" "D1.S2_SKSL_to_GLSL" "d1_s2_sk2gl" \
            "$SKSLC_CUSTOM" "$sksl_as_rts" "$d1_glsl" || return 1
    fi

    # Step 3: Render converted GLSL
    local d1_step3_out="$V1/to_glsl/${shader}.png"
    local d1_glsl_ppm="$TMP/${shader}_d1_glsl.ppm"
    mkdir -p "$V1/to_glsl"
    if [ -f "$d1_step3_out" ]; then
        log_step "$shader" "D1.S3_render_GLSL" SKIP "already exists"
    else
        local render_cmd
        render_cmd=$(build_glsl_render_cmd "$shader" "$d1_glsl" "$d1_glsl_ppm" "$width" "$height")
        if run_step "$shader" "D1.S3_render_GLSL" "d1_s3_glsl_render" \
            bash -c "$render_cmd"; then
            convert "$d1_glsl_ppm" "$d1_step3_out" 2>/dev/null || true
        fi
    fi

    # Step 4: GLSL -> SKSL (还原) with provenance
    local d1_step4_dir="$V1/glsl_to_sksl/$shader"
    local d1_sksl="$d1_step4_dir/${shader}.sksl"
    local d1_sksl_prov="$d1_step4_dir/${shader}.sksl.provenance"

    # Prepare input for glslang-to-sksl: it reads a directory of .frag files
    local gl2sk_input="$TMP/${shader}_glsl_to_sksl_input"
    local gl2sk_output="$TMP/${shader}_glsl_to_sksl_output"

    mkdir -p "$d1_step4_dir"
    if [ -f "$d1_sksl" ]; then
        log_step "$shader" "D1.S4_GLSL_to_SKSL" SKIP "already exists"
    else
        rm -rf "$gl2sk_input" "$gl2sk_output"
        mkdir -p "$gl2sk_input"
        cp "$d1_glsl" "$gl2sk_input/${shader}.frag"
        if [ -f "$d1_glsl_prov" ]; then
            cp "$d1_glsl_prov" "$gl2sk_input/${shader}.frag.provenance"
        fi

        if run_step "$shader" "D1.S4_GLSL_to_SKSL" "d1_s4_gl2sk" \
            "$GLSLANG_TO_SKSL" "$gl2sk_input" "$gl2sk_output"; then
            # glslang-to-sksl puts output in <outdir>/<basename>.sksl
            cp "$gl2sk_output/${shader}.sksl" "$d1_sksl" 2>/dev/null || true
            cp "$gl2sk_output/${shader}.sksl.provenance" "$d1_sksl_prov" 2>/dev/null || true
        else
            log_step "$shader" "D1.S4_GLSL_to_SKSL" FAIL "glslang-to-sksl failed — skipping rest of D1"
            return 1
        fi
    fi

    # Step 5: Render 还原 SKSL
    local d1_step5_out="$V1/to_sksl/${shader}.png"
    mkdir -p "$V1/to_sksl"
    if [ -f "$d1_step5_out" ]; then
        log_step "$shader" "D1.S5_render_restored_SKSL" SKIP "already exists"
    else
        if [ ! -f "$d1_sksl" ]; then
            log_step "$shader" "D1.S5_render_restored_SKSL" SKIP "no SKSL to render"
        else
            # The round-tripped SKSL may use 'sampler2D' instead of 'shader' (known converter issue)
            # Attempt to fix: sampler2D -> shader for RuntimeEffect compatibility
            local d1_fixed_sksl="$TMP/${shader}_d1_fixed.sksl"
            sed 's/uniform sampler2D /uniform shader /g' "$d1_sksl" > "$d1_fixed_sksl"
            local sksl_cmd
            sksl_cmd=$(build_sksl_render_cmd "$shader" "$d1_fixed_sksl" "$d1_step5_out" "$width" "$height")
            run_step "$shader" "D1.S5_render_restored_SKSL" "d1_s5_sk_render" \
                bash -c "$sksl_cmd" || true
        fi
    fi

    return 0
}

# ============================================================================
# Direction 2: GLSL -> SKSL -> GLSL
# ============================================================================
run_direction2() {
    local shader="$1"
    local width=1280 height=720

    # The "original" GLSL for Direction 2 is the SKSL→GLSL output from Direction 1
    local d2_glsl_src="$V1/sksl_to_glsl/$shader/${shader}.glsl"
    local d2_glsl_prov_src="$V1/sksl_to_glsl/$shader/${shader}.glsl.provenance"
    if [ ! -f "$d2_glsl_src" ]; then
        log_step "$shader" "D2" SKIP "no GLSL from Dir1 — run Direction 1 first"
        return 1
    fi

    # Step 1: Render original GLSL
    local d2_step1_out="$V12/glsl/${shader}.png"
    local d2_step1_ppm="$TMP/${shader}_d2_glsl.ppm"
    mkdir -p "$V12/glsl"
    if [ -f "$d2_step1_out" ]; then
        log_step "$shader" "D2.S1_render_orig_GLSL" SKIP "already exists"
    else
        local render_cmd
        render_cmd=$(build_glsl_render_cmd "$shader" "$d2_glsl_src" "$d2_step1_ppm" "$width" "$height")
        if run_step "$shader" "D2.S1_render_orig_GLSL" "d2_s1_glsl_render" \
            bash -c "$render_cmd"; then
            convert "$d2_step1_ppm" "$d2_step1_out" 2>/dev/null || true
        fi
    fi

    # Step 2: GLSL -> SKSL + provenance
    local d2_step2_dir="$V12/glsl_to_sksl/$shader"
    local d2_sksl="$d2_step2_dir/${shader}.sksl"
    local d2_sksl_prov="$d2_step2_dir/${shader}.sksl.provenance"

    local gl2sk_input="$TMP/${shader}_d2_glsl_to_sksl_input"
    local gl2sk_output="$TMP/${shader}_d2_glsl_to_sksl_output"

    mkdir -p "$d2_step2_dir"
    if [ -f "$d2_sksl" ]; then
        log_step "$shader" "D2.S2_GLSL_to_SKSL" SKIP "already exists"
    else
        rm -rf "$gl2sk_input" "$gl2sk_output"
        mkdir -p "$gl2sk_input"
        cp "$d2_glsl_src" "$gl2sk_input/${shader}.frag"
        if [ -f "$d2_glsl_prov_src" ]; then
            cp "$d2_glsl_prov_src" "$gl2sk_input/${shader}.frag.provenance"
        fi

        if run_step "$shader" "D2.S2_GLSL_to_SKSL" "d2_s2_gl2sk" \
            "$GLSLANG_TO_SKSL" "$gl2sk_input" "$gl2sk_output"; then
            cp "$gl2sk_output/${shader}.sksl" "$d2_sksl" 2>/dev/null || true
            cp "$gl2sk_output/${shader}.sksl.provenance" "$d2_sksl_prov" 2>/dev/null || true
        else
            log_step "$shader" "D2" FAIL "glslang-to-sksl failed — skipping rest of D2"
            return 1
        fi
    fi

    # Step 3: Render converted SKSL
    local d2_step3_out="$V12/to_sksl/${shader}.png"
    mkdir -p "$V12/to_sksl"
    if [ -f "$d2_step3_out" ]; then
        log_step "$shader" "D2.S3_render_SKSL" SKIP "already exists"
    else
        if [ ! -f "$d2_sksl" ]; then
            log_step "$shader" "D2.S3_render_SKSL" SKIP "no SKSL to render"
        else
            # Fix sampler2D -> shader for RuntimeEffect compatibility (known converter issue)
            local d2_fixed_sksl="$TMP/${shader}_d2_fixed.sksl"
            sed 's/uniform sampler2D /uniform shader /g' "$d2_sksl" > "$d2_fixed_sksl"
            local sksl_cmd
            sksl_cmd=$(build_sksl_render_cmd "$shader" "$d2_fixed_sksl" "$d2_step3_out" "$width" "$height")
            run_step "$shader" "D2.S3_render_SKSL" "d2_s3_sk_render" \
                bash -c "$sksl_cmd" || true
        fi
    fi

    # Step 4: SKSL -> GLSL 还原
    local d2_step4_dir="$V12/sksl_to_glsl/$shader"
    local d2_glsl="$d2_step4_dir/${shader}.glsl"
    local d2_glsl_prov="$d2_step4_dir/${shader}.glsl.provenance"

    mkdir -p "$d2_step4_dir"
    if [ -f "$d2_glsl" ]; then
        log_step "$shader" "D2.S4_SKSL_to_GLSL" SKIP "already exists"
    else
        if [ ! -f "$d2_sksl" ]; then
            log_step "$shader" "D2.S4_SKSL_to_GLSL" SKIP "no SKSL input"
        else
            # Fix sampler2D -> shader (known converter issue in glslang-to-sksl)
            # and use .rts extension for RuntimeEffect shader compatibility
            local d2_fixed4="$TMP/${shader}_d2_fixed4.rts"
            sed 's/uniform sampler2D /uniform shader /g' "$d2_sksl" > "$d2_fixed4"
            if run_step "$shader" "D2.S4_SKSL_to_GLSL" "d2_s4_sk2gl" \
                "$SKSLC_CUSTOM" "$d2_fixed4" "$d2_glsl"; then
                # Fix FragColor shadowing: if the output variable name appears
                # as a local declaration inside main(), remove it to avoid
                # shadowing the global 'out vec4' declaration.
                sed -i '/^    vec4 FragColor;$/d' "$d2_glsl"
                sed -i '/^    FragColor = FragColor;$/d' "$d2_glsl"
            else
                log_step "$shader" "D2.S4_SKSL_to_GLSL" FAIL "see log"
            fi
        fi
    fi

    # Step 5: Render 还原 GLSL
    local d2_step5_out="$V12/to_glsl/${shader}.png"
    local d2_step5_ppm="$TMP/${shader}_d2_glsl_rt.ppm"
    mkdir -p "$V12/to_glsl"
    if [ -f "$d2_step5_out" ]; then
        log_step "$shader" "D2.S5_render_restored_GLSL" SKIP "already exists"
    else
        if [ ! -f "$d2_glsl" ]; then
            log_step "$shader" "D2.S5_render_restored_GLSL" SKIP "no GLSL to render"
        else
            local render_cmd
            render_cmd=$(build_glsl_render_cmd "$shader" "$d2_glsl" "$d2_step5_ppm" "$width" "$height")
            if run_step "$shader" "D2.S5_render_restored_GLSL" "d2_s5_gl_render" \
                bash -c "$render_cmd"; then
                convert "$d2_step5_ppm" "$d2_step5_out" 2>/dev/null || true
            fi
        fi
    fi

    return 0
}

# ============================================================================
# Comparison
# ============================================================================
run_comparisons() {
    local shader="$1"

    # Direction 1: compare v1/sksl/ (original SKSL) vs v1/to_sksl/ (restored SKSL)
    local d1_orig="$V1/sksl/${shader}.png"
    local d1_restored="$V1/to_sksl/${shader}.png"
    local d1_report="$REPORTS/${shader}_dir1.json"
    local d1_diffmap="$REPORTS/${shader}_dir1_diffmap.png"

    if [ -f "$d1_orig" ] && [ -f "$d1_restored" ]; then
        if python3 "$COMPARE_PY" "$d1_orig" "$d1_restored" \
            --output-json "$d1_report" --diffmap "$d1_diffmap" 2>/dev/null; then
            log_step "$shader" "D1_COMPARE_SKSL" OK "$(python3 -c "import json; d=json.load(open('$d1_report')); print(f\"PSNR={d['psnr']}dB diff={d['pixel_diff_percent']}%\")" 2>/dev/null)"
        else
            log_step "$shader" "D1_COMPARE_SKSL" FAIL "see $d1_report"
        fi
    else
        log_step "$shader" "D1_COMPARE_SKSL" SKIP "missing render(s)"
    fi

    # Direction 2: compare v12/glsl/ (original GLSL) vs v12/to_glsl/ (restored GLSL)
    local d2_orig="$V12/glsl/${shader}.png"
    local d2_restored="$V12/to_glsl/${shader}.png"
    local d2_report="$REPORTS/${shader}_dir2.json"
    local d2_diffmap="$REPORTS/${shader}_dir2_diffmap.png"

    if [ -f "$d2_orig" ] && [ -f "$d2_restored" ]; then
        if python3 "$COMPARE_PY" "$d2_orig" "$d2_restored" \
            --output-json "$d2_report" --diffmap "$d2_diffmap" 2>/dev/null; then
            log_step "$shader" "D2_COMPARE_GLSL" OK "$(python3 -c "import json; d=json.load(open('$d2_report')); print(f\"PSNR={d['psnr']}dB diff={d['pixel_diff_percent']}%\")" 2>/dev/null)"
        else
            log_step "$shader" "D2_COMPARE_GLSL" FAIL "see $d2_report"
        fi
    else
        log_step "$shader" "D2_COMPARE_GLSL" SKIP "missing render(s)"
    fi
}

# ============================================================================
# Summary
# ============================================================================
print_summary() {
    echo ""
    echo "=============================================="
    echo "  BIDIRECTIONAL TEST SUMMARY"
    echo "=============================================="
    echo ""

    printf "%-35s %-15s %-15s\n" "Shader" "Dir1(SKSL)" "Dir2(GLSL)"
    printf "%-35s %-15s %-15s\n" "-----------------------------------" "---------------" "---------------"

    for shader in "${SHADERS[@]}"; do
        local d1_status="SKIP" d2_status="SKIP"

        local d1_report="$REPORTS/${shader}_dir1.json"
        if [ -f "$d1_report" ]; then
            d1_status=$(python3 -c "import json; d=json.load(open('$d1_report')); print('PASS' if d['passed'] else 'FAIL')" 2>/dev/null || echo "FAIL")
        fi

        local d2_report="$REPORTS/${shader}_dir2.json"
        if [ -f "$d2_report" ]; then
            d2_status=$(python3 -c "import json; d=json.load(open('$d2_report')); print('PASS' if d['passed'] else 'FAIL')" 2>/dev/null || echo "FAIL")
        fi

        printf "%-35s %-15s %-15s\n" "$shader" "$d1_status" "$d2_status"
    done

    echo ""
    echo "Reports: $REPORTS/"
    echo "Results: $RESULTS_DIR/"
    echo ""

    # Count failures
    if [ ${#FAILURES[@]} -gt 0 ]; then
        echo "Failures/Skips:"
        for f in "${FAILURES[@]}"; do
            echo "  $f"
        done
    fi
}

# ============================================================================
# Pipeline Mode — flexible single-direction conversion + render + animate
# ============================================================================

PIPELINE_OUTPUT_ROOT="$PROJECT_ROOT/results/glsltosksl/v1"


render_single_frame_glsl() {
    local frag_path="$1" out_path="$2" width="$3" height="$4" config_json="$5"
    local out_ppm="${out_path%.png}.ppm"

    # Flatten uniform blocks: render_glsl can't access block members by name.
    # Extract "vec2 iResolution;" from "uniform Params { vec2 iResolution; } u;"
    # and replace "u.iResolution" references with "iResolution".
    local flat_glsl="$TMP/$(basename "${frag_path%.*}")_flat.glsl"
    cp "$frag_path" "$flat_glsl"
    python3 -c "
import re
with open('$flat_glsl', 'r') as f: src = f.read()
# Flatten: 'layout(...) uniform Name { type member; } inst;' → 'uniform type member;'
def flatten_block(m):
    body = m.group(1).strip()
    # body is like 'vec2 iResolution;' or 'float x;\\n    vec2 y;'
    members = [s.strip() for s in body.split(';') if s.strip()]
    return '\\n'.join('uniform ' + s + ';' for s in members)
src = re.sub(r'layout\([^)]*\)\s*uniform\s+\w+\s*\{([^}]*)\}\s*\w+\s*;',
             flatten_block, src)
# Replace block instance member access: u.iResolution → iResolution
src = re.sub(r'\bu\.(\w+)\b', r'\1', src)
with open('$flat_glsl', 'w') as f: f.write(src)
" 2>/dev/null || cp "$frag_path" "$flat_glsl"

    # Auto-set iResolution with correct component count
    local ires_vals="$width $height"
    if grep -qE 'vec3\s+iResolution|float3\s+iResolution' "$flat_glsl" 2>/dev/null; then
        ires_vals="$width $height 1"
    fi
    local render_cmd=("$RENDER_GLSL" "$flat_glsl" "$out_ppm" "$width" "$height"
        "--uniform" "iResolution" $ires_vals)

    # Add textures from config (process substitution avoids subshell)
    while read -r tname tpath traw; do
        [ -z "$tname" ] && continue
        local raw="$TMP/pipeline_${tname}.raw"
        png_to_raw_rgba "$tpath" "$raw" || continue
        render_cmd+=("--rgatex" "$tname" "$raw")
        [ "$traw" = "true" ] && render_cmd+=("--rawtex" "$tname")
    done < <(python3 -c "
import json, sys
cfg = json.loads('''$config_json''')
for name, val in cfg.get('textures', {}).items():
    if isinstance(val, str):
        print(f'{name} {val} false')
    else:
        print(f'{name} {val[\"path\"]} {val.get(\"raw\", False)}')
")

    # Add uniforms from config
    while read -r uname uvals; do
        [ -z "$uname" ] && continue
        render_cmd+=("--uniform" "$uname" $uvals)
    done < <(python3 -c "
import json
cfg = json.loads('''$config_json''')
for name, val in cfg.get('uniforms', {}).items():
    if isinstance(val, list):
        print(f'{name} {\" \".join(str(v) for v in val)}')
    else:
        print(f'{name} {val}')
")

    local err_log="${out_path%.png}.errors.txt"
    if "${render_cmd[@]}" > "$err_log" 2>&1; then
        rm -f "$err_log"
    else
        echo "ERROR: GLSL render failed — see $err_log" >&2
        return 1
    fi
    convert "$out_ppm" "$out_path" 2>/dev/null || return 1
    rm -f "$out_ppm"
}

render_single_frame_sksl() {
    local sksl_path="$1" out_path="$2" width="$3" height="$4" config_json="$5"
    local cmd=("uv" "run" "--project" "$PROJECT_ROOT" "python3" "$RENDER_SKSL"
        "--sksl" "$sksl_path" "--output" "$out_path"
        "--width" "$width" "--height" "$height")

    # Add textures from config
    while read -r tex_arg; do
        [ -z "$tex_arg" ] && continue
        cmd+=("--texture" "$tex_arg")
    done < <(python3 -c "
import json
cfg = json.loads('''$config_json''')
for name, val in cfg.get('textures', {}).items():
    path = val if isinstance(val, str) else val['path']
    print(f'{name}={path}')
")

    # Add uniforms from config
    while read -r uni_arg; do
        [ -z "$uni_arg" ] && continue
        cmd+=("--uniform" "$uni_arg")
    done < <(python3 -c "
import json
cfg = json.loads('''$config_json''')
for name, val in cfg.get('uniforms', {}).items():
    if isinstance(val, list):
        print(f'{name}={\",\".join(str(v) for v in val)}')
    else:
        print(f'{name}={val}')
")

    # Add raw children
    while read -r rname; do
        [ -z "$rname" ] && continue
        cmd+=("--raw" "$rname")
    done < <(python3 -c "
import json
cfg = json.loads('''$config_json''')
for name, val in cfg.get('textures', {}).items():
    if isinstance(val, dict) and val.get('raw'):
        print(name)
")

    local err_log="${out_path%.png}.errors.txt"
    if "${cmd[@]}" > "$err_log" 2>&1; then
        rm -f "$err_log"
    else
        # Preserve error log for diagnosis
        echo "ERROR: SKSL render failed — see $err_log" >&2
        return 1
    fi
}

render_single_frame_sksl_gpu() {
    # Render SKSL via GPU path: skslc (.frag mode, preserves fwidth/dFdx)
    # → GLSL → render_glsl (OpenGL, natively supports derivatives).
    local sksl="$1" out_png="$2" width="$3" height="$4" config_json="$5"
    local sksl_as_frag="$TMP/sksl_gpu_$(basename "${sksl%.*}").frag"
    local out_glsl="$TMP/sksl_gpu_$(basename "${sksl%.*}").glsl"
    local out_ppm="${out_png%.png}.ppm"

    # Step 1: Compile SKSL → GLSL using skslc with .frag input mode
    # .frag mode = GPU fragment processor, preserves fwidth/dFdx natively
    cp "$sksl" "$sksl_as_frag"
    "$SKSLC" "$sksl_as_frag" "$out_glsl" 2>&1 || { echo "skslc compile failed"; return 1; }

    # skslc outputs Skia-internal GLSL; rewrite for standard GLSL.
    python3 -c "
import re
with open('$out_glsl', 'r') as f: src = f.read()
# vec4/float4 main() → void main()
src = re.sub(r'\b(vec4|float4|half4)\s+main\s*\(\)', 'void main()', src)
# 'return EXPR;' → 'sk_FragColor = EXPR; return;'
src = re.sub(r'return\s+(.+?);', r'sk_FragColor = \1; return;', src)
# fragCoord → gl_FragCoord
src = re.sub(r'\bfragCoord\b', 'gl_FragCoord', src)
src = re.sub(r'gl_FragCoord\s*-\s*(?!\.xy)', 'gl_FragCoord.xy - ', src)
src = re.sub(r'gl_FragCoord\s*\+\s*(?!\.xy)', 'gl_FragCoord.xy + ', src)
with open('$out_glsl', 'w') as f: f.write(src)
"

    # Step 2: Render GLSL via OpenGL (fwidth/dFdx work natively)
    local render_cmd=("$RENDER_GLSL" "$out_glsl" "$out_ppm" "$width" "$height")
    # Add textures/uniforms from config
    while read -r tname tpath traw; do
        [ -z "$tname" ] && continue
        local raw="$TMP/pipeline_${tname}.raw"
        png_to_raw_rgba "$tpath" "$raw" || continue
        render_cmd+=("--rgatex" "$tname" "$raw")
        [ "$traw" = "true" ] && render_cmd+=("--rawtex" "$tname")
    done < <(python3 -c "
import json
cfg = json.loads('''$config_json''')
for name, val in cfg.get('textures', {}).items():
    if isinstance(val, str): print(f'{name} {val} false')
    else: print(f'{name} {val[\"path\"]} {val.get(\"raw\", False)}')
" 2>/dev/null)
    while read -r uname uvals; do
        [ -z "$uname" ] && continue
        render_cmd+=("--uniform" "$uname" $uvals)
    done < <(python3 -c "
import json
cfg = json.loads('''$config_json''')
for name, val in cfg.get('uniforms', {}).items():
    if isinstance(val, list): print(f'{name} {\" \".join(str(v) for v in val)}')
    else: print(f'{name} {val}')
" 2>/dev/null)
    # Auto-set Shadertoy defaults
    render_cmd+=("--itime" "1.5")

    local err_log="${out_png%.png}.errors.txt"
    if "${render_cmd[@]}" > "$err_log" 2>&1; then
        rm -f "$err_log"
        convert "$out_ppm" "$out_png" 2>/dev/null || return 1
        rm -f "$out_ppm"
    else
        echo "ERROR: GPU SKSL render failed — see $err_log" >&2
        return 1
    fi
}

convert_sksl_to_glsl() {
    local sksl="$1" out_glsl="$2"
    # Try .rts mode first (supports shader/eval)
    local sksl_as_rts="$TMP/$(basename "${sksl%.*}").rts"
    cp "$sksl" "$sksl_as_rts"
    if "$SKSLC_CUSTOM" "$sksl_as_rts" "$out_glsl" 2>/dev/null; then
        rm -f "$sksl_as_rts"
        sed -i '/^    vec4 FragColor;$/d' "$out_glsl"
        sed -i '/^    FragColor = FragColor;$/d' "$out_glsl"
        return 0
    fi
    rm -f "$sksl_as_rts" "$out_glsl"

    # Fall back to .frag mode via skslc (supports fwidth/dFdx for procedural shaders)
    local sksl_as_frag="$TMP/$(basename "${sksl%.*}").frag"
    cp "$sksl" "$sksl_as_frag"
    if "$SKSLC" "$sksl_as_frag" "$out_glsl" 2>/dev/null; then
        python3 -c "
import re
with open('$out_glsl', 'r') as f: src = f.read()
src = re.sub(r'\b(vec4|float4|half4)\s+main\s*\(\)', 'void main()', src)
src = re.sub(r'return\s+(.+?);', r'sk_FragColor = \1; return;', src)
src = re.sub(r'\bfragCoord\b', 'gl_FragCoord', src)
src = re.sub(r'gl_FragCoord\s*-\s*(?!\.xy)', 'gl_FragCoord.xy - ', src)
src = re.sub(r'gl_FragCoord\s*\+\s*(?!\.xy)', 'gl_FragCoord.xy + ', src)
with open('$out_glsl', 'w') as f: f.write(src)
"
        rm -f "$sksl_as_frag"
        return 0
    fi
    rm -f "$sksl_as_frag"
    return 1
}

convert_glsl_to_sksl() {
    local glsl="$1" out_sksl="$2"
    local tmp_in="$TMP/pipeline_glsl_to_sksl_in"
    local tmp_out="$TMP/pipeline_glsl_to_sksl_out"
    rm -rf "$tmp_in" "$tmp_out"
    mkdir -p "$tmp_in"
    local base; base=$(basename "${glsl%.*}")

    cp "$glsl" "$tmp_in/${base}.frag"
    local prov="${glsl}.provenance"
    [ -f "$prov" ] && cp "$prov" "$tmp_in/${base}.frag.provenance"
    "$GLSLANG_TO_SKSL" "$tmp_in" "$tmp_out" 2>&1 || return 1
    cp "$tmp_out/${base}.sksl" "$out_sksl" 2>/dev/null || return 1
    cp "$tmp_out/${base}.sksl.provenance" "$(dirname "$out_sksl")/${base}.sksl.provenance" 2>/dev/null || true
    # Dedup: glslang may produce both "type name;" and "type name = init;"
    # for the same global. Remove the bare declaration if an initialized
    # one follows within 3 lines.
    python3 -c "
import re
with open('$out_sksl', 'r') as f:
    lines = f.readlines()
i = 0
while i < len(lines):
    m = re.match(r'^(\w[\w\d]*)\s+(\w[\w\d]*)\s*;\s*$', lines[i])
    if m:
        typ, name = m.group(1), m.group(2)
        for j in range(i+1, min(i+4, len(lines))):
            if re.match(rf'^{re.escape(typ)}\s+{re.escape(name)}\s*=', lines[j]):
                lines[i] = ''  # remove bare decl
                break
    i += 1
with open('$out_sksl', 'w') as f:
    f.writelines([l for l in lines if l != ''])
" 2>/dev/null || true
    # Also dump glslang AST intermediate representation.
    # Shadertoy shaders lack #version; prepend #version 330 for proper AST dump.
    local out_ast="$(dirname "$out_sksl")/${base}.ast"
    if head -1 "$glsl" | grep -q "#version"; then
        "$GLSLANG" -i -S frag "$glsl" > "$out_ast" 2>&1 || true
    else
        local tmp_glsl="$TMP/${base}_with_version.frag"
        { echo "#version 330"; cat "$glsl"; } > "$tmp_glsl"
        "$GLSLANG" -i -S frag "$tmp_glsl" > "$out_ast" 2>&1 || true
        rm -f "$tmp_glsl"
    fi
    rm -rf "$tmp_in" "$tmp_out"
}

render_glsl_animated() {
    local frag_path="$1" frame_dir="$2" config_json="$3"
    local width height fps time_start time_end time_uniform
    width=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['dimensions']['width'])")
    height=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['dimensions']['height'])")
    fps=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['fps'])")
    time_start=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['start'])")
    time_end=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['end'])")
    time_uniform=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['uniform'])")

    local total_frames
    total_frames=$(python3 -c "print(int(($time_end - $time_start) * $fps))")
    [ "$total_frames" -le 0 ] && { echo "ERROR: invalid time range"; return 1; }

    echo "Rendering $total_frames frames to $frame_dir (${width}x${height}, ${fps}fps)"
    mkdir -p "$frame_dir"

    # Fix premultiplied alpha on a temp copy (matching single-frame behavior)
    local fixed_frag="$TMP/pipeline_animated_fixed.frag"
    cp "$frag_path" "$fixed_frag"
    fix_premultiplied_alpha "$fixed_frag"

    local frame=1
    while [ "$frame" -le "$total_frames" ]; do
        local t
        t=$(python3 -c "print($time_start + ($frame - 1) / $fps)")
        local frame_out="$frame_dir/frame_$(printf '%04d' "$frame").png"

        if [ -f "$frame_out" ] && [ -z "${FORCE:-}" ]; then
            frame=$((frame + 1))
            continue
        fi

        local out_ppm="${frame_out%.png}.ppm"
        local render_cmd=("$RENDER_GLSL" "$fixed_frag" "$out_ppm" "$width" "$height")

        # Add textures
        while read -r tname tpath traw; do
            [ -z "$tname" ] && continue
            local raw="$TMP/pipeline_${tname}.raw"
            png_to_raw_rgba "$tpath" "$raw" 2>/dev/null || continue
            render_cmd+=("--rgatex" "$tname" "$raw")
            [ "$traw" = "true" ] && render_cmd+=("--rawtex" "$tname")
        done < <(python3 -c "
import json
cfg = json.loads('''$config_json''')
for name, val in cfg.get('textures', {}).items():
    if isinstance(val, str):
        print(f'{name} {val} false')
    else:
        print(f'{name} {val[\"path\"]} {val.get(\"raw\", False)}')
")

        # Add static uniforms (skip time uniform)
        while read -r uname uvals; do
            [ -z "$uname" ] && continue
            render_cmd+=("--uniform" "$uname" $uvals)
        done < <(python3 -c "
import json
cfg = json.loads('''$config_json''')
time_u = cfg['animation']['uniform']
for name, val in cfg.get('uniforms', {}).items():
    if name == time_u:
        continue
    if isinstance(val, list):
        print(f'{name} {\" \".join(str(v) for v in val)}')
    else:
        print(f'{name} {val}')
")

        # Set time uniform
        render_cmd+=("--uniform" "$time_uniform" "$t")

        "${render_cmd[@]}" >/dev/null 2>&1 || {
            echo "WARNING: frame $frame render failed (t=$t)"
            frame=$((frame + 1))
            continue
        }
        convert "$out_ppm" "$frame_out" 2>/dev/null || true
        rm -f "$out_ppm"

        pct=$((100 * frame / total_frames))
        printf "\r  Frame %d/%d (%d%%) t=%.2f" "$frame" "$total_frames" "$pct" "$t"
        frame=$((frame + 1))
    done
    echo ""

    # Write sentinel
    touch "$frame_dir/render_done.marker"
}

render_sksl_animated_wrapper() {
    local sksl_path="$1" frame_dir="$2" config_json="$3"
    local width height fps time_start time_end time_uniform
    width=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['dimensions']['width'])")
    height=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['dimensions']['height'])")
    fps=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['fps'])")
    time_start=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['start'])")
    time_end=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['end'])")
    time_uniform=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['uniform'])")

    local cmd=("uv" "run" "--project" "$PROJECT_ROOT" "python3" "$SCRIPT_DIR/render_animated.py"
        "--sksl" "$sksl_path"
        "--output-dir" "$frame_dir"
        "--time-start" "$time_start"
        "--time-end" "$time_end"
        "--fps" "$fps"
        "--time-uniform" "$time_uniform"
        "--width" "$width" "--height" "$height")

    # Add textures
    while read -r tex_arg; do
        [ -z "$tex_arg" ] && continue
        cmd+=("--texture" "$tex_arg")
    done < <(python3 -c "
import json
cfg = json.loads('''$config_json''')
for name, val in cfg.get('textures', {}).items():
    path = val if isinstance(val, str) else val['path']
    print(f'{name}={path}')
")

    # Add static uniforms (skip time uniform)
    while read -r uni_arg; do
        [ -z "$uni_arg" ] && continue
        cmd+=("--uniform" "$uni_arg")
    done < <(python3 -c "
import json
cfg = json.loads('''$config_json''')
time_u = cfg['animation']['uniform']
for name, val in cfg.get('uniforms', {}).items():
    if name == time_u:
        continue
    if isinstance(val, list):
        print(f'{name}={\",\".join(str(v) for v in val)}')
    else:
        print(f'{name}={val}')
")

    # Add raw children
    while read -r rname; do
        [ -z "$rname" ] && continue
        cmd+=("--raw" "$rname")
    done < <(python3 -c "
import json
cfg = json.loads('''$config_json''')
for name, val in cfg.get('textures', {}).items():
    if isinstance(val, dict) and val.get('raw'):
        print(name)
")

    "${cmd[@]}" 2>&1
}

display_ascii_animation() {
    local frame_dir="$1" display_fps="$2"
    python3 "$SCRIPT_DIR/render_ascii.py" \
        --frame-dir "$frame_dir" --fps "$display_fps" --wait &
    echo $!
}

encode_mp4() {
    local frame_dir="$1" out_mp4="$2" fps="$3"
    if ! command -v ffmpeg &>/dev/null; then
        echo "WARNING: ffmpeg not found, skipping MP4 encoding"
        return 1
    fi
    ffmpeg -y -v warning -framerate "$fps" -i "$frame_dir/frame_%04d.png" \
        -c:v libx264 -pix_fmt yuv420p -preset fast -crf 18 "$out_mp4" 2>&1
    echo "MP4 written: $out_mp4"
}

run_pipeline_sksl_to_glsl() {
    local input="$1" config_json="$2" out_dir="$3"
    local name width height
    name=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['name'])")
    width=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['dimensions']['width'])")
    height=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['dimensions']['height'])")
    local animated
    animated=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['enabled'])")

    echo "=== Pipeline: SKSL -> GLSL ($name) ==="

    # before/: copy original and render
    cp "$input" "$out_dir/before/${name}.sksl"
    echo "  [before] copied $name.sksl"

    if [ "$animated" = "True" ]; then
        local ascii_enabled ascii_fps
        ascii_enabled=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['ascii']['enabled'])")
        ascii_fps=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['ascii']['fps'])")

        # Start ASCII animation in background
        if [ "$ascii_enabled" = "True" ] && [ -t 1 ]; then
            ASCII_PID=$(display_ascii_animation "$out_dir/before" "$ascii_fps")
            echo "  [ascii] started PID=$ASCII_PID"
        fi

        render_sksl_animated_wrapper "$input" "$out_dir/before" "$config_json"
        echo "  [before] animated render done"

        if [ -n "${ASCII_PID:-}" ]; then
            kill "$ASCII_PID" 2>/dev/null || true
            wait "$ASCII_PID" 2>/dev/null || true
        fi
        encode_mp4 "$out_dir/before" "$out_dir/before/${name}.mp4" \
            "$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['fps'])")" || true
    else
        render_single_frame_sksl "$input" "$out_dir/before/${name}.png" "$width" "$height" "$config_json"
        echo "  [before] rendered $name.png"
    fi

    # code/: convert SKSL->GLSL
    convert_sksl_to_glsl "$input" "$out_dir/code/${name}.glsl" || {
        echo "ERROR: SKSL->GLSL conversion failed"
        return 1
    }
    echo "  [code] converted $name.glsl"

    # after/: render converted GLSL
    local glsl="$out_dir/code/${name}.glsl"
    fix_premultiplied_alpha "$glsl"

    if [ "$animated" = "True" ]; then
        local ascii_enabled ascii_fps
        ascii_enabled=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['ascii']['enabled'])")
        ascii_fps=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['ascii']['fps'])")

        if [ "$ascii_enabled" = "True" ] && [ -t 1 ]; then
            ASCII_PID=$(display_ascii_animation "$out_dir/after" "$ascii_fps")
            echo "  [ascii] started PID=$ASCII_PID"
        fi

        render_glsl_animated "$glsl" "$out_dir/after" "$config_json"
        echo "  [after] animated render done"

        if [ -n "${ASCII_PID:-}" ]; then
            kill "$ASCII_PID" 2>/dev/null || true
            wait "$ASCII_PID" 2>/dev/null || true
        fi
        encode_mp4 "$out_dir/after" "$out_dir/after/${name}.mp4" \
            "$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['fps'])")" || true
    else
        render_single_frame_glsl "$glsl" "$out_dir/after/${name}.png" "$width" "$height" "$config_json"
        echo "  [after] rendered $name.png"
    fi

    echo "  Output: $out_dir/"
}

run_pipeline_glsl_to_sksl() {
    local input="$1" config_json="$2" out_dir="$3"
    local name width height
    name=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['name'])")
    width=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['dimensions']['width'])")
    height=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['dimensions']['height'])")
    local animated
    animated=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['enabled'])")

    echo "=== Pipeline: GLSL -> SKSL ($name) ==="

    # before/: pre-process GLSL (default inits + fwidth), save, and render
    local preprocessed="$TMP/${name}.frag"
    local mods_json="$out_dir/code/${name}.modifications.json"
    python3 "$SCRIPT_DIR/preprocess_glsl.py" "$input" \
        --output "$preprocessed" --mods "$mods_json" 2>&1 || cp "$input" "$preprocessed"
    cp "$preprocessed" "$out_dir/before/${name}.glsl"
    echo "  [before] pre-processed $name.glsl"

    if [ "$animated" = "True" ]; then
        local ascii_enabled ascii_fps
        ascii_enabled=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['ascii']['enabled'])")
        ascii_fps=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['ascii']['fps'])")
        if [ "$ascii_enabled" = "True" ] && [ -t 1 ]; then
            ASCII_PID=$(display_ascii_animation "$out_dir/before" "$ascii_fps")
            echo "  [ascii] started PID=$ASCII_PID"
        fi
        render_glsl_animated "$preprocessed" "$out_dir/before" "$config_json"
        echo "  [before] animated render done"
        if [ -n "${ASCII_PID:-}" ]; then
            kill "$ASCII_PID" 2>/dev/null || true; wait "$ASCII_PID" 2>/dev/null || true
        fi
        encode_mp4 "$out_dir/before" "$out_dir/before/${name}.mp4" \
            "$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['fps'])")" || true
    else
        # No premultiplied-alpha fix here: the "after" SkSL is rendered via
        # the GPU path (skslc→GLSL→OpenGL), which naturally produces straight
        # alpha from texture().  Both sides use the same convention.
        render_single_frame_glsl "$preprocessed" "$out_dir/before/${name}.png" "$width" "$height" "$config_json"
        echo "  [before] rendered $name.png"
    fi

    # code/: convert GLSL->SKSL (uses pre-processed GLSL)
    convert_glsl_to_sksl "$preprocessed" "$out_dir/code/${name}.sksl" || {
        echo "ERROR: GLSL->SKSL conversion failed"
        return 1
    }
    echo "  [code] converted $name.sksl"

    # after/: render converted SKSL
    local sksl="$out_dir/code/${name}.sksl"
    if [ "$animated" = "True" ]; then
        local ascii_enabled ascii_fps
        ascii_enabled=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['ascii']['enabled'])")
        ascii_fps=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['ascii']['fps'])")

        if [ "$ascii_enabled" = "True" ] && [ -t 1 ]; then
            ASCII_PID=$(display_ascii_animation "$out_dir/after" "$ascii_fps")
            echo "  [ascii] started PID=$ASCII_PID"
        fi

        render_sksl_animated_wrapper "$sksl" "$out_dir/after" "$config_json"
        echo "  [after] animated render done"

        if [ -n "${ASCII_PID:-}" ]; then
            kill "$ASCII_PID" 2>/dev/null || true
            wait "$ASCII_PID" 2>/dev/null || true
        fi
        encode_mp4 "$out_dir/after" "$out_dir/after/${name}.mp4" \
            "$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['fps'])")" || true
    else
        # Render converted SKSL via GPU path (skslc→GLSL→OpenGL) to match
        # the "before" GLSL rendering backend, eliminating CPU vs GPU differences.
        # This is essential for: raymarched shaders (purple_cloud), shaders
        # using fwidth/dFdx (curve), and any numerically-sensitive computation.
        if ! render_single_frame_sksl_gpu "$sksl" "$out_dir/after/${name}.png" "$width" "$height" "$config_json"; then
            echo "  [after] GPU path failed, trying CPU RuntimeEffect..."
            render_single_frame_sksl "$sksl" "$out_dir/after/${name}.png" "$width" "$height" "$config_json" || true
        fi
        echo "  [after] rendered $name.png"
    fi

    echo "  Output: $out_dir/"
}

pipeline_show_help() {
    cat <<'EOF'
Pipeline mode — flexible single-direction conversion + render.

Usage: run_bidirectional_tests.sh pipeline <input> [options]

<input> can be a .sksl, .glsl, or .frag file, or a directory.

Options:
  --config FILE       Explicit config file (default: auto-detect <name>.params.json)
  --width W           Override output width
  --height H          Override output height
  --fps N             Animation frames per second (default: 30)
  --duration SEC      Animation duration in seconds (overrides config)
  --no-animate        Force single-frame render even if animation is detected
  --no-ascii          Disable terminal ASCII animation preview
  --direction DIR     "sksl_to_glsl", "glsl_to_sksl", or "bidirectional"
  --output DIR        Override output root directory (default: ./output)
  --force             Re-render even if outputs exist
  --help              Show this help

Config file (<name>.params.json, auto-detected alongside input):
  {
    "dimensions": {"width": 1280, "height": 720},
    "textures": {"image": "path/to/tex.png", "map": {"path": "p.png", "raw": true}},
    "uniforms": {"strength": 1.2, "center": [0.5, 0.7]},
    "animation": {"enabled": true, "uniform": "iTime", "start": 0.0, "end": 5.0, "fps": 30},
    "direction": "sksl_to_glsl"
  }

Examples:
  ./run_bidirectional_tests.sh pipeline my_shader.sksl
  ./run_bidirectional_tests.sh pipeline my_shader.sksl --config my.params.json
  ./run_bidirectional_tests.sh pipeline spread.frag --direction glsl_to_sksl
  ./run_bidirectional_tests.sh pipeline myshader.glsl --fps 60 --duration 10
EOF
}

run_pipeline() {
    local input="" config_file="" out_root="$PIPELINE_OUTPUT_ROOT"
    local override_width="" override_height="" override_fps="" override_duration=""
    local force_animate="" no_animate="" no_ascii="" direction="" force=""
    mkdir -p "$TMP"

    # Parse pipeline args
    while [ $# -gt 0 ]; do
        case "$1" in
            --config) config_file="$2"; shift 2 ;;
            --width) override_width="$2"; shift 2 ;;
            --height) override_height="$2"; shift 2 ;;
            --fps) override_fps="$2"; shift 2 ;;
            --duration) override_duration="$2"; shift 2 ;;
            --no-animate) no_animate="true"; shift ;;
            --no-ascii) no_ascii="true"; shift ;;
            --direction) direction="$2"; shift 2 ;;
            --output) out_root="$2"; shift 2 ;;
            --force) force="true"; shift ;;
            --help|-h) pipeline_show_help; return 0 ;;
            -*)
                echo "Unknown option: $1" >&2
                pipeline_show_help
                return 2
                ;;
            *)
                if [ -z "$input" ]; then input="$1"
                else echo "ERROR: unexpected argument: $1" >&2; return 2
                fi
                shift
                ;;
        esac
    done

    if [ -z "$input" ]; then
        echo "ERROR: no input specified" >&2
        pipeline_show_help
        return 2
    fi

    if [ ! -e "$input" ]; then
        echo "ERROR: input not found: $input" >&2
        return 1
    fi

    # Detect shader type
    local shader_type
    shader_type=$(detect_shader_type "$input")
    if [ "$shader_type" = "unknown" ]; then
        echo "ERROR: cannot determine shader type from: $input" >&2
        return 1
    fi

    local shader_name
    shader_name=$(extract_shader_name "$input")

    # Load config (auto-detect or explicit)
    local cfg_file
    if [ -n "$config_file" ]; then
        cfg_file="$config_file"
    else
        cfg_file=$(find_config_file "$input")
    fi
    [ -n "$cfg_file" ] && echo "Config: $cfg_file" || echo "Config: (defaults)"

    local config_json
    config_json=$(load_and_validate_config "$cfg_file" "$shader_name" "$shader_type")

    # Apply CLI overrides
    if [ -n "$override_width" ]; then
        config_json=$(echo "$config_json" | python3 -c "import json,sys; d=json.load(sys.stdin); d['dimensions']['width']=$override_width; print(json.dumps(d))")
    fi
    if [ -n "$override_height" ]; then
        config_json=$(echo "$config_json" | python3 -c "import json,sys; d=json.load(sys.stdin); d['dimensions']['height']=$override_height; print(json.dumps(d))")
    fi
    if [ -n "$override_fps" ]; then
        config_json=$(echo "$config_json" | python3 -c "import json,sys; d=json.load(sys.stdin); d['animation']['fps']=$override_fps; print(json.dumps(d))")
    fi
    if [ -n "$override_duration" ]; then
        config_json=$(echo "$config_json" | python3 -c "import json,sys; d=json.load(sys.stdin); d['animation']['end']=d['animation']['start']+$override_duration; print(json.dumps(d))")
    fi
    if [ -n "$no_animate" ]; then
        config_json=$(echo "$config_json" | python3 -c "import json,sys; d=json.load(sys.stdin); d['animation']['enabled']=False; print(json.dumps(d))")
    fi
    if [ -n "$no_ascii" ]; then
        config_json=$(echo "$config_json" | python3 -c "import json,sys; d=json.load(sys.stdin); d['animation']['ascii']['enabled']=False; print(json.dumps(d))")
    fi
    if [ -n "$direction" ]; then
        config_json=$(echo "$config_json" | python3 -c "import json,sys; d=json.load(sys.stdin); d['direction']='$direction'; print(json.dumps(d))")
    fi
    if [ -n "$force" ]; then
        export FORCE=1
    fi

    # Auto-detect animation if not explicitly set
    local animated
    animated=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['enabled'])")
    if [ "$animated" = "False" ] && [ -z "$no_animate" ]; then
        local detected
        detected=$(detect_animation_from_source "$input")
        if [ "$detected" = "true" ]; then
            echo "Auto-detected animation (time uniforms found in source)"
            config_json=$(echo "$config_json" | python3 -c "import json,sys; d=json.load(sys.stdin); d['animation']['enabled']=True; print(json.dumps(d))")
            animated="True"
        fi
    fi

    # Ensure output directories
    local out_dir
    out_dir=$(ensure_output_dirs "$shader_name" "$out_root")

    local pipe_direction
    pipe_direction=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['direction'])")

    # Run the pipeline
    case "$pipe_direction" in
        sksl_to_glsl)
            run_pipeline_sksl_to_glsl "$input" "$config_json" "$out_dir"
            ;;
        glsl_to_sksl)
            run_pipeline_glsl_to_sksl "$input" "$config_json" "$out_dir"
            ;;
        bidirectional)
            echo "=== Bidirectional pipeline ==="
            run_pipeline_sksl_to_glsl "$input" "$config_json" "${out_dir}_s2g"
            echo ""
            local glsl="${out_dir}_s2g/code/${shader_name}.glsl"
            if [ -f "$glsl" ]; then
                run_pipeline_glsl_to_sksl "$glsl" "$config_json" "${out_dir}_g2s"
            fi
            ;;
        *)
            echo "ERROR: unknown direction: $pipe_direction" >&2
            return 1
            ;;
    esac
}

# ============================================================================
# Fulltest Mode — v1/v2/roundtrip bidirectional verification
# ============================================================================
# Usage: runner.sh fulltest <input> --output <dir> [--stage v1|v2|report]
#
# Directory layout:
#   <output>/<name>/
#     v1/{before,code,after,report}/
#     v2/{before,code,after,report}/
#     report/
#
# v1:  original → convert → render → compare
# v2:  v1/code → reverse-convert → render → compare
# report (roundtrip):  v1/before vs v2/after

render_sksl_with_fallback() {
    # Render SKSL: CPU RuntimeEffect first; fall back to GPU via
    # convert_sksl_to_glsl (tries .rts then .frag) → render_glsl.
    # Set FT_GPU=1 to skip CPU and use GPU directly (e.g. for raymarched shaders).
    local sksl="$1" out_png="$2" width="$3" height="$4" config_json="$5"
    if [ "${FT_GPU:-0}" != "1" ]; then
        if render_single_frame_sksl "$sksl" "$out_png" "$width" "$height" "$config_json"; then
            return 0
        fi
        echo "  [sksl] CPU failed, trying GPU path..." >&2
    else
        echo "  [sksl] --gpu flag set, using GPU path directly..." >&2
    fi
    local tmp_glsl="$TMP/ft_$(basename "${sksl%.*}").glsl"
    if convert_sksl_to_glsl "$sksl" "$tmp_glsl"; then
        fix_premultiplied_alpha "$tmp_glsl"
        if render_single_frame_glsl "$tmp_glsl" "$out_png" "$width" "$height" "$config_json" 2>/dev/null; then
            return 0
        fi
    fi
    echo "  [sksl] All GPU paths failed" >&2
    return 1
}

render_glsl_for_fulltest() {
    # Render GLSL via GPU, applying premultiplied-alpha fix so the result
    # matches SkSL's .eval() convention.  Renders from a temp copy.
    local glsl="$1" out_png="$2" width="$3" height="$4" config_json="$5"
    local tmp="$TMP/ft_$(basename "${glsl%.*}").glsl"
    cp "$glsl" "$tmp"
    fix_premultiplied_alpha "$tmp"
    render_single_frame_glsl "$tmp" "$out_png" "$width" "$height" "$config_json"
}

run_fulltest() {
    local input="" output_root="" stage=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --output) output_root="$2"; shift 2 ;;
            --stage) stage="$2"; shift 2 ;;
            --no-animate) export FT_NO_ANIMATE=1; shift ;;
            --gpu) export FT_GPU=1; shift ;;
            --force) export FT_FORCE=1; shift ;;
            --help|-h) _ft_usage; return 0 ;;
            -*) echo "Unknown option: $1" >&2; _ft_usage; return 2 ;;
            *)  if [ -z "$input" ]; then input="$1"
                else echo "ERROR: unexpected arg: $1" >&2; return 2; fi
                shift ;;
        esac
    done

    if [ -z "$input" ] || [ -z "$output_root" ]; then
        echo "ERROR: <input> and --output <dir> are required" >&2
        _ft_usage; return 2
    fi
    [ -e "$input" ] || { echo "ERROR: input not found: $input" >&2; return 1; }

    local shader_type; shader_type=$(detect_shader_type "$input")
    [ "$shader_type" != "unknown" ] || { echo "ERROR: unknown shader type: $input" >&2; return 1; }
    local name; name=$(extract_shader_name "$input")

    # Load config
    local cfg_file; cfg_file=$(find_config_file "$input")
    local config_json; config_json=$(load_and_validate_config "$cfg_file" "$name" "$shader_type")
    # Override animation: fulltest always uses single-frame at fixed time (iTime=1.5)
    config_json=$(echo "$config_json" | python3 -c "import json,sys; d=json.load(sys.stdin); d['animation']['enabled']=False; print(json.dumps(d))")
    local width; width=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['dimensions']['width'])")
    local height; height=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['dimensions']['height'])")
    # Direction is determined by the input file type, overriding any config setting.
    local direction
    if [ "$shader_type" = "sksl" ]; then direction="sksl_to_glsl"
    else direction="glsl_to_sksl"; fi

    local out="$output_root/$name"
    mkdir -p "$out"/v1/{before,code,after,report} "$out"/v2/{before,code,after,report} "$out"/report

    echo "=== Fulltest: $name ($direction) → $out ==="

    # --- Stage helpers -------------------------------------------------------
    _ft_compare() {
        local a="$1" b="$2" report_json="$3" diffmap="$4"
        python3 "$COMPARE_PY" "$a" "$b" \
            --output-json "$report_json" --diffmap "$diffmap" 2>/dev/null
        python3 -c "import json; d=json.load(open('$report_json')); print(f\"  PSNR={d['psnr']}dB diff={d['pixel_diff_percent']}% maxΔ={d['max_pixel_diff']} px_diff={d['different_pixels']}\")" 2>/dev/null
    }

    # --- v1: original → convert → render → compare --------------------------
    _ft_v1() {
        echo "  [v1] $direction"
        local v1_before="$out/v1/before/$name"
        local v1_code="$out/v1/code/$name"
        local v1_after="$out/v1/after/$name"
        local v1_report="$out/v1/report/${name}_comparison.json"
        local v1_diffmap="$out/v1/report/${name}_diffmap.png"

        # 1. Render original
        if [ "$shader_type" = "sksl" ]; then
            cp "$input" "$out/v1/before/${name}.sksl"
            render_sksl_with_fallback "$input" "${v1_before}.png" "$width" "$height" "$config_json"
        else
            cp "$input" "$out/v1/before/${name}.glsl"
            render_glsl_for_fulltest "$input" "${v1_before}.png" "$width" "$height" "$config_json"
        fi

        # 2. Convert
        if [ "$direction" = "sksl_to_glsl" ]; then
            convert_sksl_to_glsl "$input" "${v1_code}.glsl"
            [ -f "${v1_code}.glsl.provenance" ] && cp "${v1_code}.glsl.provenance" "$out/v1/code/"
        else
            convert_glsl_to_sksl "$input" "${v1_code}.sksl"
            [ -f "${v1_code}.sksl.provenance" ] && cp "${v1_code}.sksl.provenance" "$out/v1/code/"
        fi

        # 3. Render converted
        if [ "$direction" = "sksl_to_glsl" ]; then
            render_glsl_for_fulltest "${v1_code}.glsl" "${v1_after}.png" "$width" "$height" "$config_json"
        else
            render_sksl_with_fallback "${v1_code}.sksl" "${v1_after}.png" "$width" "$height" "$config_json"
        fi

        # 4. Compare
        _ft_compare "${v1_before}.png" "${v1_after}.png" "$v1_report" "$v1_diffmap"
    }

    # --- v2: v1/code → reverse-convert → render → compare -------------------
    _ft_v2() {
        echo "  [v2] $direction → reverse"
        local v1_code_src; local v2_before_ext; local v2_code_ext
        if [ "$direction" = "sksl_to_glsl" ]; then
            v1_code_src="$out/v1/code/${name}.glsl"
            v2_before_ext="glsl"; v2_code_ext="sksl"
        else
            v1_code_src="$out/v1/code/${name}.sksl"
            v2_before_ext="sksl"; v2_code_ext="glsl"
        fi
        [ -f "$v1_code_src" ] || { echo "  [v2] SKIP — v1/code missing"; return 1; }

        local v2_before="$out/v2/before/$name"
        local v2_code="$out/v2/code/$name"
        local v2_after="$out/v2/after/$name"
        local v2_report="$out/v2/report/${name}_comparison.json"
        local v2_diffmap="$out/v2/report/${name}_diffmap.png"

        # 1. Copy v1/code → v2/before + render
        cp "$v1_code_src" "$out/v2/before/${name}.${v2_before_ext}"
        # Also copy provenance if present
        local v1_prov="${v1_code_src}.provenance"
        [ -f "$v1_prov" ] && cp "$v1_prov" "$out/v2/before/"

        if [ "$direction" = "sksl_to_glsl" ]; then
            # v1/code is GLSL → v2/before is GLSL
            render_glsl_for_fulltest "$v1_code_src" "${v2_before}.png" "$width" "$height" "$config_json"
        else
            # v1/code is SkSL → v2/before is SkSL
            render_sksl_with_fallback "$v1_code_src" "${v2_before}.png" "$width" "$height" "$config_json"
        fi

        # 2. Reverse convert (with provenance if available)
        if [ "$direction" = "sksl_to_glsl" ]; then
            # GLSL → SkSL
            convert_glsl_to_sksl "$v1_code_src" "${v2_code}.sksl"
            [ -f "${v2_code}.sksl.provenance" ] && cp "${v2_code}.sksl.provenance" "$out/v2/code/"
        else
            # SkSL → GLSL
            convert_sksl_to_glsl "$v1_code_src" "${v2_code}.glsl"
            [ -f "${v2_code}.glsl.provenance" ] && cp "${v2_code}.glsl.provenance" "$out/v2/code/"
        fi

        # 3. Render reverse-converted
        if [ "$direction" = "sksl_to_glsl" ]; then
            render_sksl_with_fallback "${v2_code}.sksl" "${v2_after}.png" "$width" "$height" "$config_json"
        else
            render_glsl_for_fulltest "${v2_code}.glsl" "${v2_after}.png" "$width" "$height" "$config_json"
        fi

        # 4. Compare
        _ft_compare "${v2_before}.png" "${v2_after}.png" "$v2_report" "$v2_diffmap"
    }

    # --- Roundtrip: v1/before vs v2/after ------------------------------------
    _ft_report() {
        echo "  [roundtrip] v1/before vs v2/after"
        local before_src="$out/v1/before/${name}.png"
        local after_src="$out/v2/after/${name}.png"
        if [ ! -f "$before_src" ] || [ ! -f "$after_src" ]; then
            echo "  [roundtrip] SKIP — missing renders"
            return 1
        fi
        _ft_compare "$before_src" "$after_src" \
            "$out/report/${name}_roundtrip.json" \
            "$out/report/${name}_roundtrip_diffmap.png"
    }

    # --- Execute stages ------------------------------------------------------
    case "$stage" in
        v1) _ft_v1 ;;
        v2) _ft_v2 ;;
        report) _ft_report ;;
        *)  _ft_v1 && _ft_v2 && _ft_report ;;
    esac

    echo "  Done: $out"
}

_ft_usage() {
    cat <<'EOF'
fulltest — Bidirectional round-trip verification (v1 + v2 + roundtrip)

Usage: runner.sh fulltest <input> --output <dir> [options]

  <input>            .sksl or .glsl/.frag file
  --output DIR       Output root directory (required)

Options:
  --stage v1|v2|report  Run a single stage (default: all three)
  --no-animate       Force single-frame (default: on; iTime locked to 1.5)
  --force            Re-render even if outputs exist
  --help             Show this help

Examples:
  runner.sh fulltest tests/shaders/water_ripple.sksl --output results/sksltoglsl/test2
  runner.sh fulltest tests/frag/curve.frag --output results/glsltosksl/test2
  runner.sh fulltest tests/frag/purple_cloud.frag --output results/glsltosksl/test2 --stage v1
EOF
}
main() {
    local cmd="${1:-test}"
    case "$cmd" in
        pipeline)
            shift
            run_pipeline "$@"
            return $?
            ;;
        fulltest)
            shift
            run_fulltest "$@"
            return $?
            ;;
        live)
            shift
            if [ $# -lt 1 ]; then
                echo "ERROR: live mode requires a shader file" >&2
                usage; return 2
            fi
            run_live_preview "$@"
            return $?
            ;;
        compare)
            shift
            run_compare "$@"
            return $?
            ;;
        help|--help|-h)
            usage; return 0
            ;;
        test)
            shift
            ;;
        *)
            # Legacy backward compatibility: first arg looks like a flag, treat as test
            if [ "${1:-}" = "--shader" ] || [ "${1:-}" = "--dir1" ] || [ "${1:-}" = "--dir2" ] || [ "${1:-}" = "--all" ]; then
                cmd="test"
            else
                echo "Unknown command: $cmd" >&2
                usage; return 2
            fi
            ;;
    esac

    # Parse arguments
    while [ $# -gt 0 ]; do
        case "$1" in
            --shader)
                SHADERS+=("$2"); shift 2 ;;
            --dir1)
                RUN_DIR2=false; shift ;;
            --dir2)
                RUN_DIR1=false; shift ;;
            --all)
                SHADERS=("${ALL_SHADERS[@]}")
                RUN_DIR1=true; RUN_DIR2=true; shift ;;
            --help|-h)
                usage; exit 0 ;;
            *)
                echo "Unknown option: $1" >&2
                usage; exit 2 ;;
        esac
    done

    # Default: all shaders
    if [ ${#SHADERS[@]} -eq 0 ]; then
        SHADERS=("${ALL_SHADERS[@]}")
    fi

    echo "=== Bidirectional SKSL<->GLSL Round-Trip Tests ==="
    echo "Shaders: ${SHADERS[*]}"
    echo "Dir1 (SKSL->GLSL->SKSL): $RUN_DIR1"
    echo "Dir2 (GLSL->SKSL->GLSL): $RUN_DIR2"
    echo ""

    # Check tools
    if ! check_tools; then
        echo "FATAL: missing required tools. Please build them first." >&2
        exit 1
    fi

    # Create directories
    mkdir -p "$V1/sksl" "$V1/sksl_to_glsl" "$V1/to_glsl" "$V1/glsl_to_sksl" "$V1/to_sksl"
    mkdir -p "$V12/glsl" "$V12/glsl_to_sksl" "$V12/to_sksl" "$V12/sksl_to_glsl" "$V12/to_glsl"
    mkdir -p "$REPORTS" "$TMP"

    # Global failure list
    FAILURES=()

    # Run tests
    for shader in "${SHADERS[@]}"; do
        echo "──────────────────────────────────────────────"
        echo "  Shader: $shader"
        echo "──────────────────────────────────────────────"

        if $RUN_DIR1; then
            run_direction1 "$shader" || true
        fi

        if $RUN_DIR2; then
            run_direction2 "$shader" || true
        fi

        run_comparisons "$shader"
        echo ""
    done

    print_summary
}

main "$@"
