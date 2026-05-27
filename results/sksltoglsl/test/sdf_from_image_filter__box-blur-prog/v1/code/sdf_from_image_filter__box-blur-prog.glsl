#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform float iScale;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float h = iScale * 0.5;
    float s1 = texture(image, (fragCoord + vec2(h)) / vec2(textureSize(image, 0))).w;
    float s2 = texture(image, (fragCoord - vec2(h)) / vec2(textureSize(image, 0))).w;
    float s3 = texture(image, (fragCoord + vec2(h, -h)) / vec2(textureSize(image, 0))).w;
    float s4 = texture(image, (fragCoord - vec2(h, -h)) / vec2(textureSize(image, 0))).w;
    FragColor = vec4(vec3(0.0), (((s1 + s2) + s3) + s4) * 0.25);
    return;
}
