#version 450 core
out vec4 FragColor;
uniform sampler2D imageInput;
uniform vec2 in_blurOffset;
uniform vec2 in_maxSizeXY;
void main() {
    vec2 xy = gl_FragCoord.xy;
    vec4 c = texture(imageInput, (xy) / vec2(textureSize(imageInput, 0)));
    c += texture(imageInput, (vec2(clamp(in_blurOffset.x + xy.x, 0.0, in_maxSizeXY.x), clamp(in_blurOffset.y + xy.y, 0.0, in_maxSizeXY.y))) / vec2(textureSize(imageInput, 0)));
    c += texture(imageInput, (vec2(clamp(in_blurOffset.x + xy.x, 0.0, in_maxSizeXY.x), clamp(-in_blurOffset.y + xy.y, 0.0, in_maxSizeXY.y))) / vec2(textureSize(imageInput, 0)));
    c += texture(imageInput, (vec2(clamp(-in_blurOffset.x + xy.x, 0.0, in_maxSizeXY.x), clamp(in_blurOffset.y + xy.y, 0.0, in_maxSizeXY.y))) / vec2(textureSize(imageInput, 0)));
    c += texture(imageInput, (vec2(clamp(-in_blurOffset.x + xy.x, 0.0, in_maxSizeXY.x), clamp(-in_blurOffset.y + xy.y, 0.0, in_maxSizeXY.y))) / vec2(textureSize(imageInput, 0)));
    FragColor = c * 0.2;
    return;
}
