#version 450 core
out vec4 FragColor;
uniform sampler2D blur_tex;
uniform sampler2D original_tex;
uniform sampler2D blur_mask;
uniform vec2 iResolution;
uniform float mixStrength;
uniform float progress;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / iResolution;
    vec2 maskUV = uv - vec2(0.0, progress * 0.07);
    vec4 maskColor = texture(blur_mask, maskUV);
    maskColor.rgb *= maskColor.a;
    float maskValue = maskColor.x;
    float blend = clamp(maskValue * mixStrength, 0.0, 1.0);
    vec4 originColor = texture(original_tex, (fragCoord) / iResolution);
    originColor.rgb *= originColor.a;
    vec4 blurredColor = texture(blur_tex, (fragCoord) / iResolution);
    blurredColor.rgb *= blurredColor.a;
    vec3 mixedRgb = mix(originColor.xyz, blurredColor.xyz, vec3(blend));
    FragColor = vec4(mixedRgb, originColor.w);
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
