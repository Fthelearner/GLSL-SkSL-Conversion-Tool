#!/usr/bin/env python3
"""
Phase 1: Generate missing intermediate images (blur levels, etc.)
Phase 2: Update params.json with semantically correct asset references
Phase 3: Re-render all SKSL files
"""

import json
import subprocess
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
SHADERS_DIR = Path(__file__).resolve().parent / "shaders"
ASSETS_DIR = Path(__file__).resolve().parent / "assets"
RESULTS_DIR = PROJECT / "results" / "sksl_render"
RENDER_SCRIPT = Path(__file__).resolve().parent / "render_sksl.py"

DEFAULT_W = 1280
DEFAULT_H = 720

# ============================================================
# Phase 1: Generate missing intermediate images
# ============================================================

def run_render(sksl_rel, output_png, textures=None, uniforms=None, width=1280, height=720):
    """Run render_sksl.py and return True if successful."""
    sksl_path = SHADERS_DIR / sksl_rel
    output_path = ASSETS_DIR / output_png
    output_path.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        sys.executable, str(RENDER_SCRIPT),
        "--sksl", str(sksl_path),
        "--output", str(output_path),
        "--width", str(width),
        "--height", str(height),
    ]
    for t in (textures or []):
        cmd.extend(["--texture", t])
    for u in (uniforms or []):
        cmd.extend(["--uniform", u])

    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        if r.returncode == 0:
            print(f"  Generated: {output_png}")
            return True
        else:
            print(f"  FAILED: {output_png} — {r.stderr.strip()[:120]}")
            return False
    except Exception as e:
        print(f"  ERROR: {output_png} — {e}")
        return False


