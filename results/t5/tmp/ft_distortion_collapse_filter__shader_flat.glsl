#version 450 core
out vec4 FragColor;
uniform sampler2D imageShader;
uniform vec2 imageSize;
uniform vec2 invImageSize;
uniform vec2 lu;
uniform vec2 e;
uniform vec2 f;
uniform vec2 g;
uniform float k2;
uniform float ik2;
uniform float k1Base;
uniform float k0Base;
uniform vec4 barrelDistortion;
uniform float distortionEnabled;
vec2 InvBilinear_f2f2_f2f2(vec2 p) {
    vec2 res = vec2(-1.0);
    vec2 h = p - lu;
    float localK1 = k1Base + (p.x * g.y - p.y * g.x);
    float localK0 = k0Base + (p.x * e.y - p.y * e.x);
    if (abs(k2) < 0.001) {
        res = vec2((h.x * localK1 + f.x * localK0) / (e.x * localK1 - g.x * localK0), -localK0 / localK1);
    } else {
        float w = localK1 * localK1 - (4.0 * localK0) * k2;
        if (w < 0.0) {
            return vec2(-1.0);
        }
        w = sqrt(w);
        float v = (-localK1 - w) * ik2;
        float u = (h.x - f.x * v) / (e.x + g.x * v);
        if (((u < 0.0 || u > 1.0) || v < 0.0) || v > 1.0) {
            v = (-localK1 + w) * ik2;
            u = (h.x - f.x * v) / (e.x + g.x * v);
        }
        res = vec2(u, v);
    }
    return res;
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord * invImageSize;
    vec2 newUV = InvBilinear_f2f2_f2f2(uv);
    if (distortionEnabled > 0.0) {
        vec2 lerpDistortion = vec2(mix(barrelDistortion.x, barrelDistortion.y, newUV.x), mix(barrelDistortion.z, barrelDistortion.w, newUV.y));
        vec2 centerNewUV = newUV - vec2(0.5);
        vec2 normFactor = 1.0 / (1.0 + lerpDistortion * 0.5);
        float l2 = dot(centerNewUV, centerNewUV);
        centerNewUV *= 1.0 + lerpDistortion * l2;
        centerNewUV *= normFactor;
        newUV = centerNewUV + vec2(0.5);
    }
    FragColor = texture(imageShader, (newUV * imageSize) / vec2(textureSize(imageShader, 0)));
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
