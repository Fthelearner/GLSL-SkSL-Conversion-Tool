/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ge_direction_light_shader_filter.h"
#include <memory>
#include "ge_log.h"

namespace OHOS {
namespace Rosen {
namespace {
inline static const std::string g_shaderStringNormalMask = R"(
    uniform shader mask;
    uniform vec2 iResolution;

    float bump(vec2 uv)
    {
        return mask.eval(uv).a;
    }

    vec4 main(vec2 fragCoord)
    {
        vec2 offset = vec2(1.0, iResolution.y / iResolution.x);

        float h1_u = bump(fragCoord - vec2(offset.x, 0.0));
        float h2_u = bump(fragCoord + vec2(offset.x, 0.0));
        vec3 tangent_u = vec3(2.0, 0.0, (h2_u - h1_u));

        float h1_v = bump(fragCoord - vec2(0.0, offset.y));
        float h2_v = bump(fragCoord + vec2(0.0, offset.y));
        vec3 tangent_v = vec3(0.0, 2.0, (h2_v - h1_v));

        return vec4(normalize(cross(tangent_u, tangent_v)), 1.0);
    }
)";

inline static const std::string g_shaderStringDirectionLight = R"(
    uniform shader image;
    uniform shader mask;
    uniform vec2 iResolution;
    uniform vec3 lightDirection;
    uniform vec4 lightColor;
    uniform float lightIntensity;

    const float piInv = 0.3183099;
    const float aspect = 2.17;
    const float scale = 1.27;
    const float height = 0.10;
    const float displacement = 20.0;
    const float opacity = 1.0;
    const float eta = 0.20;
    const float specular = 0.40;
    const float roughness = 0.50;
    const float metal = 0.50;

    float pow2(float x)
    {
        return x * x;
    }

    float RoughnessToAlpha(float roughness)
    {
        const float minRoughness = 0.089;
        const float minRoughnessSquare = 0.007921;
        return clamp(pow2(roughness), minRoughnessSquare, 1.0);
    }

    float DistributionGGX(float roughness, float cosNToH)
    {
        float alpha = RoughnessToAlpha(roughness);
        float alpha2 = pow2(alpha);
        float f = (cosNToH * alpha2 - cosNToH) * cosNToH + 1.0;
        return alpha2 / pow2(f) * piInv;
    }

    float VisibilitySmithGGXCorrelatedApprox(float roughness, float cosNToV, float cosNToL)
    {
        float alpha = RoughnessToAlpha(roughness);
        float termV = cosNToL * (cosNToV * (1.0 - alpha) + alpha);
        float termL = cosNToV * (cosNToL * (1.0 - alpha) + alpha);
        return 0.5 / (termV + termL);
    }

    vec3 FresnelSchlick(vec3 F0, float cosVToH)
    {
        float f = pow(1.0 - cosVToH, 5.0);
        return saturate(50.0 * F0.g) * f + (1.0 - f) * F0;
    }

    vec4 scatter(vec3 pos, vec3 displacementNormal, vec3 shadingNormal, float eta)
    {
        vec3 viewDir = vec3(0.0, 0.0, 1.0);
        vec3 lightDir = normalize(lightDirection);

        float cosNToL = dot(shadingNormal, lightDir);
        if (cosNToL <= 0.0) return vec4(0.0);

        vec3 halfDir = normalize(viewDir + lightDir);
        float cosNToV = saturate(dot(shadingNormal, viewDir));
        float cosNToH = saturate(dot(shadingNormal, halfDir));
        float cosVToH = saturate(dot(viewDir, halfDir));

        float distribution = DistributionGGX(roughness, cosNToH);
        float visibility = VisibilitySmithGGXCorrelatedApprox(roughness, cosNToV, cosNToL);

        vec3 albedo = image.eval(pos.xy).rgb;
        vec3 diffuseColor = mix(albedo, vec3(0.0), metal);
        vec3 specularColor = mix(vec3(0.08 * specular), albedo, metal);
        vec3 fresnel = FresnelSchlick(specularColor, cosVToH);

        vec3 reflectedColor = distribution * visibility * fresnel * cosNToL * lightColor.rgb;
        vec3 refractedColor = diffuseColor * cosNToL * lightColor.rgb;
        return vec4((reflectedColor + refractedColor) * lightIntensity, 1.0);
    }

