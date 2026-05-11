#!/usr/bin/env python3
"""
Real-time shader preview window via ffplay.
Renders frames continuously and pipes raw RGB24 to ffplay for live display.

Usage (SKSL):
  python3 shader_preview.py --sksl my_shader.sksl [--fps 30] [--duration 10]
  python3 shader_preview.py --sksl my.sksl --texture image=img.png --uniform strength=1.5

Usage (GLSL):
  python3 shader_preview.py --glsl my_shader.glsl [--fps 30] [--duration 10]

The script outputs raw RGB24 frames to stdout. Pipe to ffplay:
  python3 shader_preview.py --sksl my.sksl | ffplay -f rawvideo -pixel_format rgb24 \\
      -video_size 1280x720 -framerate 30 -i -

Or just run the script — it auto-launches ffplay if stdout is a terminal.
"""

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

SRC_DIR = Path(__file__).resolve().parent.parent / "src" / "renderer"
sys.path.insert(0, str(SRC_DIR))


def parse_args():
    p = argparse.ArgumentParser(description="Real-time shader preview window")
    src = p.add_mutually_exclusive_group(required=True)
    src.add_argument("--sksl", help="Path to SKSL shader file")
    src.add_argument("--glsl", help="Path to GLSL shader file")
    p.add_argument("--fps", type=int, default=30)
    p.add_argument("--duration", type=float, default=10.0,
                   help="Animation duration in seconds (default: 10, 0 = single frame)")
    p.add_argument("--time-uniform", default="iTime",
                   help="Name of time uniform (default: iTime)")
    p.add_argument("--width", type=int, default=1280)
    p.add_argument("--height", type=int, default=720)
    p.add_argument("--texture", action="append", default=[],
                   help="Child texture: name=path (repeatable)")
    p.add_argument("--uniform", action="append", default=[],
                   help="Static uniform: name=value (repeatable, comma-sep for vec)")
    p.add_argument("--raw", action="append", default=[],
                   help="Child names to use nearest-neighbor for (repeatable)")
    p.add_argument("--no-ffplay", action="store_true",
                   help="Don't auto-launch ffplay (raw rgb24 goes to stdout)")
    p.add_argument("--fullscreen", action="store_true",
                   help="Launch ffplay in fullscreen mode")
    return p.parse_args()


def build_glsl_preview_cmd(args):
    """Render GLSL frames via subprocess to render_glsl, output raw RGB24."""
    render_glsl = os.path.join(
        Path(__file__).resolve().parent.parent, "glslang", "glslang_demo", "render_glsl"
    )
    return [render_glsl, args.glsl, "-", str(args.width), str(args.height)]


def render_sksl_frame(skia, effect, child_images, raw_names, base_uniforms,
                      time_uniform, time_val, width, height):
    """Render a single SKSL frame, return raw RGB24 bytes."""
    uniforms = dict(base_uniforms)
    uniforms[time_uniform] = time_val

    builder = skia.RuntimeShaderBuilder(effect)
    for name, value in uniforms.items():
        if isinstance(value, list):
            builder.setUniform(name, value)
        else:
            builder.setUniform(name, value)

    for child_name, image in child_images.items():
        if child_name in raw_names:
            child_shader = image.makeRawShader()
        else:
            child_shader = image.makeShader()
        builder.setChild(child_name, child_shader)

    shader = builder.makeShader()
    surface = skia.Surface(width, height)
    canvas = surface.getCanvas()
    paint = skia.Paint(Shader=shader, AntiAlias=True)
    canvas.drawRect(skia.Rect.MakeWH(width, height), paint)

    image = surface.makeImageSnapshot()
    pixmap = skia.Pixmap()
    if not image.peekPixels(pixmap):
        raise RuntimeError("Failed to read pixels from surface")

    rgba = bytes(pixmap)
    # Convert RGBA → RGB24 (drop alpha)
    rgb24 = bytearray(width * height * 3)
    for i in range(width * height):
        src = i * 4
        dst = i * 3
        rgb24[dst] = rgba[src]       # R
        rgb24[dst + 1] = rgba[src + 1]  # G
        rgb24[dst + 2] = rgba[src + 2]  # B
    return bytes(rgb24)


def render_glsl_frame(proc, time_val, time_uniform, extra_uniforms, width, height):
    """Render a single GLSL frame via an already-running render_glsl subprocess.
    Returns raw RGB24 bytes (render_glsl needs raw output support for this).
    """
    # For GLSL, we spawn render_glsl per frame since it doesn't support stdin control
    render_glsl = os.path.join(
        Path(__file__).resolve().parent.parent, "glslang", "glslang_demo", "render_glsl"
    )
    cmd = [render_glsl, args.glsl, "-", str(width), str(height),
           "--uniform", time_uniform, str(time_val)]
    for name, val in extra_uniforms:
        cmd.extend(["--uniform", name, str(val)])

    result = subprocess.run(cmd, capture_output=True, timeout=30)
    if result.returncode != 0:
        stderr = result.stderr.decode(errors="replace")
        raise RuntimeError(f"render_glsl failed: {stderr.strip().split(chr(10))[-1]}")

    # render_glsl outputs PPM to stdout when output path is "-"
    ppm = result.stdout
    # Parse PPM P6 header, extract raw RGB data
    idx = ppm.find(b"\n255\n")
    if idx < 0:
        raise RuntimeError("Invalid PPM output from render_glsl")
    return ppm[idx + 5:]  # skip "\n255\n"


