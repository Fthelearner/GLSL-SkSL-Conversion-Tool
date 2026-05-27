#version 450 core
out vec4 FragColor;
uniform sampler2D imageShader;
uniform float coefficient1;
uniform float coefficient2;
void main() {
    vec2 coord = gl_FragCoord.xy;
    vec3 color = vec3(texture(imageShader, (coord) / vec2(textureSize(imageShader, 0))).x, texture(imageShader, (coord) / vec2(textureSize(imageShader, 0))).y, texture(imageShader, (coord) / vec2(textureSize(imageShader, 0))).z);
    float Y = ((0.299 * color.x + 0.587 * color.y) + 0.114 * color.z) * 255.0;
    float U = ((-0.147 * color.x - 0.289 * color.y) + 0.436 * color.z) * 255.0;
    float V = ((0.615 * color.x - 0.515 * color.y) - 0.1 * color.z) * 255.0;
    float _16_rgb = Y;
    if (_16_rgb > 127.5) {
        _16_rgb = 255.0 - _16_rgb;
    }
    float _24_q = -_16_rgb * 0.0093896715 + 0.2622535;
    float _25_s1 = -(_24_q * 0.5);
    float _26_s2 = sqrt(_25_s1 * _25_s1 + 0.0201414227);
    float _27_x = _25_s1 + _26_s2;
    float _28_x = _25_s1 - _26_s2;
    float _29_res = ((_27_x < 0.0 ? -pow(-_27_x, 0.333333343) : pow(_27_x, 0.333333343)) + (_28_x < 0.0 ? -pow(-_28_x, 0.333333343) : pow(_28_x, 0.333333343))) - -0.291079819;
    float _30_t_r = min(_29_res, 1.0);
    Y = Y < 127.5 ? Y + coefficient1 * pow(1.0 - _30_t_r, 3.0) : Y - coefficient2 * pow(1.0 - _30_t_r, 3.0);
    color.x = (Y + 1.14 * V) * 0.003921569;
    color.y = ((Y - 0.39 * U) - 0.58 * V) * 0.003921569;
    color.z = (Y + 2.03 * U) * 0.003921569;
    FragColor = vec4(color, 1.0);
    return;
}
