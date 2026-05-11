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
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    args = parser.parse_args()

    # Build child_images dict
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

    # Build uniforms dict
    uniforms = {}
    for u in args.uniform:
        if "=" not in u:
            print(f"ERROR: --uniform must be name=value, got '{u}'", file=sys.stderr)
            return 1
        name, _, val = u.partition("=")
        if "," in val:
            uniforms[name] = [float(v) for v in val.split(",")]
        else:
            uniforms[name] = float(val)

    raw_child_names = frozenset(args.raw)

    request = ShaderRenderRequest(
        shader_path=Path(args.sksl),
        output_path=Path(args.output),
        size=(args.width, args.height),
        child_images=child_images,
        uniforms=uniforms,
        raw_child_names=raw_child_names,
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