def launch_ffplay(width, height, fps, fullscreen=False):
    """Launch ffplay as a subprocess, return the Popen object.
    We'll write raw RGB24 frames to its stdin."""
    cmd = [
        "ffplay",
        "-f", "rawvideo",
        "-pixel_format", "rgb24",
        "-video_size", f"{width}x{height}",
        "-framerate", str(fps),
        "-window_title", "Shader Preview",
        "-infbuf",           # no frame buffering (low latency)
        "-fast",             # non-blocking input
    ]
    if fullscreen:
        cmd.append("-fs")
    cmd.extend(["-i", "pipe:0"])

    return subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def main():
    args = parse_args()

    width, height = args.width, args.height
    total_frames = int(args.duration * args.fps) if args.duration > 0 else 1
    is_animated = total_frames > 1

    # Parse extra uniforms (static values, not time)
    base_uniforms = {}
    for u in args.uniform:
        if "=" not in u:
            print(f"ERROR: --uniform must be name=value, got '{u}'", file=sys.stderr)
            return 1
        name, _, val = u.partition("=")
        if "," in val:
            base_uniforms[name] = [float(v) for v in val.split(",")]
        else:
            base_uniforms[name] = float(val)

    # Resolve iResolution if not explicitly provided
    if "iResolution" not in base_uniforms:
        base_uniforms["iResolution"] = [float(width), float(height)]
    if "iTime" not in base_uniforms:
        base_uniforms["iTime"] = 0.0

    # Launch ffplay (unless --no-ffplay, then raw rgb24 goes to stdout)
    ffplay_proc = None
    out_fd = sys.stdout.buffer
    if not args.no_ffplay and sys.stdout.isatty():
        ffplay_proc = launch_ffplay(width, height, args.fps, args.fullscreen)
        out_fd = ffplay_proc.stdin
        print(f"Shader Preview: {width}x{height} @ {args.fps}fps, {args.duration}s",
              file=sys.stderr)
        if is_animated:
            print(f"  Rendering {total_frames} frames...", file=sys.stderr)
        else:
            print("  Single frame mode", file=sys.stderr)

    frame_size = width * height * 3

    try:
        if args.sksl:
            # ── SKSL path ──────────────────────────────────────
            from image_loader import require_skia, load_image

            skia = require_skia()
            shader_src = Path(args.sksl).read_text(encoding="utf-8")
            effect = skia.RuntimeEffect.MakeForShader(shader_src)
            if effect is None:
                print(f"ERROR: Failed to compile shader: {args.sksl}", file=sys.stderr)
                return 1

            child_images = {}
            for t in args.texture:
                if "=" not in t:
                    print(f"ERROR: --texture must be name=path, got '{t}'",
                          file=sys.stderr)
                    return 1
                name, _, path = t.partition("=")
                img = load_image(Path(path))
                if img is None:
                    print(f"ERROR: cannot load texture '{path}'", file=sys.stderr)
                    return 1
                child_images[name] = img

            raw_names = frozenset(args.raw)

            for frame in range(total_frames):
                if total_frames == 1:
                    t = base_uniforms.get(args.time_uniform, 0.0)
                else:
                    t = frame / args.fps
                rgb24 = render_sksl_frame(
                    skia, effect, child_images, raw_names, base_uniforms,
                    args.time_uniform, t, width, height
                )
                out_fd.write(rgb24)
                out_fd.flush()

                if is_animated and frame % max(1, args.fps) == 0:
                    pct = 100 * (frame + 1) / total_frames
                    print(f"\r  {pct:.0f}%", end="", file=sys.stderr, flush=True)

        elif args.glsl:
            # ── GLSL path ──────────────────────────────────────
            render_glsl_bin = os.path.join(
                Path(__file__).resolve().parent.parent, "glslang",
                "glslang_demo", "render_glsl"
            )
            if not os.path.isfile(render_glsl_bin):
                print(f"ERROR: render_glsl not found at {render_glsl_bin}",
                      file=sys.stderr)
                return 1

            extra = [(k, v if not isinstance(v, list) else v[0])
                     for k, v in base_uniforms.items() if k != args.time_uniform]

            for frame in range(total_frames):
                t = 0.0 if total_frames == 1 else frame / args.fps
                cmd = [render_glsl_bin, args.glsl, "-",
                       str(width), str(height),
                       "--uniform", args.time_uniform, str(t)]
                for name, val in extra:
                    if isinstance(val, list):
                        cmd.extend(["--uniform", name] + [str(x) for x in val])
                    else:
                        cmd.extend(["--uniform", name, str(val)])

                result = subprocess.run(cmd, capture_output=True, timeout=30)
                if result.returncode != 0:
                    err = result.stderr.decode(errors="replace").strip()
                    raise RuntimeError(f"render_glsl frame {frame}: {err.split(chr(10))[-1]}")

                ppm = result.stdout
                idx = ppm.find(b"\n255\n")
                if idx < 0:
                    raise RuntimeError("Invalid PPM output")
                rgb24 = ppm[idx + 5:]
                out_fd.write(rgb24)
                out_fd.flush()

                if is_animated and frame % max(1, args.fps) == 0:
                    pct = 100 * (frame + 1) / total_frames
                    print(f"\r  {pct:.0f}%", end="", file=sys.stderr, flush=True)

        if is_animated:
            print(file=sys.stderr)

    except KeyboardInterrupt:
        pass
    except BrokenPipeError:
        pass  # ffplay closed
    finally:
        if ffplay_proc:
            try:
                ffplay_proc.stdin.close()
                ffplay_proc.wait(timeout=3)
            except Exception:
                ffplay_proc.kill()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
