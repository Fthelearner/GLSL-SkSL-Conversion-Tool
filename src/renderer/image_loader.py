from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

from params import DEFAULT_HEIGHT, DEFAULT_WIDTH

if TYPE_CHECKING:
    import skia


def load_image(path: Path | None) -> "skia.Image | None":
    if path is None or not path.exists():
        return None
    skia = require_skia()
    image = skia.Image.open(str(path))
    if image is None:
        raise ValueError(f"Failed to decode image: {path}")
    return image


def resolve_output_size(
    requested_width: int | None,
    requested_height: int | None,
    primary_image: "skia.Image | None",
) -> tuple[int, int]:
    width = requested_width
    height = requested_height

    if primary_image is not None:
        if width is None:
            width = primary_image.width()
        if height is None:
            height = primary_image.height()

    if width is None:
        width = DEFAULT_WIDTH
    if height is None:
        height = DEFAULT_HEIGHT
    return width, height


def make_demo_image(width: int, height: int) -> "skia.Image":
    skia = require_skia()
    surface = skia.Surface(width, height)
    canvas = surface.getCanvas()
    canvas.clear(skia.ColorSetRGB(246, 241, 232))

    stripe_colors = [
        skia.ColorSetRGB(33, 87, 123),
        skia.ColorSetRGB(220, 113, 54),
        skia.ColorSetRGB(242, 178, 57),
    ]
    stripe_width = max(width // 6, 1)
    for index, color in enumerate(stripe_colors):
        paint = skia.Paint(Color=color, AntiAlias=True)
        x = (index + 1) * stripe_width
        rect = skia.Rect.MakeXYWH(x, 0, stripe_width, height)
        canvas.drawRect(rect, paint)

    circle_paint = skia.Paint(Color=skia.ColorSetARGB(220, 32, 38, 57), AntiAlias=True)
    canvas.drawCircle(width * 0.72, height * 0.38, min(width, height) * 0.18, circle_paint)

    ring_paint = skia.Paint(
        AntiAlias=True,
        Color=skia.ColorSetARGB(210, 250, 246, 239),
        Style=skia.Paint.kStroke_Style,
        StrokeWidth=max(min(width, height) * 0.03, 2),
    )
    canvas.drawCircle(width * 0.32, height * 0.62, min(width, height) * 0.16, ring_paint)

    bar_paint = skia.Paint(Color=skia.ColorSetARGB(200, 20, 20, 20), AntiAlias=True)
    bar = skia.Rect.MakeXYWH(width * 0.12, height * 0.72, width * 0.48, max(height * 0.08, 1))
    canvas.drawRoundRect(bar, 18, 18, bar_paint)

    return surface.makeImageSnapshot()


def make_demo_displacement_map(width: int, height: int) -> "skia.Image":
    skia = require_skia()
    import numpy as np

    center_x = width * 0.5
    center_y = height * 0.58
    max_radius = max(min(width, height) * 0.32, 1.0)
    y_coords, x_coords = np.mgrid[0:height, 0:width]
    dx = (x_coords - center_x) / max_radius
    dy = (y_coords - center_y) / max_radius
    radius = np.sqrt(dx * dx + dy * dy)

    red = np.clip(0.5 + 0.5 * dx, 0.0, 1.0)
    green = np.clip(0.5 + 0.5 * dy, 0.0, 1.0)
    ripple = 0.5 + 0.5 * np.sin(14.0 * radius)
    fade = np.clip(1.0 - radius, 0.0, 1.0) ** 1.8
    alpha = np.where(radius < 1.0, fade * (0.55 + 0.45 * ripple), 0.0)

    rgba = np.zeros((height, width, 4), dtype=np.uint8)
    rgba[..., 0] = (red * 255.0).astype(np.uint8)
    rgba[..., 1] = (green * 255.0).astype(np.uint8)
    rgba[..., 2] = 128
    rgba[..., 3] = (np.clip(alpha, 0.0, 1.0) * 255.0).astype(np.uint8)
    return skia.Image.fromarray(rgba, colorType=skia.ColorType.kRGBA_8888_ColorType)


def make_blurred_image(image: "skia.Image", sigma_x: float, sigma_y: float) -> "skia.Image":
    skia = require_skia()
    surface = skia.Surface(image.width(), image.height())
    canvas = surface.getCanvas()
    blur_filter = skia.ImageFilters.Blur(sigma_x, sigma_y)
    paint = skia.Paint(ImageFilter=blur_filter, AntiAlias=True)
    canvas.drawImage(image, 0, 0, paint=paint)
    return surface.makeImageSnapshot()


def make_demo_blur_mask(width: int, height: int) -> "skia.Image":
    skia = require_skia()
    import numpy as np

    x = np.linspace(0.0, 1.0, width, dtype=np.float32)
    y = np.linspace(0.0, 1.0, height, dtype=np.float32)
    xx, yy = np.meshgrid(x, y)

    def smoothstep(edge0: float, edge1: float, values):
        t = np.clip((values - edge0) / max(edge1 - edge0, 1e-6), 0.0, 1.0)
        return t * t * (3.0 - 2.0 * t)

    horizontal_ramp = smoothstep(0.14, 0.92, xx)
    focus_band = smoothstep(0.08, 0.35, yy) * (1.0 - smoothstep(0.72, 0.98, yy))

    center_x = 0.62
    center_y = 0.52
    radius = np.sqrt(((xx - center_x) / 0.33) ** 2 + ((yy - center_y) / 0.44) ** 2)
    lobe = np.clip(1.0 - radius, 0.0, 1.0) ** 1.6

    alpha = np.clip(horizontal_ramp * 0.78 + focus_band * 0.18 + lobe * 0.32, 0.0, 1.0)
    rgba = np.full((height, width, 4), 255, dtype=np.uint8)
    rgba[..., 3] = (alpha * 255.0).astype(np.uint8)
    return skia.Image.fromarray(rgba, colorType=skia.ColorType.kRGBA_8888_ColorType)


def load_or_create_primary_image(
    path: Path | None,
    requested_width: int | None,
    requested_height: int | None,
) -> tuple["skia.Image", tuple[int, int]]:
    image = load_image(path)
    width, height = resolve_output_size(requested_width, requested_height, image)
    if image is None:
        image = make_demo_image(width, height)
    return image, (width, height)


def require_skia():
    try:
        import skia  # type: ignore
    except ImportError as exc:
        raise RuntimeError(
            "skia-python is not installed. Run 'uv sync' in graphics_effect_demo first."
        ) from exc
    return skia
