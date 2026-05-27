#version 450 core
out vec4 FragColor;
uniform float low;
uniform float high;
uniform float threshold;
uniform float opacity;
uniform float saturation;
uniform sampler2D imageShader;
const vec3 toLuminance = vec3(0.3086, 0.6094, 0.082);
void main() {
    vec2 coord = gl_FragCoord.xy;
    vec3 c = texture(imageShader, (coord) / vec2(textureSize(imageShader, 0))).xyz;
    float gray = (0.299 * c.x + 0.587 * c.y) + 0.114 * c.z;
    float bin = mix(high, low, step(threshold, gray));
    float luminance = dot(c, toLuminance);
    vec3 satAdjust = mix(vec3(luminance), c, vec3(saturation));
    vec3 res = (satAdjust - (opacity + 1.0) * gray) + bin;
    FragColor = vec4(mix(c, res, vec3(0.42857)), 1.0);
    return;
}
