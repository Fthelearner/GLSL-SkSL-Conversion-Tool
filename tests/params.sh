#!/usr/bin/env bash
# Per-shader parameter configuration for bidirectional tests.
# Source this file to get arrays of textures, uniforms, and dimensions.

# Project root (relative to this script)
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ASSETS_DIR="$PROJECT_ROOT/tests/assets"
SIZES_DEFAULT="1280 720"

# ---------------------------------------------------------------------------
# passthrough
# ---------------------------------------------------------------------------
passthrough_textures() {
    echo "image $ASSETS_DIR/input.png"
}
passthrough_uniforms() {
    echo ""
}
passthrough_skip_demo_args() {
    echo ""
}
passthrough_raw_children() {
    echo ""
}

# ---------------------------------------------------------------------------
# water_ripple
# ---------------------------------------------------------------------------
water_ripple_textures() {
    echo "image $ASSETS_DIR/input.png"
}
water_ripple_uniforms() {
    echo "iResolution 1280 720"
    echo "progress 0.35"
    echo "waveCount 2.0"
    echo "rippleCenter 0.5 0.7"
}
water_ripple_skip_demo_args() {
    echo "--progress 0.35 --wave-count 2 --ripple-center-x 0.5 --ripple-center-y 0.7"
}
water_ripple_raw_children() {
    echo ""
}

# ---------------------------------------------------------------------------
# displacement_distort
# ---------------------------------------------------------------------------
displacement_distort_textures() {
    echo "image $ASSETS_DIR/input.png"
    echo "displacementMap $ASSETS_DIR/displacement.png"
}
displacement_distort_uniforms() {
    echo "iResolution 1280 720"
    echo "factor 1.0 0.7"
    echo "strength 1.2"
}
displacement_distort_skip_demo_args() {
    echo "--strength 1.2 --factor-x 1.0 --factor-y 0.7"
}
displacement_distort_raw_children() {
    echo "displacementMap"
}

# ---------------------------------------------------------------------------
# linear_gradient_blend
# ---------------------------------------------------------------------------
linear_gradient_blend_textures() {
    echo "image $ASSETS_DIR/input.png"
    echo "preblurImage $ASSETS_DIR/preblur.png"
}
linear_gradient_blend_uniforms() {
    echo "iResolution 1280 720"
    echo "startPoint 0.5 0.15"
    echo "endPoint 0.5 0.85"
    echo "softness 0.2"
    echo "invert 0.0"
    echo "blurMix 1.0"
}
linear_gradient_blend_skip_demo_args() {
    echo "--start-y 0.15 --end-y 0.85 --softness 0.2 --strength 1.0"
}
linear_gradient_blend_raw_children() {
    echo ""
}

# ---------------------------------------------------------------------------
# variable_radius_blur_approx
# ---------------------------------------------------------------------------
variable_radius_blur_approx_textures() {
    echo "image $ASSETS_DIR/input.png"
    echo "blurMask $ASSETS_DIR/blur_mask.png"
}
variable_radius_blur_approx_uniforms() {
    echo "iResolution 1280 720"
    echo "maxRadius 24.0"
    echo "strength 1.0"
}
variable_radius_blur_approx_skip_demo_args() {
    echo "--max-radius 24 --strength 1.0"
}
variable_radius_blur_approx_raw_children() {
    echo ""
}

# ---------------------------------------------------------------------------
# Master lists
# ---------------------------------------------------------------------------
ALL_SHADERS=(
    passthrough
    water_ripple
    displacement_distort
    linear_gradient_blend
    variable_radius_blur_approx
)
