#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 iResolution;
uniform float progress;
uniform float waveCount;
uniform vec2 rippleCenter;
const float basicSlope = 0.5;
const float gAmplSupress = 0.012;
const float waveFreq = 31.0;
const float wavePropRatio = 2.0;
const float ampSupArea = 0.45;
const float intensity = 0.15;
float calcWave_hh(float dis) {
    float preWave = smoothstep(0.0, -0.3, dis);
    float waveForm = waveCount == 1.0 ? smoothstep(-0.4, -0.2, dis) * smoothstep(0.0, -0.2, dis) : (waveCount == 2.0 ? smoothstep(-0.6, -0.3, dis) * preWave : (smoothstep(-0.9, -0.6, dis) * step(abs(dis + 0.45), 0.45)) * preWave);
    return -sin(waveFreq * dis) * waveForm;
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float shortEdge = min(iResolution.x, iResolution.y);
    vec2 uv = fragCoord / iResolution;
    vec2 uvHomo = fragCoord / shortEdge;
    vec2 resRatio = iResolution / shortEdge;
    float progSlope = basicSlope + 0.1 * waveCount;
    float t = progSlope * progress;
    vec2 waveCenter = rippleCenter * resRatio;
    float propDis = distance(uvHomo, waveCenter);
    vec2 v = uvHomo - waveCenter;
    float ampDecayByT = propDis < 1.0 ? pow(1.0 - propDis, 4.0) : 0.0;
    float ampSupByDis = smoothstep(0.0, ampSupArea, propDis);
    float _0_dis = propDis - wavePropRatio * t;
    const float _1_h = 0.001;
    float _2_d1 = _0_dis - _1_h;
    float _3_d2 = _0_dis + _1_h;
    float hIntense = ((((calcWave_hh(_3_d2) - calcWave_hh(_2_d1)) * 499.999969) * ampDecayByT) * ampSupByDis) * gAmplSupress;
    vec2 circles = normalize(v) * hIntense;
    vec3 norm = vec3(circles, hIntense);
    vec2 expandUV = (uv - intensity * norm.xy) * iResolution;
    vec3 color = texture(image, (expandUV) / vec2(textureSize(image, 0))).xyz;
    color += 150.0 * pow(clamp(dot(norm, vec3(0.0, 0.992277861, -0.124034733)), 0.0, 1.0), 2.5);
    FragColor = vec4(color, 1.0);
    return;
}
