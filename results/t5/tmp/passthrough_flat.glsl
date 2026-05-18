#version 450 core

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D image;

uniform vec2 iResolution;

vec4 sampleImage(vec2 coord)
{
    return texture(image, coord / iResolution);
}

void main()
{
    outColor = sampleImage(gl_FragCoord.xy);
}
