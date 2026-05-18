
out vec4 FragColor;
uniform vec4 colorRed;
uniform vec4 colorGreen;
void main() {
    vec2 coords = gl_FragCoord.xy;
    bool _2_ok = true;
    _2_ok = _2_ok && colorGreen != colorRed;
    FragColor = _2_ok ? colorGreen : colorRed;
    return;
}
