from __future__ import annotations

import argparse
from pathlib import Path
from typing import Sequence

from effects import get_effect, list_effects
from image_loader import load_image, load_or_create_primary_image
from params import EffectParams, build_project_paths
from shader_runner import ShaderRenderRequest, render_to_png

DEFAULT_EFFECT = "passthrough"


def main(argv: Sequence[str] | None = None) -> int:
    bootstrap_parser = _build_bootstrap_parser()
    bootstrap_args, _ = bootstrap_parser.parse_known_args(argv)

    if bootstrap_args.list_effects:
        _print_effects()
        return 0

    effect = get_effect(bootstrap_args.effect)
    parser = _build_parser(effect)
    args = parser.parse_args(argv)

    project_paths = build_project_paths(Path(__file__))
    child_paths = effect.get_child_paths(args, project_paths)
    primary_path = child_paths.get("image")
    primary_image, size = load_or_create_primary_image(primary_path, args.width, args.height)
    generated_child_images = effect.get_generated_child_images(args, project_paths, size, primary_image)

    child_images = {"image": primary_image}
    for child_name, child_path in child_paths.items():
        if child_name == "image":
            continue
        image = load_image(child_path)
        if image is None:
            image = generated_child_images.get(child_name)
        if image is None:
            raise FileNotFoundError(f"Missing child image for '{child_name}': {child_path}")
        child_images[child_name] = image

    params = EffectParams(
        resolution=(float(size[0]), float(size[1])),
        time=args.time,
        progress=args.progress,
        strength=args.strength,
        center=(args.center_x, args.center_y),
    )
    uniforms = effect.get_uniforms(args, params)

    output_path = _resolve_output_path(args.output, project_paths.outputs, effect.default_output_file)
    request = ShaderRenderRequest(
        shader_path=project_paths.shaders / effect.shader_file,
        output_path=output_path,
        size=size,
        child_images=child_images,
        uniforms=uniforms,
        raw_child_names=effect.raw_child_names,
    )
    render_to_png(request)
    print(f"Rendered {effect.name} -> {output_path}")
    return 0


def _build_bootstrap_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--effect", default=DEFAULT_EFFECT)
    parser.add_argument("--list-effects", action="store_true")
    return parser


def _build_parser(effect) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Render a graphics effect demo with skia-python.")
    parser.add_argument("--effect", default=effect.name)
    parser.add_argument("--list-effects", action="store_true", help="List available effects and exit.")
    parser.add_argument("--input", help="Optional primary input image. Relative paths are resolved from repo root.")
    parser.add_argument("--output", help="Optional output PNG path. Defaults to outputs/<effect>.png.")
    parser.add_argument("--width", type=int, help="Optional output width. Defaults to input width or 1280.")
    parser.add_argument("--height", type=int, help="Optional output height. Defaults to input height or 720.")
    parser.add_argument("--time", type=float, default=0.0, help="Shared time-like parameter for animated shaders.")
    parser.add_argument("--progress", type=float, default=0.0, help="Shared progress parameter in range [0, 1].")
    parser.add_argument("--strength", type=float, default=1.0, help="Shared strength parameter.")
    parser.add_argument("--center-x", type=float, default=0.5, help="Shared center x in normalized coordinates.")
    parser.add_argument("--center-y", type=float, default=0.5, help="Shared center y in normalized coordinates.")
    effect.configure_parser(parser)
    return parser


def _resolve_output_path(value: str | None, output_root: Path, default_name: str) -> Path:
    if value is None:
        return output_root / default_name
    path = Path(value)
    if path.is_absolute():
        return path
    return output_root.parent / path


def _print_effects() -> None:
    for effect in list_effects():
        print(f"{effect.name}: {effect.description}")
