#!/usr/bin/env python3
"""Fix all identified render issues: array uniforms, uniform values, multi-pass deps."""

import json, math, subprocess, sys, os
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
SHADERS = Path(__file__).resolve().parent / "shaders"
ASSETS = Path(__file__).resolve().parent / "assets"
RESULTS = PROJECT / "results" / "sksl_render"
RENDER = Path(__file__).resolve().parent / "render_sksl.py"

DIM = {"width": 1280, "height": 720}

# ============================================================
# Fix 1: Rewrite shaders with array uniforms -> individual uniforms
# ============================================================

def fix_color_gradient_shader_filter_prog():
    """Rewrite prog.sksl - replace color[12], position[12], strength[12] arrays."""
    src = SHADERS / "filter/color_gradient_shader_filter/prog.sksl"
    content = '''uniform shader srcImageShader;
uniform half2 iResolution;
uniform half4 color0; uniform half4 color1; uniform half4 color2; uniform half4 color3;
uniform half4 color4; uniform half4 color5; uniform half4 color6; uniform half4 color7;
uniform half4 color8; uniform half4 color9; uniform half4 color10; uniform half4 color11;
uniform half2 position0; uniform half2 position1; uniform half2 position2; uniform half2 position3;
uniform half2 position4; uniform half2 position5; uniform half2 position6; uniform half2 position7;
uniform half2 position8; uniform half2 position9; uniform half2 position10; uniform half2 position11;
uniform half strength0; uniform half strength1; uniform half strength2; uniform half strength3;
uniform half strength4; uniform half strength5; uniform half strength6; uniform half strength7;
uniform half strength8; uniform half strength9; uniform half strength10; uniform half strength11;

half blendMultipleColorsByDistance(half2 uv, half2 positions, half strength)
{
    positions.x *= iResolution.x / iResolution.y;
    half2 dist = uv - positions;
    half weight = strength / (dot(dist, dist) + 0.0001);
    return weight;
}

half4 main(vec2 fragCoord)
{
    half2 uv = fragCoord / iResolution.xy;
    half screenRatio = iResolution.x / iResolution.y;
    uv.x *= screenRatio;
    half totalWeight = 0.0;
    half4 blendColor = half4(0.0);

    half w0 = blendMultipleColorsByDistance(uv, position0, strength0);
    totalWeight += w0; blendColor += color0 * w0;
    half w1 = blendMultipleColorsByDistance(uv, position1, strength1);
    totalWeight += w1; blendColor += color1 * w1;
    half w2 = blendMultipleColorsByDistance(uv, position2, strength2);
    totalWeight += w2; blendColor += color2 * w2;
    half w3 = blendMultipleColorsByDistance(uv, position3, strength3);
    totalWeight += w3; blendColor += color3 * w3;
    half w4 = blendMultipleColorsByDistance(uv, position4, strength4);
    totalWeight += w4; blendColor += color4 * w4;
    half w5 = blendMultipleColorsByDistance(uv, position5, strength5);
    totalWeight += w5; blendColor += color5 * w5;
    half w6 = blendMultipleColorsByDistance(uv, position6, strength6);
    totalWeight += w6; blendColor += color6 * w6;
    half w7 = blendMultipleColorsByDistance(uv, position7, strength7);
    totalWeight += w7; blendColor += color7 * w7;
    half w8 = blendMultipleColorsByDistance(uv, position8, strength8);
    totalWeight += w8; blendColor += color8 * w8;
    half w9 = blendMultipleColorsByDistance(uv, position9, strength9);
    totalWeight += w9; blendColor += color9 * w9;
    half w10 = blendMultipleColorsByDistance(uv, position10, strength10);
    totalWeight += w10; blendColor += color10 * w10;
    half w11 = blendMultipleColorsByDistance(uv, position11, strength11);
    totalWeight += w11; blendColor += color11 * w11;

    half4 finalColor = half4(blendColor / totalWeight);
    finalColor.rgb = mix(srcImageShader.eval(fragCoord).rgb, finalColor.rgb, finalColor.a);
    return half4(finalColor.rgb, 1.0);
}
'''
    src.write_text(content)
    print("  Fixed: filter/color_gradient_shader_filter/prog.sksl")

