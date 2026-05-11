#!/usr/bin/env python3
"""
Real-time shader preview GUI window.

SKSL: Uses skia-python RuntimeEffect to render each frame,
      displays in an SDL2 window at real-time FPS.
GLSL: Spawns render_glsl per-frame (with wall-clock iTime),
      displays in an SDL2 window at real-time FPS.

Close the window or press Esc to exit.
"""

import argparse
import ctypes
import os
import subprocess
import sys
import time
from pathlib import Path

_HERE = Path(__file__).resolve().parent
_PROJECT_ROOT = _HERE.parent.parent
sys.path.insert(0, str(_HERE))

# ── SDL2 ctypes bindings ─────────────────────────────────────────────

SDL_INIT_VIDEO = 0x00000020
SDL_WINDOW_SHOWN = 0x00000004
SDL_PIXELFORMAT_RGB24 = 0x17101803  # SDL_PIXELFORMAT_RGB24 (3 bytes R,G,B)
SDL_TEXTUREACCESS_STREAMING = 1
SDL_QUIT = 0x100
SDL_KEYDOWN = 0x300
SDLK_ESCAPE = 41

_sdl = None


def _load_sdl():
    global _sdl
    if _sdl:
        return _sdl
    lib = ctypes.CDLL("libSDL2-2.0.so.0")
    # SDL_Init
    lib.SDL_Init.argtypes = [ctypes.c_uint32]
    lib.SDL_Init.restype = ctypes.c_int
    # SDL_Quit
    lib.SDL_Quit.argtypes = []
    # SDL_CreateWindow
    lib.SDL_CreateWindow.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int,
                                      ctypes.c_int, ctypes.c_int, ctypes.c_uint32]
    lib.SDL_CreateWindow.restype = ctypes.c_void_p
    # SDL_DestroyWindow
    lib.SDL_DestroyWindow.argtypes = [ctypes.c_void_p]
    # SDL_CreateRenderer
    lib.SDL_CreateRenderer.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_uint32]
    lib.SDL_CreateRenderer.restype = ctypes.c_void_p
    # SDL_DestroyRenderer
    lib.SDL_DestroyRenderer.argtypes = [ctypes.c_void_p]
    # SDL_CreateTexture
    lib.SDL_CreateTexture.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int,
                                       ctypes.c_int, ctypes.c_int]
    lib.SDL_CreateTexture.restype = ctypes.c_void_p
    # SDL_DestroyTexture
    lib.SDL_DestroyTexture.argtypes = [ctypes.c_void_p]
    # SDL_UpdateTexture
    lib.SDL_UpdateTexture.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                       ctypes.c_void_p, ctypes.c_int]
    lib.SDL_UpdateTexture.restype = ctypes.c_int
    # SDL_RenderClear
    lib.SDL_RenderClear.argtypes = [ctypes.c_void_p]
    # SDL_RenderCopy
    lib.SDL_RenderCopy.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                    ctypes.c_void_p, ctypes.c_void_p]
    # SDL_RenderPresent
    lib.SDL_RenderPresent.argtypes = [ctypes.c_void_p]
    # SDL_PollEvent
    lib.SDL_PollEvent.argtypes = [ctypes.c_void_p]
    lib.SDL_PollEvent.restype = ctypes.c_int
    # SDL_GetTicks
    lib.SDL_GetTicks.argtypes = []
    lib.SDL_GetTicks.restype = ctypes.c_uint32
    # SDL_Delay
    lib.SDL_Delay.argtypes = [ctypes.c_uint32]
    # SDL_GetError
    lib.SDL_GetError.argtypes = []
    lib.SDL_GetError.restype = ctypes.c_char_p
    # SDL_SetWindowTitle
    lib.SDL_SetWindowTitle.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    _sdl = lib
    return lib


