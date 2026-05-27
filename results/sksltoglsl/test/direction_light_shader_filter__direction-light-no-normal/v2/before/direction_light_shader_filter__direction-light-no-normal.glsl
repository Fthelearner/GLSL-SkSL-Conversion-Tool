#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 iResolution;
uniform vec3 lightDirection;
uniform vec4 lightColor;
uniform float lightIntensity;
const float piInv = 0.3183099;
const float eta = 0.2;
const float roughness = 0.5;
const float metal = 0.5;
float pow2_ff(float x) {
    return x * x;
}
float RoughnessToAlpha_ff(float roughness) {
    const float minRoughnessSquare = 0.007921;
    return clamp(pow2_ff(roughness), minRoughnessSquare, 1.0);
}
float DistributionGGX_fff(float roughness, float cosNToH) {
    float alpha = RoughnessToAlpha_ff(roughness);
    float alpha2 = pow2_ff(alpha);
    float f = (cosNToH * alpha2 - cosNToH) * cosNToH + 1.0;
    return (alpha2 / pow2_ff(f)) * piInv;
}
float VisibilitySmithGGXCorrelatedApprox_ffff(float roughness, float cosNToV, float cosNToL) {
    float alpha = RoughnessToAlpha_ff(roughness);
    float termV = cosNToL * (cosNToV * (1.0 - alpha) + alpha);
    float termL = cosNToV * (cosNToL * (1.0 - alpha) + alpha);
    return 0.5 / (termV + termL);
}
vec3 FresnelSchlick_f3f3f(vec3 F0, float cosVToH) {
    float f = pow(1.0 - cosVToH, 5.0);
    return clamp(50.0 * F0.y, 0.0, 1.0) * f + (1.0 - f) * F0;
}
vec4 scatter_f4f3f3f3f(vec3 pos, vec3 displacementNormal, vec3 shadingNormal, float eta) {
    vec3 viewDir = vec3(0.0, 0.0, 1.0);
    vec3 lightDir = normalize(lightDirection);
    float cosNToL = dot(shadingNormal, lightDir);
    if (cosNToL <= 0.0) return vec4(0.0);
    vec3 halfDir = normalize(viewDir + lightDir);
    float cosNToV = clamp(dot(shadingNormal, viewDir), 0.0, 1.0);
    float cosNToH = clamp(dot(shadingNormal, halfDir), 0.0, 1.0);
    float cosVToH = clamp(dot(viewDir, halfDir), 0.0, 1.0);
    float distribution = DistributionGGX_fff(roughness, cosNToH);
    float visibility = VisibilitySmithGGXCorrelatedApprox_ffff(roughness, cosNToV, cosNToL);
    vec3 albedo = texture(image, (pos.xy) / iResolution).xyz;
    vec3 diffuseColor = mix(albedo, vec3(0.0), vec3(metal));
    vec3 specularColor = mix(vec3(0.032), albedo, vec3(metal));
    vec3 fresnel = FresnelSchlick_f3f3f(specularColor, cosVToH);
    vec3 reflectedColor = (((distribution * visibility) * fresnel) * cosNToL) * lightColor.xyz;
    vec3 refractedColor = (diffuseColor * cosNToL) * lightColor.xyz;
    return vec4((reflectedColor + refractedColor) * lightIntensity, 1.0);
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec3 pos = vec3(fragCoord, 20.0);
    vec3 normal = vec3(0.0, 0.0, 1.0);
    vec3 displacementNormal = normalize(vec3(0.787401557, 0.787401557, 0.5) * normal);
    vec3 shadingNormal = normalize(vec3(0.787401557, 0.787401557, 10.0) * normal);
    FragColor = scatter_f4f3f3f3f(pos, displacementNormal, shadingNormal, eta);
    return;
}