def fix_color_gradient_shader_filter_with_mask():
    """Rewrite with-mask.sksl - replace arrays."""
    src = SHADERS / "filter/color_gradient_shader_filter/with-mask.sksl"
    content = '''uniform shader srcImageShader;
uniform shader maskImageShader;
uniform half2 iResolution;
uniform half4 color0; uniform half4 color1; uniform half4 color2; uniform half4 color3;
uniform half4 color4; uniform half4 color5; uniform half4 color6; uniform half4 color7;
uniform half4 color8; uniform half4 color9; uniform half4 color10; uniform half4 color11;
uniform half2 position0; uniform half2 position1; uniform half2 position2; uniform half2 position3;
uniform half2 position4; uniform half2 position5; uniform half2 position6; uniform half2 position7;
uniform half2 position8; uniform half2 position9; uniform half2 position10; uniform half2 position11;
uniform half strength0; uniform half strength1; uniform half strength2; uniform half strength3;
uniform half strength4; uniform half strength5; uniform half strength6; uniform half strength7;
uniform half strength8; uniform half strength9; uniform half strength10; uniform half strength11;

half blendMultipleColorsByDistance(half2 uv, half2 positions, half strength)
{
    positions.x *= iResolution.x / iResolution.y;
    half2 dist = uv - positions;
    half weight = strength / (dot(dist, dist) + 0.0001);
    return weight;
}

half4 main(vec2 fragCoord)
{
    half2 uv = fragCoord / iResolution.xy;
    half screenRatio = iResolution.x / iResolution.y;
    uv.x *= screenRatio;
    half totalWeight = 0.0;
    half4 blendColor = half4(0.0);
    half4 finalColor = half4(0.0);
    half maskValue = maskImageShader.eval(fragCoord).a;
    if (maskValue > 0.0) {
        half w0 = blendMultipleColorsByDistance(uv, position0, strength0);
        totalWeight += w0; blendColor += color0 * w0;
        half w1 = blendMultipleColorsByDistance(uv, position1, strength1);
        totalWeight += w1; blendColor += color1 * w1;
        half w2 = blendMultipleColorsByDistance(uv, position2, strength2);
        totalWeight += w2; blendColor += color2 * w2;
        half w3 = blendMultipleColorsByDistance(uv, position3, strength3);
        totalWeight += w3; blendColor += color3 * w3;
        half w4 = blendMultipleColorsByDistance(uv, position4, strength4);
        totalWeight += w4; blendColor += color4 * w4;
        half w5 = blendMultipleColorsByDistance(uv, position5, strength5);
        totalWeight += w5; blendColor += color5 * w5;
        half w6 = blendMultipleColorsByDistance(uv, position6, strength6);
        totalWeight += w6; blendColor += color6 * w6;
        half w7 = blendMultipleColorsByDistance(uv, position7, strength7);
        totalWeight += w7; blendColor += color7 * w7;
        half w8 = blendMultipleColorsByDistance(uv, position8, strength8);
        totalWeight += w8; blendColor += color8 * w8;
        half w9 = blendMultipleColorsByDistance(uv, position9, strength9);
        totalWeight += w9; blendColor += color9 * w9;
        half w10 = blendMultipleColorsByDistance(uv, position10, strength10);
        totalWeight += w10; blendColor += color10 * w10;
        half w11 = blendMultipleColorsByDistance(uv, position11, strength11);
        totalWeight += w11; blendColor += color11 * w11;
        finalColor = blendColor / totalWeight;
        finalColor.a *= maskValue;
    }
    finalColor.rgb = mix(srcImageShader.eval(fragCoord).rgb, finalColor.rgb, finalColor.a);
    return half4(finalColor.rgb, 1.0);
}
'''
    src.write_text(content)
    print("  Fixed: filter/color_gradient_shader_filter/with-mask.sksl")

