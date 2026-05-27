#version 450 core
out vec4 FragColor;
uniform sampler2D imageInput;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 xy = fragCoord;
    FragColor = texture(imageInput, (xy) / vec2(textureSize(imageInput, 0)));
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
