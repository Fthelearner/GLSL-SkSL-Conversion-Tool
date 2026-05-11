from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

DEFAULT_WIDTH = 1280
DEFAULT_HEIGHT = 720


@dataclass(slots=True)
class EffectParams:
    resolution: tuple[float, float]
    time: float = 0.0
    progress: float = 0.0
    strength: float = 1.0
    center: tuple[float, float] = (0.5, 0.5)


@dataclass(slots=True)
class ProjectPaths:
    root: Path
    assets: Path
    shaders: Path
    outputs: Path

    def resolve(self, value: str | None) -> Path | None:
        if not value:
            return None
        path = Path(value)
        if path.is_absolute():
            return path
        return self.root / path


def build_project_paths(script_path: Path) -> ProjectPaths:
    root = script_path.resolve().parent.parent.parent
    return ProjectPaths(
        root=root,
        assets=root / "tests" / "assets",
        shaders=root / "tests" / "shaders",
        outputs=root / "results",
    )
