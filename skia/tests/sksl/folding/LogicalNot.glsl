
out vec4 FragColor;
uniform vec4 colorGreen;
uniform vec4 colorRed;
void main() {
    vec2 coords = gl_FragCoord.xy;
    bool ok = true;
    ok = ok && colorGreen.y >= colorGreen.x;
    ok = ok && colorGreen.y > colorGreen.x;
    ok = ok && colorGreen.z <= colorGreen.y;
    ok = ok && colorGreen.z < colorGreen.y;
    ok = ok && colorGreen.y >= colorGreen.w;
    ok = ok && colorGreen.x <= colorGreen.z;
    ok = ok && colorGreen.y != colorGreen.x;
    ok = ok && colorGreen.y == colorGreen.w;
    FragColor = ok ? colorGreen : colorRed;
    return;
}
