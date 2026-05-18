#version 450 core

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D image;
layout(binding = 1) uniform sampler2D preblurImage;

uniform vec2 iResolution;
uniform vec2 startPoint;
uniform vec2 endPoint;
uniform float softness;
uniform float invert;
uniform float blurMix;

vec4 sampleImage(vec2 coord)
{
    return texture(image, coord / iResolution);
}

vec4 samplePreblurImage(vec2 coord)
{
    return texture(preblurImage, coord / iResolution);
}

void main()
{
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 startCoord = startPoint * iResolution;
    vec2 endCoord = endPoint * iResolution;
    vec2 axis = endCoord - startCoord;
    float axisLengthSquared = max(dot(axis, axis), 1e-5);
    float projected = dot(fragCoord - startCoord, axis) / axisLengthSquared;

    float edge0 = -softness;
    float edge1 = 1.0 + softness;
    float gradient = smoothstep(edge0, edge1, projected);
    if (invert > 0.5) {
        gradient = 1.0 - gradient;
    }

    float mixAmount = clamp(gradient * blurMix, 0.0, 1.0);
    vec4 sourceColor = sampleImage(fragCoord);
    vec4 blurColor = samplePreblurImage(fragCoord);
    outColor = mix(sourceColor, blurColor, mixAmount);
}
