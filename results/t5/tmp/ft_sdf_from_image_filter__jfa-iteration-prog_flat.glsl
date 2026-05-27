#version 450 core
out vec4 FragColor;
uniform vec2 iResolution;
uniform float jfaRadius;
uniform float spreadFactor;
uniform sampler2D imageInput;
vec4 SafeFetch_f4f2_f4f2(vec2 fragCoord) {
    if (((fragCoord.x < 0.0 || fragCoord.x > iResolution.x) || fragCoord.y < 0.0) || fragCoord.y > iResolution.y) {
        return vec4(1000000.0);
    }
    return texture(imageInput, (fragCoord) / iResolution);
}
void SearchNeighbors_vf4f2f2_vf4f2f2(inout vec4 O, vec2 fragCoord, vec2 sampleCoord) {
    vec4 imgSample = SafeFetch_f4f2_f4f2(sampleCoord);
    vec4 a = vec4((imgSample.xy * 2.0 - 1.0) * spreadFactor + sampleCoord, (imgSample.zw * 2.0 - 1.0) * spreadFactor + sampleCoord);
    if (((imgSample.x < 1.0 && imgSample.y < 1.0) && imgSample.x > 0.0) && imgSample.y > 0.0) {
        vec2 diffA = fragCoord - a.xy;
        vec2 diffO = O.xy - fragCoord;
        O.xy = dot(diffA, diffA) < dot(diffO, diffO) ? a.xy : O.xy;
    }
    if (((imgSample.z < 1.0 && imgSample.w < 1.0) && imgSample.z > 0.0) && imgSample.w > 0.0) {
        vec2 diffA = fragCoord - a.zw;
        vec2 diffO = O.zw - fragCoord;
        O.zw = dot(diffA, diffA) < dot(diffO, diffO) ? a.zw : O.zw;
    }
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 O = texture(imageInput, (fragCoord) / iResolution);
    O.rgb *= O.a;
    O = vec4((O.xy * 2.0 - 1.0) * spreadFactor + fragCoord, (O.zw * 2.0 - 1.0) * spreadFactor + fragCoord);
    SearchNeighbors_vf4f2f2_vf4f2f2(O, fragCoord, fragCoord + jfaRadius * vec2(0.0, -1.0));
    SearchNeighbors_vf4f2f2_vf4f2f2(O, fragCoord, fragCoord + jfaRadius * vec2(0.0, 1.0));
    SearchNeighbors_vf4f2f2_vf4f2f2(O, fragCoord, fragCoord + jfaRadius * vec2(-1.0, 0.0));
    SearchNeighbors_vf4f2f2_vf4f2f2(O, fragCoord, fragCoord + jfaRadius * vec2(1.0, 0.0));
    O = vec4(clamp(((O.xy - fragCoord) / spreadFactor + 1.0) * 0.5, vec2(0.0), vec2(1.0)), clamp(((O.zw - fragCoord) / spreadFactor + 1.0) * 0.5, vec2(0.0), vec2(1.0)));
    FragColor = O;
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