def generate_missing_assets():
    """Generate blur images and other intermediate assets."""
    print("=" * 60)
    print("Phase 1: Generating missing intermediate assets")
    print("=" * 60)

    input_img = str(ASSETS_DIR / "input.png")

    # --- Blur images at different radii ---
    # Use mesa_blur/simple (passthrough) + kawase blur to generate blur levels
    blur_configs = [
        # (sksl_to_use, output_name, blur_params)
        ("filter/kawase_blur_shader_filter/blur.sksl", "blur_level1.png",
         [f"imageInput={input_img}", "in_blurOffset=2.0,0.0", "in_maxSizeXY=1280,720"]),
        ("filter/kawase_blur_shader_filter/blur.sksl", "blur_level2.png",
         [f"imageInput={input_img}", "in_blurOffset=6.0,0.0", "in_maxSizeXY=1280,720"]),
        ("filter/kawase_blur_shader_filter/blur.sksl", "blur_level3.png",
         [f"imageInput={input_img}", "in_blurOffset=14.0,0.0", "in_maxSizeXY=1280,720"]),
        ("filter/kawase_blur_shader_filter/blur.sksl", "blur_level4.png",
         [f"imageInput={input_img}", "in_blurOffset=30.0,0.0", "in_maxSizeXY=1280,720"]),
        # Edge blur (larger radius)
        ("filter/kawase_blur_shader_filter/blur.sksl", "blur_edge.png",
         [f"imageInput={input_img}", "in_blurOffset=24.0,0.0", "in_maxSizeXY=1280,720"]),
        # Background blur (small radius)
        ("filter/kawase_blur_shader_filter/blur.sksl", "blur_bg.png",
         [f"imageInput={input_img}", "in_blurOffset=3.0,0.0", "in_maxSizeXY=1280,720"]),
    ]

    for sksl_rel, out_name, textures in blur_configs:
        out_path = ASSETS_DIR / out_name
        if out_path.exists():
            print(f"  Skip (exists): {out_name}")
            continue
        run_render(sksl_rel, out_name, textures=textures)

    # --- Generate edge-detected image (for edge_light pipeline) ---
    edge_detect_out = ASSETS_DIR / "edge_detect.png"
    if not edge_detect_out.exists():
        print("  Generating edge-detected image...")
        # First generate a raw edge detection
        run_render("filter/edge_light_shader_filter/detect-frag.sksl", "edge_detect.png",
                   textures=[f"image={input_img}"],
                   uniforms=["edgeThreshold=0.3", "edgeIntensity=0.8",
                             "edgeSoftThreshold=0.3",
                             "edgeDetectColor=0.2126729,0.7151522,0.0721750",
                             "edgeColor=1.0,0.5,0.2", "ifRawColor=0.0"])

    # --- Generate edge blur images at different levels ---
    # Blur the edge detection result
    edge_img = str(ASSETS_DIR / "edge_detect.png")
    for i in range(5):
        out_name = f"edge_blur{i}.png"
        out_path = ASSETS_DIR / out_name
        if out_path.exists():
            print(f"  Skip (exists): {out_name}")
            continue
        offset = 2.0 * (i + 1)
        run_render("filter/kawase_blur_shader_filter/blur.sksl", out_name,
                   textures=[f"imageInput={edge_img}",
                             f"in_blurOffset={offset},0.0",
                             "in_maxSizeXY=1280,720"])

    # --- Generate a SDF rounded rect image (for SDF-dependent shaders) ---
    sdf_rrect = ASSETS_DIR / "sdf_rrect.png"
    if not sdf_rrect.exists():
        print("  Generating SDF rounded rect...")
        run_render("shape/sdf_rrect_shader_shape/rrect-shader-prog.sksl", "sdf_rrect.png",
                   uniforms=["centerPos=640.0,360.0", "halfSize=300.0,200.0",
                             "cornerRadiusTL=40.0,40.0", "cornerRadiusTR=40.0,40.0",
                             "cornerRadiusBR=40.0,40.0", "cornerRadiusBL=40.0,40.0"])

    sdf_triangle = ASSETS_DIR / "sdf_triangle.png"
    if not sdf_triangle.exists():
        print("  Generating SDF triangle...")
        run_render("shape/sdf_triangle_shader_shape/prog.sksl", "sdf_triangle.png",
                   uniforms=["vertex0=640.0,100.0", "vertex1=200.0,620.0",
                             "vertex2=1080.0,620.0", "radius=20.0"])

    # --- Generate normal maps for frosted glass ---
    ripple_normal = ASSETS_DIR / "ripple_normal.png"
    if not ripple_normal.exists():
        print("  Generating ripple normal map...")
        run_render("mask/ripple_shader_mask/prog_1.sksl", "ripple_normal.png",
                   uniforms=["iResolution=1280.0,720.0", "centerPos=640.0,360.0",
                             "rippleRadius=200.0", "rippleWidth=40.0",
                             "widthCenterOffset=0.0"])

    wave_normal = ASSETS_DIR / "wave_normal.png"
    if not wave_normal.exists():
        print("  Generating wave gradient normal...")
        run_render("mask/wave_gradient_shader_mask/prog_1.sksl", "wave_normal.png",
                   uniforms=["iResolution=1280.0,720.0", "waveCenter=640.0,360.0",
                             "waveWidth=0.03", "turbulenceStrength=0.05",
                             "blurRadius=0.02", "propagationRadius=0.4"])

    print("Phase 1 complete.\n")


# ============================================================
# Phase 2: Update params.json with correct asset references
# ============================================================

