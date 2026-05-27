#version 450 core
out vec4 FragColor;
uniform sampler2D image;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    FragColor = texture(image, (fragCoord) / vec2(textureSize(image, 0)));
    return;
}
