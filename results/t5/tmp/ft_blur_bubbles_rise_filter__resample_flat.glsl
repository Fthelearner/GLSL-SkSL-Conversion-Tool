#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 srcResolution;
uniform vec2 dstResolution;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / max(dstResolution, vec2(1.0));
    FragColor = texture(image, (uv * srcResolution) / vec2(textureSize(image, 0)));
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
