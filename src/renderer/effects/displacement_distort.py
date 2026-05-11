from __future__ import annotations

from argparse import ArgumentParser, Namespace

from effects.base import ChildPathMap, EffectDefinition, GeneratedChildMap, UniformMap
from image_loader import make_demo_displacement_map
from params import EffectParams, ProjectPaths

DEFAULT_FACTOR_X = 1.0
DEFAULT_FACTOR_Y = 1.0


def add_arguments(parser: ArgumentParser) -> None:
    parser.add_argument(
        "--displacement-map",
        help="Optional displacement map image. Relative paths are resolved from repo root.",
    )
    parser.add_argument(
        "--factor-x",
        type=float,
        default=DEFAULT_FACTOR_X,
        help="Horizontal distortion factor.",
    )
    parser.add_argument(
        "--factor-y",
        type=float,
        default=DEFAULT_FACTOR_Y,
        help="Vertical distortion factor.",
    )


def resolve_children(args: Namespace, project_paths: ProjectPaths) -> ChildPathMap:
    requested_input = project_paths.resolve(args.input)
    requested_map = project_paths.resolve(args.displacement_map)
    return {
        "image": requested_input if requested_input is not None else project_paths.assets / "input.png",
        "displacementMap": (
            requested_map if requested_map is not None else project_paths.assets / "displacement.png"
        ),
    }


def build_child_images(
    _args: Namespace,
    _project_paths: ProjectPaths,
    size: tuple[int, int],
    _primary_image,
) -> GeneratedChildMap:
    width, height = size
    return {
        "displacementMap": make_demo_displacement_map(width, height),
    }


def build_uniforms(args: Namespace, params: EffectParams) -> UniformMap:
    return {
        "iResolution": params.resolution,
        "factor": (args.factor_x, args.factor_y),
        "strength": args.strength,
    }


EFFECT = EffectDefinition(
    name="displacement_distort",
    description="Single-pass displacement distortion driven by an external RG displacement map.",
    shader_file="displacement_distort.sksl",
    child_names=("image", "displacementMap"),
    default_output_file="displacement_distort.png",
    add_arguments=add_arguments,
    resolve_children=resolve_children,
    build_child_images=build_child_images,
    build_uniforms=build_uniforms,
    raw_child_names=frozenset({"displacementMap"}),
)