    vec4 main(vec2 fragCoord)
    {
        vec3 pos = vec3(fragCoord.xy, displacement);
        vec3 tempNormalValue = mask.eval(fragCoord).xyz;
        vec3 normal = normalize(tempNormalValue * 2.0 - 1.0);
        if (all(lessThan(abs(tempNormalValue), vec3(1e-6)))) {
            normal = vec3(0.0, 0.0, 1.0);
        }
        vec3 displacementNormal =
            normalize(vec3(1.0 / scale, 1.0 / scale, 1.0 / (height * displacement)) * normal);
        vec3 shadingNormal = normalize(vec3(1.0 / scale, 1.0 / scale, 1.0 / height) * normal);
        return scatter(pos, displacementNormal, shadingNormal, eta);
    }
)";

inline static const std::string g_shaderStringDirectionLightNoNormal = R"(
    uniform shader image;
    uniform vec2 iResolution;
    uniform vec3 lightDirection;
    uniform vec4 lightColor;
    uniform float lightIntensity;

    const float piInv = 0.3183099;
    const float aspect = 2.17;
    const float scale = 1.27;
    const float height = 0.10;
    const float displacement = 20.0;
    const float opacity = 1.0;
    const float eta = 0.20;
    const float specular = 0.40;
    const float roughness = 0.50;
    const float metal = 0.50;

    float pow2(float x)
    {
        return x * x;
    }

    float RoughnessToAlpha(float roughness)
    {
        const float minRoughness = 0.089;
        const float minRoughnessSquare = 0.007921;
        return clamp(pow2(roughness), minRoughnessSquare, 1.0);
    }

    float DistributionGGX(float roughness, float cosNToH)
    {
        float alpha = RoughnessToAlpha(roughness);
        float alpha2 = pow2(alpha);
        float f = (cosNToH * alpha2 - cosNToH) * cosNToH + 1.0;
        return alpha2 / pow2(f) * piInv;
    }

    float VisibilitySmithGGXCorrelatedApprox(float roughness, float cosNToV, float cosNToL)
    {
        float alpha = RoughnessToAlpha(roughness);
        float termV = cosNToL * (cosNToV * (1.0 - alpha) + alpha);
        float termL = cosNToV * (cosNToL * (1.0 - alpha) + alpha);
        return 0.5 / (termV + termL);
    }

    vec3 FresnelSchlick(vec3 F0, float cosVToH)
    {
        float f = pow(1.0 - cosVToH, 5.0);
        return saturate(50.0 * F0.g) * f + (1.0 - f) * F0;
    }

    vec4 scatter(vec3 pos, vec3 displacementNormal, vec3 shadingNormal, float eta)
    {
        vec3 viewDir = vec3(0.0, 0.0, 1.0);
        vec3 lightDir = normalize(lightDirection);

        float cosNToL = dot(shadingNormal, lightDir);
        if (cosNToL <= 0.0) return vec4(0.0);

        vec3 halfDir = normalize(viewDir + lightDir);
        float cosNToV = saturate(dot(shadingNormal, viewDir));
        float cosNToH = saturate(dot(shadingNormal, halfDir));
        float cosVToH = saturate(dot(viewDir, halfDir));

        float distribution = DistributionGGX(roughness, cosNToH);
        float visibility = VisibilitySmithGGXCorrelatedApprox(roughness, cosNToV, cosNToL);

        vec3 albedo = image.eval(pos.xy).rgb;
        vec3 diffuseColor = mix(albedo, vec3(0.0), metal);
        vec3 specularColor = mix(vec3(0.08 * specular), albedo, metal);
        vec3 fresnel = FresnelSchlick(specularColor, cosVToH);

        vec3 reflectedColor = distribution * visibility * fresnel * cosNToL * lightColor.rgb;
        vec3 refractedColor = diffuseColor * cosNToL * lightColor.rgb;
        return vec4((reflectedColor + refractedColor) * lightIntensity, 1.0);
    }

    vec4 main(vec2 fragCoord)
    {
        vec3 pos = vec3(fragCoord.xy, displacement);
        vec3 normal = vec3(0.0, 0.0, 1.0);
        vec3 displacementNormal =
            normalize(vec3(1.0 / scale, 1.0 / scale, 1.0 / (height * displacement)) * normal);
        vec3 shadingNormal = normalize(vec3(1.0 / scale, 1.0 / scale, 1.0 / height) * normal);
        return scatter(pos, displacementNormal, shadingNormal, eta);
    }
)";

}

GEDirectionLightShaderFilter::GEDirectionLightShaderFilter(const Drawing::GEDirectionLightShaderFilterParams& params)
    : params_(params)
{}

