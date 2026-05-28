#version 450 core
out vec4 FragColor;
uniform vec3 iResolution;
uniform float iTime;
uniform vec3 iChannelResolution[4];
mat3 m = mat3(0.0, 0.8, 0.6, -0.8, 0.36, -0.48, -0.6, -0.48, 0.64);
float hash_ff(float n) {
    return fract(sin(n) * 43758.5469);
}
float noise_ff3(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = (f * f) * (3.0 - 2.0 * f);
    float n = (p.x + p.y * 57.0) + 113.0 * p.z;
    float res = mix(mix(mix(hash_ff(n), hash_ff(n + 1.0), f.x), mix(hash_ff(n + 57.0), hash_ff(n + 58.0), f.x), f.y), mix(mix(hash_ff(n + 113.0), hash_ff(n + 114.0), f.x), mix(hash_ff(n + 170.0), hash_ff(n + 171.0), f.x), f.y), f.z);
    return res;
}
float scene_ff3(vec3 pos) {
    vec3 _0_p = pos * 0.3;
    float _1_f = 0.5 * noise_ff3(_0_p);
    _0_p = (m * _0_p) * 2.02;
    _1_f += 0.25 * noise_ff3(_0_p);
    _0_p = (m * _0_p) * 2.03;
    _1_f += 0.125 * noise_ff3(_0_p);
    return (0.1 - length(pos) * 0.05) + _1_f;
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 _skOut;
    vec2 uv = (fragCoord * 2.0 - iResolution.xy) / min(iResolution.x, iResolution.y);
    vec2 mo = vec2(iTime * 0.1, cos(iTime * 0.25) * 3.0);
    float camDist = 25.0;
    vec3 ta = vec3(0.0, 1.0, 0.0);
    vec3 ro = camDist * normalize(vec3(cos(2.75 - 3.0 * mo.x), 0.7 - (mo.y - 1.0), sin(2.75 - 3.0 * mo.x)));
    float targetDepth = 1.3;
    vec3 _2_cw = normalize(ta - ro);
    const vec3 _3_cp = vec3(0.0, 1.0, 0.0);
    vec3 _4_cu = cross(_2_cw, _3_cp);
    vec3 _5_cv = cross(_4_cu, _2_cw);
    mat3 c = mat3(_4_cu, _5_cv, _2_cw);
    vec3 dir = c * normalize(vec3(uv, targetDepth));
    float zMax = 40.0;
    float zstep = zMax * 0.015625;
    vec3 p = ro;
    float T = 1.0;
    float absorption = 100.0;
    vec4 color = vec4(0.0);
    for (int i = 0;i < 64; i++) {
        float density = scene_ff3(p);
        if (density > 0.0) {
            float tmp = density * 0.015625;
            T *= 1.0 - tmp * absorption;
            if (T <= 0.01) {
                break;
            }
            float opaity = 50.0;
            float k = (opaity * tmp) * T;
            vec4 cloudColor = vec4(1.0);
            vec4 col1 = cloudColor * k;
            vec4 col2 = vec4(0.0);
            color += col1 + col2;
        }
        p += dir * zstep;
    }
    vec3 bg = mix(vec3(0.3, 0.1, 0.8), vec3(0.7, 0.7, 1.0), vec3(1.0 - (uv.y + 1.0) * 0.5));
    color.xyz += bg;
    _skOut = color;
    FragColor = _skOut;
    return;
}
