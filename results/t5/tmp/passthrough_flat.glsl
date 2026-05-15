#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 iResolution;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 outColor = texture(image, (fragCoord) / iResolution);
    outColor.rgb *= outColor.a;
    FragColor = vec4(outColor.xyz, 1.0);
    return;
}
