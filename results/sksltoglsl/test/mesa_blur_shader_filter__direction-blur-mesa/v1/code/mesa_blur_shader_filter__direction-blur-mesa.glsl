#version 450 core
out vec4 FragColor;
uniform sampler2D imageInput;
uniform vec2 in_blurOffset;
uniform vec2 in_dir;
void main() {
    vec2 xy = gl_FragCoord.xy;
    vec4 c = texture(imageInput, (vec2(in_blurOffset.x * in_dir.x + xy.x, in_blurOffset.y * in_dir.y + xy.y)) / vec2(textureSize(imageInput, 0)));
    c += texture(imageInput, (vec2(-in_blurOffset.x * in_dir.x + xy.x, -in_blurOffset.y * in_dir.y + xy.y)) / vec2(textureSize(imageInput, 0)));
    FragColor = c * 0.5;
    return;
}
