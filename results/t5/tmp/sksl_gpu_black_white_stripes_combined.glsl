
out vec4 sk_FragColor;
uniform vec3 iResolution;
uniform float iTime;
uniform vec4 iMouse;
float GetDist_ff3(vec3 p) {
    p.xz *= mat2(cos(0.5 * iTime), -sin(0.5 * iTime), sin(0.5 * iTime), cos(0.5 * iTime));
    p.x -= 1.0;
    p.xz *= mat2(cos(-1.25 * iTime), -sin(-1.25 * iTime), sin(-1.25 * iTime), cos(-1.25 * iTime));
    float r1 = 0.5;
    float r2 = 0.2;
    float td1 = length(p.xy) - r1;
    float td2 = length(vec2(td1, p.z)) - r2;
    float sd1 = td2;
    p.xz *= mat2(cos(1.25 * iTime), -sin(1.25 * iTime), sin(1.25 * iTime), cos(1.25 * iTime));
    p.x += 1.0;
    float sd2 = length(p + vec3(1.0, 0.25 * sin(iTime), 0.0)) - 0.5;
    float d = (p.y + 1.0) - 0.001 * dot(p.xz, p.xz);
    d = min(d, sd1);
    d = min(d, sd2);
    sk_FragColor = d;
}
float RayMarch_ff3f3f(vec3 ro, vec3 rd, float z) {
    float dO = 0.0;
    float s = sign(z);
    for (int i = 0;i < 200; i++) {
        vec3 p = ro + rd * dO;
        float dS = GetDist_ff3(p);
        if (s != sign(dS)) {
            z *= 0.5;
            s = sign(dS);
        }
        if (abs(dS) < 0.001 || dO > 100.0) {
            break;
        }
        dO += dS * z;
    }
    sk_FragColor = min(dO, 100.0);
}
vec3 GetNormal_f3f3(vec3 p) {
    float d = GetDist_ff3(p);
    vec2 e = vec2(0.001, 0.0);
    vec3 n = d - vec3(GetDist_ff3(p - e.xyy), GetDist_ff3(p - e.yxy), GetDist_ff3(p - e.yyx));
    sk_FragColor = normalize(n);
}
void main() {
    vec4 fragColor;
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;
    const float _1_r = 4.0;
    float _2_a = 0.15 * iTime;
    vec3 _3_ro = vec3(_1_r * cos(_2_a), 1.0 + cos(0.4 * iTime), _1_r * sin(_2_a));
    vec3 ro = _3_ro;
    vec3 _4_f = normalize(-ro);
    vec3 _5_r = normalize(cross(vec3(0.0, 1.0, 0.0), _4_f));
    vec3 _6_u = cross(_4_f, _5_r);
    vec3 _7_c = _4_f;
    vec3 _8_i = (_7_c + uv.x * _5_r) + uv.y * _6_u;
    vec3 _9_d = normalize(_8_i);
    vec3 rd = _9_d;
    vec3 col = vec3(0.0);
    float d = RayMarch_ff3f3f(ro, rd, 1.0);
    vec3 p = ro + rd * d;
    float IOR = 1.5;
    if (d < 100.0) {
        vec3 n = GetNormal_f3f3(p);
        vec3 r = reflect(rd, n);
        if (p.y > -0.9) {
            r = reflect(rd, (n * 50.0) * exp(-3.0 * length(p.xz)));
        }
        vec3 pIn = p - 0.004 * n;
        vec3 rdIn = refract(rd, n, 1.0 / IOR);
        float dIn = RayMarch_ff3f3f(pIn, rdIn, -1.0);
        vec3 pExit = pIn + dIn * rdIn;
        -GetNormal_f3f3(pExit);
        vec3 p2 = p + 0.004 * n;
        vec3 cr = normalize(p - pExit);
        float d2 = RayMarch_ff3f3f(p2, cr, 1.0);
        vec3 p3 = p2 + d2 * cr;
        vec3 n2 = GetNormal_f3f3(p3);
        float dif = dot(n, normalize(vec3(0.5 * cos(iTime), 1.0, 0.5 * sin(iTime)))) * 0.5 + 0.5;
        float dif2 = dot(n2, vec3(0.267261237, 0.5345225, 0.801783741)) * 0.5 + 0.5;
        col = vec3(1.0);
        col *= dif;
        col *= 0.5 + (0.5 * tanh(4.0 * cos((2.0 * iTime - 0.5 * length(p.xz)) + 50.0 * abs(dif)))) * 1.00067115;
        col *= exp(-0.5 * length(p));
        col *= 2.0 * dif2;
        float fres = pow(1.0 + dot(rd, n), 5.0);
        float csh = 1.0 / cosh(0.2 * length(p.xz));
        col = clamp(col, vec3(0.0), vec3(1.0));
        col = mix(col, abs(r) * csh, vec3(fres));
        if (p.y < -0.9) {
            col += ((0.6 * tanh(4.0 * cos(iTime + 6.0 * log(length(p.xz))))) * 1.00067115) * csh;
        }
    }
    col = pow(col, vec3(0.4545));
    fragColor = vec4(col, 1.0);
    sk_FragColor = fragColor;
}
