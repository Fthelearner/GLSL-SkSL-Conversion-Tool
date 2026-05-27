#version 450 core
out vec4 FragColor;
uniform sampler2D srcImageShader;
uniform sampler2D blurImageShader;
uniform sampler2D gradientShader;
void main() {
    vec2 coord = gl_FragCoord.xy;
    vec3 srcColor = texture(srcImageShader, (coord) / vec2(textureSize(srcImageShader, 0))).xyz;
    vec3 blurColor = texture(blurImageShader, (coord) / vec2(textureSize(blurImageShader, 0))).xyz;
    float gradient = texture(gradientShader, (coord) / vec2(textureSize(gradientShader, 0))).w;
    vec3 color = blurColor * gradient + srcColor * (1.0 - gradient);
    FragColor = vec4(color, 1.0);
    return;
}
