
out vec4 sk_FragColor;
uniform vec3 iResolution;
uniform float iTime;
uniform vec3 iChannelResolution[4];
void main() {
    vec4 fragColor;
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;
    vec3 ro = vec3(0.0, 3.0, iTime);
    vec3 rd = normalize(vec3(uv.x, uv.y - 0.5, 1.0));
    float t = 0.0;
    vec3 p = vec3(0.0);
    for (int i = 0;i < 100; i++) {
        p = ro + rd * t;
        vec2 _0_offset = vec2(sin(p.z + cos(p.x)), cos(p.x + sin(p.z)));
        float h = p.y - (sin(p.x + _0_offset.x) + cos(p.z + _0_offset.y)) * 0.5;
        if (h < 0.001 || t > 20.0) {
            break;
        }
        t += h * 0.5;
    }
    vec3 col = vec3(1.0);
    if (t < 20.0) {
        float height = p.y;
        float lines = fract(height * 10.0);
        float thickness = fwidth(height * 10.0) * 1.5;
        float lineMask = smoothstep(thickness, 0.0, lines);
        col = mix(vec3(1.0), vec3(0.0), vec3(lineMask));
        col = mix(col, vec3(1.0), vec3(1.0 - exp(-0.05 * t)));
    }
    fragColor = vec4(col, 1.0);
    sk_FragColor = vec4(fragColor.xyz, 1.0);
}
