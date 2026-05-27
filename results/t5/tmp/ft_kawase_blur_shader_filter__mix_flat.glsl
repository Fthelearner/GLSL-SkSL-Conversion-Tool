#version 450 core
out vec4 FragColor;
uniform sampler2D blurredInput;
uniform sampler2D originalInput;
uniform float mixFactor;
uniform float inColorFactor;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 xy = fragCoord;
    float noiseGranularity = inColorFactor * 0.003921569;
    vec4 finalColor = mix(texture(originalInput, (xy) / vec2(textureSize(originalInput, 0))), texture(blurredInput, (xy) / vec2(textureSize(blurredInput, 0))), vec4(mixFactor));
    float _0_t = dot(xy, vec2(78.233, 12.9898));
    float noise = mix(-noiseGranularity, noiseGranularity, fract(sin(_0_t) * 43758.5469));
    finalColor.xyz += noise;
    FragColor = finalColor;
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
