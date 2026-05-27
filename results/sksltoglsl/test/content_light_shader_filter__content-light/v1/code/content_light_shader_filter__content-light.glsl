#version 450 core
out vec4 FragColor;
uniform sampler2D image;
uniform vec2 iResolution;
uniform float lightIntensity;
uniform vec3 lightPosition;
uniform vec4 lightColor;
uniform vec3 contentRotationAngle;
vec4 createContentNormal_h4h2h(vec2 pos, float maskAlpha) {
    if (maskAlpha < 0.01) {
        return vec4(0.0, 0.0, -1.0, -1.0);
    }
    float R = (max(iResolution.x, iResolution.y) * 4.0) / iResolution.y;
    float z = sqrt((R * R - pos.x * pos.x) - pos.y * pos.y);
    vec3 normal = normalize(vec3(pos.x / z, pos.y / z, 0.5));
    return vec4(normal, 1.0);
}
vec4 ContentShinning_h4h2h4hh3h3f33h(vec2 uv, vec4 specularColor, float shinning, vec3 lightPos, vec3 viewPos, mat3 rotM, float maskAlpha) {
    vec3 fragPos = vec3(uv, 0.0);
    vec4 normal = createContentNormal_h4h2h(uv, maskAlpha);
    if (normal.w < 0.0) {
        return vec4(0.0);
    }
    vec3 fragNormal = rotM * normal.xyz;
    vec3 _0_lightDir = normalize(lightPos - fragPos);
    vec3 _1_viewDir = normalize(viewPos - fragPos);
    vec3 _2_halfwayDir = normalize(_0_lightDir + _1_viewDir);
    vec4 _3_specularC = specularColor * pow(max(dot(fragNormal, _2_halfwayDir), 0.0), shinning);
    vec4 shinningColor = _3_specularC;
    return shinningColor;
}
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 inputImage = texture(image, (fragCoord) / vec2(textureSize(image, 0)));
    if (inputImage.w > 0.0) {
        inputImage.xyz /= inputImage.w;
    } else {
        FragColor = inputImage;
        return;
    }
    vec2 uv = fragCoord / iResolution;
    uv = (uv + uv) - 1.0;
    float screenRatio = iResolution.x / iResolution.y;
    uv.x *= screenRatio;
    vec4 shinningColor = vec4(0.0);
    vec3 lightPos = vec3(lightPosition.x * screenRatio, lightPosition.y, lightPosition.z);
    vec3 _4_rotAngle = contentRotationAngle;
    _4_rotAngle *= 0.0174532924;
    mat3 _5_Rx = mat3(1.0, 0.0, 0.0, 0.0, cos(_4_rotAngle.x), -sin(_4_rotAngle.x), 0.0, sin(_4_rotAngle.x), cos(_4_rotAngle.x));
    mat3 _6_Ry = mat3(cos(_4_rotAngle.y), 0.0, sin(_4_rotAngle.y), 0.0, 1.0, 0.0, -sin(_4_rotAngle.y), 0.0, cos(_4_rotAngle.y));
    mat3 _7_Rz = mat3(cos(_4_rotAngle.z), -sin(_4_rotAngle.z), 0.0, sin(_4_rotAngle.z), cos(_4_rotAngle.z), 0.0, 0.0, 0.0, 1.0);
    mat3 rotM = (_7_Rz * _6_Ry) * _5_Rx;
    vec4 specularColor = lightColor;
    float shinning = 36.0;
    vec3 viewPos = lightPos;
    shinningColor = ContentShinning_h4h2h4hh3h3f33h(uv, specularColor, shinning, lightPos, viewPos, rotM, inputImage.w);
    float intensity = clamp(lightIntensity, 0.0, 1.0) * lightColor.w;
    shinningColor.xyz *= intensity;
    shinningColor.xyz += inputImage.xyz - (inputImage.xyz * shinningColor.xyz) * 0.85;
    FragColor = vec4(shinningColor.xyz * inputImage.w, inputImage.w);
    return;
}
