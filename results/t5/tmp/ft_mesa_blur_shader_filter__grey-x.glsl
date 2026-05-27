#version 450 core
out vec4 FragColor;
uniform sampler2D imageShader;
uniform float coefficient1;
uniform float coefficient2;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 drawing_coord = fragCoord;
    vec3 color = texture(imageShader, (drawing_coord) / vec2(textureSize(imageShader, 0))).xyz;
    float Y = ((0.299 * color.x + 0.587 * color.y) + 0.114 * color.z) * 255.0;
    float _7_rgb = Y;
    if (_7_rgb > 127.5) {
        _7_rgb = 255.0 - _7_rgb;
    }
    float _11_s1 = _7_rgb * 0.00469483575 - 0.131126747;
    float _12_s2 = sqrt(_11_s1 * _11_s1 + 0.0201414227);
    float _13_res = (pow(_11_s1 + _12_s2, 0.333333343) - pow(_12_s2 - _11_s1, 0.333333343)) - -0.291079819;
    float _14_t_r = min(_13_res, 1.0);
    float dY = Y < 127.5 ? coefficient1 * pow(1.0 - _14_t_r, 3.0) : (-coefficient2 * pow(1.0 - _14_t_r, 3.0)) * 0.003921569;
    FragColor = vec4(color + dY, 1.0);
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
