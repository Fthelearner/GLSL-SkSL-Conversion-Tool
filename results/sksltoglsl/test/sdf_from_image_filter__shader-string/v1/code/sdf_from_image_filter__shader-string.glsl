#version 450 core
out vec4 FragColor;
uniform sampler2D imageInput;
uniform float spreadFactor;
const float SQRT_2 = 1.41421354;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 centerFragCoord = fragCoord + vec2(0.5);
    float imageSample = texture(imageInput, (centerFragCoord) / vec2(textureSize(imageInput, 0))).w;
    if (imageSample <= 0.0) {
        FragColor = vec4(1.0, 1.0, 0.5, 0.5);
        return;
    }
    if (imageSample >= 1.0) {
        FragColor = vec4(0.5, 0.5, 1.0, 1.0);
        return;
    }
    const vec2 _0_h = vec2(0.5, 0.0);
    vec2 grad = vec2(texture(imageInput, (centerFragCoord + _0_h) / vec2(textureSize(imageInput, 0))).w - texture(imageInput, (centerFragCoord - _0_h) / vec2(textureSize(imageInput, 0))).w, texture(imageInput, (centerFragCoord + vec2(0.0, 0.5)) / vec2(textureSize(imageInput, 0))).w - texture(imageInput, (centerFragCoord - vec2(0.0, 0.5)) / vec2(textureSize(imageInput, 0))).w);
    vec2 normGrad = normalize(grad);
    vec2 edgeCoords = vec2(0.5);
    float borderCoeff = (imageSample - 0.5) * SQRT_2;
    edgeCoords -= (normGrad * borderCoeff) / (2.0 * spreadFactor);
    if (borderCoeff < 0.0) {
        FragColor = vec4(edgeCoords, 0.5, 0.5);
        return;
    }
    FragColor = vec4(0.5, 0.5, edgeCoords);
    return;
}
