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
  live <shader>    Open real-time GUI preview window (SDL2, wall-clock time)
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
    python3 -c "
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
    # For intermediate computations (not direct FragColor output), premultiply
    # to match Skia's behavior. Pattern: "vec4 VAR = texture(...);"
    local glsl="$1"
    python3 -c "
import re
with open('$glsl', 'r') as f:
    content = f.read()
# After each 'vec4 VAR = texture(...);' add 'VAR.rgb *= VAR.a;'
content = re.sub(
    r'    vec4 (\w+) = texture\([^;]+\);',
    r'\g<0>\n    \1.rgb *= \1.a;',
    content
)
with open('$glsl', 'w') as f:
    f.write(content)
"
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

    if [ -f "$d1_orig" ] && [ -f "$d1_restored" ]; then
        if python3 "$COMPARE_PY" "$d1_orig" "$d1_restored" \
            --output-json "$d1_report" 2>/dev/null; then
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

    if [ -f "$d2_orig" ] && [ -f "$d2_restored" ]; then
        if python3 "$COMPARE_PY" "$d2_orig" "$d2_restored" \
            --output-json "$d2_report" 2>/dev/null; then
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

PIPELINE_OUTPUT_ROOT="$PROJECT_ROOT/results"


render_single_frame_glsl() {
    local frag_path="$1" out_path="$2" width="$3" height="$4" config_json="$5"
    local out_ppm="${out_path%.png}.ppm"

    local render_cmd=("$RENDER_GLSL" "$frag_path" "$out_ppm" "$width" "$height")

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

    "${render_cmd[@]}" 2>&1 || return 1
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

    "${cmd[@]}" 2>&1 || return 1
}

convert_sksl_to_glsl() {
    local sksl="$1" out_glsl="$2"
    local sksl_as_rts="$TMP/$(basename "${sksl%.*}").rts"
    cp "$sksl" "$sksl_as_rts"
    "$SKSLC_CUSTOM" "$sksl_as_rts" "$out_glsl" 2>&1 || return 1
    rm -f "$sksl_as_rts"
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

    # before/: copy original and render
    cp "$input" "$out_dir/before/${name}.glsl"
    echo "  [before] copied $name.glsl"

    if [ "$animated" = "True" ]; then
        local ascii_enabled ascii_fps
        ascii_enabled=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['ascii']['enabled'])")
        ascii_fps=$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['ascii']['fps'])")

        if [ "$ascii_enabled" = "True" ] && [ -t 1 ]; then
            ASCII_PID=$(display_ascii_animation "$out_dir/before" "$ascii_fps")
            echo "  [ascii] started PID=$ASCII_PID"
        fi

        render_glsl_animated "$input" "$out_dir/before" "$config_json"
        echo "  [before] animated render done"

        if [ -n "${ASCII_PID:-}" ]; then
            kill "$ASCII_PID" 2>/dev/null || true
            wait "$ASCII_PID" 2>/dev/null || true
        fi
        encode_mp4 "$out_dir/before" "$out_dir/before/${name}.mp4" \
            "$(echo "$config_json" | python3 -c "import json,sys; print(json.load(sys.stdin)['animation']['fps'])")" || true
    else
        render_single_frame_glsl "$input" "$out_dir/before/${name}.png" "$width" "$height" "$config_json"
        echo "  [before] rendered $name.png"
    fi

    # code/: convert GLSL->SKSL
    convert_glsl_to_sksl "$input" "$out_dir/code/${name}.sksl" || {
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
        render_single_frame_sksl "$sksl" "$out_dir/after/${name}.png" "$width" "$height" "$config_json"
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
# Main
# ============================================================================
main() {
    local cmd="${1:-test}"
    case "$cmd" in
        pipeline)
            shift
            run_pipeline "$@"
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