def fix_circle_flowlight():
    """Rewrite circle-flowlight-shader.sksl - replace color[4], strength[4] arrays."""
    src = SHADERS / "shader/circle_flowlight_effect/circle-flowlight-shader.sksl"
    content = '''uniform half2 iResolution;
uniform half4 color0; uniform half4 color1; uniform half4 color2; uniform half4 color3;
uniform half4 rotationFrequency;
uniform half4 rotationAmplitude;
uniform half4 rotationSeed;
uniform half4 gradientX;
uniform half4 gradientY;
uniform half strength0; uniform half strength1; uniform half strength2; uniform half strength3;
uniform half distortStrength;
uniform half blendGradient;
uniform half progress;

float randWave(float frequency, float amplitude, float time, float seed)
{
    return amplitude * sin(dot(vec2(time, seed), vec2(frequency, 78.233)));
}

vec4 colorGradient(vec2 fragPos, float radius, float timeValue)
{
    vec4 colorOne = color0;
    vec4 colorTwo = color1;
    vec4 colorThree = color2;
    vec4 colorFour = color3;

    if (radius >= 1.0) {
        return vec4(0.0);
    }

    float freqTime = -2.0 * timeValue;
    mat2 rotationMatrix = mat2(cos(freqTime), sin(freqTime), -sin(freqTime), cos(freqTime));
    vec2 gradientPos[4];

    float wave = randWave(rotationFrequency.x, rotationAmplitude.x, timeValue, rotationSeed.x);
    vec2 tempPos = rotationMatrix * (vec2(gradientX.x, gradientY.x) * 2.0 - 1.0);
    gradientPos[0] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;

    wave = randWave(rotationFrequency.y, rotationAmplitude.y, timeValue, rotationSeed.y);
    tempPos = rotationMatrix * (vec2(gradientX.y, gradientY.y) * 2.0 - 1.0);
    gradientPos[1] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;

    wave = randWave(rotationFrequency.z, rotationAmplitude.z, timeValue, rotationSeed.z);
    tempPos = rotationMatrix * (vec2(gradientX.z, gradientY.z) * 2.0 - 1.0);
    gradientPos[2] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;

    wave = randWave(rotationFrequency.w, rotationAmplitude.w, timeValue, rotationSeed.w);
    tempPos = rotationMatrix * (vec2(gradientX.w, gradientY.w) * 2.0 - 1.0);
    gradientPos[3] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;

    float distorted_radius = pow(radius, distortStrength);
    vec2 fragPosNew = fragPos / radius * distorted_radius;
    vec2 gradientUV = fragPosNew * 0.5 + 0.5;
    float gradientTotalWeight = 0.0;
    vec4 gradientInterpColor = vec4(0.0);

    float distance0 = max(0.01, length(gradientUV - gradientPos[0]));
    float weight0 = strength0 / pow(distance0, blendGradient);
    gradientInterpColor += weight0 * colorOne;
    gradientTotalWeight += weight0;

    float distance1 = max(0.01, length(gradientUV - gradientPos[1]));
    float weight1 = strength1 / pow(distance1, blendGradient);
    gradientInterpColor += weight1 * colorTwo;
    gradientTotalWeight += weight1;

    float distance2 = max(0.01, length(gradientUV - gradientPos[2]));
    float weight2 = strength2 / pow(distance2, blendGradient);
    gradientInterpColor += weight2 * colorThree;
    gradientTotalWeight += weight2;

    float distance3 = max(0.01, length(gradientUV - gradientPos[3]));
    float weight3 = strength3 / pow(distance3, blendGradient);
    gradientInterpColor += weight3 * colorFour;
    gradientTotalWeight += weight3;

    gradientInterpColor = pow(gradientInterpColor / gradientTotalWeight, vec4(1.0 / 2.2));
    return gradientInterpColor;
}

float sdf_circle(vec2 uv, vec2 centerPos, float radius)
{
    return length(uv - centerPos) - radius;
}

half4 main(vec2 fragCoord) {
    vec2 fragPos = fragCoord.xy / iResolution.xy * 2.0 - 1.0;
    float radius = length(fragPos);
    vec4 gradient_color = colorGradient(fragPos, radius, progress);
    return gradient_color;
}
'''
    src.write_text(content)
    print("  Fixed: shader/circle_flowlight_effect/circle-flowlight-shader.sksl")

