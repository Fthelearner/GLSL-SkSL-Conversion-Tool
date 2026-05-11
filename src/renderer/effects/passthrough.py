from __future__ import annotations

from argparse import Namespace

from effects.base import ChildPathMap, EffectDefinition, UniformMap
from params import EffectParams, ProjectPaths


def resolve_children(args: Namespace, project_paths: ProjectPaths) -> ChildPathMap:
    requested_input = project_paths.resolve(args.input)
    default_input = project_paths.assets / "input.png"
    return {
        "image": requested_input if requested_input is not None else default_input,
    }


def build_uniforms(_args: Namespace, _params: EffectParams) -> UniformMap:
    return {}


EFFECT = EffectDefinition(
    name="passthrough",
    description="Framework smoke test that draws the source image through a RuntimeEffect shader.",
    shader_file="passthrough.sksl",
    child_names=("image",),
    default_output_file="passthrough.png",
    resolve_children=resolve_children,
    build_uniforms=build_uniforms,
)
