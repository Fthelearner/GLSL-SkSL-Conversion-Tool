from __future__ import annotations

from argparse import ArgumentParser, Namespace
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

from params import EffectParams, ProjectPaths

UniformValue = int | float | list[int] | list[float] | tuple[int, ...] | tuple[float, ...]
UniformMap = dict[str, UniformValue]
ChildPathMap = dict[str, Path | None]
GeneratedChildMap = dict[str, Any]


@dataclass(slots=True)
class EffectDefinition:
    name: str
    description: str
    shader_file: str
    child_names: tuple[str, ...]
    default_output_file: str
    add_arguments: Callable[[ArgumentParser], None] | None = None
    resolve_children: Callable[[Namespace, ProjectPaths], ChildPathMap] | None = None
    build_child_images: Callable[
        [Namespace, ProjectPaths, tuple[int, int], Any],
        GeneratedChildMap,
    ] | None = None
    build_uniforms: Callable[[Namespace, EffectParams], UniformMap] | None = None
    raw_child_names: frozenset[str] = field(default_factory=frozenset)

    def configure_parser(self, parser: ArgumentParser) -> None:
        if self.add_arguments is not None:
            self.add_arguments(parser)

    def get_child_paths(self, args: Namespace, project_paths: ProjectPaths) -> ChildPathMap:
        if self.resolve_children is None:
            return {}
        return self.resolve_children(args, project_paths)

    def get_generated_child_images(
        self,
        args: Namespace,
        project_paths: ProjectPaths,
        size: tuple[int, int],
        primary_image: Any,
    ) -> GeneratedChildMap:
        if self.build_child_images is None:
            return {}
        return self.build_child_images(args, project_paths, size, primary_image)

    def get_uniforms(self, args: Namespace, params: EffectParams) -> UniformMap:
        if self.build_uniforms is None:
            return {}
        return self.build_uniforms(args, params)
