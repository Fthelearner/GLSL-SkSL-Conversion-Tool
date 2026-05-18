#version 450 core

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D image;
layout(binding = 1) uniform sampler2D blurMask;

uniform vec2 iResolution;
uniform float maxRadius;
uniform float strength;

vec4 sampleImage(vec2 coord)
{
    return texture(image, coord / iResolution);
}

vec4 sampleBlurMask(vec2 coord)
{
    return texture(blurMask, coord / iResolution);
}

vec4 samplePair(vec2 coord, vec2 direction, float radius, float factor, inout float totalWeight)
{
    vec2 offset = direction * radius * factor;
    float weight = 1.0 - 0.18 * factor;
    totalWeight += 2.0 * weight;
    return (sampleImage(coord + offset) + sampleImage(coord - offset)) * weight;
}

void main()
{
    vec2 coord = gl_FragCoord.xy;
    float maskAlpha = sampleBlurMask(coord).a;
    float radius = clamp(maskAlpha * maxRadius * strength, 0.0, maxRadius);
    if (radius < 0.5) {
        outColor = sampleImage(coord);
        return;
    }

    vec4 sum = sampleImage(coord) * 1.4;
    float totalWeight = 1.4;

    vec2 horizontal = vec2(1.0, 0.0);
    vec2 vertical = vec2(0.0, 1.0);
    vec2 diagonalA = normalize(vec2(1.0, 1.0));
    vec2 diagonalB = normalize(vec2(1.0, -1.0));

    sum += samplePair(coord, horizontal, radius, 0.33, totalWeight);
    sum += samplePair(coord, horizontal, radius, 0.66, totalWeight);
    sum += samplePair(coord, horizontal, radius, 1.00, totalWeight);

    sum += samplePair(coord, vertical, radius, 0.33, totalWeight);
    sum += samplePair(coord, vertical, radius, 0.66, totalWeight);
    sum += samplePair(coord, vertical, radius, 1.00, totalWeight);

    sum += samplePair(coord, diagonalA, radius, 0.5, totalWeight);
    sum += samplePair(coord, diagonalA, radius, 0.9, totalWeight);
    sum += samplePair(coord, diagonalB, radius, 0.5, totalWeight);
    sum += samplePair(coord, diagonalB, radius, 0.9, totalWeight);

    outColor = sum / totalWeight;
}
