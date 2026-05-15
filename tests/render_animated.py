#!/usr/bin/env python3
"""Multi-frame SKSL renderer. Renders an animated sequence by varying a time uniform."""

import argparse
import sys
from pathlib import Path

SRC_DIR = Path(__file__).resolve().parent.parent / "src" / "renderer"
sys.path.insert(0, str(SRC_DIR))

from image_loader import require_skia, load_image  # noqa: E402
from shader_runner import ShaderRenderRequest  # noqa: E402


def main():
    parser = argparse.ArgumentParser(
        description="Render animated SKSL frames to PNG via skia-python RuntimeEffect."
    )
    parser.add_argument("--sksl", required=True, help="Path to .sksl file")
    parser.add_argument("--output-dir", required=True, help="Output directory for frame PNGs")
    parser.add_argument("--time-start", type=float, default=0.0, help="Start time (default: 0.0)")
    parser.add_argument("--time-end", type=float, default=5.0, help="End time (default: 5.0)")
    parser.add_argument("--fps", type=int, default=30, help="Frames per second (default: 30)")
    parser.add_argument("--time-uniform", default="iTime", help="Name of time uniform (default: iTime)")
    parser.add_argument("--texture", action="append", default=[],
                        help="Child texture: name=path (repeatable)")
    parser.add_argument("--uniform", action="append", default=[],
                        help="Uniform: name=value (repeatable, comma-sep for vec)")
    parser.add_argument("--raw", action="append", default=[],
                        help="Child names to use makeRawShader for (repeatable)")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--single-frame", type=int, default=None,
                        help="Render only a specific frame number (1-based)")
    args = parser.parse_args()

    # Auto-defaults for common Shadertoy uniforms
    if "iResolution" not in [u.partition("=")[0] for u in args.uniform]:
        args.uniform.append(f"iResolution={args.width},{args.height},1")
    if "iTime" not in [u.partition("=")[0] for u in args.uniform]:
        args.uniform.append("iTime=1.5")

    skia = require_skia()

    # Parse child images
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

    # Parse static uniforms (all except time)
    base_uniforms = {}
    for u in args.uniform:
        if "=" not in u:
            print(f"ERROR: --uniform must be name=value, got '{u}'", file=sys.stderr)
            return 1
        name, _, val = u.partition("=")
        if "," in val:
            vals = [float(v) for v in val.split(",")]
            if name == "iResolution":
                sksl_src = Path(args.sksl).read_text(encoding='utf-8')
                if 'float3 iResolution' in sksl_src or 'half3 iResolution' in sksl_src:
                    while len(vals) < 3:
                        vals.append(1.0)
            base_uniforms[name] = vals
        else:
            base_uniforms[name] = float(val)

    raw_child_names = frozenset(args.raw)

    # Compile shader once
    shader_source = Path(args.sksl).read_text(encoding="utf-8")
    effect = skia.RuntimeEffect.MakeForShader(shader_source)
    if effect is None:
        print(f"ERROR: Failed to compile shader: {args.sksl}", file=sys.stderr)
        return 1

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    total_frames = int((args.time_end - args.time_start) * args.fps)
    if total_frames <= 0:
        print("ERROR: Invalid time range or fps", file=sys.stderr)
        return 1

    frame_range = range(1, total_frames + 1)
    if args.single_frame is not None:
        if args.single_frame < 1 or args.single_frame > total_frames:
            print(f"ERROR: single-frame {args.single_frame} out of range [1, {total_frames}]",
                  file=sys.stderr)
            return 1
        frame_range = [args.single_frame]
        print(f"Rendering single frame {args.single_frame}/{total_frames}")

    for frame in frame_range:
        time_val = args.time_start + (frame - 1) / args.fps

        uniforms = dict(base_uniforms)
        uniforms[args.time_uniform] = time_val

        builder = skia.RuntimeShaderBuilder(effect)
        for name, value in uniforms.items():
            if isinstance(value, list):
                builder.setUniform(name, value)
            else:
                builder.setUniform(name, value)

        for child_name, image in child_images.items():
            if child_name in raw_child_names:
                child_shader = image.makeRawShader()
            else:
                child_shader = image.makeShader()
            builder.setChild(child_name, child_shader)

        shader = builder.makeShader()
        surface = skia.Surface(args.width, args.height)
        canvas = surface.getCanvas()
        paint = skia.Paint(Shader=shader, AntiAlias=True)
        canvas.drawRect(skia.Rect.MakeWH(args.width, args.height), paint)

        image = surface.makeImageSnapshot()
        encoded = image.encodeToData()
        out_path = output_dir / f"frame_{frame:04d}.png"
        out_path.write_bytes(bytes(encoded))

        if args.single_frame is None:
            pct = 100 * frame / total_frames
            print(f"Frame {frame}/{total_frames} ({pct:.0f}%) t={time_val:.3f} -> {out_path}",
                  flush=True)

    # Write sentinel file to signal completion
    (output_dir / "render_done.marker").touch()
    print(f"Done. {len(frame_range)} frame(s) written to {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
