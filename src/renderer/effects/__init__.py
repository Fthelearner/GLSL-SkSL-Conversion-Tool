from effects.base import EffectDefinition
from effects.displacement_distort import EFFECT as DISPLACEMENT_DISTORT_EFFECT
from effects.linear_gradient_blend import EFFECT as LINEAR_GRADIENT_BLEND_EFFECT
from effects.passthrough import EFFECT as PASSTHROUGH_EFFECT
from effects.variable_radius_blur_approx import EFFECT as VARIABLE_RADIUS_BLUR_APPROX_EFFECT
from effects.water_ripple import EFFECT as WATER_RIPPLE_EFFECT

EFFECTS: dict[str, EffectDefinition] = {
    DISPLACEMENT_DISTORT_EFFECT.name: DISPLACEMENT_DISTORT_EFFECT,
    LINEAR_GRADIENT_BLEND_EFFECT.name: LINEAR_GRADIENT_BLEND_EFFECT,
    PASSTHROUGH_EFFECT.name: PASSTHROUGH_EFFECT,
    VARIABLE_RADIUS_BLUR_APPROX_EFFECT.name: VARIABLE_RADIUS_BLUR_APPROX_EFFECT,
    WATER_RIPPLE_EFFECT.name: WATER_RIPPLE_EFFECT,
}


def get_effect(name: str) -> EffectDefinition:
    try:
        return EFFECTS[name]
    except KeyError as exc:
        known = ", ".join(sorted(EFFECTS))
        raise KeyError(f"Unknown effect '{name}'. Available effects: {known}") from exc


def list_effects() -> list[EffectDefinition]:
    return [EFFECTS[name] for name in sorted(EFFECTS)]
