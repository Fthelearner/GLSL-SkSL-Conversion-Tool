
out vec4 FragColor;
uniform vec4 colorRed;
uniform vec4 colorGreen;
bool do_side_effect_bb(out bool x) {
    x = true;
    return false;
}
void main() {
    vec2 coords = gl_FragCoord.xy;
    bool ok = true;
    vec4 green = colorGreen;
    vec4 red = colorRed;
    bool param = false;
    bool call = (do_side_effect_bb(param), true);
    FragColor = (ok && param) && call ? green : red;
    return;
}
