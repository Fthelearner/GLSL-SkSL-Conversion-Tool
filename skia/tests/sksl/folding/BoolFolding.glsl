
out vec4 FragColor;
uniform vec4 colorRed;
uniform vec4 colorGreen;
void main() {
    vec2 coords = gl_FragCoord.xy;
    FragColor = true ? colorGreen : colorRed;
    return;
}
