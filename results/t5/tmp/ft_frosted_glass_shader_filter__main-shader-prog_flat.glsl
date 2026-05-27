#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform sampler2D edgeBlurredImg;
uniform sampler2D bgBlurredImg;
uniform sampler2D sdfNormalImg;
uniform vec2 iResolution;
uniform float borderWidth;
uniform float offset;
uniform float downSampleFactor;
uniform float bgFactor;
uniform float innerShadowRefractPx;
uniform float innerShadowWidth;
uniform float innerShadowExp;
uniform float sdK;
uniform float sdB;
uniform float sdS;
uniform float refractOutPx;
uniform float envK;
uniform float envB;
uniform float envS;
uniform float highLightAngleDeg;
uniform float highLightFeatherDeg;
uniform float highLightWidthPx;
uniform float highLightFeatherPx;
uniform float highLightShiftPx;
uniform vec2 highLightDirection;
uniform float hlK;
uniform float hlB;
uniform float hlS;
vec3 Sat_f3f3fffffff_f3f3fffffff(vec3 src255, float n, float p1r, float p2r, float p1g, float p2g, float p1b, float p2b) {
    float r = src255.x;
    float g = src255.y;
    float b = src255.z;
    float rnn = (r * (0.2412016 * (1.0 - n) + n) + g * (0.6922296 * (1.0 - n))) + b * (0.0665688 * (1.0 - n));
    float gnn = (r * (0.2412016 * (1.0 - n)) + g * (0.6922296 * (1.0 - n) + n)) + b * (0.0665688 * (1.0 - n));
    float bnn = (r * (0.2412016 * (1.0 - n)) + g * (0.6922296 * (1.0 - n))) + b * (0.0665688 * (1.0 - n) + n);
    float dr = rnn - r;
    float grt = step(0.0, dr);
    float rr = (r + dr * p1r) * grt + (r + dr * p2r) * (1.0 - grt);
    float dg = gnn - g;
    grt = step(0.0, dg);
    float gg = (g + dg * p1g) * grt + (g + dg * p2g) * (1.0 - grt);
    float db = bnn - b;
    grt = step(0.0, db);
    float bb = (b + db * p1b) * grt + (b + db * p2b) * (1.0 - grt);
    return vec3(rr, gg, bb);
}
vec3 ApplyKBS_f3f3fffffffff_f3f3fffffffff(vec3 c01, float K, float B, float S, float p1r, float p2r, float p1g, float p2g, float p1b, float p2b) {
    vec3 x = c01 * 255.0;
    x = x * K + vec3(B);
    x = Sat_f3f3fffffff_f3f3fffffff(x, S, p1r, p2r, p1g, p2g, p1b, p2b);
    return clamp(x * 0.003921569, vec3(0.0), vec3(1.0));
}
vec3 BlurVibrancy_f3f3_f3f3(vec3 c01) {
    vec3 x = c01 * 255.0;
    x = ((-2.89e-05 * pow(x, vec3(3.0)) + 0.0108341 * pow(x, vec3(2.0))) + 0.0073494 * x) + 25.470911;
    x = Sat_f3f3fffffff_f3f3fffffff(x, 1.2, 0.3, 0.5, 0.5, 0.5, 1.0, 1.0);
    return clamp(x * 0.003921569, vec3(0.0), vec3(1.0));
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 sdfNormalTex = texture(sdfNormalImg, (fragCoord) / iResolution);
    sdfNormalTex.rgb *= sdfNormalTex.a;
    float sd = (sdfNormalTex.w - 0.5) * 800.0;
    vec4 sdfNormal = vec4(sdfNormalTex.xy * 2.0 - 1.0, sdfNormalTex.zw);
    float sdBlack = sd + offset;
    float border = smoothstep(-0.5, max(1.0, (borderWidth * 0.5) * 0.5), -sd * 0.5) - smoothstep(min((-borderWidth * 0.5) * 0.5, -1.0), 0.5, (-sd - borderWidth) * 0.5);
    float borderBlack = smoothstep(-0.5, max(1.0, (borderWidth * 0.5) * 0.5), -sdBlack * 0.5) - smoothstep(min((-borderWidth * 0.5) * 0.5, -1.0), 0.5, (-sdBlack - borderWidth) * 0.5);
    vec4 blurredBgColor = texture(bgBlurredImg, (fragCoord) / iResolution) * bgFactor;
    blurredBgColor.xyz = BlurVibrancy_f3f3_f3f3(blurredBgColor.xyz);
    float embossNeg = exp(innerShadowExp * ((sdBlack - 1.0) + innerShadowWidth));
    if (embossNeg > 0.0) {
        vec2 tileSize = iResolution / max(downSampleFactor, 1e-06);
        vec2 uvInTile = fragCoord / iResolution;
        vec2 pixelDS = uvInTile * (tileSize - 1.0) + 0.5;
        vec2 nOut = sdfNormal.xy;
        vec2 deltaInDS = (nOut * innerShadowRefractPx) / max(downSampleFactor, 1e-06);
        vec2 negCoord = pixelDS + deltaInDS;
        vec4 refractionNeg = texture(edgeBlurredImg, (negCoord) / iResolution) * bgFactor;
        refractionNeg.xyz = BlurVibrancy_f3f3_f3f3(refractionNeg.xyz);
        float _2_lb = dot(blurredBgColor.xyz, vec3(0.2412016, 0.6922296, 0.0665688));
        float _3_le = dot(refractionNeg.xyz, vec3(0.2412016, 0.6922296, 0.0665688));
        refractionNeg.xyz = mix(blurredBgColor.xyz, refractionNeg.xyz, vec3(_3_le / max(_2_lb + _3_le, 0.0001)));
        refractionNeg.xyz = ApplyKBS_f3f3fffffffff_f3f3fffffffff(refractionNeg.xyz, sdK, sdB, sdS, 1.0, 1.7, 1.5, 3.0, 2.0, 1.0);
        blurredBgColor = mix(blurredBgColor, refractionNeg, vec4(clamp(embossNeg, 0.0, 1.0)));
    }
    float embossPos = (((border - borderBlack) + 1.0) * 0.5) * clamp(border + borderBlack, 0.0, 1.0);
    if (embossPos > 0.0) {
        vec2 tileSize = iResolution / max(downSampleFactor, 1e-06);
        vec2 uvInTile = fragCoord / iResolution;
        vec2 pixelDS = uvInTile * (tileSize - 1.0) + 0.5;
        vec2 nOut = sdfNormal.xy;
        vec2 deltaOutDS = (nOut * refractOutPx) / max(downSampleFactor, 1e-06);
        vec2 posCoord = pixelDS + deltaOutDS;
        vec4 refractionPos = texture(edgeBlurredImg, (posCoord) / iResolution) * bgFactor;
        float _5_lb = dot(blurredBgColor.xyz, vec3(0.2412016, 0.6922296, 0.0665688));
        float _6_le = dot(refractionPos.xyz, vec3(0.2412016, 0.6922296, 0.0665688));
        refractionPos.xyz = mix(blurredBgColor.xyz, refractionPos.xyz, vec3(_6_le / max(_5_lb + _6_le, 0.0001)));
        refractionPos.xyz = ApplyKBS_f3f3fffffffff_f3f3fffffffff(refractionPos.xyz, envK, envB, envS, 1.0, 1.7, 1.5, 3.0, 2.0, 1.0);
        blurredBgColor = mix(blurredBgColor, refractionPos, vec4(clamp(embossPos, 0.0, 1.0)));
    }
    vec2 uv = ((fragCoord + fragCoord) - iResolution) * 0.5;
    float widthClamped = min(highLightWidthPx, max(borderWidth, 0.0));
    float _7_a = max(highLightFeatherPx, 1e-06);
    float _8_coverOuter = smoothstep(_7_a, -_7_a, sd + highLightShiftPx);
    float _9_coverInner = smoothstep(_7_a, -_7_a, (sd + highLightShiftPx) + max(widthClamped, 0.0));
    float edgeBand = clamp(_8_coverOuter - _9_coverInner, 0.0, 1.0);
    vec2 _10_p = normalize(uv);
    vec2 _11_d = normalize(normalize(highLightDirection));
    float _12_angle = highLightAngleDeg * 0.0174532924;
    float _13_feather = max(0.0001, highLightFeatherDeg * 0.0174532924);
    float _14_c1 = clamp(dot(_11_d, _10_p), -1.0, 1.0);
    float _15_c2 = clamp(dot(-_11_d, _10_p), -1.0, 1.0);
    float _16_theta1 = acos(_14_c1);
    float _17_theta2 = acos(_15_c2);
    float _18_lobe1 = 1.0 - smoothstep(_12_angle * 0.5, _12_angle * 0.5 + _13_feather, _16_theta1);
    float _19_lobe2 = 1.0 - smoothstep(_12_angle * 0.5, _12_angle * 0.5 + _13_feather, _17_theta2);
    float diagMask = clamp(_18_lobe1 + _19_lobe2, 0.0, 1.0);
    float edge = edgeBand * diagMask;
    vec3 hlBase = ApplyKBS_f3f3fffffffff_f3f3fffffffff(blurredBgColor.xyz, hlK, hlB, hlS, 1.0, 1.7, 1.5, 3.0, 2.0, 1.0);
    blurredBgColor = mix(blurredBgColor, vec4(hlBase, 1.0), vec4(edge));
    blurredBgColor = mix(texture(image, (fragCoord) / iResolution), blurredBgColor, vec4(clamp(-min(sd, sdBlack), 0.0, 1.0)));
    FragColor = vec4(blurredBgColor.xyz, 1.0);
    FragColor = vec4(FragColor.xyz, 1.0);
    return;
}
