/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "ge_spatial_point_light.h"

#include <algorithm>

#include "ge_log.h"
#include "ge_visual_effect_impl.h"

#undef LOG_TAG
#define LOG_TAG "GESpatialPointLightShader"
namespace OHOS {
namespace Rosen {
namespace {
    constexpr int32_t COLOR_CHANNEL = 4;
    constexpr int32_t POSITION_DIMENSION = 3;

    // Shader without mask - full screen effect
    static constexpr char PROG_NO_MASK[] = R"(
        uniform vec2 iResolution;
        uniform vec3 lightPosition;
        uniform half4 lightColor;
        uniform half lightIntensity;
        uniform half attenuation;

        half4 main(vec2 fragCoord)
        {
            vec3 lightDirection = lightPosition - vec3(fragCoord.x, fragCoord.y, 0.0);
            vec3 halfwayDir = normalize(lightDirection);
            float spec = pow(max(dot(vec3(0.0, 0.0, 1.0), halfwayDir), 0.0), attenuation);

            vec4 fragColor = spec * lightIntensity * lightColor;
            return fragColor;
        }
    )";

    // Shader with mask - uses mask for lighting effect
    static constexpr char PROG_WITH_MASK[] = R"(
        uniform vec2 iResolution;
        uniform vec3 lightPosition;
        uniform half4 lightColor;
        uniform half lightIntensity;
        uniform half attenuation;
        uniform shader mask;

        half4 main(vec2 fragCoord)
        {
            half4 normalMap = mask.eval(fragCoord);
            if (normalMap.r <= 1e-4) { //minEpsilon in half is 2^-14
                return vec4(0.0);
            }
            vec3 lightDirection = lightPosition - vec3(fragCoord.x, fragCoord.y, 0.0);
            vec3 halfwayDir = normalize(lightDirection);
            float spec = pow(max(dot(vec3(0.0, 0.0, 1.0), halfwayDir), 0.0), attenuation);

            vec4 fragColor = spec * lightIntensity * lightColor * normalMap.r;
            return fragColor;
        }
    )";
    Drawing::RuntimeEffectOptions g_highpUVOption{ .useHighpLocalCoords = true };
}

GESpatialPointLightShader::GESpatialPointLightShader() {}

GESpatialPointLightShader::GESpatialPointLightShader(Drawing::GESpatialPointLightShaderParams& param)
{
    pointLightParams_ = param;
    GE_LOGD("ctor: intensity=%{public}f, attenuation=%{public}f",
        pointLightParams_.lightIntensity, pointLightParams_.attenuation);
}

void GESpatialPointLightShader::MakeDrawingShader(const Drawing::Rect& rect, float progress)
{
    drShader_ = MakeSpatialPointLightShader(rect);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESpatialPointLightShader::GetSpatialPointLightBuilderNoMask()
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> shaderBuilder;
    if (shaderBuilder) {
        return shaderBuilder;
    }
    auto effect = Drawing::RuntimeEffect::CreateForShader(PROG_NO_MASK, g_highpUVOption);
    if (effect == nullptr) {
        GE_LOGE("GetSpatialPointLightBuilderNoMask effect is nullptr.");
        return nullptr;
    }
    GE_LOGD("RuntimeEffect (no mask) created");
    shaderBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(effect);
    return shaderBuilder;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESpatialPointLightShader::GetSpatialPointLightBuilderWithMask()
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> shaderBuilder;
    if (shaderBuilder) {
        return shaderBuilder;
    }
    auto effect = Drawing::RuntimeEffect::CreateForShader(PROG_WITH_MASK, g_highpUVOption);
    if (effect == nullptr) {
        GE_LOGE("GetSpatialPointLightBuilderWithMask effect is nullptr.");
        return nullptr;
    }
    GE_LOGD("RuntimeEffect (with mask) created");
    shaderBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(effect);
    return shaderBuilder;
}

std::shared_ptr<Drawing::ShaderEffect> GESpatialPointLightShader::MakeSpatialPointLightShader(const Drawing::Rect& rect)
{
    auto width = rect.GetWidth();
    auto height = rect.GetHeight();
    if (width < 1e-6 || height < 1e-6) {
        GE_LOGE("MakeSpatialPointLightShader invalid rect size: width=%{public}f, height=%{public}f",
            width, height);
        return nullptr;
    }

    // Choose builder based on whether mask exists
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder;
    if (pointLightParams_.mask != nullptr) {
        builder = GetSpatialPointLightBuilderWithMask();
    } else {
        builder = GetSpatialPointLightBuilderNoMask();
    }

    if (builder == nullptr) {
        GE_LOGE("MakeSpatialPointLightShader builder is nullptr.");
        return nullptr;
    }
    builder->SetUniform("iResolution", width, height);
    builder->SetUniform("lightPosition", pointLightParams_.lightPosition.GetData(), POSITION_DIMENSION);
    builder->SetUniform("lightColor", pointLightParams_.lightColor.GetData(), COLOR_CHANNEL);
    builder->SetUniform("lightIntensity", pointLightParams_.lightIntensity);
    float clampedAttenuation = std::max(0.001f, pointLightParams_.attenuation);
    builder->SetUniform("attenuation", clampedAttenuation);

    // Only set mask child when mask exists
    if (pointLightParams_.mask != nullptr) {
        auto maskShader = pointLightParams_.mask->GenerateDrawingShader(width, height);
        if (maskShader == nullptr) {
            GE_LOGE("MakeSpatialPointLightShader mask shader is nullptr.");
            return nullptr;
        }
        builder->SetChild("mask", maskShader);
    }

    auto spatialPointLightShader = builder->MakeShader(nullptr, false);
    if (spatialPointLightShader == nullptr) {
        GE_LOGE("MakeSpatialPointLightShader shader is nullptr.");
        return nullptr;
    }
    return spatialPointLightShader;
}

} // namespace Rosen
} // namespace OHOS