#version 450 core
out vec4 FragColor;
uniform vec3 iResolution;
uniform float iTime;
uniform vec3 iChannelResolution[4];
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 _skOut;
    vec2 uv = (fragCoord * 2.0 - iResolution.xy) / iResolution.y;
    vec2 uv0 = uv;
    vec3 finalColor = vec3(0.0);
    for (float i = 0.0;i < 4.0; i++) {
        uv = fract(uv * 1.5) - 0.5;
        float d = length(uv) * exp(-length(uv0));
        const vec3 _0_a = vec3(0.5);
        const vec3 _1_b = vec3(0.5);
        const vec3 _3_d = vec3(0.263, 0.416, 0.557);
        vec3 col = _0_a + _1_b * cos(6.28318 * (vec3((length(uv0) + i * 0.4) + iTime * 0.4) + _3_d));
        d = sin(d * 8.0 + iTime) * 0.125;
        d = abs(d);
        d = pow(0.01 / d, 1.2);
        finalColor += col * d;
    }
    _skOut = vec4(finalColor, 1.0);
    FragColor = _skOut;
    return;
}
