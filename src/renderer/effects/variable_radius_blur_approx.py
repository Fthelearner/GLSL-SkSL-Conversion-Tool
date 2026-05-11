from __future__ import annotations

from argparse import ArgumentParser, Namespace

from effects.base import ChildPathMap, EffectDefinition, GeneratedChildMap, UniformMap
from image_loader import make_demo_blur_mask
from params import EffectParams, ProjectPaths

DEFAULT_MAX_RADIUS = 24.0


def add_arguments(parser: ArgumentParser) -> None:
    parser.add_argument(
        "--blur-mask",
        help="Optional blur mask image. Relative paths are resolved from repo root.",
    )
    parser.add_argument(
        "--max-radius",
        type=float,
        default=DEFAULT_MAX_RADIUS,
        help="Maximum sample radius in pixels for the approximation shader.",
    )


def resolve_children(args: Namespace, project_paths: ProjectPaths) -> ChildPathMap:
    requested_input = project_paths.resolve(args.input)
    requested_mask = project_paths.resolve(args.blur_mask)
    return {
        "image": requested_input if requested_input is not None else project_paths.assets / "input.png",
        "blurMask": requested_mask if requested_mask is not None else project_paths.assets / "blur_mask.png",
    }


def build_child_images(
    _args: Namespace,
    _project_paths: ProjectPaths,
    size: tuple[int, int],
    _primary_image,
) -> GeneratedChildMap:
    width, height = size
    return {
        "blurMask": make_demo_blur_mask(width, height),
    }


def build_uniforms(args: Namespace, params: EffectParams) -> UniformMap:
    return {
        "iResolution": params.resolution,
        "maxRadius": args.max_radius,
        "strength": args.strength,
    }


EFFECT = EffectDefinition(
    name="variable_radius_blur_approx",
    description="Single-pass approximate variable-radius blur driven by a mask alpha field.",
    shader_file="variable_radius_blur_approx.sksl",
    child_names=("image", "blurMask"),
    default_output_file="variable_radius_blur_approx.png",
    add_arguments=add_arguments,
    resolve_children=resolve_children,
    build_child_images=build_child_images,
    build_uniforms=build_uniforms,
)
