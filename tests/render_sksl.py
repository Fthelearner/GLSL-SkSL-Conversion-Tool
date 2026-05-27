#!/usr/bin/env python3
"""Render an arbitrary SKSL file to PNG using skia-python RuntimeEffect."""

import argparse
import json
import sys
from pathlib import Path

SRC_DIR = Path(__file__).resolve().parent.parent / "src" / "renderer"
sys.path.insert(0, str(SRC_DIR))

from image_loader import require_skia, load_image  # noqa: E402
from shader_runner import ShaderRenderRequest, render_to_png  # noqa: E402


def main():
    parser = argparse.ArgumentParser(
        description="Render a SKSL file to PNG via skia-python RuntimeEffect."
    )
    parser.add_argument("--sksl", required=True, help="Path to .sksl file")
    parser.add_argument("--output", required=True, help="Output PNG path")
    parser.add_argument("--texture", action="append", default=[],
                        help="Child texture: name=path (repeatable)")
    parser.add_argument("--uniform", action="append", default=[],
                        help="Uniform: name=value (repeatable, comma-sep for vec)")
    parser.add_argument("--raw", action="append", default=[],
                        help="Child names to use makeRawShader for (repeatable)")
    parser.add_argument("--child-sksl", action="append", default=[],
                        help="Child SkSL shader: name=path.sksl (repeatable)")
    parser.add_argument("--params", default=None,
                        help="Path to .params.json file (auto-loads textures and uniforms)")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    args = parser.parse_args()

    # Auto-load from .params.json if specified
    if args.params:
        import json as _json
        params_path = Path(args.params)
        if not params_path.exists():
            print(f"ERROR: params file not found: {args.params}", file=sys.stderr)
            return 1
        with open(params_path) as f:
            cfg = _json.load(f)
        # Apply dimensions from params if not overridden by CLI defaults
        dims = cfg.get("dimensions", {})
        if dims.get("width") and args.width == 1280:
            args.width = dims["width"]
        if dims.get("height") and args.height == 720:
            args.height = dims["height"]
        # Load textures (resolve paths relative to tests/ dir)
        tests_dir = params_path.parent
        while tests_dir.name != 'tests' and tests_dir != tests_dir.parent:
            tests_dir = tests_dir.parent
        assets_dir = tests_dir / "assets"
        def _resolve_tex(path_str):
            for base in [params_path.parent, tests_dir, assets_dir]:
                cand = base / path_str
                if cand.exists():
                    return str(cand)
            cand = assets_dir / Path(path_str).name
            if cand.exists():
                return str(cand)
            return path_str  # fallback, will error later
        for name, val in cfg.get("textures", {}).items():
            if isinstance(val, str):
                args.texture.append(f"{name}={_resolve_tex(val)}")
            elif isinstance(val, dict):
                if val.get("raw"):
                    path = val.get("path", "")
                    args.texture.append(f"{name}={_resolve_tex(path)}")
                    args.raw.append(name)
        # Load uniforms
        for name, val in cfg.get("uniforms", {}).items():
            if isinstance(val, list):
                args.uniform.append(f"{name}={','.join(str(v) for v in val)}")
            else:
                args.uniform.append(f"{name}={val}")

    # Auto-defaults for common Shadertoy uniforms when not explicitly set
    if "iResolution" not in [u.partition("=")[0] for u in args.uniform]:
        args.uniform.append(f"iResolution={args.width},{args.height},1")
    if "iTime" not in [u.partition("=")[0] for u in args.uniform]:
        args.uniform.append("iTime=1.5")

    # Build child_images dict, and auto-set iChannelResolution from texture sizes
    child_images = {}
    for t in args.texture:
        if "=" not in t:
            print(f"ERROR: --texture must be name=path, got '{t}'", file=sys.stderr)
            return 1
        name, _, path = t.partition("=")
        img = load_image(Path(path))
        if img is None:
            print(f"ERROR: cannot load texture '{path}' for '{name}'", file=sys.stderr)
            return 1
        child_images[name] = img
        # Auto-set iChannelResolution[N] from this texture's dimensions
        if name.startswith("iChannel") and name[8:].isdigit():
            import re as _re
            w, h = img.width(), img.height()
            idx = name[8:]
            if f"iChannelResolution[{idx}]" not in [u.partition("=")[0] for u in args.uniform]:
                args.uniform.append(f"iChannelResolution[{idx}]={w},{h},1")

    # Build uniforms dict
    uniforms = {}
    for u in args.uniform:
        if "=" not in u:
            print(f"ERROR: --uniform must be name=value, got '{u}'", file=sys.stderr)
            return 1
        name, _, val = u.partition("=")
        if "," in val:
            vals = [float(v) for v in val.split(",")]
            # Pad iResolution to 3 components if the shader declares float3
            if name == "iResolution":
                sksl_src = Path(args.sksl).read_text(encoding='utf-8')
                if 'float3 iResolution' in sksl_src or 'half3 iResolution' in sksl_src:
                    while len(vals) < 3:
                        vals.append(1.0)
            uniforms[name] = vals
        else:
            uniforms[name] = float(val)

    raw_child_names = frozenset(args.raw)

    # Build child_sksl dict from --child-sksl name=path.sksl
    child_sksl = {}
    for c in args.child_sksl:
        if "=" not in c:
            print(f"ERROR: --child-sksl must be name=path.sksl, got '{c}'", file=sys.stderr)
            return 1
        cname, _, cpath = c.partition("=")
        csrc = Path(cpath).read_text(encoding="utf-8")
        # Check for an optional .params.json alongside the child sksl
        child_params = Path(cpath).with_suffix(".params.json")
        child_uniforms = {}
        if child_params.exists():
            import json as _json
            with open(child_params) as f:
                cfg = _json.load(f)
            for k, v in cfg.get("uniforms", {}).items():
                child_uniforms[k] = v if isinstance(v, list) else float(v)
        child_sksl[cname] = (csrc, child_uniforms)

    request = ShaderRenderRequest(
        shader_path=Path(args.sksl),
        output_path=Path(args.output),
        size=(args.width, args.height),
        child_images=child_images,
        uniforms=uniforms,
        raw_child_names=raw_child_names,
        child_sksl=child_sksl if child_sksl else None,
    )

    try:
        render_to_png(request)
        print(f"Rendered -> {args.output}")
    except Exception as e:
        print(f"ERROR: render failed — {e}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