def update_params():
    """Update all params.json files with semantically correct texture references."""
    print("=" * 60)
    print("Phase 2: Updating params.json configurations")
    print("=" * 60)

    # ---- Per-file texture overrides ----
    # Map: (category/folder/file.sksl) -> {texture_name: asset_path}
    overrides = {}

    # === FILTER: blur_bubbles_rise_filter ===
    overrides["filter/blur_bubbles_rise_filter/gaussian-blur.sksl"] = {
        "image": "assets/input.png"
    }
    overrides["filter/blur_bubbles_rise_filter/resample.sksl"] = {
        "image": "assets/input.png"
    }
    overrides["filter/blur_bubbles_rise_filter/mask-mix.sksl"] = {
        "blur_tex": "assets/blur_level3.png",
        "original_tex": "assets/input.png",
        "blur_mask": "assets/blur_mask.png"
    }

    # === FILTER: edge_light_shader_filter ===
    overrides["filter/edge_light_shader_filter/convert-frag.sksl"] = {
        "image": "assets/input.png"
    }
    overrides["filter/edge_light_shader_filter/detect-frag.sksl"] = {
        "image": "assets/input.png"
    }
    overrides["filter/edge_light_shader_filter/gaussian-frag.sksl"] = {
        "image": "assets/edge_detect.png"
    }
    overrides["filter/edge_light_shader_filter/composite-frag.sksl"] = {
        "imageBlur0": "assets/edge_blur0.png",
        "imageBlur1": "assets/edge_blur1.png",
        "imageBlur2": "assets/edge_blur2.png",
        "imageBlur3": "assets/edge_blur3.png",
        "imageBlur4": "assets/edge_blur4.png"
    }
    overrides["filter/edge_light_shader_filter/alpha-gradient.sksl"] = {
        "image": "assets/input.png",
        "imageBloom": "assets/edge_blur2.png"
    }

    # === FILTER: frosted_glass_shader_filter ===
    overrides["filter/frosted_glass_shader_filter/main-shader-prog.sksl"] = {
        "image": "assets/input.png",
        "edgeBlurredImg": "assets/blur_edge.png",
        "bgBlurredImg": "assets/blur_bg.png",
        "sdfNormalImg": "assets/ripple_normal.png"
    }

    # === FILTER: kawase_blur_shader_filter ===
    overrides["filter/kawase_blur_shader_filter/blur.sksl"] = {
        "imageInput": "assets/input.png"
    }
    overrides["filter/kawase_blur_shader_filter/blur-af.sksl"] = {
        "imageInput": "assets/input.png"
    }
    overrides["filter/kawase_blur_shader_filter/simple.sksl"] = {
        "imageInput": "assets/input.png"
    }
    overrides["filter/kawase_blur_shader_filter/mix.sksl"] = {
        "blurredInput": "assets/blur_level2.png",
        "originalInput": "assets/input.png"
    }

    # === FILTER: mesa_blur_shader_filter ===
    overrides["filter/mesa_blur_shader_filter/blur-mesa.sksl"] = {
        "imageInput": "assets/input.png"
    }
    overrides["filter/mesa_blur_shader_filter/direction-blur-mesa.sksl"] = {
        "imageInput": "assets/input.png"
    }
    overrides["filter/mesa_blur_shader_filter/simple.sksl"] = {
        "imageInput": "assets/input.png"
    }
    overrides["filter/mesa_blur_shader_filter/mix-mesa.sksl"] = {
        "blurredInput": "assets/blur_level2.png"
    }
    overrides["filter/mesa_blur_shader_filter/grey-x.sksl"] = {
        "imageShader": "assets/input.png"
    }

    # === FILTER: variable_radius_blur_shader_filter ===
    overrides["filter/variable_radius_blur_shader_filter/generate-texture.sksl"] = {
        "imageInput": "assets/input.png"
    }
    overrides["filter/variable_radius_blur_shader_filter/horizontal-blur.sksl"] = {
        "imageShader": "assets/input.png",
        "gradientShader": "assets/blur_mask.png"
    }
    overrides["filter/variable_radius_blur_shader_filter/horizontal-blur-masked.sksl"] = {
        "imageShader": "assets/input.png",
        "gradientShader": "assets/blur_mask.png"
    }
    overrides["filter/variable_radius_blur_shader_filter/vertical-blur.sksl"] = {
        "imageShader": "assets/input.png",
        "gradientShader": "assets/blur_mask.png"
    }
    overrides["filter/variable_radius_blur_shader_filter/vertical-blur-masked.sksl"] = {
        "imageShader": "assets/input.png",
        "gradientShader": "assets/blur_mask.png"
    }

    # === FILTER: linear_gradient_blur_shader_filter ===
    overrides["filter/linear_gradient_blur_shader_filter/prog.sksl"] = {
        "srcImageShader": "assets/input.png",
        "blurImageShader": "assets/blur_level2.png",
        "gradientShader": "assets/blur_mask.png"
    }

    # === FILTER: dispersion ===
    overrides["filter/dispersion_shader_filter/dispersion.sksl"] = {
        "image": "assets/input.png",
        "mask": "assets/blur_mask.png"
    }

    # === FILTER: direction_light ===
    overrides["filter/direction_light_shader_filter/direction-light.sksl"] = {
        "image": "assets/input.png",
        "mask": "assets/blur_mask.png"
    }
    overrides["filter/direction_light_shader_filter/direction-light-no-normal.sksl"] = {
        "image": "assets/input.png"
    }
    overrides["filter/direction_light_shader_filter/normal-mask.sksl"] = {
        "mask": "assets/blur_mask.png"
    }

    # === FILTER: displacement_distort ===
    overrides["filter/displacement_distort_shader_filter/displacement-distort.sksl"] = {
        "image": "assets/input.png",
        "maskEffect": "assets/displacement.png"
    }

    # === FILTER: magnifier ===
    overrides["filter/magnifier_shader_filter/magnifier-shader-with-sdf-prog.sksl"] = {
        "imageShader": "assets/input.png",
        "sdfShader": "assets/sdf_rrect.png"
    }

    # === FILTER: mask_transition ===
    overrides["filter/mask_transition_shader_filter/prog.sksl"] = {
        "alphaMask": "assets/blur_mask.png",
        "topLayer": "assets/input.png",
        "bottomLayer": "assets/blur_level3.png"
    }

    # === FILTER: sdf_edge_light ===
    overrides["filter/sdf_edge_light/pass-through.sksl"] = {
        "inputShader": "assets/sdf_rrect.png"
    }
    overrides["filter/sdf_edge_light/shade-code.sksl"] = {
        "image": "assets/input.png",
        "composeImage": "assets/edge_blur2.png"
    }
    overrides["filter/sdf_edge_light/shader.sksl"] = {
        "sdfImageShader": "assets/sdf_rrect.png",
        "blurredSdfImageShader": "assets/blur_level2.png",
        "lightMaskShader": "assets/light_mask.png"
    }

    # === FILTER: sdf_from_image ===
    overrides["filter/sdf_from_image_filter/box-blur-prog.sksl"] = {
        "image": "assets/input.png"
    }
    overrides["filter/sdf_from_image_filter/jfa-iteration-prog.sksl"] = {
        "imageInput": "assets/input.png"
    }
    overrides["filter/sdf_from_image_filter/shader-string.sksl"] = {
        "imageInput": "assets/input.png"
    }
    overrides["filter/sdf_from_image_filter/shader-string_1.sksl"] = {
        "imageInput": "assets/input.png"
    }
    overrides["filter/sdf_from_image_filter/shader-string_2.sksl"] = {
        "imageInput": "assets/input.png",
        "blurredSDFInput": "assets/blur_level2.png"
    }

    # === FILTER: color_gradient ===
    overrides["filter/color_gradient_shader_filter/prog.sksl"] = {
        "srcImageShader": "assets/input.png"
    }

    # === FILTER: others with simple image input ===
    for key in [
        "filter/aibar_shader_filter/prog.sksl",
        "filter/content_light_shader_filter/content-light.sksl",
        "filter/distortion_collapse_filter/shader.sksl",
        "filter/grey_shader_filter/grey-gradation.sksl",
        "filter/heat_distortion_filter/heat-distortion.sksl",
        "filter/motion_blur_shader_filter/motion-blur.sksl",
        "filter/sound_wave_filter/sound-wave.sksl",
        "filter/water_ripple_filter/mini-recv.sksl",
        "filter/water_ripple_filter/smrecv.sksl",
        "filter/water_ripple_filter/smsend.sksl",
        "filter/water_ripple_filter/ssmutual.sksl",
    ]:
        if key not in overrides:
            overrides[key] = {"image": "assets/input.png"}

    # === SHADER: aurora_noise ===
    overrides["shader/aurora_noise_shader/prog_1.sksl"] = {
        "auroraNoiseTexture": "assets/input.png"
    }
    overrides["shader/aurora_noise_shader/prog_2.sksl"] = {
        "verticalBlurTexture": "assets/input.png"
    }

    # === SHADER: circle_flowlight ===
    overrides["shader/circle_flowlight_effect/circle-flowlight-shader-with-mask.sksl"] = {
        "maskImageShader": "assets/blur_mask.png"
    }

    # === SHADER: contour_diagonal_flow_light (use generated images) ===
    overrides["shader/contour_diagonal_flow_light_shader/blend-img-prog.sksl"] = {
        "precalculationImage": "assets/input.png",
        "image1": "assets/blur_edge.png",
        "image2": "assets/blur_level1.png"
    }
    overrides["shader/contour_diagonal_flow_light_shader/convert-img-prog.sksl"] = {
        "sdfImage": "assets/sdf_rrect.png",
        "progressImage": "assets/sdf_rrect.png"
    }
    overrides["shader/contour_diagonal_flow_light_shader/flow-light-prog.sksl"] = {
        "precalculationImage": "assets/sdf_rrect.png"
    }
    overrides["shader/contour_diagonal_flow_light_shader/precalculationformorecurves-prog.sksl"] = {
        "loopImage": "assets/sdf_rrect.png"
    }
    overrides["shader/contour_diagonal_flow_light_shader/sdf-mask-prog.sksl"] = {
        "precalculationImage": "assets/sdf_rrect.png"
    }

    # === SHADER: frosted_glass_effect ===
    overrides["shader/frosted_glass_effect/main-shader-prog.sksl"] = {
        "image": "assets/input.png",
        "edgeBlurredImg": "assets/blur_edge.png",
        "bgBlurredImg": "assets/blur_bg.png",
        "sdfNormalImg": "assets/wave_normal.png"
    }

    # === SHADER: particle_circular_halo ===
    overrides["shader/particle_circular_halo_shader/particle-halo-prog.sksl"] = {
        "singleParticleHalo": "assets/input.png"
    }
    overrides["shader/particle_circular_halo_shader/main-shader-prog.sksl"] = {
        "glowHalo": "assets/blur_level1.png",
        "particleHalo": "assets/blur_level2.png"
    }

    # === SHADER: sdf_edge_light_shader ===
    overrides["shader/sdf_edge_light_shader/shader.sksl"] = {
        "sdfShader": "assets/sdf_rrect.png",
        "lightMaskShader": "assets/light_mask.png"
    }

    # === SHADER: spatial_glass_effect ===
    overrides["shader/spatial_glass_effect/main-shader-prog.sksl"] = {
        "baseBlurImg": "assets/blur_bg.png",
        "sdfNormalImg": "assets/wave_normal.png"
    }

    # === SHADER: spatial_point_light ===
    overrides["shader/spatial_point_light/prog-with-mask.sksl"] = {
        "mask": "assets/light_mask.png"
    }

    # === SHADER: border_sdf ===
    overrides["shader/border_sdf_shader/border-code.sksl"] = {
        "sdfShape": "assets/sdf_rrect.png"
    }

    # === SHADER: color_gradient_effect ===
    overrides["shader/color_gradient_effect/brightness-shader-code.sksl"] = {
        "colorGradientShader": "assets/input.png"
    }
    overrides["shader/color_gradient_effect/color-gradient-shader-with-mask-head.sksl"] = {
        "maskImageShader": "assets/blur_mask.png"
    }

    # === MASK: pixel_map ===
    overrides["mask/pixel_map_shader_mask/prog.sksl"] = {
        "image": "assets/input.png"
    }

    # === MASK: use_effect ===
    overrides["mask/use_effect_shader_mask/prog.sksl"] = {
        "image": "assets/input.png"
    }

    # === SHAPE: border, clip, color, shadow (use SDF shapes) ===
    for key in [
        "shape/sdf_border_shader/code.sksl",
        "shape/sdf_border_shader/outline-code.sksl",
        "shape/sdf_clip_shader/code.sksl",
        "shape/sdf_color_shader/code.sksl",
        "shape/sdf_shadow_shader/code.sksl",
        "shape/sdf_shadow_shader/elevation-code.sksl",
        "shape/sdf_distort_op_shader_shape/shader.sksl",
    ]:
        overrides[key] = {"sdfShape": "assets/sdf_rrect.png"}

    # === SHAPE: transform (use SDF shape child) ===
    for key in [
        "shape/sdf_transform_shader_shape/prog.sksl",
        "shape/sdf_transform_shader_shape/prog_1.sksl",
        "shape/sdf_transform_shader_shape/gravity-pull-prog.sksl",
        "shape/sdf_transform_shader_shape/gravity-pull-normal-prog.sksl",
    ]:
        overrides[key] = {"shapeShader": "assets/sdf_rrect.png"}

    # === SHAPE: union (use SDF shapes as left/right) ===
    for key in [
        "shape/sdf_union_op_shader_shape/prog.sksl",
        "shape/sdf_union_op_shader_shape/prog_1.sksl",
        "shape/sdf_union_op_shader_shape/prog_2.sksl",
    ]:
        overrides[key] = {"left": "assets/sdf_rrect.png", "right": "assets/sdf_triangle.png"}

    # === SHAPE: path ===
    overrides["shape/sdf_path_shader_shape/normal-calculation-shader.sksl"] = {
        "u_seeds": "assets/sdf_rrect.png",
        "pathShader": "assets/sdf_rrect.png"
    }
    overrides["shape/sdf_path_shader_shape/precalculation-for-sdf-shader.sksl"] = {
        "u_prevD": "assets/sdf_rrect.png"
    }
    overrides["shape/sdf_path_shader_shape/sdf-propagation-shader.sksl"] = {
        "u_sdfTex": "assets/sdf_rrect.png",
        "u_maskTex": "assets/blur_mask.png"
    }

    # === SHAPE: pixelmap ===
    overrides["shape/sdf_pixelmap_shader_shape/prog.sksl"] = {
        "pixelmapShader": "assets/input.png"
    }
    overrides["shape/sdf_pixelmap_shader_shape/prog_1.sksl"] = {
        "pixelmapShader": "assets/input.png"
    }

    # Apply overrides
    count = 0
    for rel_path, textures in overrides.items():
        params_path = SHADERS_DIR / rel_path
        params_path = params_path.with_suffix(".params.json")
        if not params_path.exists():
            print(f"  WARNING: params file not found: {params_path}")
            continue

        cfg = json.loads(params_path.read_text())
        if "textures" not in cfg:
            cfg["textures"] = {}

        for name, val in textures.items():
            cfg["textures"][name] = val

        params_path.write_text(json.dumps(cfg, indent=2, ensure_ascii=False))
        count += 1

    print(f"  Updated {count} params files")
    print("Phase 2 complete.\n")


