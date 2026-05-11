#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform sampler2D displacementMap;
uniform vec2 iResolution;
uniform vec2 factor;
uniform float strength;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / iResolution;
    vec4 displacement = texture(displacementMap, (fragCoord) / iResolution);
    displacement.rgb *= displacement.a;
    displacement.rgb *= displacement.a;
    if (displacement.w <= 0.0) {
        FragColor = texture(image, (fragCoord) / iResolution);
        return;
    }
    vec2 direction = 2.0 * (displacement.xy - 0.5);
    vec2 normal = (direction * factor) * strength;
    vec2 refracted_uv = clamp(uv - normal * 0.05, vec2(0.001), vec2(0.999));
    FragColor = texture(image, refracted_uv);
    return;
}
