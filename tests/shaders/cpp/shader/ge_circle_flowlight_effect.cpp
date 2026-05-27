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
#include "ge_circle_flowlight_effect.h"

#include "ge_log.h"
#include "ge_visual_effect_impl.h"

#include <cmath>

namespace OHOS {
namespace Rosen {
namespace {
thread_local std::shared_ptr<Drawing::RuntimeEffect> g_circleFlowlightEffect = nullptr;
thread_local std::shared_ptr<Drawing::RuntimeEffect> g_circleFlowlightEffectWithMask = nullptr;

const std::string CIRCLE_FLOWLIGHT_SHADER(
    R"(
    uniform half2 iResolution;
    uniform half4 color[4];
    uniform half4 rotationFrequency;
    uniform half4 rotationAmplitude;
    uniform half4 rotationSeed;
    uniform half4 gradientX;
    uniform half4 gradientY;
    uniform half strength[4];
    uniform half distortStrength;
    uniform half blendGradient;
    uniform half progress;

    float randWave(float frequency, float amplitude, float time, float seed)
    {
        // 78.233 is a random seed
        return amplitude * sin(dot(vec2(time, seed), vec2(frequency, 78.233)));
    }

    vec4 colorGradient(vec2 fragPos, float radius, float timeValue)
    {
        vec4 colorOne = color[0];
        vec4 colorTwo = color[1];
        vec4 colorThree = color[2];
        vec4 colorFour = color[3];

        if (radius >= 1.0) {
            return vec4(0.0);
        }

        float freqTime = -2.0 * timeValue;
        mat2 rotationMatrix = mat2(cos(freqTime), sin(freqTime), -sin(freqTime), cos(freqTime));
        vec2 gradientPos[4];
 
        float wave = randWave(rotationFrequency.x, rotationAmplitude.x, timeValue, rotationSeed.x);
        vec2 tempPos = rotationMatrix * (vec2(gradientX.x, gradientY.x) * 2.0 - 1.0);
        gradientPos[0] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;
 
        wave = randWave(rotationFrequency.y, rotationAmplitude.y, timeValue, rotationSeed.y);
        tempPos = rotationMatrix * (vec2(gradientX.y, gradientY.y) * 2.0 - 1.0);
        gradientPos[1] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;
 
        wave = randWave(rotationFrequency.z, rotationAmplitude.z, timeValue, rotationSeed.z);
        tempPos = rotationMatrix * (vec2(gradientX.z, gradientY.z) * 2.0 - 1.0);
        gradientPos[2] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;
 
        wave = randWave(rotationFrequency.w, rotationAmplitude.w, timeValue, rotationSeed.w);
        tempPos = rotationMatrix * (vec2(gradientX.w, gradientY.w) * 2.0 - 1.0);
        gradientPos[3] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;
 
        float distorted_radius = pow(radius, distortStrength);
        vec2 fragPosNew = fragPos / radius * distorted_radius;
        vec2 gradientUV = fragPosNew * 0.5 + 0.5;
        float gradientTotalWeight = 0.0;
        vec4 gradientInterpColor = vec4(0.0);
 
        float distance0 = max(0.01, length(gradientUV - gradientPos[0]));
        float weight0 = strength[0] / pow(distance0, blendGradient);
        gradientInterpColor += weight0 * colorOne;
        gradientTotalWeight += weight0;
 
        float distance1 = max(0.01, length(gradientUV - gradientPos[1]));
        float weight1 = strength[1] / pow(distance1, blendGradient);
        gradientInterpColor += weight1 * colorTwo;
        gradientTotalWeight += weight1;
 
        float distance2 = max(0.01, length(gradientUV - gradientPos[2]));
        float weight2 = strength[2] / pow(distance2, blendGradient);
        gradientInterpColor += weight2 * colorThree;
        gradientTotalWeight += weight2;
 
        float distance3 = max(0.01, length(gradientUV - gradientPos[3]));
        float weight3 = strength[3] / pow(distance3, blendGradient);
        gradientInterpColor += weight3 * colorFour;
        gradientTotalWeight += weight3;
 
        gradientInterpColor = pow(gradientInterpColor / gradientTotalWeight, vec4(1.0 / 2.2));
        return gradientInterpColor;
    }

    float sdf_circle(vec2 uv, vec2 centerPos, float radius)
    {
        return length(uv - centerPos) - radius;
    }

    half4 main(vec2 fragCoord) {
        vec2 fragPos = fragCoord.xy / iResolution.xy * 2.0 - 1.0;
        float radius = length(fragPos);
        vec4 gradient_color = colorGradient(fragPos, radius, progress);
        return gradient_color;
    }
    )");

const std::string CIRCLE_FLOWLIGHT_SHADER_WITH_MASK(
    R"(
    uniform shader maskImageShader;
    uniform half2 iResolution;
    uniform half4 color[4];
    uniform half4 rotationFrequency;
    uniform half4 rotationAmplitude;
    uniform half4 rotationSeed;
    uniform half4 gradientX;
    uniform half4 gradientY;
    uniform half progress;
    uniform half strength[4];
    uniform half distortStrength;
    uniform half blendGradient;
 
    float randWave(float frequency, float amplitude, float time, float seed)
    {
        // 78.233 is a random seed
        return amplitude * sin(dot(vec2(time, seed), vec2(frequency, 78.233)));
    }
 
    vec4 colorGradient(vec2 fragPos, float radius, float timeValue)
    {
        vec4 colorOne = color[0];
        vec4 colorTwo = color[1];
        vec4 colorThree = color[2];
        vec4 colorFour = color[3];
 
        if (radius >= 1.0) {
            return vec4(0.0);
        }
 
        float freqTime = -2.0 * timeValue;
        mat2 rotationMatrix = mat2(cos(freqTime), sin(freqTime), -sin(freqTime), cos(freqTime));
        vec2 gradientPos[4];
 
        float wave = randWave(rotationFrequency.x, rotationAmplitude.x, timeValue, rotationSeed.x);
        vec2 tempPos = rotationMatrix * (vec2(gradientX.x, gradientY.x) * 2.0 - 1.0);
        gradientPos[0] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;
 
        wave = randWave(rotationFrequency.y, rotationAmplitude.y, timeValue, rotationSeed.y);
        tempPos = rotationMatrix * (vec2(gradientX.y, gradientY.y) * 2.0 - 1.0);
        gradientPos[1] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;
 
        wave = randWave(rotationFrequency.z, rotationAmplitude.z, timeValue, rotationSeed.z);
        tempPos = rotationMatrix * (vec2(gradientX.z, gradientY.z) * 2.0 - 1.0);
        gradientPos[2] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;
 
        wave = randWave(rotationFrequency.w, rotationAmplitude.w, timeValue, rotationSeed.w);
        tempPos = rotationMatrix * (vec2(gradientX.w, gradientY.w) * 2.0 - 1.0);
        gradientPos[3] = (tempPos + wave * normalize(tempPos)) * 0.5 + 0.5;
 
        float distorted_radius = pow(radius, distortStrength);
        vec2 fragPosNew = fragPos / radius * distorted_radius;
        vec2 gradientUV = fragPosNew * 0.5 + 0.5;
        float gradientTotalWeight = 0.0;
        vec4 gradientInterpColor = vec4(0.0);
 
        float distance0 = max(0.01, length(gradientUV - gradientPos[0]));
        float weight0 = strength[0] / pow(distance0, blendGradient);
        gradientInterpColor += weight0 * colorOne;
        gradientTotalWeight += weight0;
 
        float distance1 = max(0.01, length(gradientUV - gradientPos[1]));
        float weight1 = strength[1] / pow(distance1, blendGradient);
        gradientInterpColor += weight1 * colorTwo;
        gradientTotalWeight += weight1;
 
        float distance2 = max(0.01, length(gradientUV - gradientPos[2]));
        float weight2 = strength[2] / pow(distance2, blendGradient);
        gradientInterpColor += weight2 * colorThree;
        gradientTotalWeight += weight2;
 
        float distance3 = max(0.01, length(gradientUV - gradientPos[3]));
        float weight3 = strength[3] / pow(distance3, blendGradient);
        gradientInterpColor += weight3 * colorFour;
        gradientTotalWeight += weight3;
 
        gradientInterpColor = pow(gradientInterpColor / gradientTotalWeight, vec4(1.0 / 2.2));
        return gradientInterpColor;
    }
 
    float sdf_circle(vec2 uv, vec2 centerPos, float radius)
    {
        return length(uv - centerPos) - radius;
    }

    half4 main(vec2 fragCoord) {
        half maskValue = maskImageShader.eval(fragCoord).a;
        if (maskValue < 0.0) {
            return half4(0.0);
        }

        vec2 fragPos = fragCoord.xy / iResolution.xy * 2.0 - 1.0;
        float radius = length(fragPos);
        vec4 gradient_color = colorGradient(fragPos, radius, progress);

        gradient_color *= maskValue;
        return gradient_color;
    }
    )");
} // namespace

GECircleFlowlightEffect::GECircleFlowlightEffect(Drawing::GECircleFlowlightEffectParams& param)
{
    for (int i = 0; i < ARRAY_NUM; i++) {
        colors_[i * COLOR_CHANNEL] = param.colors[i].x_;
        colors_[i * COLOR_CHANNEL + 1] = param.colors[i].y_;
        colors_[i * COLOR_CHANNEL + 2] = param.colors[i].z_; // 2: loop index
        colors_[i * COLOR_CHANNEL + 3] = param.colors[i].w_; // 3: loop index
    }
    rotationFrequency_ = param.rotationFrequency;
    rotationAmplitude_ = param.rotationAmplitude;
    rotationSeed_ = param.rotationSeed;
    gradientX_ = param.gradientX;
    gradientY_ = param.gradientY;
    progress_ = param.progress;
    mask_ = param.mask;
    strength_ = param.strength;
    distortStrength_ = param.distortStrength;
    blendGradient_ = param.blendGradient;
}

void GECircleFlowlightEffect::MakeCircleFlowlightEffect()
{
    if (g_circleFlowlightEffect != nullptr) {
        return;
    }
    g_circleFlowlightEffect = Drawing::RuntimeEffect::CreateForShader(CIRCLE_FLOWLIGHT_SHADER);
    if (g_circleFlowlightEffect == nullptr) {
        LOGE("GECircleFlowlightEffect::RuntimeShader circleFlowlightEffect create failed");
    }
}

void GECircleFlowlightEffect::MakeCircleFlowlightEffectWithMask()
{
    if (g_circleFlowlightEffectWithMask != nullptr) {
        return;
    }
    g_circleFlowlightEffectWithMask =
        Drawing::RuntimeEffect::CreateForShader(CIRCLE_FLOWLIGHT_SHADER_WITH_MASK);
    if (g_circleFlowlightEffectWithMask == nullptr) {
        LOGE("GECircleFlowlightEffect::RuntimeShader circleFlowlightEffectWithMask create failed");
    }
}

bool GECircleFlowlightEffect::IsValidParam(float width, float height)
{
    if (width < 1e-6 || height < 1e-6) {
        LOGE("GECircleFlowlightEffect::MakeDrawingShader width or height less than 1e-6");
        return false;
    }
    return true;
}

void GECircleFlowlightEffect::SetUniform(float width, float height)
{
    if (mask_) {
        builder_ = std::make_shared<Drawing::RuntimeShaderBuilder>(g_circleFlowlightEffectWithMask);
    } else {
        builder_ = std::make_shared<Drawing::RuntimeShaderBuilder>(g_circleFlowlightEffect);
    }

    builder_->SetUniform("iResolution", width, height);
    builder_->SetUniform("color", colors_.data(), COLOR_CHANNEL * ARRAY_NUM);
    builder_->SetUniform("rotationFrequency", rotationFrequency_.GetData(), ARRAY_NUM);
    builder_->SetUniform("rotationAmplitude", rotationAmplitude_.GetData(), ARRAY_NUM);
    builder_->SetUniform("rotationSeed", rotationSeed_.GetData(), ARRAY_NUM);
    builder_->SetUniform("gradientX", gradientX_.GetData(), ARRAY_NUM);
    builder_->SetUniform("gradientY", gradientY_.GetData(), ARRAY_NUM);
    builder_->SetUniform("progress", progress_);
    builder_->SetUniform("strength", strength_.GetData(), ARRAY_NUM);
    builder_->SetUniform("distortStrength", distortStrength_);
    builder_->SetUniform("blendGradient", blendGradient_);
}

void GECircleFlowlightEffect::MakeDrawingShader(const Drawing::Rect& rect, float progress)
{
    drShader_ = nullptr;
    auto width = rect.GetWidth();
    auto height = rect.GetHeight();
    if (!IsValidParam(width, height)) {
        return;
    }

    if (mask_) {
        MakeCircleFlowlightEffectWithMask();
        if (!g_circleFlowlightEffectWithMask) {
            return;
        }
    } else {
        MakeCircleFlowlightEffect();
        if (!g_circleFlowlightEffect) {
            return;
        }
    }

    SetUniform(width, height);
    if (mask_) {
        auto maskImageShader = mask_->GenerateDrawingShader(width, height);
        if (!maskImageShader) {
            LOGE("GECircleFlowlightEffect::MakeDrawingShader maskImageShader_ is null");
            return;
        }
        builder_->SetChild("maskImageShader", maskImageShader);
    }
    auto circleFlowlightShader = builder_->MakeShader(nullptr, false);
    if (circleFlowlightShader == nullptr) {
        LOGE("GECircleFlowlightEffect::MakeDrawingShader circleFlowlightShader is nullptr.");
        return;
    }
    drShader_ = circleFlowlightShader;
}

} // namespace Rosen
} // namespace OHOS