#version 450 core
out vec4 FragColor;
uniform sampler2D imageShader;
uniform sampler2D sdfShader;
uniform vec2 iResolution;
uniform float factor;
uniform vec2 zoomOffset;
uniform float borderSize;
uniform vec4 borderColor;
uniform float shadowSize;
uniform float shadowStrength;
float height_hh_ff(float sd) {
    if (sd >= 0.0) {
        return 0.0;
    }
    if (sd < -8.0) {
        return 8.0;
    }
    float x = 8.0 + sd;
    return sqrt(64.0 - x * x);
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 sdfTex = texture(sdfShader, (fragCoord) / iResolution);
    sdfTex.rgb *= sdfTex.a;
    float rawDist = (sdfTex.w - 0.5) * 600.0;
    vec4 sdfResult = vec4(sdfTex.xy * 2.0 - 1.0, sdfTex.z, rawDist);
    float sd = rawDist * 0.5;
    float shadow = 1.0 - smoothstep(0.0, shadowSize, rawDist);
    vec4 fragColor = vec4(0.0, 0.0, 0.0, shadow * shadowStrength);
    if (sd < 0.0) {
        if (sd < -8.0) {
            vec2 uv = fragCoord / iResolution;
            vec2 zoomUV = ((uv - vec2(0.5)) + zoomOffset) / factor + vec2(0.5);
            FragColor = texture(imageShader, zoomUV);
            return;
        }
        float shapeMask = 1.0 - smoothstep(-1.0, 1.0, sd);
        vec2 preNormal = normalize(sdfResult.xy);
        float nc = clamp((8.0 + sd) * 0.125, 0.0, 1.0);
        vec3 normal = normalize(vec3(preNormal * nc, sqrt(1.0 - nc * nc)));
        float h = height_hh_ff(sd);
        vec2 _0_res = iResolution;
        vec3 _1_refr = refract(vec3(0.0, 0.0, -1.0), normal, 0.6666667);
        float _2_ndotv = max(dot(vec3(0.0, 0.0, -1.0), _1_refr), 0.05);
        float _3_refractLen = (h + 64.0) / _2_ndotv;
        vec2 _4_uv = (fragCoord + _1_refr.xy * _3_refractLen) / _0_res;
        _4_uv = ((_4_uv - vec2(0.5)) + zoomOffset) / factor + vec2(0.5);
        fragColor = texture(imageShader, _4_uv);
        float edgeFactor = smoothstep(-15.0, 0.0, sd);
        if (edgeFactor > 0.001) {
            vec3 refr = refract(vec3(0.0, 0.0, -1.0), normal, 0.6666667);
            float ndotv = max(dot(vec3(0.0, 0.0, -1.0), refr), 0.05);
            float refractLen = (h + 64.0) / ndotv;
            vec2 _5_res = iResolution;
            vec3 _6_refrR = refract(vec3(0.0, 0.0, -1.0), normal, 0.7407407);
            vec3 _7_refrB = refract(vec3(0.0, 0.0, -1.0), normal, 0.6060606);
            vec2 _8_uvR = (fragCoord + _6_refrR.xy * refractLen) / _5_res;
            vec2 _9_uvB = (fragCoord + _7_refrB.xy * refractLen) / _5_res;
            _8_uvR = ((_8_uvR - vec2(0.5)) + zoomOffset) / factor + vec2(0.5);
            _9_uvB = ((_9_uvB - vec2(0.5)) + zoomOffset) / factor + vec2(0.5);
            vec4 dispColor = vec4(texture(imageShader, _8_uvR).x, 0.0, texture(imageShader, _9_uvB).z, 1.0);
            dispColor.y = fragColor.y;
            fragColor = mix(fragColor, dispColor, vec4(edgeFactor));
        }
        fragColor *= shapeMask;
    }
    float border = smoothstep(-borderSize, -borderSize + 1.0, rawDist) * smoothstep(1.0, 0.0, rawDist);
    fragColor = mix(fragColor, vec4(borderColor.xyz, 1.0), vec4(border * borderColor.w));
    vec4 bgColor = texture(imageShader, fragCoord / iResolution);
    bgColor.rgb *= bgColor.a;
    FragColor = mix(bgColor, fragColor, vec4(fragColor.w));
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
