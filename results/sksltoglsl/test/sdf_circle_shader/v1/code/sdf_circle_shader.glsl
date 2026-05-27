#version 450 core
out vec4 FragColor;
uniform vec2 iResolution;
void main() {
    vec2 coord = gl_FragCoord.xy;
    vec2 center = vec2(iResolution.x * 0.5, iResolution.y * 0.45);
    float radius = min(iResolution.x, iResolution.y) * 0.25;
    float d = length(coord - center) - radius;
    FragColor = vec4(0.0, 0.0, 0.0, clamp(d, -1.0, 1.0) * 0.5 + 0.5);
    return;
}