class SDL2Window:
    def __init__(self, width, height, title="Shader Live Preview"):
        sdl = _load_sdl()
        if sdl.SDL_Init(SDL_INIT_VIDEO) != 0:
            raise RuntimeError(f"SDL_Init failed: {sdl.SDL_GetError()}")

        self.win = sdl.SDL_CreateWindow(
            title.encode(), 100, 100, width, height, SDL_WINDOW_SHOWN
        )
        if not self.win:
            raise RuntimeError("SDL_CreateWindow failed")

        self.renderer = sdl.SDL_CreateRenderer(self.win, -1, 0)
        if not self.renderer:
            raise RuntimeError("SDL_CreateRenderer failed")

        self.texture = sdl.SDL_CreateTexture(
            self.renderer, SDL_PIXELFORMAT_RGB24,
            SDL_TEXTUREACCESS_STREAMING, width, height
        )
        if not self.texture:
            raise RuntimeError("SDL_CreateTexture failed")

        self.width = width
        self.height = height
        self.running = True

    def update_frame(self, rgb24_data):
        """Upload RGB24 pixel data to the SDL texture and present."""
        sdl = _load_sdl()
        sdl.SDL_UpdateTexture(self.texture, None, rgb24_data, self.width * 3)
        sdl.SDL_RenderClear(self.renderer)
        sdl.SDL_RenderCopy(self.renderer, self.texture, None, None)
        sdl.SDL_RenderPresent(self.renderer)

    def poll_events(self):
        sdl = _load_sdl()
        event = (ctypes.c_uint8 * 56)()  # SDL_Event is ~56 bytes
        while sdl.SDL_PollEvent(event):
            etype = ctypes.c_uint32.from_buffer(event, 0).value
            if etype == SDL_QUIT:
                self.running = False
            elif etype == SDL_KEYDOWN:
                keysym = ctypes.c_uint32.from_buffer(event, 16).value
                if keysym == SDLK_ESCAPE:
                    self.running = False

    def close(self):
        sdl = _load_sdl()
        if self.texture:
            sdl.SDL_DestroyTexture(self.texture)
        if self.renderer:
            sdl.SDL_DestroyRenderer(self.renderer)
        if self.win:
            sdl.SDL_DestroyWindow(self.win)
        sdl.SDL_Quit()


# ── Shader rendering ─────────────────────────────────────────────────

def create_sksl_renderer(args):
    """Set up skia RuntimeEffect and return a callable render(time_val) -> rgb24 bytes."""
    from image_loader import require_skia, load_image

    skia = require_skia()
    shader_src = Path(args.sksl).read_text(encoding="utf-8")
    effect = skia.RuntimeEffect.MakeForShader(shader_src)
    if effect is None:
        raise RuntimeError(f"Failed to compile SKSL: {args.sksl}")

    # Parse uniforms
    base_uniforms = {}
    for u in args.uniform:
        if "=" not in u:
            raise RuntimeError(f"--uniform must be name=value, got '{u}'")
        name, _, val = u.partition("=")
        if "," in val:
            base_uniforms[name] = [float(v) for v in val.split(",")]
        else:
            base_uniforms[name] = float(val)

    if "iResolution" not in base_uniforms:
        # Detect whether shader declares float2 or float3 iResolution
        is_float3 = "float3 iResolution" in shader_src or "vec3 iResolution" in shader_src
        if is_float3:
            base_uniforms["iResolution"] = [float(args.width), float(args.height), 1.0]
        else:
            base_uniforms["iResolution"] = [float(args.width), float(args.height)]

    # Load child textures
    child_images = {}
    for t in args.texture:
        if "=" not in t:
            raise RuntimeError(f"--texture must be name=path, got '{t}'")
        name, _, path = t.partition("=")
        img = load_image(Path(path))
        if img is None:
            raise RuntimeError(f"Cannot load texture: {path}")
        child_images[name] = img
    raw_names = frozenset(args.raw)

    def render(time_val):
        uniforms = dict(base_uniforms)
        uniforms[args.time_uniform] = time_val

        builder = skia.RuntimeShaderBuilder(effect)
        for name, value in uniforms.items():
            if isinstance(value, list):
                builder.setUniform(name, value)
            else:
                builder.setUniform(name, value)
        for child_name, image in child_images.items():
            if child_name in raw_names:
                builder.setChild(child_name, image.makeRawShader())
            else:
                builder.setChild(child_name, image.makeShader())

        shader = builder.makeShader()
        surface = skia.Surface(args.width, args.height)
        canvas = surface.getCanvas()
        paint = skia.Paint(Shader=shader, AntiAlias=True)
        canvas.drawRect(skia.Rect.MakeWH(args.width, args.height), paint)
        snap = surface.makeImageSnapshot()
        # Use readPixels for consistent RGBA byte order (peekPixels may be BGRA)
        info = skia.ImageInfo.Make(
            args.width, args.height,
            skia.ColorType.kRGBA_8888_ColorType,
            skia.AlphaType.kUnpremul_AlphaType
        )
        rgba = bytearray(args.width * args.height * 4)
        if not snap.readPixels(info, rgba, args.width * 4, 0, 0):
            raise RuntimeError("Failed to read pixels")
        # Convert RGBA→RGB24
        rgb24 = bytearray(args.width * args.height * 3)
        for i in range(args.width * args.height):
            s, d = i * 4, i * 3
            rgb24[d] = rgba[s]
            rgb24[d + 1] = rgba[s + 1]
            rgb24[d + 2] = rgba[s + 2]
        return bytes(rgb24)

    return render