def fix_circle_flowlight_with_mask():
    """Rewrite circle-flowlight-shader-with-mask.sksl - replace arrays."""
    src = SHADERS / "shader/circle_flowlight_effect/circle-flowlight-shader-with-mask.sksl"
    # Read original to get mask-specific parts
    orig = src.read_text()
    content = '''uniform shader maskImageShader;
uniform half2 iResolution;
uniform half4 color0; uniform half4 color1; uniform half4 color2; uniform half4 color3;
uniform half4 rotationFrequency;
uniform half4 rotationAmplitude;
uniform half4 rotationSeed;
uniform half4 gradientX;
uniform half4 gradientY;
uniform half progress;
uniform half strength0; uniform half strength1; uniform half strength2; uniform half strength3;
uniform half distortStrength;
uniform half blendGradient;

float randWave(float frequency, float amplitude, float time, float seed)
{
    return amplitude * sin(dot(vec2(time, seed), vec2(frequency, 78.233)));
}

vec4 colorGradient(vec2 fragPos, float radius, float timeValue)
{
    vec4 colorOne = color0;
    vec4 colorTwo = color1;
    vec4 colorThree = color2;
    vec4 colorFour = color3;

    if (radius >= 1.0) {
        return vec4(0.0);
    }

    float freqTime = -2.0 * timeValue;
    mat2 rotationMatrix = mat2(cos(freqTime), sin(freqTime), -sin(freqTime), cos(freqTime));
    vec2 gradientPos[4];

    float wave = randWave(rotationFrequency.x, rotationAmplitude.x, timeValue, rotationSeed.x);
    vec2 tempPos = rotationMatrix * (vec2(gradientX.x, gradientY.x) * 2.0 - 1.0);
    gradientPos[0] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;

    wave = randWave(rotationFrequency.y, rotationAmplitude.y, timeValue, rotationSeed.y);
    tempPos = rotationMatrix * (vec2(gradientX.y, gradientY.y) * 2.0 - 1.0);
    gradientPos[1] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;

    wave = randWave(rotationFrequency.z, rotationAmplitude.z, timeValue, rotationSeed.z);
    tempPos = rotationMatrix * (vec2(gradientX.z, gradientY.z) * 2.0 - 1.0);
    gradientPos[2] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;

    wave = randWave(rotationFrequency.w, rotationAmplitude.w, timeValue, rotationSeed.w);
    tempPos = rotationMatrix * (vec2(gradientX.w, gradientY.w) * 2.0 - 1.0);
    gradientPos[3] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;

    float distorted_radius = pow(radius, distortStrength);
    vec2 fragPosNew = fragPos / radius * distorted_radius;
    vec2 gradientUV = fragPosNew * 0.5 + 0.5;
    float gradientTotalWeight = 0.0;
    vec4 gradientInterpColor = vec4(0.0);

    float distance0 = max(0.01, length(gradientUV - gradientPos[0]));
    float weight0 = strength0 / pow(distance0, blendGradient);
    gradientInterpColor += weight0 * colorOne;
    gradientTotalWeight += weight0;

    float distance1 = max(0.01, length(gradientUV - gradientPos[1]));
    float weight1 = strength1 / pow(distance1, blendGradient);
    gradientInterpColor += weight1 * colorTwo;
    gradientTotalWeight += weight1;

    float distance2 = max(0.01, length(gradientUV - gradientPos[2]));
    float weight2 = strength2 / pow(distance2, blendGradient);
    gradientInterpColor += weight2 * colorThree;
    gradientTotalWeight += weight2;

    float distance3 = max(0.01, length(gradientUV - gradientPos[3]));
    float weight3 = strength3 / pow(distance3, blendGradient);
    gradientInterpColor += weight3 * colorFour;
    gradientTotalWeight += weight3;

    gradientInterpColor = pow(gradientInterpColor / gradientTotalWeight, vec4(1.0 / 2.2));
    return gradientInterpColor;
}

half4 main(vec2 fragCoord) {
    half maskValue = maskImageShader.eval(fragCoord).a;
    if (maskValue < 0.01) {
        return half4(0.0);
    }
    vec2 fragPos = fragCoord.xy / iResolution.xy * 2.0 - 1.0;
    float radius = length(fragPos);
    vec4 gradient_color = colorGradient(fragPos, radius, progress);
    return half4(gradient_color.rgb, gradient_color.a * maskValue);
}
'''
    src.write_text(content)
    print("  Fixed: shader/circle_flowlight_effect/circle-flowlight-shader-with-mask.sksl")

