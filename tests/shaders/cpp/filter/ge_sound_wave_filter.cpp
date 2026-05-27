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

#include "ge_sound_wave_filter.h"

#include <algorithm>
#include <chrono>
 
#include "ge_log.h"
#include "ge_tone_mapping_helper.h"
 
namespace OHOS {
namespace Rosen {

namespace {
constexpr static uint8_t COLOR_CHANNEL = 3; // 3 len of rgb

inline void MultiplyColor4f(Drawing::Color4f& color, float ratio)
{
    color.redF_ *= ratio;
    color.greenF_ *= ratio;
    color.blueF_ *= ratio;
}
} // namespace

GESoundWaveFilter::GESoundWaveFilter(const Drawing::GESoundWaveFilterParams& params)
    : colorProgress_(params.colorProgress), soundIntensity_(params.soundIntensity),
      shockWaveAlphaA_(params.shockWaveAlphaA), shockWaveAlphaB_(params.shockWaveAlphaB),
      shockWaveProgressA_(params.shockWaveProgressA), shockWaveProgressB_(params.shockWaveProgressB),
      shockWaveTotalAlpha_(params.shockWaveTotalAlpha)
{
    colorA_ = params.colorA;
    colorB_ = params.colorB;
    colorC_ = params.colorC;
}
 
std::shared_ptr<Drawing::Image> GESoundWaveFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (image == nullptr) {
        LOGE("GESoundWaveFilter::OnProcessImage input is invalid");
        return nullptr;
    }
 
    Drawing::Matrix matrix = canvasInfo_.mat;
    matrix.PostTranslate(-canvasInfo_.tranX, -canvasInfo_.tranY);
    Drawing::Matrix invertMatrix;
    if (!matrix.Invert(invertMatrix)) {
        LOGE("GESoundWaveFilter::ProcessImage Invert matrix failed");
        return image;
    }
    auto shader = Drawing::ShaderEffect::CreateImageShader(*image, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), invertMatrix);
    auto imageInfo = image->GetImageInfo();
    float height = imageInfo.GetHeight();
    float width = imageInfo.GetWidth();
    if (height < 1e-6 || width < 1e-6) {
        return image;
    }
    auto soundWaveShader = GetSoundWaveEffect();
    if (soundWaveShader == nullptr) {
        LOGE("GESoundWaveFilter::OnProcessImage g_SoundWaveEffect init failed");
        return image;
    }

    CheckSoundWaveParams();
    float colorA[COLOR_CHANNEL] = {colorA_.redF_, colorA_.greenF_, colorA_.blueF_};
    float colorB[COLOR_CHANNEL] = {colorB_.redF_, colorB_.greenF_, colorB_.blueF_};
    float colorC[COLOR_CHANNEL] = {colorC_.redF_, colorC_.greenF_, colorC_.blueF_};

    Drawing::RuntimeShaderBuilder builder(soundWaveShader);
    builder.SetChild("image", shader);
    builder.SetUniform("iResolution", canvasInfo_.geoWidth, canvasInfo_.geoHeight);
    builder.SetUniform("colorA", colorA, COLOR_CHANNEL);
    builder.SetUniform("colorB", colorB, COLOR_CHANNEL);
    builder.SetUniform("colorC", colorC, COLOR_CHANNEL);
    builder.SetUniform("colorProgress", colorProgress_);
    builder.SetUniform("soundIntensity", soundIntensity_);
    builder.SetUniform("shockWaveAlphaA", shockWaveAlphaA_);
    builder.SetUniform("shockWaveAlphaB", shockWaveAlphaB_);
    builder.SetUniform("shockWaveProgressA", shockWaveProgressA_);
    builder.SetUniform("shockWaveProgressB", shockWaveProgressB_);
    builder.SetUniform("shockWaveTotalAlpha", shockWaveTotalAlpha_);
 
    auto invertedImage = builder.MakeImage(canvas.GetGPUContext().get(), &(matrix), imageInfo, false);
    if (invertedImage == nullptr) {
        LOGE("GESoundWaveFilter::OnProcessImage make image failed");
        return image;
    }
    return invertedImage;
}

void GESoundWaveFilter::Preprocess(Drawing::Canvas& canvas, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    // Do tone mapping when enable edr effect
    if (!GEToneMappingHelper::NeedToneMapping(supportHeadroom_)) {
        return;
    }

    float highColor = std::max({colorA_.redF_, colorA_.greenF_, colorA_.blueF_, colorB_.redF_, colorB_.greenF_,
        colorB_.blueF_, colorC_.redF_, colorC_.greenF_, colorC_.blueF_});
    if (ROSEN_GNE(highColor, 1.0f)) {
        float compressRatio = GEToneMappingHelper::GetBrightnessMapping(supportHeadroom_, highColor) / highColor;
        MultiplyColor4f(colorA_, compressRatio);
        MultiplyColor4f(colorB_, compressRatio);
        MultiplyColor4f(colorC_, compressRatio);
    }
}

void GESoundWaveFilter::CheckSoundWaveParams()
{
    CheckSoundWaveColor4f(colorA_);
    CheckSoundWaveColor4f(colorB_);
    CheckSoundWaveColor4f(colorC_);
    soundIntensity_ = std::clamp(soundIntensity_, 0.0f, 1.0f);
    shockWaveAlphaA_ = std::clamp(shockWaveAlphaA_, 0.0f, 1.0f);
    shockWaveAlphaB_ = std::clamp(shockWaveAlphaB_, 0.0f, 1.0f);
    shockWaveTotalAlpha_ = std::clamp(shockWaveTotalAlpha_, 0.0f, 1.0f);
}

void GESoundWaveFilter::CheckSoundWaveColor4f(Drawing::Color4f& color)
{
    color.redF_ = std::clamp(color.redF_, 0.0f, 10.0f);
    color.greenF_ = std::clamp(color.greenF_, 0.0f, 10.0f);
    color.blueF_ = std::clamp(color.blueF_, 0.0f, 10.0f);
    color.alphaF_ = std::clamp(color.alphaF_, 0.0f, 1.0f);
}

std::shared_ptr<Drawing::RuntimeEffect> GESoundWaveFilter::GetSoundWaveEffect()
{
    static std::shared_ptr<Drawing::RuntimeEffect> g_soundWaveShader = nullptr;
    if (g_soundWaveShader == nullptr) {
        g_soundWaveShader = Drawing::RuntimeEffect::CreateForShader(shaderStringSoundWave);
    }
    return g_soundWaveShader;
}
} // namespace Rosen
} // namespace OHOS