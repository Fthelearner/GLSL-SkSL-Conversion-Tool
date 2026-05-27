#version 450 core
out vec4 FragColor;
uniform sampler2D imageInput;
uniform vec2 in_blurOffset;
void main() {
    vec2 xy = gl_FragCoord.xy;
    vec4 c = texture(imageInput, (vec2(in_blurOffset.x + xy.x, in_blurOffset.y + xy.y)) / vec2(textureSize(imageInput, 0)));
    c += texture(imageInput, (vec2(-in_blurOffset.y + xy.x, in_blurOffset.x + xy.y)) / vec2(textureSize(imageInput, 0)));
    c += texture(imageInput, (vec2(-in_blurOffset.x + xy.x, -in_blurOffset.y + xy.y)) / vec2(textureSize(imageInput, 0)));
    c += texture(imageInput, (vec2(in_blurOffset.y + xy.x, -in_blurOffset.x + xy.y)) / vec2(textureSize(imageInput, 0)));
    FragColor = c * 0.25;
    return;
}
