#!/usr/bin/env python3
"""Generate params.json for each SKSL file based on C++ analysis."""

import json
import os
import re
from pathlib import Path

SHADERS_DIR = Path(__file__).resolve().parent / "shaders"

# Default image texture to use for shaders that take an input image
DEFAULT_IMAGE = "assets/input.png"
DEFAULT_DIMENSIONS = {"width": 1280, "height": 720}

def parse_sksl_uniforms(sksl_path):
    """Parse uniform declarations from a SKSL file."""
    text = sksl_path.read_text(encoding='utf-8')
    uniforms = []
    textures = []

    for line in text.split('\n'):
        line = line.strip().rstrip(';')
        # Match: uniform <type> <name>;
        # or: uniform <type> <name>[N];
        m = re.match(r'uniform\s+(shader|half|float|vec\d|half\d|float\dx\d+)\s+(\w+)(?:\[(\d+)\])?\s*$', line)
        if m:
            utype, name, arr = m.group(1), m.group(2), m.group(3)
            if utype == 'shader':
                textures.append(name)
            else:
                uniforms.append({'name': name, 'type': utype, 'array': int(arr) if arr else None})
    return uniforms, textures

def uval(uniforms, name, default=None):
    """Get a value for a uniform, or default."""
    return default

def generate_params(sksl_path, category, folder_name):
    """Generate params for a single SKSL file."""
    uniforms, textures = parse_sksl_uniforms(sksl_path)

    config = {
        "dimensions": dict(DEFAULT_DIMENSIONS),
        "textures": {},
        "uniforms": {}
    }

    # Map texture names to image paths (for image-type children)
    # Most 'image', 'imageShader', 'srcImageShader', 'imageInput' are image textures
    image_texture_names = {
        'image', 'imageShader', 'srcImageShader', 'imageInput', 'inputShader',
        'pixelmapShader', 'imageShader', 'originalInput', 'blurredInput',
        'topLayer', 'bottomLayer', 'original_tex', 'blur_tex', 'blur_mask',
        'colorGradientShader', 'imageBlur0', 'imageBlur1', 'imageBlur2',
        'imageBlur3', 'imageBlur4', 'imageBloom',
    }

    # Sub-shader children (not images) - these are multi-pass intermediate results
    subshader_names = {
        'mask', 'maskEffect', 'alphaMask', 'sdfShader', 'sdfShape',
        'lightMaskShader', 'shape', 'shapeShader', 'left', 'right',
        'maskImageShader', 'gradientShader', 'blurImageShader',
        'singleParticleHalo', 'glowHalo', 'particleHalo',
        'edgeBlurredImg', 'bgBlurredImg', 'sdfNormalImg', 'baseBlurImg',
        'precalculationImage', 'loopImage', 'sdfImage', 'progressImage',
        'image1', 'image2', 'maskImageShader', 'blurredSDFInput',
        'sdfImageShader', 'blurredSdfImageShader', 'composeImage',
        'auroraNoiseTexture', 'verticalBlurTexture',
        'u_sdfTex', 'u_maskTex', 'u_seeds', 'u_prevD', 'pathShader',
        'imageMask',
    }

    for tex_name in textures:
        if tex_name in subshader_names:
            # Sub-shader child - mark with a special indicator
            config["textures"][tex_name] = {"childShader": True, "comment": "sub-shader, needs parent pipeline"}
        elif tex_name in image_texture_names:
            config["textures"][tex_name] = DEFAULT_IMAGE
        else:
            # Unknown - default to image texture
            config["textures"][tex_name] = DEFAULT_IMAGE

    def expand_array(u, val):
        """If val is a single element and u is an array, repeat it."""
        if u['array'] and val is not None:
            # val is a single element value (scalar or list)
            # Repeat it array times
            return [val] * u['array']
        return val

    # Generate uniform defaults based on name and type conventions
    for u in uniforms:
        name = u['name']
        utype = u['type']
        val = None

        if name == 'iResolution':
            val = [1280.0, 720.0]
        elif name in ('invResolution', 'invImageSize'):
            val = [1.0/1280.0, 1.0/720.0]
        elif name in ('imageSize', 'srcResolution', 'dstResolution', 'pixelmapSize'):
            val = [1280.0, 720.0]
        elif name == 'progress':
            val = 0.5
        elif name == 'time' or name == 'iTime':
            val = 1.5
        elif name == 'opacity':
            val = 1.0
        elif name == 'alphaProgress':
            val = 0.5
        elif name == 'lightIntensity' or name == 'intensity':
            val = 0.8
        elif name == 'soundIntensity':
            val = 0.5
        elif name == 'lightPosition':
            val = [0.5, 0.5, 1.0]
        elif name == 'lightColor' or name == 'u_color':
            val = [1.0, 1.0, 1.0, 1.0]
        elif name == 'lightDirection':
            val = [0.0, -1.0, 0.5]
        elif name == 'contentRotationAngle' or name == 'borderLightRotationAngle':
            val = [0.0, 0.0, 0.0]
        elif name == 'lightWidth' or name == 'u_width' or name == 'borderWidth' or name == 'u_borderWidth':
            val = 4.0
        elif name == 'cornerRadius':
            val = 16.0
        elif name == 'color' and u['array']:
            n = u['array']
            colors = []
            for i in range(n):
                hue = i / max(n, 1)
                import math as _m2
                r = 0.5 + 0.5 * _m2.cos(hue * 6.28)
                g = 0.5 + 0.5 * _m2.sin(hue * 6.28)
                b = 0.5 - 0.5 * _m2.cos(hue * 6.28)
                colors.append([r, g, b, 1.0])
            val = colors
        elif name == 'color' or name == 'u_borderColor':
            val = [1.0, 0.3, 0.3, 0.8]
        elif name == 'shadowColor':
            val = [0.0, 0.0, 0.0, 0.5]
        elif name == 'shadowOffset':
            val = [4.0, 4.0]
        elif name == 'shadowRadius' or name == 'shadowSize':
            val = 8.0
        elif name == 'shadowStrength':
            val = 0.5
        elif name == 'ambientColor':
            val = [0.0, 0.0, 0.0, 0.04]
        elif name == 'ambientBlurRadius':
            val = 50.0
        elif name == 'ambientOutset':
            val = 2.0
        elif name == 'spotColor':
            val = [0.0, 0.0, 0.0, 0.3]
        elif name == 'spotBlurRadius':
            val = 10.0
        elif name == 'isFilled':
            val = 0.0
        elif name == 'u_isOutline':
            val = 0.0
        elif name == 'u_dashWidth':
            val = 10.0
        elif name == 'u_dashGap':
            val = 5.0
        elif name == 'isFilled':
            val = 1.0
        elif name == 'blurIntensity':
            val = 5.0
        elif name == 'horizontal':
            val = 0.0
        elif name == 'mixStrength':
            val = 0.5
        elif name == 'noise' or name == 'randomNoise':
            val = 0.5
        elif name == 'freqX':
            val = 4.0
        elif name == 'freqY':
            val = 3.0
        elif name == 'spreadFactor' or name == 'sdfSpreadFactor':
            val = 10.0
        elif name == 'bloomIntensityCutoff':
            val = 0.3
        elif name == 'maxIntensity':
            val = 1.0
        elif name == 'maxBloomIntensity':
            val = 0.6
        elif name == 'bloomFalloffPow':
            val = 2.0
        elif name == 'minBorderWidth':
            val = 2.0
        elif name == 'maxBorderWidth' or name == 'borderWidthDelta':
            val = 8.0
        elif name == 'innerBorderBloomWidth':
            val = 4.0
        elif name == 'outerBorderBloomWidth':
            val = 12.0
        elif name == 'invInnerBorderBloomWidth':
            val = 0.25
        elif name == 'invOuterBorderBloomWidth':
            val = 0.0833
        elif name == 'enableBloom':
            val = 1.0
        elif name in ('coefficient1', 'greyCoef1', 'coefficient1'):
            val = 0.5
        elif name in ('coefficient2', 'greyCoef2', 'coefficient2'):
            val = 0.5
        elif name == 'low':
            val = 0.2
        elif name == 'high':
            val = 0.8
        elif name == 'threshold':
            val = 0.5
        elif name == 'saturation':
            val = 1.0
        elif name == 'edgeThreshold' or name == 'edgeSoftThreshold':
            val = 0.3
        elif name == 'edgeIntensity':
            val = 0.8
        elif name == 'edgeDetectColor':
            val = [0.2126729, 0.7151522, 0.0721750]
        elif name == 'edgeColor' or name == 'lightColor':
            val = [1.0, 1.0, 1.0]
        elif name == 'ifRawColor' or name == 'useEllipse' or name == 'useRawColor':
            val = 0.0
        elif name == 'blurDirection' or name == 'sigma':
            val = 3.0
        elif name == 'brightness':
            val = 0.1
        elif name == 'center' or name == 'centerPos' or name == 'shapeCenterPos':
            val = [0.5, 0.5]
        elif name in ('centerPos1', 'center1'):
            val = [0.35, 0.5]
        elif name in ('centerPos2', 'center2'):
            val = [0.65, 0.5]
        elif name in ('rippleCenter', 'rippleCenter1', 'rippleCenter2'):
            val = [0.5, 0.7]
        elif name == 'waveCenter' or name == 'clickPos':
            val = [0.5, 0.5]
        elif name == 'radius' or name == 'globalRadius':
            val = 0.3
        elif name == 'thickness' or name == 'haloThickness':
            val = 0.05
        elif name == 'rippleRadius':
            val = 0.25
        elif name == 'rippleWidth' or name == 'waveWidth' or name == 'width' or name == 'u_width':
            val = 0.05
        elif name == 'widthCenterOffset':
            val = 0.01
        elif name == 'turbulence' or name == 'turbulenceStrength':
            val = 0.05
        elif name in ('waveCount', 'propagationRadius', 'blurRadius'):
            val = 2.0
        elif name == 'spacing':
            val = 0.01
        elif name == 'factor' or name == 'mixFactor' or name == 'inColorFactor' or name == 'bgFactor':
            val = 0.5
        elif name == 'inverseFlag' or name == 'invert' or name == 'inverse' or name == 'distortionEnable' or name == 'distortionEnabled' or name == 'axialEnable':
            val = 0.0
        elif name == 'sampleCount':
            val = 20.0
        elif name == 'strength' or name == 'warpStrength' or name == 'lightIntensity' or name == 'intensity':
            val = 0.5
        elif name == 'attenuation':
            val = 2.0
        elif name == 'waveLength':
            val = 0.15
        elif name in ('reflectRatio', 'ratio'):
            val = 0.5
        elif name in ('waveHeight', 'amplitude'):
            val = 0.1
        elif name == 'waveDown':
            val = 0.3
        elif name == 'processTime':
            val = 0.5
        elif name == 'hotZone':
            val = 0.3
        elif name == 'influenceRadii':
            val = [0.5, 0.5]
        elif name == 'rotationCenter':
            val = [0.5, 0.5]
        elif name == 'vertex0':
            val = [0.5, 0.2]
        elif name == 'vertex1':
            val = [0.2, 0.8]
        elif name == 'vertex2':
            val = [0.8, 0.8]
        elif name in ('halfSize', 'boxHalfSize'):
            val = [0.3, 0.2]
        elif name == 'cornerRadiusTL':
            val = [0.05, 0.05]
        elif name == 'cornerRadiusTR':
            val = [0.05, 0.05]
        elif name == 'cornerRadiusBR':
            val = [0.05, 0.05]
        elif name == 'cornerRadiusBL':
            val = [0.05, 0.05]
        elif name == 'clampedCornerRadius':
            val = 0.05
        elif name in ('innerFrameWidth', 'outerFrameWidth'):
            val = 0.02
        elif name in ('invInnerFrameWidth', 'invOuterFrameWidth'):
            val = 50.0
        elif name == 'rectPos':
            val = [0.0, 0.0]
        elif name in ('localBasis0', 'localBasis1'):
            val = [1.0, 0.0]
        elif name == 'axialFeatherStrength':
            val = 0.5
        elif name == 'axialCoordWeights':
            val = [0.5, 0.5]
        elif name == 'axialInvSpan':
            val = 2.0
        elif name == 'axialRiseEnd':
            val = 0.2
        elif name == 'axialFallStart':
            val = 0.8
        elif name == 'innerBezierCoeff' or name == 'outerBezierCoeff':
            val = [0.0, 0.0, 1.0]
        elif name in ('offset', 'downSampleFactor'):
            val = 1.0
        elif name == 'innerShadowRefractPx' or name == 'refractOutPx':
            val = 2.0
        elif name == 'innerShadowWidth':
            val = 4.0
        elif name == 'innerShadowExp':
            val = 4.62
        elif name == 'sdK' or name == 'envK' or name == 'hlK':
            val = 0.5
        elif name == 'sdB' or name == 'envB' or name == 'hlB':
            val = 0.0
        elif name == 'sdS' or name == 'envS' or name == 'hlS':
            val = 0.0
        elif name == 'highLightAngleDeg':
            val = 45.0
        elif name == 'highLightFeatherDeg':
            val = 15.0
        elif name == 'highLightWidthPx':
            val = 2.0
        elif name == 'highLightFeatherPx':
            val = 1.0
        elif name == 'highLightShiftPx':
            val = 0.0
        elif name == 'highLightDirection':
            val = [0.0, -1.0]
        elif name == 'factorlight':
            val = [1.0, 1.0]
        elif name == 'factorhalo':
            val = [1.0, 1.0]
        elif name == 'factorprecalc':
            val = [1.0, 1.0]
        elif name == 'lightWeight':
            val = 0.8
        elif name == 'haloWeight':
            val = 0.4
        elif name == 'headRoom':
            val = 1.0
        elif name == 'line1Start':
            val = 0.1
        elif name == 'line1Length':
            val = 0.3
        elif name == 'line1Color':
            val = [1.0, 0.5, 0.2]
        elif name == 'line2Start':
            val = 0.5
        elif name == 'line2Length':
            val = 0.3
        elif name == 'line2Color':
            val = [0.2, 0.5, 1.0]
        elif name == 'lineThickness' or name == 'thickness':
            val = 0.02
        elif name == 'count' or name == 'u_curveCount':
            val = 4.0
        elif name == 'leftGridBoundary':
            val = 0.0
        elif name == 'topGridBoundary':
            val = 0.0
        elif name == 'u_step':
            val = 1.0
        elif name == 'u_isFirstBatch':
            val = 1.0
        elif name == 'iScale' or name == 'jfaRadius':
            val = 3.0
        elif name == 'dst' or name == 'fillPixel':
            val = [0.0, 0.0, 1.0, 1.0]
        elif name in ('src', 'srcRect'):
            val = [0.0, 0.0, 1.0, 1.0]
        elif name == 'colorA':
            val = [1.0, 0.3, 0.3]
        elif name == 'colorB':
            val = [0.3, 1.0, 0.3]
        elif name == 'colorC':
            val = [0.3, 0.3, 1.0]
        elif name == 'colorProgress':
            val = 0.3
        elif name == 'shockWaveAlphaA' or name == 'shockWaveAlphaB':
            val = 0.5
        elif name == 'shockWaveProgressA':
            val = 0.3
        elif name == 'shockWaveProgressB':
            val = 0.6
        elif name == 'shockWaveTotalAlpha':
            val = 0.5
        elif name == 'noiseScale':
            val = 2.0
        elif name == 'riseWeight':
            val = 0.5
        elif name == 'factor':
            val = [1.0, 0.7]
        elif name == 'zoomOffset':
            val = [0.0, 0.0]
        elif name == 'borderSize':
            val = 2.0
        elif name == 'borderColor':
            val = [1.0, 1.0, 1.0, 1.0]
        elif name == 'scaleAnchor':
            val = [0.5, 0.5]
        elif name == 'scaleSize':
            val = [1.0, 1.0]
        elif name == 'rectOffset':
            val = [0.0, 0.0]
        elif name in ('in_blurOffset', 'blurOffset'):
            val = [1.0, 0.0]
        elif name == 'in_maxSizeXY':
            val = [1280.0, 720.0]
        elif name == 'in_dir':
            val = [1.0, 0.0]
        elif name in ('lu', 'rb', 'e', 'f', 'g'):
            val = [0.1, 0.1]
        elif name in ('k2', 'ik2', 'k1Base', 'k0Base'):
            val = 0.5
        elif name == 'barrelDistortion':
            val = [0.0, 0.0, 0.0, 0.0]
        elif name == 'maskFactor':
            val = 1.0
        elif name in ('redOffset', 'greenOffset', 'blueOffset'):
            val = [0.01, 0.0]
        elif name == 'blendGradient':
            val = 0.5
        elif name == 'distortStrength':
            val = 0.1
        elif name == 'position' and u['array']:
            n = u['array']
            val = [[i / max(n-1, 1), 0.5] for i in range(n)]
        elif name == 'strength' and u['array']:
            n = u['array']
            import math as _m
            val = [0.5 + 0.5 * _m.sin(i * 3.14 / max(n-1, 1)) for i in range(n)]
        elif name == 'rotationFrequency' and u['array']:
            val = [[1.0, 0.0, 0.0, 0.0] for _ in range(u['array'])]
        elif name == 'rotationAmplitude' and u['array']:
            val = [[0.3, 0.0, 0.0, 0.0] for _ in range(u['array'])]
        elif name == 'rotationSeed' and u['array']:
            val = [[float(i), 0.0, 0.0, 0.0] for i in range(u['array'])]
        elif name == 'gradientX' and u['array']:
            val = [[float(i) / max(u['array']-1, 1), 0.0, 0.0, 0.0] for i in range(u['array'])]
        elif name == 'gradientY' and u['array']:
            val = [[0.5, 0.0, 0.0, 0.0] for _ in range(u['array'])]
        elif name == 'factor' and u['array']:
            val = [1.0 for _ in range(u['array'])]
        elif name == 'blend_c' and u['array']:
            n = u['array']
            colors = []
            for i in range(n):
                hue = i / max(n, 1)
                r = 0.5 + 0.5 * (__import__('math').cos(hue * 6.28) if n > 1 else 0)
                g = 0.5 + 0.5 * (__import__('math').sin(hue * 6.28) if n > 1 else 0)
                b = 0.5 - 0.5 * (__import__('math').cos(hue * 6.28) if n > 1 else 0)
                colors.append([r, g, b, 1.0])
            val = colors
        elif name == 'gradient_p' and u['array']:
            n = u['array']
            val = [[i / max(n-1, 1), 0.5] for i in range(n)]
        elif name == 'gradient_s' and u['array']:
            import math as _m
            n = u['array']
            val = [0.5 + 0.5 * _m.sin(i * 3.14 / max(n-1, 1)) for i in range(n)]
        elif name == 'colors' and u['array']:
            n = u['array']
            val = [i / max(n-1, 1) for i in range(n)]
        elif name == 'positions' and u['array']:
            n = u['array']
            val = [i / max(n-1, 1) for i in range(n)]
        elif name == 'controlPoints' and u['array']:
            n = u['array']
            pts = []
            for i in range(n):
                angle = i * 6.28 / n
                r = 0.3
                pts.append([0.5 + r * __import__('math').cos(angle), 0.5 + r * __import__('math').sin(angle)])
            val = pts
        elif name == 'segmentIndex' and u['array']:
            n = u['array']
            val = [float(i) for i in range(n)]
        elif name == 'curveWeightPrefix' and u['array']:
            n = u['array']
            val = [1.0 for _ in range(n)]
        elif name == 'curveWeightCurrent' and u['array']:
            n = u['array']
            val = [0.5 for _ in range(n)]
        elif name == 'in_blurOffset' and u['array']:
            n = u['array']
            val = [[1.0 * (i+1), 0.0] for i in range(n)]
        elif name == 'transformMatrix':
            val = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
        elif name in ('maskIntensity', 'lightIntensity'):
            val = 0.8
        else:
            # Generic defaults based on type
            if '4' in utype or (utype == 'vec4' or utype == 'half4'):
                val = [1.0, 1.0, 1.0, 1.0]
            elif '3' in utype or (utype == 'vec3' or utype == 'half3'):
                val = [1.0, 1.0, 1.0]
            elif '2' in utype or (utype == 'vec2' or utype == 'half2'):
                val = [0.5, 0.5]
            elif utype == 'float3x3':
                val = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
            else:
                val = 0.5

        # Handle array uniforms
        if u['array'] and val is not None and not isinstance(val, list):
            val = [val] * u['array']

        config["uniforms"][name] = val

    # Remove empty collections
    if not config["textures"]:
        del config["textures"]
    if not config["uniforms"]:
        del config["uniforms"]

    return config

def main():
    count = 0
    for category in ['filter', 'mask', 'shader', 'shape']:
        cat_dir = SHADERS_DIR / category
        if not cat_dir.exists():
            continue
        for folder in sorted(cat_dir.iterdir()):
            if not folder.is_dir():
                continue
            for sksl_file in sorted(folder.glob("*.sksl")):
                config = generate_params(sksl_file, category, folder.name)
                params_path = sksl_file.with_suffix(".params.json")
                with open(params_path, 'w', encoding='utf-8') as f:
                    json.dump(config, f, indent=2, ensure_ascii=False)
                print(f"Generated: {params_path.relative_to(SHADERS_DIR.parent)}")
                count += 1
    print(f"\nTotal: {count} params files generated")

if __name__ == "__main__":
    main()
