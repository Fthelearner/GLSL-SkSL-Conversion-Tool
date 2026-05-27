#version 450 core
out vec4 FragColor;
uniform sampler2D imageBlur0;
uniform sampler2D imageBlur1;
uniform sampler2D imageBlur2;
uniform sampler2D imageBlur3;
uniform sampler2D imageBlur4;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec3 composited_color = texture(imageBlur0, (fragCoord) / vec2(textureSize(imageBlur0, 0))).xyz;
    float weight = 0.25;
    composited_color += weight * (((texture(imageBlur1, (fragCoord) / vec2(textureSize(imageBlur1, 0))).xyz + texture(imageBlur2, (fragCoord) / vec2(textureSize(imageBlur2, 0))).xyz) + texture(imageBlur3, (fragCoord) / vec2(textureSize(imageBlur3, 0))).xyz) + texture(imageBlur4, (fragCoord) / vec2(textureSize(imageBlur4, 0))).xyz);
    FragColor = vec4(composited_color, 1.0);
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
