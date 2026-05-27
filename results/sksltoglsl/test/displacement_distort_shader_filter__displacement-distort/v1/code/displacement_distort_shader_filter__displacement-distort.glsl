#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform sampler2D maskEffect;
uniform vec2 iResolution;
uniform vec2 factor;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / iResolution;
    vec4 maskColor = texture(maskEffect, (fragCoord) / vec2(textureSize(maskEffect, 0)));
    vec4 finalColor = vec4(0.0);
    if (maskColor.w > 0.0) {
        vec2 directionVector = 2.0 * (maskColor.xy - 0.5);
        vec2 normal = (directionVector * factor) * 1.2;
        vec2 refractedUVs = clamp(uv - normal * 0.05, vec2(0.001), vec2(0.999));
        finalColor = texture(image, (iResolution * refractedUVs) / vec2(textureSize(image, 0)));
    } else {
        finalColor = texture(image, (fragCoord) / vec2(textureSize(image, 0)));
    }
    FragColor = finalColor;
    return;
}