std::shared_ptr<Drawing::Image> GEDirectionLightShaderFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (image == nullptr) {
        return nullptr;
    }

    auto imageInfo = image->GetImageInfo();
    float height = imageInfo.GetHeight();
    float width = imageInfo.GetWidth();
    if (height < 1e-6 || width < 1e-6) {
        return nullptr;
    }

    std::shared_ptr<Drawing::RuntimeShaderBuilder> lightingBuilder = nullptr;

    if (params_.mask != nullptr) {
        auto directionLightShader = GetDirectionLightEffect();
        if (directionLightShader == nullptr) {
            LOGE("GEDirectionLightShaderFilter::OnProcessImage directionLightShader init failed");
            return image;
        }
        lightingBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(directionLightShader);
        if (lightingBuilder == nullptr) {
            LOGE("GEDirectionLightShaderFilter::OnProcessImage lightingBuilder is nullptr");
            return image;
        }
        auto maskShader = params_.mask->GenerateDrawingShader(canvasInfo_.geoWidth, canvasInfo_.geoHeight);
        if (maskShader == nullptr) {
            LOGE("GEDirectionLightShaderFilter::OnProcessImage mask generate failed");
            return image;
        }
        lightingBuilder->SetChild("mask", maskShader);
    } else {
        auto directionLightNoNormalShader = GetDirectionLightNoNormalEffect();
        if (directionLightNoNormalShader == nullptr) {
            LOGE("GEDirectionLightShaderFilter::OnProcessImage directionLightNoNormalShader init failed");
            return image;
        }
        lightingBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(directionLightNoNormalShader);
        if (lightingBuilder == nullptr) {
            LOGE("GEDirectionLightShaderFilter::OnProcessImage lightingBuilder is nullptr");
            return image;
        }
    }

    Drawing::Matrix imageMatrix = canvasInfo_.mat;
    imageMatrix.PostTranslate(-canvasInfo_.tranX, -canvasInfo_.tranY);
    Drawing::Matrix imageInvertMatrix;
    if (!imageMatrix.Invert(imageInvertMatrix)) {
        LOGE("GEDirectionLightShaderFilter::OnProcessImage Invert imageMatrix failed.");
        return image;
    }
    auto imageShader = Drawing::ShaderEffect::CreateImageShader(*image, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), imageInvertMatrix);
    lightingBuilder->SetChild("image", imageShader);
    lightingBuilder->SetUniform("iResolution", canvasInfo_.geoWidth, canvasInfo_.geoHeight);
    lightingBuilder->SetUniform("lightDirection",
        params_.lightDirection.x_, params_.lightDirection.y_, -params_.lightDirection.z_);
    lightingBuilder->SetUniformVec4("lightColor",
        params_.lightColor.x_, params_.lightColor.y_, params_.lightColor.z_, params_.lightColor.w_);
    lightingBuilder->SetUniform("lightIntensity", params_.lightIntensity);

#ifdef RS_ENABLE_GPU
    auto lightingImage = lightingBuilder->MakeImage(canvas.GetGPUContext().get(), &imageMatrix, imageInfo, false);
#else
    auto lightingImage = lightingBuilder->MakeImage(nullptr, &imageMatrix, imageInfo, false);
#endif
    if (lightingImage == nullptr) {
        LOGE("GEDirectionLightShaderFilter::OnProcessImage make lightingImage failed");
        return image;
    }
    return lightingImage;
}

std::shared_ptr<Drawing::RuntimeEffect> GEDirectionLightShaderFilter::GetNormalMaskEffect()
{
    static std::shared_ptr<Drawing::RuntimeEffect> g_normalMaskShaderEffect = nullptr;
    if (g_normalMaskShaderEffect == nullptr) {
        g_normalMaskShaderEffect = Drawing::RuntimeEffect::CreateForShader(g_shaderStringNormalMask);
    }
    return g_normalMaskShaderEffect;
}

std::shared_ptr<Drawing::RuntimeEffect> GEDirectionLightShaderFilter::GetDirectionLightEffect()
{
    static std::shared_ptr<Drawing::RuntimeEffect> g_directionLightShaderEffect = nullptr;
    if (g_directionLightShaderEffect == nullptr) {
        g_directionLightShaderEffect = Drawing::RuntimeEffect::CreateForShader(g_shaderStringDirectionLight);
    }
    return g_directionLightShaderEffect;
}

std::shared_ptr<Drawing::RuntimeEffect> GEDirectionLightShaderFilter::GetDirectionLightNoNormalEffect()
{
    static std::shared_ptr<Drawing::RuntimeEffect> g_directionLightNoNormalShaderEffect = nullptr;
    if (g_directionLightNoNormalShaderEffect == nullptr) {
        g_directionLightNoNormalShaderEffect =
            Drawing::RuntimeEffect::CreateForShader(g_shaderStringDirectionLightNoNormal);
    }
    return g_directionLightNoNormalShaderEffect;
}

} // namespace Rosen
} // namespace OHOS