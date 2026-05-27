#version 450 core
out vec4 FragColor;
uniform sampler2D imageInput;
uniform sampler2D blurredSDFInput;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float alpha = texture(imageInput, (fragCoord) / vec2(textureSize(imageInput, 0))).w;
    float sdfRaw = texture(blurredSDFInput, (fragCoord) / vec2(textureSize(blurredSDFInput, 0))).w;
    float d0 = sdfRaw * 2.0 - 1.0;
    float dist = abs(d0) * 64.0;
    float h = 2.0 + clamp(dist * 0.2, 0.0, 8.0);
    float vR = texture(blurredSDFInput, (fragCoord + vec2(h, 0.0)) / vec2(textureSize(blurredSDFInput, 0))).w;
    float vL = texture(blurredSDFInput, (fragCoord - vec2(h, 0.0)) / vec2(textureSize(blurredSDFInput, 0))).w;
    float vB = texture(blurredSDFInput, (fragCoord + vec2(0.0, h)) / vec2(textureSize(blurredSDFInput, 0))).w;
    float vT = texture(blurredSDFInput, (fragCoord - vec2(0.0, h)) / vec2(textureSize(blurredSDFInput, 0))).w;
    vec2 grad = vec2(vR - vL, vB - vT);
    bool isInvalid = (sdfRaw > 0.95 || sdfRaw < 0.05) && dot(grad, grad) < 0.0001;
    float spineSmooth = smoothstep(64.0, 12.8, dist);
    vec2 dir = isInvalid ? vec2(0.0) : (grad + 0.0001) * spineSmooth;
    vec4 O = vec4(0.0);
    O.x = clamp((dir.x + 1.0) * 0.5, 0.0, 1.0);
    O.y = clamp((dir.y + 1.0) * 0.5, 0.0, 1.0);
    O.w = clamp(alpha, 0.0, 1.0);
    FragColor = O;
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
