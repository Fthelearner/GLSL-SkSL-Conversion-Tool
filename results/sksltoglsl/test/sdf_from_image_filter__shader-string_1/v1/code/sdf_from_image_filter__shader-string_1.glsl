#version 450 core
out vec4 FragColor;
uniform sampler2D imageInput;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 O = vec4(0.0);
    vec4 imageSample = texture(imageInput, (fragCoord) / vec2(textureSize(imageInput, 0)));
    vec2 xy = imageSample.xy * 2.0 - 1.0;
    vec2 zw = imageSample.zw * 2.0 - 1.0;
    O.w = length(xy) - length(zw);
    O.w = (O.w + 1.0) * 0.5;
    O.w = clamp(O.w, 0.0, 1.0);
    FragColor = O;
    return;
}
