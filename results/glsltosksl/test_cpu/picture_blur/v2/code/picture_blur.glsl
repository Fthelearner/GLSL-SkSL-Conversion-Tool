#version 450 core
out vec4 FragColor;
uniform vec3 iResolution;
uniform vec3 iChannelResolution[4];
uniform vec4 iMouse;
uniform sampler2D iChannel0;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 _skOut;
    vec2 uv = fragCoord / iResolution.xy;
    vec2 mouse = iMouse.xy;
    if (length(mouse) < 1.0) {
        mouse = iResolution.xy * 0.5;
    }
    vec2 m2 = uv - mouse / iResolution.xy;
    float roundedBox = pow(abs((m2.x * iResolution.x) / iResolution.y), 8.0) + pow(abs(m2.y), 8.0);
    float rb1 = clamp((1.0 - roundedBox * 10000.0) * 8.0, 0.0, 1.0);
    float rb2 = clamp((0.95 - roundedBox * 9500.0) * 16.0, 0.0, 1.0) - clamp(pow(0.9 - roundedBox * 9500.0, 1.0) * 16.0, 0.0, 1.0);
    float rb3 = clamp((1.5 - roundedBox * 11000.0) * 2.0, 0.0, 1.0) - clamp(pow(1.0 - roundedBox * 11000.0, 1.0) * 2.0, 0.0, 1.0);
    _skOut = vec4(0.0);
    float transition = smoothstep(0.0, 1.0, rb1 + rb2);
    if (transition > 0.0) {
        vec2 lens = (uv - 0.5) * (1.0 - roundedBox * 5000.0) + 0.5;
        float total = 0.0;
        for (float x = -4.0;x <= 4.0; x++) {
            for (float y = -4.0;y <= 4.0; y++) {
                vec2 offset = (vec2(x, y) * 0.5) / iResolution.xy;
                _skOut += texture(iChannel0, (iResolution.xy * (offset + lens)) / vec2(textureSize(iChannel0, 0)));
                total += 1.0;
            }
        }
        _skOut /= total;
        float gradient = clamp((clamp(m2.y, 0.0, 0.2) + 0.1) * 0.5, 0.0, 1.0) + clamp((clamp(-m2.y, -1000.0, 0.2) * rb3 + 0.1) * 0.5, 0.0, 1.0);
        vec4 lighting = clamp((_skOut + vec4(rb1) * gradient) + vec4(rb2) * 0.3, vec4(0.0), vec4(1.0));
        _skOut = mix(texture(iChannel0, (iResolution.xy * uv) / vec2(textureSize(iChannel0, 0))), lighting, vec4(transition));
    } else {
        _skOut = texture(iChannel0, (iResolution.xy * uv) / vec2(textureSize(iChannel0, 0)));
    }
    FragColor = _skOut;
    return;
}
