#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 iResolution;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 _skOut = texture(image, (fragCoord) / iResolution);
    FragColor = _skOut;
    return;
}
