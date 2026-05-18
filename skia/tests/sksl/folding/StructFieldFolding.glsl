
out vec4 FragColor;
uniform vec4 colorRed;
uniform vec4 colorGreen;
struct S {
    int a;
    int b;
    int c;
};
void main() {
    vec2 coords = gl_FragCoord.xy;
    const int _6_two = 2;
    int _8_flatten1 = _6_two;
    FragColor = _8_flatten1 == 2 ? colorGreen : colorRed;
    return;
}
