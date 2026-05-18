
out vec4 FragColor;
uniform vec4 colorRed;
uniform vec4 colorGreen;
void main() {
    vec2 coords = gl_FragCoord.xy;
    const bool _4_ok = true;
    FragColor = _4_ok ? colorGreen : colorRed;
    return;
}
