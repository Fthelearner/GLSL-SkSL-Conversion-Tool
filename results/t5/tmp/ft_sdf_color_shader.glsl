#version 450 core
out vec4 FragColor;
uniform sampler2D sdfShape;
uniform vec4 color;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float d = texture(sdfShape, (fragCoord) / vec2(textureSize(sdfShape, 0))).w * 2.0 - 1.0;
    float alpha = color.w * (1.0 - smoothstep(-1.0, 0.0, d));
    FragColor = vec4(color.xyz * alpha, alpha);
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