def create_glsl_renderer(args):
    """Return a callable render(time_val) -> rgb24 bytes using render_glsl subprocess."""
    render_glsl_bin = os.path.join(
        _PROJECT_ROOT, "glslang", "glslang_demo", "render_glsl"
    )
    if not os.path.isfile(render_glsl_bin):
        raise RuntimeError(f"render_glsl not found: {render_glsl_bin}")

    # Parse extra uniforms (all except time)
    extra_uniforms = {}
    for u in args.uniform:
        if "=" not in u:
            raise RuntimeError(f"--uniform must be name=value, got '{u}'")
        name, _, val = u.partition("=")
        if name == args.time_uniform:
            continue
        if "," in val:
            extra_uniforms[name] = [float(v) for v in val.split(",")]
        else:
            extra_uniforms[name] = float(val)

    if "iResolution" not in extra_uniforms:
        extra_uniforms["iResolution"] = [float(args.width), float(args.height), 1.0]

    def render(time_val):
        cmd = [render_glsl_bin, args.glsl, "-",
               str(args.width), str(args.height),
               "--uniform", args.time_uniform, str(time_val)]
        for name, val in extra_uniforms.items():
            if isinstance(val, list):
                cmd.extend(["--uniform", name] + [str(v) for v in val])
            else:
                cmd.extend(["--uniform", name, str(val)])
        result = subprocess.run(cmd, capture_output=True, timeout=30)
        if result.returncode != 0:
            err = result.stderr.decode(errors="replace").strip()
            raise RuntimeError(f"render_glsl failed: {err.split(chr(10))[-1]}")
        ppm = result.stdout
        idx = ppm.find(b"\n255\n")
        if idx < 0:
            raise RuntimeError("Invalid PPM output")
        return ppm[idx + 5:]

    return render


# ── Main ─────────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(description="Real-time shader preview GUI window")
    src = p.add_mutually_exclusive_group(required=True)
    src.add_argument("--sksl", help="SKSL shader file")
    src.add_argument("--glsl", help="GLSL shader file")
    p.add_argument("--fps", type=int, default=0,
                   help="Target FPS (0 = unlimited)")
    p.add_argument("--time-uniform", default="iTime",
                   help="Name of time uniform (default: iTime)")
    p.add_argument("--width", type=int, default=1280)
    p.add_argument("--height", type=int, default=720)
    p.add_argument("--texture", action="append", default=[],
                   help="Child texture: name=path (repeatable)")
    p.add_argument("--uniform", action="append", default=[],
                   help="Static uniform: name=value (repeatable)")
    p.add_argument("--raw", action="append", default=[],
                   help="Child names for nearest-neighbor (repeatable)")
    p.add_argument("--title", default="Shader Live Preview",
                   help="Window title")
    args = p.parse_args()

    print(f"Compiling shader...", file=sys.stderr)

    if args.sksl:
        render_fn = create_sksl_renderer(args)
        shader_label = args.sksl
    else:
        render_fn = create_glsl_renderer(args)
        shader_label = args.glsl

    window = SDL2Window(args.width, args.height, args.title)
    sdl = _load_sdl()
    print(f"Live preview: {args.width}x{args.height}  [{shader_label}]",
          file=sys.stderr)
    print("  Press Esc or close window to exit.", file=sys.stderr)

    start_ticks = sdl.SDL_GetTicks()
    frame_count = 0
    frame_interval = 1.0 / args.fps if args.fps > 0 else 0

    try:
        while window.running:
            frame_start = sdl.SDL_GetTicks()

            # Compute elapsed time from start (wall clock)
            elapsed = (frame_start - start_ticks) / 1000.0

            try:
                rgb24 = render_fn(elapsed)
            except Exception as e:
                print(f"Render error: {e}", file=sys.stderr)
                break

            window.update_frame(rgb24)
            window.poll_events()
            frame_count += 1

            # FPS limiting
            if frame_interval > 0:
                frame_end = sdl.SDL_GetTicks()
                frame_time = (frame_end - frame_start) / 1000.0
                if frame_time < frame_interval:
                    sdl.SDL_Delay(int((frame_interval - frame_time) * 1000))

            # FPS counter (every second)
            if frame_count % 60 == 0:
                now = sdl.SDL_GetTicks()
                dt = (now - start_ticks) / 1000.0
                if dt > 0:
                    fps = frame_count / dt
                    title = f"{args.title} | {fps:.0f} fps | t={elapsed:.2f}s"
                    sdl.SDL_SetWindowTitle(
                        window.win, title.encode()
                    )

    except KeyboardInterrupt:
        pass
    finally:
        actual_fps = frame_count / max((sdl.SDL_GetTicks() - start_ticks) / 1000.0, 0.001)
        print(f"\n{frame_count} frames in {elapsed:.1f}s ({actual_fps:.0f} fps)",
              file=sys.stderr)
        window.close()


if __name__ == "__main__":
    raise SystemExit(main())
