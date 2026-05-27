from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from image_loader import require_skia


@dataclass(slots=True)
class ShaderRenderRequest:
    shader_path: Path
    output_path: Path
    size: tuple[int, int]
    child_images: dict[str, Any]
    uniforms: dict[str, Any]
    raw_child_names: frozenset[str]
    child_sksl: dict[str, tuple[str, dict[str, Any]]] | None = None


def _compile_child_sksl(skia, source: str, child_uniforms: dict[str, Any]):
    """Compile a child SkSL shader and return a RuntimeShaderBuilder."""
    effect = skia.RuntimeEffect.MakeForShader(source)
    if effect is None:
        raise RuntimeError(f"Failed to compile child SkSL shader")
    child_builder = skia.RuntimeShaderBuilder(effect)
    for name, value in child_uniforms.items():
        child_builder.setUniform(name, _normalize_uniform_value(value))
    return child_builder.makeShader()


def render_to_png(request: ShaderRenderRequest) -> None:
    skia = require_skia()

    shader_source = request.shader_path.read_text(encoding="utf-8")
    effect = skia.RuntimeEffect.MakeForShader(shader_source)
    if effect is None:
        raise RuntimeError(f"Failed to compile shader: {request.shader_path}")

    builder = skia.RuntimeShaderBuilder(effect)
    for name, value in request.uniforms.items():
        builder.setUniform(name, _normalize_uniform_value(value))

    linear_sampling = skia.SamplingOptions(skia.FilterMode.kLinear)
    for child_name, image in request.child_images.items():
        if child_name in request.raw_child_names:
            child_shader = image.makeRawShader()
        else:
            child_shader = image.makeShader(linear_sampling)
        builder.setChild(child_name, child_shader)

    if request.child_sksl:
        for child_name, (source, child_uniforms) in request.child_sksl.items():
            shader = _compile_child_sksl(skia, source, child_uniforms)
            builder.setChild(child_name, shader)

    shader = builder.makeShader()
    surface = skia.Surface(*request.size)
    canvas = surface.getCanvas()
    paint = skia.Paint(Shader=shader, AntiAlias=True)
    canvas.drawRect(skia.Rect.MakeWH(*request.size), paint)

    request.output_path.parent.mkdir(parents=True, exist_ok=True)
    image = surface.makeImageSnapshot()
    encoded = image.encodeToData()
    if encoded is None:
        raise RuntimeError(f"Failed to encode PNG output: {request.output_path}")
    request.output_path.write_bytes(bytes(encoded))


def _normalize_uniform_value(value: Any) -> Any:
    if isinstance(value, tuple):
        return list(value)
    return value
