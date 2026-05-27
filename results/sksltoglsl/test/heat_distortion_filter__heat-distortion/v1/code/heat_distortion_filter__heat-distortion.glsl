#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 iResolution;
uniform float progress;
uniform float intensity;
uniform float noiseScale;
uniform float riseWeight;
float perlinNoise_hh2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = (f * f) * (3.0 - 2.0 * f);
    float _0_angle = fract(sin(dot(i, vec2(12.9898, 78.233))) * 43758.5469) * 6.28318548;
    vec2 grad00 = vec2(cos(_0_angle), sin(_0_angle));
    float _1_angle = fract(sin(dot(i + vec2(1.0, 0.0), vec2(12.9898, 78.233))) * 43758.5469) * 6.28318548;
    vec2 grad10 = vec2(cos(_1_angle), sin(_1_angle));
    float _2_angle = fract(sin(dot(i + vec2(0.0, 1.0), vec2(12.9898, 78.233))) * 43758.5469) * 6.28318548;
    vec2 grad01 = vec2(cos(_2_angle), sin(_2_angle));
    float _3_angle = fract(sin(dot(i + vec2(1.0), vec2(12.9898, 78.233))) * 43758.5469) * 6.28318548;
    vec2 grad11 = vec2(cos(_3_angle), sin(_3_angle));
    vec2 d00 = f;
    vec2 d10 = f - vec2(1.0, 0.0);
    vec2 d01 = f - vec2(0.0, 1.0);
    vec2 d11 = f - vec2(1.0);
    float dot00 = dot(grad00, d00);
    float dot10 = dot(grad10, d10);
    float dot01 = dot(grad01, d01);
    float dot11 = dot(grad11, d11);
    return mix(mix(dot00, dot10, u.x), mix(dot01, dot11, u.x), u.y) * 0.5 + 0.5;
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / iResolution;
    float time = progress;
    const float riseFrequency = 15.0;
    const float riseAmplitude = 0.5;
    const float riseOffset = 0.5;
    const float fixedNoiseSpeed = 0.4;
    float _4_freq = 1.0;
    float _5_value = 0.0;
    float _6_amp = 1.0;
    const int _7_turbulenceOctaves = 4;
    const float _8_turbulenceGain = 0.5;
    const float _9_turbulenceLacunarity = 2.0;
    for (int _10_i = 0;_10_i < _7_turbulenceOctaves; ++_10_i) {
        _5_value += abs(perlinNoise_hh2(((uv * noiseScale) * 2.0 + vec2(time * fixedNoiseSpeed, 0.0)) * _4_freq) * 2.0 - 1.0) * _6_amp;
        _4_freq *= _9_turbulenceLacunarity;
        _6_amp *= _8_turbulenceGain;
    }
    float turb = _5_value;
    float rise = sin(uv.y * riseFrequency + time) * riseAmplitude + riseOffset;
    vec2 distortion = vec2(perlinNoise_hh2((uv * noiseScale) * 3.0 + vec2((time * fixedNoiseSpeed) * 0.6, 0.0)) * 0.02 - 0.01, (turb * (1.0 - riseWeight) + rise * riseWeight) * 0.03);
    vec2 detailDistort = vec2(perlinNoise_hh2((uv * noiseScale) * 10.0 + vec2(0.0, (time * fixedNoiseSpeed) * 2.4)) * 0.005, perlinNoise_hh2((uv * noiseScale) * 8.0 + vec2((time * fixedNoiseSpeed) * 1.8, 0.0)) * 0.005);
    distortion += detailDistort;
    vec2 distortedUV = clamp(uv + distortion * intensity, vec2(0.001), vec2(0.999));
    vec4 finalColor = texture(image, (distortedUV * iResolution) / vec2(textureSize(image, 0)));
    FragColor = finalColor;
    return;
}
