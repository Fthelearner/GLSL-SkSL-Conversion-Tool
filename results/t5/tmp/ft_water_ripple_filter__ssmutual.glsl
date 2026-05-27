#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 iResolution;
uniform float progress;
uniform float waveCount;
uniform vec2 rippleCenter;
float s_wavePropRatio = 2.0;
float b_wavePropRatio = 6.9;
float calcWave_ffff_ffff(float count, float freq, float dis) {
    float axisVal = count == 1.0 ? 2.0 : (count == 2.0 ? 3.0 : 5.0);
    float axisPoint = (-axisVal * 3.1416) / freq;
    float waveForm = smoothstep(axisPoint * 2.0, axisPoint, dis) * smoothstep(0.0, axisPoint, dis);
    float downCond = count == 3.0 ? -1.0 : 1.0;
    return (sin(freq * dis) * waveForm) * downCond;
}
float calcBLight_fff_fff(float dis, float freq) {
    float currentX = dis + 6.2832 / freq;
    return 1.2 * exp((-55.0 * currentX) * currentX);
}
float calcSLight_ffff_ffff(float dis, float freq, float yShift) {
    float pivot1 = (dis + 9.4248 / freq) - 0.14;
    float pivot2 = (dis + 9.4248 / freq) + 0.01;
    return (2.0 * yShift) * (exp((-1000.0 * pivot2) * pivot2) + exp((-1000.0 * pivot1) * pivot1));
}
vec2 waveGenerator_f2ffffff_f2ffffff(float propDis, float t, float count, float freq, float prop, float yShift) {
    float dis = propDis - prop * t;
    float h = 0.001;
    float d1 = dis - h;
    float d2 = dis + h;
    float waveVal = (calcWave_ffff_ffff(count, freq, d2) - calcWave_ffff_ffff(count, freq, d1)) / (2.0 * h);
    float lightAdjust = freq < 10.0 ? calcBLight_fff_fff(dis, freq) : calcSLight_ffff_ffff(dis, freq, yShift);
    return vec2(waveVal, lightAdjust);
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float s_waveCount = waveCount;
    vec2 b_rippleCenter = rippleCenter;
    float shortEdge = min(iResolution.x, iResolution.y);
    vec2 uv = fragCoord / iResolution;
    vec2 uvHomo = fragCoord / shortEdge;
    vec2 resRatio = iResolution / shortEdge;
    float b_progSlope = 0.4;
    float s_progSlope = 0.5 + 0.1 * s_waveCount;
    float b_t = b_progSlope * (progress + 0.4);
    float s_t = s_progSlope * (progress + 0.11);
    float veloDecay = 1.0 - 0.04 * (smoothstep(0.2, 0.16, progress) + smoothstep(0.2, 1.2, progress));
    b_wavePropRatio *= veloDecay;
    s_wavePropRatio *= veloDecay;
    vec2 b_waveCenter = b_rippleCenter * resRatio;
    vec2 s_rippleCenter = vec2(0.5, 0.0);
    s_rippleCenter.x = b_rippleCenter.x == 0.5 ? 0.5 : floor(b_rippleCenter.x + 0.5);
    s_rippleCenter.y = b_rippleCenter.y == 0.5 ? 0.5 : floor(b_rippleCenter.y + 0.5);
    vec2 s_waveCenter = s_rippleCenter * resRatio;
    float b_propDis = distance(uvHomo, b_waveCenter);
    float s_propDis = distance(uvHomo, s_waveCenter);
    vec2 b_vec = uvHomo - b_waveCenter;
    vec2 s_vec = uvHomo - s_waveCenter;
    float b_ampDecayByDis = b_propDis < 1.9 ? clamp(pow(1.9 - b_propDis, 4.0), 0.0, 1.0) : 0.0;
    float s_ampDecayByDis = s_propDis < 0.7 ? clamp(pow(0.7 - s_propDis, 4.0), 0.0, 1.0) : 0.0;
    float s_ampSupCenter = smoothstep(0.0, 0.3, s_propDis);
    vec2 b_waveRes = waveGenerator_f2ffffff_f2ffffff(b_propDis, b_t, 1.0, 7.0, b_wavePropRatio, 1.0);
    vec2 s_waveRes = waveGenerator_f2ffffff_f2ffffff(s_propDis, s_t, s_waveCount, 31.0, s_wavePropRatio, abs(normalize(s_vec).y));
    float b_intense = (b_waveRes.x * b_ampDecayByDis) * 0.01;
    float s_intense = ((s_waveRes.x * s_ampDecayByDis) * s_ampSupCenter) * 0.04;
    float b_Prime = (b_waveRes.y * b_ampDecayByDis) * 0.01;
    float s_Prime = ((s_waveRes.y * s_ampDecayByDis) * s_ampSupCenter) * 0.04;
    vec2 b_circles = normalize(b_vec) * b_intense;
    vec2 s_circles = normalize(s_vec) * s_intense;
    vec3 b_norm = vec3(b_circles, b_intense);
    vec3 s_norm = vec3(s_circles, s_intense);
    vec2 warp = (0.15 * b_norm.xy + 0.15 * s_norm.xy) * smoothstep(0.0, 0.07, progress);
    vec2 expandUV = (uv - warp) * iResolution;
    vec3 color = texture(image, (expandUV) / iResolution).xyz;
    float b_light = (30.0 * clamp(b_Prime, 0.0, 1.0)) * smoothstep(0.0, 0.125, progress);
    float s_light = 60.0 * clamp(s_Prime, 0.0, 1.0);
    color += s_light;
    color = vec3(1.0) - (vec3(1.0) - color) * (vec3(1.0) - vec3(b_light));
    FragColor = vec4(color, 1.0);
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
