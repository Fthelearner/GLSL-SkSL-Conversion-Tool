#version 450 core
out vec4 FragColor;
uniform sampler2D image;
void main() {
    vec2 coord = gl_FragCoord.xy;
    FragColor = texture(image, (coord) / vec2(textureSize(image, 0)));
    return;
}
