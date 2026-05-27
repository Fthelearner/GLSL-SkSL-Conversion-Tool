#version 450 core
out vec4 FragColor;
uniform vec2 iResolution;
uniform sampler2D sdfImageShader;
uniform sampler2D blurredSdfImageShader;
uniform sampler2D lightMaskShader;
uniform float spreadFactor;
uniform vec3 lightColor;
uniform float bloomIntensityCutoff;
uniform float maxIntensity;
uniform float maxBloomIntensity;
uniform float bloomFalloffPow;
uniform float minBorderWidth;
uniform float maxBorderWidth;
uniform float innerBorderBloomWidth;
uniform float outerBorderBloomWidth;
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    float lightMaskValue = texture(lightMaskShader, (fragCoord) / iResolution).x;
    vec4 sdfMapSample = (texture(sdfImageShader, (fragCoord) / iResolution) * 2.0 - vec4(1.0)) * spreadFactor;
    vec4 blurredSdfMapSample = (texture(blurredSdfImageShader, (fragCoord) / iResolution) * 2.0 - vec4(1.0)) * spreadFactor;
    float _2_bloomBorder = 1.0 - step(outerBorderBloomWidth, sdfMapSample.w);
    _2_bloomBorder *= step(-innerBorderBloomWidth, sdfMapSample.w);
    float _3_edgeThickness = smoothstep(0.0, 1.0, lightMaskValue) * (maxBorderWidth - minBorderWidth) + minBorderWidth;
    float _4_thinBorder = (1.0 - smoothstep(0.0, _3_edgeThickness, sdfMapSample.w)) * smoothstep(-_3_edgeThickness, 0.0, sdfMapSample.w);
    float _5_dNorm = abs(blurredSdfMapSample.w) / blurredSdfMapSample.w > 0.0 ? outerBorderBloomWidth : innerBorderBloomWidth;
    float _6_falloff = max((1.0 - _5_dNorm) / pow(1.0 + _5_dNorm, bloomFalloffPow), 0.0);
    float _7_b = (lightMaskValue * maxIntensity) * (_4_thinBorder + (smoothstep(bloomIntensityCutoff, 1.0, lightMaskValue) * _2_bloomBorder) * (maxBloomIntensity * _6_falloff));
    vec3 rgb = lightColor * _7_b;
    float alpha = clamp(length(rgb) * 3.0, 0.0, 1.0);
    FragColor = vec4(rgb, alpha);
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
