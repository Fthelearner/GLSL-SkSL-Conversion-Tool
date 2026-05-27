#version 450 core
out vec4 FragColor;
uniform sampler2D blurredInput;
uniform float inColorFactor;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 xy = fragCoord;
    float noiseGranularity = inColorFactor * 0.003921569;
    vec4 finalColor = texture(blurredInput, (xy) / vec2(textureSize(blurredInput, 0)));
    finalColor.rgb *= finalColor.a;
    float _0_t = dot(xy, vec2(78.233, 12.9898));
    float noise = mix(-noiseGranularity, noiseGranularity, fract(sin(_0_t) * 43758.5469));
    finalColor.xyz += noise;
    FragColor = finalColor;
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
