#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 iResolution;
uniform float blurIntensity;
uniform float horizontal;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / iResolution;
    vec2 texelSize = vec2(1.0 / iResolution.x, 1.0 / iResolution.y);
    vec2 direction = horizontal > 0.5 ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    float sigma = max(0.35, blurIntensity * 0.5);
    vec4 centerColor = texture(image, (uv * iResolution) / vec2(textureSize(image, 0)));
    vec3 color = centerColor.xyz;
    float totalWeight = 1.0;
    const int MAX_SAMPLES = 10;
    for (int i = 1;i <= MAX_SAMPLES; ++i) {
        float fi = float(i);
        float sampleMask = step(fi, blurIntensity + 0.5);
        float weight = exp(-(fi * fi) / ((2.0 * sigma) * sigma)) * sampleMask;
        vec2 offset = (direction * texelSize) * fi;
        color += texture(image, ((uv + offset) * iResolution) / vec2(textureSize(image, 0))).xyz * weight;
        color += texture(image, ((uv - offset) * iResolution) / vec2(textureSize(image, 0))).xyz * weight;
        totalWeight += 2.0 * weight;
    }
    FragColor = vec4(color / max(totalWeight, 0.0001), centerColor.w);
    return;
}
