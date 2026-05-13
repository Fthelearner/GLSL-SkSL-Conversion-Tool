#version 450 core

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D image;
layout(binding = 1) uniform sampler2D displacementMap;

layout(std140, binding = 2) uniform Params {
    vec2 iResolution;
    vec2 factor;
    float strength;
} u;

vec4 sampleImage(vec2 coord)
{
    return texture(image, coord / u.iResolution);
}

vec4 sampleDisplacementMap(vec2 coord)
{
    return texture(displacementMap, coord / u.iResolution);
}

void main()
{
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / u.iResolution;
    vec4 displacement = sampleDisplacementMap(fragCoord);
    if (displacement.a <= 0.0) {
        outColor = sampleImage(fragCoord);
        return;
    }

    vec2 direction = 2.0 * (displacement.rg - 0.5);
    vec2 normal = direction * u.factor * u.strength;
    vec2 refractedUv = clamp(uv - normal * 0.05, vec2(0.001), vec2(0.999));
    outColor = texture(image, refractedUv);
}
