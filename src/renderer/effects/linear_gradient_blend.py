from __future__ import annotations

from argparse import ArgumentParser, Namespace

from effects.base import ChildPathMap, EffectDefinition, GeneratedChildMap, UniformMap
from image_loader import make_blurred_image
from params import EffectParams, ProjectPaths

DEFAULT_START_X = 0.5
DEFAULT_START_Y = 0.0
DEFAULT_END_X = 0.5
DEFAULT_END_Y = 1.0
DEFAULT_SOFTNESS = 0.15
DEFAULT_BLUR_SIGMA = 12.0


def add_arguments(parser: ArgumentParser) -> None:
    parser.add_argument(
        "--preblur-image",
        help="Optional pre-blurred image. Relative paths are resolved from repo root.",
    )
    parser.add_argument("--start-x", type=float, default=DEFAULT_START_X, help="Gradient start x.")
    parser.add_argument("--start-y", type=float, default=DEFAULT_START_Y, help="Gradient start y.")
    parser.add_argument("--end-x", type=float, default=DEFAULT_END_X, help="Gradient end x.")
    parser.add_argument("--end-y", type=float, default=DEFAULT_END_Y, help="Gradient end y.")
    parser.add_argument(
        "--softness",
        type=float,
        default=DEFAULT_SOFTNESS,
        help="Expands the transition band beyond the start and end points.",
    )
    parser.add_argument(
        "--invert",
        action="store_true",
        help="Invert the blur coverage so the opposite side becomes blurred.",
    )
    parser.add_argument(
        "--blur-sigma",
        type=float,
        default=DEFAULT_BLUR_SIGMA,
        help="Fallback Gaussian blur sigma when preblur image is not provided.",
    )


def resolve_children(args: Namespace, project_paths: ProjectPaths) -> ChildPathMap:
    requested_input = project_paths.resolve(args.input)
    requested_preblur = project_paths.resolve(args.preblur_image)
    return {
        "image": requested_input if requested_input is not None else project_paths.assets / "input.png",
        "preblurImage": requested_preblur if requested_preblur is not None else project_paths.assets / "preblur.png",
    }


def build_child_images(
    args: Namespace,
    _project_paths: ProjectPaths,
    _size: tuple[int, int],
    primary_image,
) -> GeneratedChildMap:
    return {
        "preblurImage": make_blurred_image(primary_image, args.blur_sigma, args.blur_sigma),
    }


def build_uniforms(args: Namespace, params: EffectParams) -> UniformMap:
    return {
        "iResolution": params.resolution,
        "startPoint": (args.start_x, args.start_y),
        "endPoint": (args.end_x, args.end_y),
        "softness": args.softness,
        "invert": 1.0 if args.invert else 0.0,
        "blurMix": args.strength,
    }


EFFECT = EffectDefinition(
    name="linear_gradient_blend",
    description="Single-pass linear gradient blend between the source image and a pre-blurred image.",
    shader_file="linear_gradient_blend.sksl",
    child_names=("image", "preblurImage"),
    default_output_file="linear_gradient_blend.png",
    add_arguments=add_arguments,
    resolve_children=resolve_children,
    build_child_images=build_child_images,
    build_uniforms=build_uniforms,
)
