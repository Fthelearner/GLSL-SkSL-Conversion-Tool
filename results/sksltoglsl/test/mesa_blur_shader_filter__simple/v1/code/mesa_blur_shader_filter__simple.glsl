#version 450 core
out vec4 FragColor;
uniform sampler2D imageInput;
void main() {
    vec2 xy = gl_FragCoord.xy;
    FragColor = texture(imageInput, (xy) / vec2(textureSize(imageInput, 0)));
    return;
}
