#version 450 core

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D image;
layout(binding = 1) uniform sampler2D preblurImage;

layout(std140, binding = 2) uniform Params {
    vec2 iResolution;
    vec2 startPoint;
    vec2 endPoint;
    float softness;
    float invert;
    float blurMix;
} u;

vec4 sampleImage(vec2 coord)
{
    return texture(image, coord / u.iResolution);
}

vec4 samplePreblurImage(vec2 coord)
{
    return texture(preblurImage, coord / u.iResolution);
}

void main()
{
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 startCoord = u.startPoint * u.iResolution;
    vec2 endCoord = u.endPoint * u.iResolution;
    vec2 axis = endCoord - startCoord;
    float axisLengthSquared = max(dot(axis, axis), 1e-5);
    float projected = dot(fragCoord - startCoord, axis) / axisLengthSquared;

    float edge0 = -u.softness;
    float edge1 = 1.0 + u.softness;
    float gradient = smoothstep(edge0, edge1, projected);
    if (u.invert > 0.5) {
        gradient = 1.0 - gradient;
    }

    float mixAmount = clamp(gradient * u.blurMix, 0.0, 1.0);
    vec4 sourceColor = sampleImage(fragCoord);
    vec4 blurColor = samplePreblurImage(fragCoord);
    outColor = mix(sourceColor, blurColor, mixAmount);
}
