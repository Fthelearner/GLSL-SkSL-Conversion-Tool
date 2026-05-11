from __future__ import annotations

from argparse import ArgumentParser, Namespace

from effects.base import ChildPathMap, EffectDefinition, UniformMap
from params import EffectParams, ProjectPaths

DEFAULT_RIPPLE_CENTER_X = 0.5
DEFAULT_RIPPLE_CENTER_Y = 0.7
DEFAULT_WAVE_COUNT = 2.0


def add_arguments(parser: ArgumentParser) -> None:
    parser.add_argument(
        "--wave-count",
        type=float,
        default=DEFAULT_WAVE_COUNT,
        help="Ripple band count variant from the GE shader. Typical values: 1, 2, 3.",
    )
    parser.add_argument(
        "--ripple-center-x",
        type=float,
        default=DEFAULT_RIPPLE_CENTER_X,
        help="Ripple center x in normalized coordinates.",
    )
    parser.add_argument(
        "--ripple-center-y",
        type=float,
        default=DEFAULT_RIPPLE_CENTER_Y,
        help="Ripple center y in normalized coordinates.",
    )


def resolve_children(args: Namespace, project_paths: ProjectPaths) -> ChildPathMap:
    requested_input = project_paths.resolve(args.input)
    default_input = project_paths.assets / "input.png"
    return {
        "image": requested_input if requested_input is not None else default_input,
    }


def build_uniforms(args: Namespace, params: EffectParams) -> UniformMap:
    return {
        "iResolution": params.resolution,
        "progress": args.progress,
        "waveCount": args.wave_count,
        "rippleCenter": (args.ripple_center_x, args.ripple_center_y),
    }


EFFECT = EffectDefinition(
    name="water_ripple",
    description="Single-pass water ripple port based on GE shaderStringSMsend.",
    shader_file="water_ripple.sksl",
    child_names=("image",),
    default_output_file="water_ripple.png",
    add_arguments=add_arguments,
    resolve_children=resolve_children,
    build_uniforms=build_uniforms,
)
