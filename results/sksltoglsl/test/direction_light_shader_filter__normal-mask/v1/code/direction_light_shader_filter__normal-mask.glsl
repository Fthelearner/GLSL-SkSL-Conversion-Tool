#version 450 core
out vec4 FragColor;
uniform sampler2D mask;
uniform vec2 iResolution;
uniform float heightScale;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 offset = vec2(1.0, iResolution.y / iResolution.x);
    float h1_u = texture(mask, (fragCoord - vec2(offset.x, 0.0)) / iResolution).w;
    float h2_u = texture(mask, (fragCoord + vec2(offset.x, 0.0)) / iResolution).w;
    vec3 tangent_u = vec3(2.0, 0.0, (h2_u - h1_u) * heightScale);
    float h1_v = texture(mask, (fragCoord - vec2(0.0, offset.y)) / iResolution).w;
    float h2_v = texture(mask, (fragCoord + vec2(0.0, offset.y)) / iResolution).w;
    vec3 tangent_v = vec3(0.0, 2.0, (h2_v - h1_v) * heightScale);
    FragColor = vec4(normalize(cross(tangent_u, tangent_v)), 1.0);
    return;
}
