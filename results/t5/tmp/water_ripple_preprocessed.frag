#version 450 core

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D image;

layout(std140, binding = 1) uniform Params {
    vec2 iResolution = vec2(0.0, 0.0);
    float progress = 0.0;
    float waveCount = 0.0;
    vec2 rippleCenter = vec2(0.0, 0.0);
} u;

const float BASIC_SLOPE = 0.5;
const float AMPLITUDE_SUPPRESS = 0.012;
const float WAVE_FREQUENCY = 31.0;
const float WAVE_PROPAGATION_RATIO = 2.0;
const float AMPLITUDE_SUPPRESS_AREA = 0.45;
const float DISTORT_INTENSITY = 0.15;

vec4 sampleImage(vec2 coord)
{
    return texture(image, coord / u.iResolution);
}

float calcWave(float distanceToWave)
{
    float preWave = smoothstep(0.0, -0.3, distanceToWave);
    float waveForm = (u.waveCount == 1.0) ?
        smoothstep(-0.4, -0.2, distanceToWave) * smoothstep(0.0, -0.2, distanceToWave) :
        (u.waveCount == 2.0) ?
        smoothstep(-0.6, -0.3, distanceToWave) * preWave :
        smoothstep(-0.9, -0.6, distanceToWave) * step(abs(distanceToWave + 0.45), 0.45) * preWave;
    return -sin(WAVE_FREQUENCY * distanceToWave) * waveForm;
}

float waveGradient(float propagatedDistance, float timeValue)
{
    float distanceToWave = propagatedDistance - WAVE_PROPAGATION_RATIO * timeValue;
    float delta = 1e-3;
    float d1 = distanceToWave - delta;
    float d2 = distanceToWave + delta;
    return (calcWave(d2) - calcWave(d1)) / (2.0 * delta);
}

void main()
{
    vec2 fragCoord = gl_FragCoord.xy;
    float shortEdge = min(u.iResolution.x, u.iResolution.y);
    vec2 uv = fragCoord / u.iResolution;
    vec2 uvHomogeneous = fragCoord / shortEdge;
    vec2 resolutionRatio = u.iResolution / shortEdge;

    float progressSlope = BASIC_SLOPE + 0.1 * u.waveCount;
    float timeValue = progressSlope * u.progress;

    vec2 waveCenter = u.rippleCenter * resolutionRatio;
    float propagatedDistance = distance(uvHomogeneous, waveCenter);
    vec2 waveVector = uvHomogeneous - waveCenter;
    float amplitudeDecay = (propagatedDistance < 1.0) ? pow(1.0 - propagatedDistance, 4.0) : 0.0;
    float amplitudeSuppress = smoothstep(0.0, AMPLITUDE_SUPPRESS_AREA, propagatedDistance);
    float intensity = waveGradient(propagatedDistance, timeValue) *
        amplitudeDecay * amplitudeSuppress * AMPLITUDE_SUPPRESS;

    vec2 normalizedWaveVector = length(waveVector) > 1e-5 ? normalize(waveVector) : vec2(0.0);
    vec2 circles = normalizedWaveVector * intensity;
    vec3 normal = vec3(circles, intensity);
    vec2 distortedCoord = (uv - DISTORT_INTENSITY * normal.xy) * u.iResolution;

    vec3 color = sampleImage(distortedCoord).rgb;
    color += 150.0 * pow(clamp(dot(normal, normalize(vec3(0.0, 4.0, -0.5))), 0.0, 1.0), 2.5);
    outColor = vec4(color, 1.0);
}