# ============================================================
# Phase 3: Re-render all SKSL files
# ============================================================

def resolve_asset(rel_path):
    """Resolve an asset path relative to the tests directory."""
    tests_dir = SHADERS_DIR.parent
    candidates = [
        tests_dir / rel_path,
        Path(rel_path),
    ]
    for c in candidates:
        if c.exists():
            return c
    # Try in assets dir with just the filename
    name = Path(rel_path).name
    asset_candidate = ASSETS_DIR / name
    if asset_candidate.exists():
        return asset_candidate
    return None

def has_subshader_children(params):
    textures = params.get("textures", {})
    for val in textures.values():
        if isinstance(val, dict) and val.get("childShader"):
            return True
    return False

def get_texture_args(params):
    args = []
    for name, val in params.get("textures", {}).items():
        if isinstance(val, str):
            full_path = resolve_asset(val)
            if full_path:
                args.extend(["--texture", f"{name}={full_path}"])
            else:
                # Don't warn for commonly-missing sub-shader references
                pass
        elif isinstance(val, dict):
            if val.get("raw"):
                path = val.get("path", "")
                full_path = resolve_asset(path)
                if full_path:
                    args.extend(["--texture", f"{name}={full_path}"])
                    args.extend(["--raw", name])
    return args

def get_uniform_args(params):
    args = []
    for name, val in params.get("uniforms", {}).items():
        if isinstance(val, list):
            # Check if this is an array of vectors (list of lists)
            if val and isinstance(val[0], list):
                # Array of vectors: color[0]=r,g,b,a, color[1]=r,g,b,a, ...
                for i, elem in enumerate(val):
                    str_val = ",".join(str(v) for v in elem)
                    args.extend(["--uniform", f"{name}[{i}]={str_val}"])
            else:
                # Flat list: either array of scalars or single vector
                str_val = ",".join(str(v) for v in val)
                args.extend(["--uniform", f"{name}={str_val}"])
        else:
            str_val = str(val)
            args.extend(["--uniform", f"{name}={str_val}"])
    return args

