#version 450 core

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D image;

layout(std140, binding = 1) uniform Params {
    vec2 iResolution = vec2(0.0, 0.0);
} u;

vec4 sampleImage(vec2 coord)
{
    return texture(image, coord / u.iResolution);
}

void main()
{
    outColor = sampleImage(gl_FragCoord.xy);
}
