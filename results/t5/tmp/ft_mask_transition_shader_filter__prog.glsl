#version 450 core
out vec4 FragColor;
uniform sampler2D alphaMask;
uniform sampler2D topLayer;
uniform sampler2D bottomLayer;
uniform float factor;
uniform float inverseFlag;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float alpha = texture(alphaMask, (fragCoord) / vec2(textureSize(alphaMask, 0))).w * factor;
    alpha = mix(alpha, 1.0 - alpha, inverseFlag);
    vec4 topColor = texture(topLayer, (fragCoord) / vec2(textureSize(topLayer, 0)));
    topColor.rgb *= topColor.a;
    vec4 bottomColor = texture(bottomLayer, (fragCoord) / vec2(textureSize(bottomLayer, 0)));
    bottomColor.rgb *= bottomColor.a;
    vec4 finalColor = mix(bottomColor, topColor, vec4(1.0 - alpha));
    FragColor = finalColor;
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
