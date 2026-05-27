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

#include "ge_color_gradient_effect.h"

#include <cmath>
#include "ge_common.h"
#include "ge_log.h"
#include "ge_visual_effect_impl.h"
#include "ge_tone_mapping_helper.h"

namespace OHOS {
namespace Rosen {

namespace {
thread_local static std::shared_ptr<Drawing::RuntimeEffect> g_colorGradientShaderEffect_ = nullptr;
thread_local static std::shared_ptr<Drawing::RuntimeEffect> g_colorGradientShaderEffectHasMask_ = nullptr;
thread_local std::shared_ptr<Drawing::RuntimeEffect> g_brightnessShaderEffect_ = nullptr;

constexpr static size_t GRADIENT_POSITION_CHANNEL = 2;
constexpr static size_t GRADIENT_ARRAY_NUM = 12;
constexpr static size_t GRADIENT_COLOR_CHANNEL = 4;
constexpr static size_t COLOR_R_CHANNEL = 0;
constexpr static size_t COLOR_G_CHANNEL = 1;
constexpr static size_t COLOR_B_CHANNEL = 2;
constexpr static size_t COLOR_A_CHANNEL = 3;

const std::string COLOR_GRADIENT_SHADER_HEAD(
    R"(
    const int NUM_COLORS = 12;
    uniform half2 iResolution;
    uniform half4 blend_c[NUM_COLORS];
    uniform half2 gradient_p[NUM_COLORS];
    uniform half gradient_s[NUM_COLORS];
    uniform half factor[NUM_COLORS];
    )");

const std::string COLOR_GRADIENT_SHADER_END(
    R"(
    half4 main(vec2 fragCoord)
    {
        half2 uv = fragCoord / iResolution.xy;
        half4 sum = interpolate(uv);
        sum.rgb += screenSpaceDither(fragCoord, 50.0).rgb;
        return half4(sum.rgb, 1.0) * sum.a;
    }
    )");

const std::string COLOR_GRADIENT_SHADER_WITH_MASK_HEAD(
    R"(
    const int NUM_COLORS = 12;
    uniform half2 iResolution;
    uniform half4 blend_c[NUM_COLORS];
    uniform half2 gradient_p[NUM_COLORS];
    uniform half gradient_s[NUM_COLORS];
    uniform shader maskImageShader;
    uniform half factor[NUM_COLORS];
    )");

const std::string COLOR_GRADIENT_SHADER_WITH_MASK_END(
    R"(
    half4 main(vec2 fragCoord)
    {
        half maskValue = maskImageShader.eval(fragCoord).a;
        if (maskValue < 0.0) {
            return half4(0.0);
        }
        half2 uv = fragCoord / iResolution.xy;
        half4 sum = interpolate(uv);
        sum.rgb += screenSpaceDither(fragCoord, 50.0).rgb;
        return half4(sum.rgb, 1.0) * sum.a * maskValue;
    }
    )");

// approximate calculation exp(-x) = (n/(x+n))^n, n = 16
const std::string COLOR_GRADIENT_SHADER_COMMN(
    R"(
    float calWeight(in half2 uv, in half2 pos, in half strength, in half f)
    {
        float a = 15.5;
        float d = a / (distance(uv, pos) * strength + a);
        d *= d;
        d *= d;
        d *= d;
        d *= d;
        return f * d;
    }

    half4 interpolate(in half2 uv)
    {
        float weights[NUM_COLORS];
        float weightSum = 0.0;

        weights[0] = calWeight(uv, gradient_p[0], gradient_s[0], factor[0]);
        weightSum += weights[0];

        weights[1] = calWeight(uv, gradient_p[1], gradient_s[1], factor[1]);
        weightSum += weights[1];

        weights[2] = calWeight(uv, gradient_p[2], gradient_s[2], factor[2]);
        weightSum += weights[2];

        weights[3] = calWeight(uv, gradient_p[3], gradient_s[3], factor[3]);
        weightSum += weights[3];

        weights[4] = calWeight(uv, gradient_p[4], gradient_s[4], factor[4]);
        weightSum += weights[4];

        weights[5] = calWeight(uv, gradient_p[5], gradient_s[5], factor[5]);
        weightSum += weights[5];

        weights[6] = calWeight(uv, gradient_p[6], gradient_s[6], factor[6]);
        weightSum += weights[6];

        weights[7] = calWeight(uv, gradient_p[7], gradient_s[7], factor[7]);
        weightSum += weights[7];

        weights[8] = calWeight(uv, gradient_p[8], gradient_s[8], factor[8]);
        weightSum += weights[8];

        weights[9] = calWeight(uv, gradient_p[9], gradient_s[9], factor[9]);
        weightSum += weights[9];

        weights[10] = calWeight(uv, gradient_p[10], gradient_s[10], factor[10]);
        weightSum += weights[10];

        weights[11] = calWeight(uv, gradient_p[11], gradient_s[11], factor[11]);
        weightSum += weights[11];

        half4 result = half4(0.0);
        result += blend_c[0] * weights[0] + blend_c[1] * weights[1] +
            blend_c[2] * weights[2] + blend_c[3] * weights[3] + blend_c[4] * weights[4] +
            blend_c[5] * weights[5] + blend_c[6] * weights[6] + blend_c[7] * weights[7] +
            blend_c[8] * weights[8] + blend_c[9] * weights[9] + blend_c[10] * weights[10] +
            blend_c[11] * weights[11];
        
        result /= weightSum;
        return result;
    }

    half3 screenSpaceDither(half2 vScreenPos, float colorDepth)
    {
        vec3 dither = vec3(dot(vec2(171.0, 231.0), vScreenPos.xy));
        dither.rgb = fract(dither.rgb / vec3(103.0, 71.0, 97.0));
        return half3(dither.rgb / colorDepth);
    }
    )");

const std::string BRIGHTNESS_SHADER_CODE(
    R"(
        uniform float brightness;
        uniform shader colorGradientShader;
        half4 main(vec2 fragCoord)
        {
            half4 color = colorGradientShader.eval(fragCoord);
            color = unpremul(color);
            color.rgb += brightness;
            color.rgb *= color.a;
            return color;
        }
    )");

const std::string COLOR_GRADIENT_SHADER_STRING = COLOR_GRADIENT_SHADER_HEAD +
    COLOR_GRADIENT_SHADER_COMMN + COLOR_GRADIENT_SHADER_END;
const std::string COLOR_GRADIENT_SHADER_STRING_WITH_MASK = COLOR_GRADIENT_SHADER_WITH_MASK_HEAD +
    COLOR_GRADIENT_SHADER_COMMN + COLOR_GRADIENT_SHADER_WITH_MASK_END;

void MakeColorGradientEffect()
{
    if (g_colorGradientShaderEffect_ != nullptr) {
        return;
    }
    g_colorGradientShaderEffect_ = Drawing::RuntimeEffect::CreateForShader(COLOR_GRADIENT_SHADER_STRING);
    if (g_colorGradientShaderEffect_ == nullptr) {
        LOGE("GEColorGradientEffect::MakeColorGradientEffect colorGradientShaderEffect create failed");
    }
}

void MakeColorGradientEffectWithMask()
{
    if (g_colorGradientShaderEffectHasMask_ != nullptr) {
        return;
    }
    g_colorGradientShaderEffectHasMask_ =
        Drawing::RuntimeEffect::CreateForShader(COLOR_GRADIENT_SHADER_STRING_WITH_MASK);
    if (g_colorGradientShaderEffectHasMask_ == nullptr) {
        LOGE("GEColorGradientEffect::MakeColorGradientEffectWithMask colorGradientShaderEffect create failed");
    }
}

void MakeBrightnessEffect()
{
    if (g_brightnessShaderEffect_ != nullptr) {
        return;
    }
    g_brightnessShaderEffect_ =
        Drawing::RuntimeEffect::CreateForShader(BRIGHTNESS_SHADER_CODE);
    if (g_brightnessShaderEffect_ == nullptr) {
        LOGE("GEColorGradientEffect::MakeBrightnessEffect brightnessShaderEffect create failed");
    }
}
} // namespace

GEColorGradientEffect::GEColorGradientEffect(const Drawing::GEXColorGradientEffectParams& param)
{
    paramColorSize_ = static_cast<size_t>(std::max(param.colorNum_, 0.0f));
    blendColors_ = std::vector<Drawing::Color4f>(GRADIENT_ARRAY_NUM, {0.0f, 0.0f, 0.0f, 0.0f});
    auto size = blendColors_.size();
    for (size_t i = 0; i < size; i++) {
        colors_.push_back(param.colors_[i]);
        positions_.push_back(param.positions_[i]);
        strengths_.push_back(param.strengths_[i]);
    }
    mask_ = param.mask_;
    gradientBlend_ = param.blend_;
    gradientBlendK_ = param.blendk_;
    brightness_ = std::clamp(param.brightness_, -1.0f, 1.0f);
}

float GEColorGradientEffect::CalculateCompressRatio()
{
    // Do tone mapping when enable edr effect
    if (!GEToneMappingHelper::NeedToneMapping(supportHeadroom_)) {
        return 1.0f;
    }
    float highColor = 1.0f;
    auto size = colors_.size();
    for (size_t index = 0; index < size; index++) {
        float maxRgb = std::max(std::max(colors_[index].redF_, colors_[index].greenF_),
            colors_[index].blueF_);
        if (highColor < maxRgb) {
            highColor = maxRgb;
        }
    }
    return GEToneMappingHelper::GetBrightnessMapping(supportHeadroom_, highColor) / highColor;
}

void GEColorGradientEffect::CalculateBlenderCol()
{
    if (paramColorSize_ > static_cast<size_t>(GRADIENT_ARRAY_NUM)) {
        LOGE("GEColorGradientEffect::CalculateBlenderCol color size out of bounds %{public}zu", paramColorSize_);
        return;
    }
    const float ratio = CalculateCompressRatio();
    for (size_t i = 0; i < paramColorSize_; i++) {
        std::vector<float> weights(paramColorSize_, 0.0f);
        float weightSum = 0.0f;
        for (size_t j = 0; j < paramColorSize_; j++) {
            float x2 = std::pow(positions_[i].GetX() - positions_[j].GetX(), 2);
            float y2 = std::pow(positions_[i].GetY() - positions_[j].GetY(), 2);
            float dist = std::sqrt(x2 + y2);
            weights[j] = 1.0 / (1.0 + exp(gradientBlendK_ * dist - gradientBlend_));
            weightSum += weights[j];
        }
        Drawing::Color4f mixedColor = {0.0f, 0.0f, 0.0f, 0.0f};
        for (size_t k = 0; k < paramColorSize_; k++) {
            weights[k] /= weightSum;
            mixedColor.redF_ += ratio * colors_[k].redF_ * weights[k];
            mixedColor.greenF_ += ratio * colors_[k].greenF_ * weights[k];
            mixedColor.blueF_ += ratio * colors_[k].blueF_ * weights[k];
            mixedColor.alphaF_ += colors_[k].alphaF_ * weights[k];
        }
        blendColors_[i] = mixedColor;
    }
}

bool GEColorGradientEffect::CreateShaderEffect(float width, float height)
{
    if (width < 1e-6 || height < 1e-6) {
        LOGE("GEColorGradientEffect::CreateShaderEffect width or height less than 1e-6");
        return false;
    }
    if (colors_.size() != positions_.size() || colors_.size() != strengths_.size() ||
        strengths_.size() != GRADIENT_ARRAY_NUM) {
        LOGE("GEColorGradientEffect::CreateShaderEffect colors, positions, or strengths size mismatch");
        return false;
    }
    bool isCreated = false;
    if (mask_) {
        MakeColorGradientEffectWithMask();
        isCreated = g_colorGradientShaderEffectHasMask_ != nullptr;
    } else {
        MakeColorGradientEffect();
        isCreated = g_colorGradientShaderEffect_ != nullptr;
    }
    MakeBrightnessEffect();
    isCreated = isCreated && (g_brightnessShaderEffect_ != nullptr);
    return isCreated;
}

void GEColorGradientEffect::SetUniform(float width, float height)
{
    builder_ = mask_ == nullptr ? std::make_shared<Drawing::RuntimeShaderBuilder>(g_colorGradientShaderEffect_) :
        std::make_shared<Drawing::RuntimeShaderBuilder>(g_colorGradientShaderEffectHasMask_);
    builder_->SetUniform("iResolution", width, height);
    CalculateBlenderCol();
    std::vector<float> blendColorArray(GRADIENT_ARRAY_NUM * GRADIENT_COLOR_CHANNEL, 0.0f);
    size_t size = std::min(paramColorSize_, GRADIENT_ARRAY_NUM);
    for (size_t i = 0; i < size; i++) {
        blendColorArray[i * GRADIENT_COLOR_CHANNEL + COLOR_R_CHANNEL] = blendColors_[i].redF_;
        blendColorArray[i * GRADIENT_COLOR_CHANNEL + COLOR_G_CHANNEL] = blendColors_[i].greenF_;
        blendColorArray[i * GRADIENT_COLOR_CHANNEL + COLOR_B_CHANNEL] = blendColors_[i].blueF_;
        blendColorArray[i * GRADIENT_COLOR_CHANNEL + COLOR_A_CHANNEL] = blendColors_[i].alphaF_;
    }

    float* blendColorArrayPtr = blendColorArray.data();
    builder_->SetUniform("blend_c", blendColorArrayPtr, GRADIENT_ARRAY_NUM * GRADIENT_COLOR_CHANNEL);

    std::vector<float> positionArray(GRADIENT_ARRAY_NUM * GRADIENT_POSITION_CHANNEL);
    for (size_t i = 0; i < GRADIENT_ARRAY_NUM; i++) {
        positionArray[i * GRADIENT_POSITION_CHANNEL + 0] = positions_[i].GetX();
        positionArray[i * GRADIENT_POSITION_CHANNEL + 1] = positions_[i].GetY();
    }
    float* positionArrayPtr = positionArray.data();
    builder_->SetUniform("gradient_p", positionArrayPtr, GRADIENT_ARRAY_NUM * GRADIENT_POSITION_CHANNEL);

    float* strengthArrayPtr = strengths_.data();
    builder_->SetUniform("gradient_s", strengthArrayPtr, GRADIENT_ARRAY_NUM);

    std::vector<float> factorArray(GRADIENT_ARRAY_NUM);
    for (size_t i = 0; i < GRADIENT_ARRAY_NUM; i++) {
        factorArray[i] = i < paramColorSize_ ? 1.0f : 0.0f;
    }
    float* factorArrayPtr = factorArray.data();
    builder_->SetUniform("factor", factorArrayPtr, GRADIENT_ARRAY_NUM);
}

void GEColorGradientEffect::MakeDrawingShader(const Drawing::Rect& rect, float progress)
{
    auto width = rect.GetWidth();
    auto height = rect.GetHeight();
    if (!CreateShaderEffect(width, height)) {
        drShader_ = nullptr;
        return;
    }
    SetUniform(width, height);

    if (mask_) {
        auto maskImageShader_ = mask_->GenerateDrawingShader(width, height);
        if (!maskImageShader_) {
            LOGE("GEColorGradientEffect::MakeDrawingShader maskImageShader_ is nullptr");
            drShader_ = nullptr;
            return;
        }
        builder_->SetChild("maskImageShader", maskImageShader_);
    }
    auto colorGradientShader = builder_->MakeShader(nullptr, false);
    if (!colorGradientShader) {
        LOGE("GEColorGradientEffect::MakeDrawingShader colorGradientShader is nullptr");
        drShader_ = nullptr;
        return;
    }
    drShader_ = colorGradientShader;
    if (!GE_EQ(brightness_, 0.0f, 0.0001f)) {
        auto brightnessBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(g_brightnessShaderEffect_);
        brightnessBuilder->SetUniform("brightness", brightness_);
        brightnessBuilder->SetChild("colorGradientShader", colorGradientShader);
        auto brightnessShader = brightnessBuilder->MakeShader(nullptr, false);
        if (!brightnessShader) {
            LOGE("GEColorGradientEffect::MakeDrawingShader brightnessShader is nullptr");
            drShader_ = nullptr;
            return;
        }
        drShader_ = brightnessShader;
    }
}

Drawing::Rect GEColorGradientEffect::GetSubtractedRect(float width, float height) const
{
    return mask_ ? mask_->GetSubtractedRect(width, height) : Drawing::Rect();
}

} // namespace Rosen
} // namespace OHOS