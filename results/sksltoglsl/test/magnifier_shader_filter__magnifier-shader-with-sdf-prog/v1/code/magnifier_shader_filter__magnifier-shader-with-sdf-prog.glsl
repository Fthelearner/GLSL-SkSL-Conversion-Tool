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
const vec2 boxPos = vec2(0.5);
const vec3 incident = vec3(0.0, 0.0, -1.0);
const float aa = 1.0;
const float dispScale = 0.5;
const float thickness = 8.0;
const float baseHeight = 64.0;
const float thicknessSq = 64.0;
const float invIndex = 0.6666667;
const float invIndexSubDisp = 0.7407407;
const float invIndexAddDisp = 0.6060606;
const float minNdotV = 0.05;
float height_hh(float sd) {
    if (sd >= 0.0) return 0.0;
    if (sd < -8.0) return thickness;
    float x = thickness + sd;
    return sqrt(thicknessSq - x * x);
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 sdfTex = texture(sdfShader, (fragCoord) / vec2(textureSize(sdfShader, 0)));
    float rawDist = (sdfTex.w - 0.5) * 600.0;
    vec4 sdfResult = vec4(sdfTex.xy * 2.0 - 1.0, sdfTex.z, rawDist);
    float sd = rawDist * dispScale;
    float shadow = 1.0 - smoothstep(0.0, shadowSize, rawDist);
    vec4 fragColor = vec4(0.0, 0.0, 0.0, shadow * shadowStrength);
    if (sd < 0.0) {
        if (sd < -8.0) {
            vec2 uv = fragCoord / iResolution;
            vec2 zoomUV = ((uv - boxPos) + zoomOffset) / factor + boxPos;
            FragColor = texture(imageShader, (zoomUV * iResolution) / vec2(textureSize(imageShader, 0)));
            return;
        }
        float shapeMask = 1.0 - smoothstep(-1.0, aa, sd);
        vec2 preNormal = normalize(sdfResult.xy);
        float nc = clamp((thickness + sd) * 0.125, 0.0, 1.0);
        vec3 normal = normalize(vec3(preNormal * nc, sqrt(1.0 - nc * nc)));
        float h = height_hh(sd);
        vec2 _0_res = iResolution;
        vec3 _1_refr = refract(incident, normal, invIndex);
        float _2_ndotv = max(dot(incident, _1_refr), minNdotV);
        float _3_refractLen = (h + baseHeight) / _2_ndotv;
        vec2 _4_uv = (fragCoord + _1_refr.xy * _3_refractLen) / _0_res;
        _4_uv = ((_4_uv - boxPos) + zoomOffset) / factor + boxPos;
        fragColor = texture(imageShader, (_4_uv * iResolution) / vec2(textureSize(imageShader, 0)));
        float edgeFactor = smoothstep(-15.0, 0.0, sd);
        if (edgeFactor > 0.001) {
            vec3 refr = refract(incident, normal, invIndex);
            float ndotv = max(dot(incident, refr), minNdotV);
            float refractLen = (h + baseHeight) / ndotv;
            vec2 _5_res = iResolution;
            vec3 _6_refrR = refract(incident, normal, invIndexSubDisp);
            vec3 _7_refrB = refract(incident, normal, invIndexAddDisp);
            vec2 _8_uvR = (fragCoord + _6_refrR.xy * refractLen) / _5_res;
            vec2 _9_uvB = (fragCoord + _7_refrB.xy * refractLen) / _5_res;
            _8_uvR = ((_8_uvR - boxPos) + zoomOffset) / factor + boxPos;
            _9_uvB = ((_9_uvB - boxPos) + zoomOffset) / factor + boxPos;
            vec4 dispColor = vec4(texture(imageShader, (_8_uvR * iResolution) / vec2(textureSize(imageShader, 0))).x, 0.0, texture(imageShader, (_9_uvB * iResolution) / vec2(textureSize(imageShader, 0))).z, 1.0);
            dispColor.y = fragColor.y;
            fragColor = mix(fragColor, dispColor, vec4(edgeFactor));
        }
        fragColor *= shapeMask;
    }
    float border = smoothstep(-borderSize, -borderSize + aa, rawDist) * smoothstep(aa, 0.0, rawDist);
    fragColor = mix(fragColor, vec4(borderColor.xyz, 1.0), vec4(border * borderColor.w));
    vec4 bgColor = texture(imageShader, ((fragCoord / iResolution) * iResolution) / vec2(textureSize(imageShader, 0)));
    FragColor = mix(bgColor, fragColor, vec4(fragColor.w));
    return;
}
