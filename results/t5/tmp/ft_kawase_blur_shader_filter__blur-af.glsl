#version 450 core
out vec4 FragColor;
uniform sampler2D imageInput;
uniform vec2 in_blurOffset0;
uniform vec2 in_blurOffset1;
uniform vec2 in_blurOffset2;
uniform vec2 in_blurOffset3;
uniform vec2 in_blurOffset4;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 xy = fragCoord;
    vec4 c = vec4(0.0);
    c += texture(imageInput, (vec2(xy.x + in_blurOffset0.x, xy.y + in_blurOffset0.y)) / vec2(textureSize(imageInput, 0)));
    c += texture(imageInput, (vec2(xy.x + in_blurOffset1.x, xy.y + in_blurOffset1.y)) / vec2(textureSize(imageInput, 0)));
    c += texture(imageInput, (vec2(xy.x + in_blurOffset2.x, xy.y + in_blurOffset2.y)) / vec2(textureSize(imageInput, 0)));
    c += texture(imageInput, (vec2(xy.x + in_blurOffset3.x, xy.y + in_blurOffset3.y)) / vec2(textureSize(imageInput, 0)));
    c += texture(imageInput, (vec2(xy.x + in_blurOffset4.x, xy.y + in_blurOffset4.y)) / vec2(textureSize(imageInput, 0)));
    FragColor = c * 0.2;
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
