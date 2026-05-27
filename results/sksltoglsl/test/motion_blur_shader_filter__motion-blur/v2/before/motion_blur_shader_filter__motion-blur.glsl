#version 450 core
out vec4 FragColor;
uniform sampler2D srcImageShader;
uniform vec2 scaleAnchor;
uniform vec2 scaleSize;
uniform vec2 rectOffset;
uniform float radius;
uniform float sampleCount;
void main() {
    vec2 coord = gl_FragCoord.xy;
    vec2 scaleSizeStep = ((scaleSize - 1.0) / sampleCount) * radius;
    vec2 rectOffsetStep = (rectOffset / sampleCount) * radius;
    vec2 samplingOffset = (coord - scaleAnchor) * scaleSizeStep + rectOffsetStep;
    vec4 color = texture(srcImageShader, (coord) / vec2(textureSize(srcImageShader, 0))) * 0.11;
    float remainingWeight = 0.89;
    float baseWeight = (remainingWeight * 2.0) / (sampleCount + 1.0);
    float weightStep = baseWeight / sampleCount;
    float weight = baseWeight;
    int sampleCountInt = int(sampleCount);
    for (int i = 0;i < 50; i++) {
        if (i >= sampleCountInt) break;
        vec2 offsetCoord = coord + samplingOffset * float(i + 1);
        color += texture(srcImageShader, (offsetCoord) / vec2(textureSize(srcImageShader, 0))) * weight;
        weight -= weightStep;
    }
    FragColor = color;
    return;
}
