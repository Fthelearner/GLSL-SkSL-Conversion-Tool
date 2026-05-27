#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform sampler2D preblurImage;
uniform vec2 iResolution;
uniform vec2 startPoint;
uniform vec2 endPoint;
uniform float softness;
uniform float invert;
uniform float blurMix;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 outColor;
    vec2 startCoord = startPoint * iResolution;
    vec2 endCoord = endPoint * iResolution;
    vec2 axis = endCoord - startCoord;
    float axisLengthSquared = max(dot(axis, axis), 1e-05);
    float projected = dot(fragCoord - startCoord, axis) / axisLengthSquared;
    float edge0 = -softness;
    float edge1 = 1.0 + softness;
    float gradient = smoothstep(edge0, edge1, projected);
    if (invert > 0.5) {
        gradient = 1.0 - gradient;
    }
    float mixAmount = clamp(gradient * blurMix, 0.0, 1.0);
    vec4 sourceColor = texture(image, (fragCoord) / iResolution);
    sourceColor.rgb *= sourceColor.a;
    vec4 blurColor = texture(preblurImage, (fragCoord) / iResolution);
    blurColor.rgb *= blurColor.a;
    outColor = mix(sourceColor, blurColor, vec4(mixAmount));
    FragColor = vec4(outColor.xyz, 1.0);
    return;
}
