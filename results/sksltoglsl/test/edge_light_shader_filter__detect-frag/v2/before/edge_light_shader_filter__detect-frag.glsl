#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform float edgeThreshold;
uniform float edgeIntensity;
uniform float edgeSoftThreshold;
uniform vec3 edgeDetectColor;
uniform vec3 edgeColor;
uniform float ifRawColor;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float x = 0.0;
    float y = 0.0;
    x += dot(edgeDetectColor, texture(image, (fragCoord + vec2(-1.0)) / vec2(textureSize(image, 0))).xyz);
    x += dot(edgeDetectColor, texture(image, (fragCoord + vec2(-1.0, 0.0)) / vec2(textureSize(image, 0))).xyz) * 2.0;
    x += dot(edgeDetectColor, texture(image, (fragCoord + vec2(-1.0, 1.0)) / vec2(textureSize(image, 0))).xyz);
    x -= dot(edgeDetectColor, texture(image, (fragCoord + vec2(1.0, -1.0)) / vec2(textureSize(image, 0))).xyz);
    x -= dot(edgeDetectColor, texture(image, (fragCoord + vec2(1.0, 0.0)) / vec2(textureSize(image, 0))).xyz) * 2.0;
    x -= dot(edgeDetectColor, texture(image, (fragCoord + vec2(1.0)) / vec2(textureSize(image, 0))).xyz);
    y += dot(edgeDetectColor, texture(image, (fragCoord + vec2(-1.0, 1.0)) / vec2(textureSize(image, 0))).xyz);
    y += dot(edgeDetectColor, texture(image, (fragCoord + vec2(0.0, 1.0)) / vec2(textureSize(image, 0))).xyz) * 2.0;
    y += dot(edgeDetectColor, texture(image, (fragCoord + vec2(1.0)) / vec2(textureSize(image, 0))).xyz);
    y -= dot(edgeDetectColor, texture(image, (fragCoord + vec2(-1.0)) / vec2(textureSize(image, 0))).xyz);
    y -= dot(edgeDetectColor, texture(image, (fragCoord + vec2(0.0, -1.0)) / vec2(textureSize(image, 0))).xyz) * 2.0;
    y -= dot(edgeDetectColor, texture(image, (fragCoord + vec2(1.0, -1.0)) / vec2(textureSize(image, 0))).xyz);
    float sobel = sqrt(x * x + y * y);
    sobel = edgeIntensity * smoothstep(edgeThreshold - edgeSoftThreshold, edgeThreshold + edgeSoftThreshold, sobel);
    vec3 color = sobel * (ifRawColor > 0.5 ? texture(image, (fragCoord) / vec2(textureSize(image, 0))).xyz : edgeColor);
    FragColor = vec4(color, 1.0);
    return;
}
