#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 iResolution;
uniform vec3 colorA;
uniform vec3 colorB;
uniform vec3 colorC;
uniform float colorProgress;
uniform float soundIntensity;
uniform float shockWaveAlphaA;
uniform float shockWaveAlphaB;
uniform float shockWaveProgressA;
uniform float shockWaveProgressB;
uniform float shockWaveTotalAlpha;
const float circleRadius = 0.125;
vec3 colorWheel_h3h2h(vec2 uv, float animationTime) {
    float mask = length(uv) * 0.470588237;
    if (mask >= 1.0) {
        return vec3(0.0);
    }
    float distanceFromCenter = fract(mask - animationTime);
    vec3 color = distanceFromCenter < 0.2 ? mix(colorA, colorB, vec3(smoothstep(0.0, 0.2, distanceFromCenter))) : (distanceFromCenter < 0.6 ? mix(colorB, colorC, vec3(smoothstep(0.2, 0.6, distanceFromCenter))) : mix(colorC, colorA, vec3(smoothstep(0.6, 1.0, distanceFromCenter))));
    return color;
}
vec4 soundWaveDistortionEffects_h4h2h2h(vec2 screenUVs, vec2 centeredUVs, float animationTime) {
    vec2 lightPulseUVs = centeredUVs + vec2(0.0, 0.1);
    float frequency = fract(animationTime);
    float radius = mix(0.14, 0.52, frequency);
    float lightPulseDistance = length(lightPulseUVs) - radius;
    float lightPulseThickness = 0.12;
    float lightPulse = smoothstep(lightPulseThickness, -0.025, abs(lightPulseDistance));
    if (lightPulse > 0.0) {
        float animationMask = smoothstep(1.0, 0.4, frequency);
        vec2 directionVector = normalize(lightPulseUVs);
        vec2 normal = ((directionVector * lightPulseDistance) * lightPulse) * animationMask;
        vec2 refractedUVs = clamp(mix(screenUVs, screenUVs - normal * 0.25, vec2(0.3)), vec2(0.001), vec2(0.999));
        return vec4(refractedUVs, max(normal.y, 0.0), pow(lightPulse, 6.0) * 0.3);
    }
    return vec4(screenUVs, 0.0, 0.0);
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / iResolution;
    uv.y = 1.0 - uv.y;
    const float screenHeight = 0.25;
    float screenRatio = (screenHeight * iResolution.x) / iResolution.y;
    vec2 centeredUVs = (uv + uv) - vec2(1.0, 0.0);
    centeredUVs.y *= screenHeight;
    centeredUVs.x *= screenRatio;
    float additionalColorStrength = 0.0;
    if (shockWaveTotalAlpha > 0.0) {
        vec4 soundWaveDistortionA = soundWaveDistortionEffects_h4h2h2h(uv, centeredUVs, shockWaveProgressA);
        vec4 soundWaveDistortionB = soundWaveDistortionEffects_h4h2h2h(soundWaveDistortionA.xy, centeredUVs, shockWaveProgressB);
        uv = soundWaveDistortionB.xy;
        float AlphaA = shockWaveAlphaA * shockWaveTotalAlpha;
        float AlphaB = shockWaveAlphaB * shockWaveTotalAlpha;
        additionalColorStrength += (soundWaveDistortionA.z + soundWaveDistortionA.w) * AlphaA;
        additionalColorStrength += (soundWaveDistortionB.z + soundWaveDistortionB.w) * AlphaB;
    }
    if (centeredUVs.y < 0.214 * soundIntensity + 0.0776) {
        vec2 _2_centeredUVs = centeredUVs;
        float _3_circleHeight = mix(-0.4, 0.03, soundIntensity);
        float _4_smoothUnionThreshold = mix(0.0657, 0.09, soundIntensity);
        vec2 _5_circlePosition = vec2(0.0, _3_circleHeight);
        float _6_barPosition = mix(0.09, 0.0, soundIntensity);
        float _7_circleSDF = length(_2_centeredUVs - _5_circlePosition) - circleRadius;
        _2_centeredUVs.y += _6_barPosition;
        _7_circleSDF += _4_smoothUnionThreshold;
        float _8_k6 = _4_smoothUnionThreshold * 6.0;
        float _9_h = max(_8_k6 - abs(_7_circleSDF - _2_centeredUVs.y), 0.0) / _8_k6;
        float _10_smoothUnionDistance = min(_7_circleSDF, _2_centeredUVs.y) - ((_9_h * _9_h) * _9_h) * _4_smoothUnionThreshold;
        float _11_horizontalGradient = smoothstep(0.75, 0.0, abs(_2_centeredUVs.x));
        float _12_smoothGap = mix(0.08, 0.1085, _11_horizontalGradient);
        float _13_smoothUnion = smoothstep(_12_smoothGap, -0.035, mix(0.0, 0.66, _10_smoothUnionDistance));
        float _14_verticalGradient = _2_centeredUVs.y - _6_barPosition;
        float _15_verticalGap = max(_4_smoothUnionThreshold - _6_barPosition, 0.0001);
        _14_verticalGradient = 1.0 - min(_15_verticalGap, _14_verticalGradient) / _15_verticalGap;
        float _16_gradient = mix(1.0, _11_horizontalGradient, 1.0 - _14_verticalGradient) * _11_horizontalGradient;
        _13_smoothUnion *= _16_gradient;
        additionalColorStrength += _13_smoothUnion;
    }
    vec3 finalColor = texture(image, (vec2(uv.x, 1.0 - uv.y) * iResolution) / vec2(textureSize(image, 0))).xyz;
    vec3 centerColor = additionalColorStrength > 0.0 ? colorWheel_h3h2h(centeredUVs, colorProgress) : vec3(0.0);
    finalColor += centerColor * additionalColorStrength;
    FragColor = vec4(finalColor, 1.0);
    return;
}