def render_all():
    print("=" * 60)
    print("Phase 3: Re-rendering all SKSL files")
    print("=" * 60)

    results = {"rendered": [], "skipped": [], "failed": []}
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)

    for category in ['filter', 'mask', 'shader', 'shape']:
        cat_dir = SHADERS_DIR / category
        if not cat_dir.exists():
            continue
        for folder in sorted(cat_dir.iterdir()):
            if not folder.is_dir():
                continue
            for sksl_file in sorted(folder.glob("*.sksl")):
                params_file = sksl_file.with_suffix(".params.json")
                if not params_file.exists():
                    results["skipped"].append(str(sksl_file.relative_to(SHADERS_DIR)))
                    continue

                cfg = json.loads(params_file.read_text())

                if has_subshader_children(cfg):
                    results["skipped"].append(str(sksl_file.relative_to(SHADERS_DIR)))
                    continue

                rel = sksl_file.relative_to(SHADERS_DIR)
                out_dir = RESULTS_DIR / rel.parent
                out_dir.mkdir(parents=True, exist_ok=True)
                out_file = out_dir / f"{sksl_file.stem}.png"

                dims = cfg.get("dimensions", {"width": DEFAULT_W, "height": DEFAULT_H})
                cmd = [
                    sys.executable, str(RENDER_SCRIPT),
                    "--sksl", str(sksl_file),
                    "--output", str(out_file),
                    "--width", str(dims["width"]),
                    "--height", str(dims["height"]),
                ]
                cmd.extend(get_texture_args(cfg))
                cmd.extend(get_uniform_args(cfg))

                try:
                    r = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
                    if r.returncode == 0:
                        results["rendered"].append(str(rel))
                    else:
                        err = r.stderr.strip()[:120]
                        print(f"  FAIL: {rel} — {err}")
                        results["failed"].append({"file": str(rel), "error": err})
                except subprocess.TimeoutExpired:
                    print(f"  TIMEOUT: {rel}")
                    results["failed"].append({"file": str(rel), "error": "timeout"})
                except Exception as e:
                    print(f"  ERROR: {rel} — {e}")
                    results["failed"].append({"file": str(rel), "error": str(e)})

    # Summary
    print(f"\nRendered: {len(results['rendered'])}")
    print(f"Skipped (sub-shader): {len(results['skipped'])}")
    print(f"Failed: {len(results['failed'])}")
    if results['failed']:
        print("Failures:")
        for f in results['failed']:
            print(f"  {f['file']}: {f['error'][:100]}")

    report_path = RESULTS_DIR / "render_report.json"
    with open(report_path, 'w') as f:
        json.dump(results, f, indent=2)
    print(f"Report: {report_path}")


if __name__ == "__main__":
    generate_missing_assets()
    update_params()
    render_all()