def fix_radial_gradient_mask():
    """Rewrite prog.sksl - replace colors[12], positions[12] with individual uniforms."""
    for variant in ['prog', 'prog_1']:
        src = SHADERS / f"mask/radial_gradient_shader_mask/{variant}.sksl"
        orig = src.read_text()
        # Check if it has normal output (prog_1) or mask output (prog)
        is_normal = 'directionVector' in orig or 'normal' in orig.lower()

        N = 12
        color_decls = ' '.join(f'uniform half color{i};' for i in range(N))
        pos_decls = ' '.join(f'uniform half position{i};' for i in range(N))

        # Build the gradient evaluation code
        grad_body = "    half color = 0.0;\n"
        for i in range(N - 1):
            grad_body += f"    color = (sdfValue >= position{i} && sdfValue < position{i+1})\n"
            grad_body += f"        ? mix(color{i}, color{i+1}, smoothstep(position{i}, position{i+1}, sdfValue)) : color;\n"

        if is_normal:
            content = f'''uniform half2 iResolution;
uniform half2 centerPos;
uniform half radiusX;
uniform half radiusY;
{color_decls}
{pos_decls}

vec2 radialGradientMask(vec2 uv, vec2 centerPosition)
{{
    float sdfValue = length(uv - centerPosition) / radiusY;
    sdfValue = clamp(sdfValue, 0.0, 1.0);

{grad_body}
    float directionX = 1.0, directionY = 0.0;
    if (sdfValue > 0.001) {{
        vec2 gradientDir = normalize(uv - centerPosition);
        directionX = gradientDir.x;
        directionY = gradientDir.y;
    }}
    return vec2(directionX, directionY);
}}

half4 main(vec2 fragCoord)
{{
    vec2 uv = fragCoord.xy / iResolution.xy;
    float screenRatio = iResolution.x / iResolution.y;
    vec2 centeredUVs = uv * 2.0 - 1.0;
    centeredUVs.x *= screenRatio * (radiusY / radiusX);
    vec2 centerPosition = centerPos * 2 - 1.0 ;
    centerPosition.x *= screenRatio * (radiusY / radiusX);

    vec2 result = radialGradientMask(centeredUVs, centerPosition);
    float maskValue = 1.0 - clamp(length(centeredUVs - centerPosition) / radiusY, 0.0, 1.0);
    float finalColor = clamp(maskValue, 0.0, 1.0);
    return half4(normalize(vec3(result, 1.0)), finalColor);
}}
'''
        else:
            content = f'''uniform half2 iResolution;
uniform half2 centerPos;
uniform half radiusX;
uniform half radiusY;
{color_decls}
{pos_decls}

float radialGradientMask(vec2 uv, vec2 centerPosition)
{{
    float sdfValue = length(uv - centerPosition) / radiusY;
    sdfValue = clamp(sdfValue, 0.0, 1.0);

{grad_body}
    return color;
}}

half4 main(vec2 fragCoord)
{{
    vec2 uv = fragCoord.xy / iResolution.xy;
    float screenRatio = iResolution.x / iResolution.y;
    vec2 centeredUVs = uv * 2.0 - 1.0;
    centeredUVs.x *= screenRatio * (radiusY / radiusX);
    vec2 centerPosition = centerPos * 2 - 1.0 ;
    centerPosition.x *= screenRatio * (radiusY / radiusX);

    half finalColor = radialGradientMask(centeredUVs, centerPosition);
    return half4(finalColor);
}}
'''
        src.write_text(content)
        print(f"  Fixed: mask/radial_gradient_shader_mask/{variant}.sksl")


# ============================================================
# Fix 2: Generate params for all fixed shaders
# ============================================================

def make_color_gradient_params(n=12):
    """Generate params for color_gradient_shader_filter with individual uniforms."""
    u = {"iResolution": [1280.0, 720.0]}
    for i in range(n):
        hue = i / n
        r = 0.5 + 0.5 * math.cos(hue * 6.28)
        g = 0.5 + 0.5 * math.sin(hue * 6.28)
        b = 0.5 - 0.5 * math.cos(hue * 6.28)
        u[f"color{i}"] = [r, g, b, 1.0]
        u[f"position{i}"] = [i / (n - 1), 0.3 + 0.4 * (i % 3) / 2.0]  # varied y positions
        u[f"strength{i}"] = 0.5 + 0.5 * math.sin(i * 3.14 / max(n-1, 1))
    return u

def make_circle_flowlight_params():
    """Generate params for circle_flowlight with individual uniforms."""
    u = {"iResolution": [1280.0, 720.0]}
    colors = [[1.0,0.3,0.3,1.0], [0.3,1.0,0.3,1.0], [0.3,0.3,1.0,1.0], [1.0,0.8,0.2,1.0]]
    for i, c in enumerate(colors):
        u[f"color{i}"] = c
    u["rotationFrequency"] = [1.0, 0.7, 1.3, 0.5]
    u["rotationAmplitude"] = [0.3, 0.2, 0.4, 0.15]
    u["rotationSeed"] = [0.0, 1.5, 3.0, 4.5]
    u["gradientX"] = [0.0, 0.33, 0.66, 1.0]
    u["gradientY"] = [0.5, 0.5, 0.5, 0.5]
    for i in range(4):
        u[f"strength{i}"] = [0.8, 0.6, 0.7, 0.9][i]
    u["distortStrength"] = 0.1
    u["blendGradient"] = 0.5
    u["progress"] = 0.5
    return u

