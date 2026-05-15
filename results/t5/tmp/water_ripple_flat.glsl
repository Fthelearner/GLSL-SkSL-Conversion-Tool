#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 iResolution;
uniform float progress;
uniform float waveCount;
uniform vec2 rippleCenter;
float calcWave_ff(float distanceToWave) {
    float preWave = smoothstep(0.0, -0.3, distanceToWave);
    float waveForm = waveCount == 1.0 ? smoothstep(-0.4, -0.2, distanceToWave) * smoothstep(0.0, -0.2, distanceToWave) : (waveCount == 2.0 ? smoothstep(-0.6, -0.3, distanceToWave) * preWave : (smoothstep(-0.9, -0.6, distanceToWave) * step(abs(distanceToWave + 0.45), 0.45)) * preWave);
    return -sin(31.0 * distanceToWave) * waveForm;
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 outColor;
    float shortEdge = min(iResolution.x, iResolution.y);
    vec2 uv = fragCoord / iResolution;
    vec2 uvHomogeneous = fragCoord / shortEdge;
    vec2 resolutionRatio = iResolution / shortEdge;
    float progressSlope = 0.5 + 0.1 * waveCount;
    float timeValue = progressSlope * progress;
    vec2 waveCenter = rippleCenter * resolutionRatio;
    float propagatedDistance = distance(uvHomogeneous, waveCenter);
    vec2 waveVector = uvHomogeneous - waveCenter;
    float amplitudeDecay = propagatedDistance < 1.0 ? pow(1.0 - propagatedDistance, 4.0) : 0.0;
    float amplitudeSuppress = smoothstep(0.0, 0.45, propagatedDistance);
    float _0_distanceToWave = propagatedDistance - 2.0 * timeValue;
    const float _1_delta = 0.001;
    float _2_d1 = _0_distanceToWave - _1_delta;
    float _3_d2 = _0_distanceToWave + _1_delta;
    float intensity = ((((calcWave_ff(_3_d2) - calcWave_ff(_2_d1)) * 499.999969) * amplitudeDecay) * amplitudeSuppress) * 0.012;
    vec2 normalizedWaveVector = length(waveVector) > 1e-05 ? normalize(waveVector) : vec2(0.0);
    vec2 circles = normalizedWaveVector * intensity;
    vec3 normal = vec3(circles, intensity);
    vec2 distortedCoord = (uv - 0.15 * normal.xy) * iResolution;
    vec3 color = texture(image, (distortedCoord) / iResolution).xyz;
    color += 150.0 * pow(clamp(dot(normal, vec3(0.0, 0.992277861, -0.124034733)), 0.0, 1.0), 2.5);
    outColor = vec4(color, 1.0);
    FragColor = vec4(outColor.xyz, 1.0);
    return;
}
