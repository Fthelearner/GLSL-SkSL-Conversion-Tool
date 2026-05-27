#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform float blurDirection;
uniform float sigma;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 blur_direct = vec2(1.0, 0.0);
    if (blurDirection > 0.5) {
        blur_direct = vec2(0.0, 1.0);
    }
    vec2 newFragCoord = vec2(0.0);
    vec3 dest = vec3(0.0);
    float w_sum = 0.0;
    for (int i = -2;i <= 2; i++) {
        float w = exp(-(float(i) * float(i)) / ((2.0 * sigma) * sigma)) / (sigma * 2.50662827);
        newFragCoord = fragCoord + float(i) * blur_direct;
        dest += w * texture(image, (newFragCoord) / vec2(textureSize(image, 0))).xyz;
        w_sum += w;
    }
    dest /= w_sum;
    FragColor = vec4(dest, 1.0);
    return;
}