def make_radial_gradient_params():
    """Generate params for radial_gradient_mask with individual uniforms."""
    u = {"iResolution": [1280.0, 720.0], "centerPos": [0.5, 0.5],
         "radiusX": 0.5, "radiusY": 0.5}
    n = 12
    for i in range(n):
        u[f"color{i}"] = i / max(n-1, 1)
        u[f"position{i}"] = i / max(n-1, 1)
    return u

def generate_all_params():
    """Generate params.json for all shaders."""
    # color_gradient_shader_filter/prog
    cfg = {"dimensions": DIM, "textures": {"srcImageShader": "assets/input.png"},
           "uniforms": make_color_gradient_params()}
    p = SHADERS / "filter/color_gradient_shader_filter/prog.params.json"
    p.write_text(json.dumps(cfg, indent=2, ensure_ascii=False))
    print(f"  Params: {p.relative_to(SHADERS.parent)}")

    # color_gradient_shader_filter/with-mask
    cfg2 = {"dimensions": DIM, "textures": {"srcImageShader": "assets/input.png", "maskImageShader": "assets/blur_mask.png"},
            "uniforms": make_color_gradient_params()}
    p2 = SHADERS / "filter/color_gradient_shader_filter/with-mask.params.json"
    p2.write_text(json.dumps(cfg2, indent=2, ensure_ascii=False))
    print(f"  Params: {p2.relative_to(SHADERS.parent)}")

    # circle_flowlight
    cf_u = make_circle_flowlight_params()
    cfg3 = {"dimensions": DIM, "uniforms": cf_u}
    p3 = SHADERS / "shader/circle_flowlight_effect/circle-flowlight-shader.params.json"
    p3.write_text(json.dumps(cfg3, indent=2, ensure_ascii=False))
    print(f"  Params: {p3.relative_to(SHADERS.parent)}")

    # circle_flowlight with mask
    cfg4 = {"dimensions": DIM, "textures": {"maskImageShader": "assets/blur_mask.png"},
            "uniforms": cf_u}
    p4 = SHADERS / "shader/circle_flowlight_effect/circle-flowlight-shader-with-mask.params.json"
    p4.write_text(json.dumps(cfg4, indent=2, ensure_ascii=False))
    print(f"  Params: {p4.relative_to(SHADERS.parent)}")

    # radial_gradient_mask
    rg_u = make_radial_gradient_params()
    for v in ['prog', 'prog_1']:
        cfg5 = {"dimensions": DIM, "uniforms": rg_u}
        p5 = SHADERS / f"mask/radial_gradient_shader_mask/{v}.params.json"
        p5.write_text(json.dumps(cfg5, indent=2, ensure_ascii=False))
        print(f"  Params: {p5.relative_to(SHADERS.parent)}")

    # Fix uniform values for problematic shaders
    fix_uniform_values()

