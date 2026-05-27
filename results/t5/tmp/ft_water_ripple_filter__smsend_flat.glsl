#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 iResolution;
uniform float progress;
uniform float waveCount;
uniform vec2 rippleCenter;
float calcWave_hh_ff(float dis) {
    float preWave = smoothstep(0.0, -0.3, dis);
    float waveForm = waveCount == 1.0 ? smoothstep(-0.4, -0.2, dis) * smoothstep(0.0, -0.2, dis) : (waveCount == 2.0 ? smoothstep(-0.6, -0.3, dis) * preWave : (smoothstep(-0.9, -0.6, dis) * step(abs(dis + 0.45), 0.45)) * preWave);
    return -sin(31.0 * dis) * waveForm;
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float shortEdge = min(iResolution.x, iResolution.y);
    vec2 uv = fragCoord / iResolution;
    vec2 uvHomo = fragCoord / shortEdge;
    vec2 resRatio = iResolution / shortEdge;
    float progSlope = 0.5 + 0.1 * waveCount;
    float t = progSlope * progress;
    vec2 waveCenter = rippleCenter * resRatio;
    float propDis = distance(uvHomo, waveCenter);
    vec2 v = uvHomo - waveCenter;
    float ampDecayByT = propDis < 1.0 ? pow(1.0 - propDis, 4.0) : 0.0;
    float ampSupByDis = smoothstep(0.0, 0.45, propDis);
    float _0_dis = propDis - 2.0 * t;
    float _2_d1 = _0_dis - 0.001;
    float _3_d2 = _0_dis + 0.001;
    float hIntense = ((((calcWave_hh_ff(_3_d2) - calcWave_hh_ff(_2_d1)) * 499.999969) * ampDecayByT) * ampSupByDis) * 0.012;
    vec2 circles = normalize(v) * hIntense;
    vec3 norm = vec3(circles, hIntense);
    vec2 expandUV = (uv - 0.15 * norm.xy) * iResolution;
    vec3 color = texture(image, (expandUV) / iResolution).xyz;
    color += 150.0 * pow(clamp(dot(norm, vec3(0.0, 0.992277861, -0.124034733)), 0.0, 1.0), 2.5);
    FragColor = vec4(color, 1.0);
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
