#version 450 core
out vec4 FragColor;
uniform float r;
uniform sampler2D imageShader;
uniform sampler2D gradientShader;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 coord = fragCoord;
    float radius = r * texture(gradientShader, (coord) / vec2(textureSize(gradientShader, 0))).w;
    if (radius < 1.0) {
        FragColor = texture(imageShader, (coord) / vec2(textureSize(imageShader, 0)));
        return;
    }
    radius = clamp(radius, 1.0, r);
    vec4 _0_sum = vec4(0.0);
    float _1_div = 0.0;
    for (float _2_x = -30.0;_2_x < 30.0; _2_x += 1.0) {
        if (_2_x > radius) {
            break;
        }
        if (abs(_2_x) < radius) {
            float _3_res = texture(gradientShader, (coord + vec2(_2_x, 0.0)) / vec2(textureSize(gradientShader, 0))).w;
            if (_3_res > 0.0) {
                _1_div += 1.0;
                _0_sum += texture(imageShader, (coord + vec2(_2_x, 0.0)) / vec2(textureSize(imageShader, 0)));
            }
        }
    }
    FragColor = vec4(_0_sum.xyz / _1_div, 1.0);
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
