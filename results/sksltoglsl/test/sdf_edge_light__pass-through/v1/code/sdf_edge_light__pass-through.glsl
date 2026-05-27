#version 450 core
out vec4 FragColor;
uniform sampler2D inputShader;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float sd = texture(inputShader, (fragCoord) / vec2(textureSize(inputShader, 0))).w;
    FragColor = vec4(sd, sd, sd, 1.0);
    return;
}