def fix_uniform_values():
    """Adjust uniform values for shaders with too-weak output."""

    # spatial_point_light/prog-no-mask: change lightPosition to be in view
    _update_params("shader/spatial_point_light/prog-no-mask.params.json", {
        "lightPosition": [640.0, 360.0, 200.0],  # center of screen
        "lightIntensity": 1.5,
        "attenuation": 1.0,
    })

    # spatial_point_light/prog-with-mask
    _update_params("shader/spatial_point_light/prog-with-mask.params.json", {
        "lightPosition": [640.0, 360.0, 200.0],
        "lightIntensity": 1.5,
        "attenuation": 1.0,
    })

    # border_light_shader/prog: make border visible
    _update_params("shader/border_light_shader/prog.params.json", {
        "lightPosition": [640.0, 360.0, 500.0],
        "lightIntensity": 1.0,
        "lightWidth": 80.0,
        "cornerRadius": 100.0,
    })

    # double_ripple_shader_mask: make ripples visible
    for v in ['prog', 'prog_1']:
        _update_params(f"mask/double_ripple_shader_mask/{v}.params.json", {
            "centerPos1": [0.4, 0.5],
            "centerPos2": [0.6, 0.5],
            "rippleRadius": 0.3,
            "rippleWidth": 0.08,
            "turbulence": 0.03,
            "haloThickness": 0.04,
        })

    # mesa_blur/mix-mesa: use blur_level2 as blurredInput
    _update_params("filter/mesa_blur_shader_filter/mix-mesa.params.json", {
        "blurredInput": "assets/blur_level2.png",
        "inColorFactor": 1.75,
    })

    # sdf_edge_light/pass-through: use input.png instead of sdf_rrect
    _update_params("filter/sdf_edge_light/pass-through.params.json", {
        "inputShader": "assets/input.png",
    })

    # sdf_edge_light/shader: fix texture references
    _update_params("filter/sdf_edge_light/shader.params.json", {
        "sdfImageShader": "assets/input.png",
        "blurredSdfImageShader": "assets/blur_level2.png",
        "lightMaskShader": "assets/light_mask.png",
    })

    # border_sdf_shader/border-code: fix sdfShape
    _update_params("shader/border_sdf_shader/border-code.params.json", {
        "sdfShape": "assets/input.png",
    })

    # frosted glass - use better inputs that actually work
    _update_params("filter/frosted_glass_shader_filter/main-shader-prog.params.json", {
        "image": "assets/input.png",
        "edgeBlurredImg": "assets/blur_edge.png",
        "bgBlurredImg": "assets/blur_bg.png",
        "sdfNormalImg": "assets/ripple_normal.png",
    })

    # spatial_glass_effect - use better inputs
    _update_params("shader/spatial_glass_effect/main-shader-prog.params.json", {
        "baseBlurImg": "assets/blur_bg.png",
        "sdfNormalImg": "assets/ripple_normal.png",
    })

    # contour flow light - use better inputs
    _update_params("shader/contour_diagonal_flow_light_shader/blend-img-prog.params.json", {
        "precalculationImage": "assets/sdf_rrect.png",
        "image1": "assets/blur_edge.png",
        "image2": "assets/blur_level1.png",
    })
    _update_params("shader/contour_diagonal_flow_light_shader/flow-light-prog.params.json", {
        "precalculationImage": "assets/sdf_rrect.png",
    })

    # particle_circular_halo/main-shader-prog - use rendered outputs
    _update_params("shader/particle_circular_halo_shader/main-shader-prog.params.json", {
        "glowHalo": "assets/blur_level1.png",
        "particleHalo": "assets/blur_level2.png",
    })

    # Generate missing intermediate images for multi-pass pipelines
    generate_intermediate_images()

def _update_params(rel_path, updates):
    """Update specific uniforms/textures in a params.json file."""
    p = SHADERS / rel_path
    if not p.exists():
        print(f"  WARNING: {rel_path} not found")
        return
    cfg = json.loads(p.read_text())
    for k, v in updates.items():
        if k in cfg.get("uniforms", {}):
            cfg["uniforms"][k] = v
        elif k in cfg.get("textures", {}):
            cfg["textures"][k] = v
        else:
            # Add to uniforms
            if "uniforms" not in cfg:
                cfg["uniforms"] = {}
            cfg["uniforms"][k] = v
    p.write_text(json.dumps(cfg, indent=2, ensure_ascii=False))
    print(f"  Updated params: {rel_path}")

def generate_intermediate_images():
    """Generate any missing intermediate assets needed for multi-pass rendering."""
    print("\n--- Generating intermediate images for multi-pass pipelines ---")
    # Already have blur levels, edge detection, etc. from previous run.
    # Just need to make sure they exist.
    needed = [
        "blur_level1.png", "blur_level2.png", "blur_edge.png", "blur_bg.png",
        "edge_blur2.png", "edge_detect.png", "ripple_normal.png", "wave_normal.png",
        "sdf_rrect.png", "sdf_triangle.png", "light_mask.png", "blur_mask.png",
    ]
    for name in needed:
        p = ASSETS / name
        if p.exists():
            print(f"  OK: {name}")
        else:
            print(f"  MISSING: {name}")


# ============================================================
# Fix 3: Re-render all files
# ============================================================

def render_all():
    print("\n" + "=" * 60)
    print("Re-rendering all SKSL files...")
    print("=" * 60)

    ok, fail, skip = 0, 0, 0
    failures = []

    for cat in ['filter', 'mask', 'shader', 'shape']:
        cat_dir = SHADERS / cat
        if not cat_dir.exists():
            continue
        for folder in sorted(cat_dir.iterdir()):
            if not folder.is_dir():
                continue
            for sksl in sorted(folder.glob("*.sksl")):
                params_path = sksl.with_suffix(".params.json")
                if not params_path.exists():
                    skip += 1
                    continue

                cfg = json.loads(params_path.read_text())

                # Skip if has sub-shader children
                has_sub = False
                for v in cfg.get("textures", {}).values():
                    if isinstance(v, dict) and v.get("childShader"):
                        has_sub = True
                        break
                if has_sub:
                    skip += 1
                    continue

                # Skip if has _note (unrenderable)
                if "_note" in cfg:
                    skip += 1
                    continue

                rel = str(sksl.relative_to(SHADERS))
                out = RESULTS / rel
                out = out.parent / (out.stem + '.png')
                out.parent.mkdir(parents=True, exist_ok=True)

                cmd = [sys.executable, str(RENDER),
                       "--sksl", str(sksl), "--output", str(out),
                       "--width", str(cfg.get("dimensions", DIM)["width"]),
                       "--height", str(cfg.get("dimensions", DIM)["height"])]

                for name, val in cfg.get("textures", {}).items():
                    if isinstance(val, str):
                        asset = resolve_asset(val)
                        if asset:
                            cmd.extend(["--texture", f"{name}={asset}"])

                for name, val in cfg.get("uniforms", {}).items():
                    if isinstance(val, list):
                        cmd.extend(["--uniform", f'{name}={",".join(str(v) for v in val)}'])
                    else:
                        cmd.extend(["--uniform", f"{name}={val}"])

                try:
                    r = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
                    if r.returncode == 0:
                        ok += 1
                    else:
                        err = r.stderr.strip()[:150]
                        print(f"  FAIL: {rel} — {err}")
                        fail += 1
                        failures.append((rel, err))
                except subprocess.TimeoutExpired:
                    print(f"  TIMEOUT: {rel}")
                    fail += 1
                    failures.append((rel, "timeout"))
                except Exception as e:
                    print(f"  ERROR: {rel} — {e}")
                    fail += 1
                    failures.append((rel, str(e)))

    print(f"\n{'='*60}")
    print(f"RESULTS: {ok} OK, {fail} FAILED, {skip} SKIPPED")
    if failures:
        print("\nFAILURES:")
        for f, e in failures:
            print(f"  {f}: {e[:120]}")
    return ok, fail, skip


def resolve_asset(rel_path):
    """Resolve asset path."""
    for c in [SHADERS.parent / rel_path, ASSETS / Path(rel_path).name]:
        if c.exists():
            return str(c)
    return None


if __name__ == "__main__":
    print("=" * 60)
    print("FIX 1: Rewriting array-uniform shaders")
    print("=" * 60)
    fix_color_gradient_shader_filter_prog()
    fix_color_gradient_shader_filter_with_mask()
    fix_circle_flowlight()
    fix_circle_flowlight_with_mask()
    fix_radial_gradient_mask()

    print("\n" + "=" * 60)
    print("FIX 2: Generating params and adjusting uniform values")
    print("=" * 60)
    generate_all_params()

    print("\n" + "=" * 60)
    print("FIX 3: Re-rendering")
    print("=" * 60)
    ok, fail, skip = render_all()

    # Quick verification
    print("\n" + "=" * 60)
    print("QUICK VERIFICATION")
    print("=" * 60)
    from PIL import Image
    # Check the previously-broken files
    check_files = [
        "filter/color_gradient_shader_filter/prog.png",
        "shader/circle_flowlight_effect/circle-flowlight-shader.png",
        "mask/radial_gradient_shader_mask/prog.png",
        "shader/spatial_point_light/prog-no-mask.png",
        "shader/border_light_shader/prog.png",
        "mask/double_ripple_shader_mask/prog.png",
        "filter/mesa_blur_shader_filter/mix-mesa.png",
    ]
    for cf in check_files:
        p = RESULTS / cf
        if p.exists():
            img = Image.open(p)
            pixels = list(img.getdata())
            total = len(pixels)
            avg_a = sum(p[3] for p in pixels[::10]) / (total/10)
            avg_r = sum(p[0] for p in pixels[::10]) / (total/10)
            size = os.path.getsize(p)
            if avg_a > 5 and size > 5000:
                print(f"  OK: {cf} ({size}B, avg_alpha={avg_a:.0f})")
            else:
                print(f"  STILL BROKEN: {cf} ({size}B, avg_alpha={avg_a:.1f}, avg_r={avg_r:.0f})")
        else:
            print(f"  MISSING: {cf}")
