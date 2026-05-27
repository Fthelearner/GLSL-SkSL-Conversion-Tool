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

#include "ge_heat_distortion_filter.h"

#include <algorithm>

#include "ge_log.h"

namespace OHOS {
namespace Rosen {

GEHeatDistortionFilter::GEHeatDistortionFilter(const Drawing::GEHeatDistortionFilterParams& params)
    : intensity_(params.intensity), noiseScale_(params.noiseScale),
    riseWeight_(params.riseWeight), progress_(params.progress)
{}

std::shared_ptr<Drawing::Image> GEHeatDistortionFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (image == nullptr) {
        LOGE("GEHeatDistortionFilter::OnProcessImage input is invalid");
        return nullptr;
    }

    Drawing::Matrix matrix = canvasInfo_.mat;
    matrix.PostTranslate(-canvasInfo_.tranX, -canvasInfo_.tranY);
    Drawing::Matrix invertMatrix;
    if (!matrix.Invert(invertMatrix)) {
        LOGE("GEHeatDistortionFilter::OnProcessImage invert matrix failed");
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

    auto heatDistortionShader = GetHeatDistortionEffect();
    if (heatDistortionShader == nullptr) {
        LOGE("GEHeatDistortionFilter::OnProcessImage heatDistortion shader init failed");
        return image;
    }

    CheckHeatDistortionParams();

    Drawing::RuntimeShaderBuilder builder(heatDistortionShader);
    builder.SetChild("image", shader);
    builder.SetUniform("iResolution", width, height);
    builder.SetUniform("progress", progress_ * timeScale_);

    builder.SetUniform("intensity", intensity_ * intensityScale_);
    builder.SetUniform("noiseScale", noiseScale_);
    builder.SetUniform("riseWeight", riseWeight_);

    auto distortedImage = builder.MakeImage(canvas.GetGPUContext().get(), &(matrix), imageInfo, false);
    if (distortedImage == nullptr) {
        LOGE("GEHeatDistortionFilter::OnProcessImage make image failed");
        return image;
    }
    return distortedImage;
}

void GEHeatDistortionFilter::CheckHeatDistortionParams()
{
    intensity_ = std::clamp(intensity_, 0.0f, 1.0f);

    noiseScale_ = std::clamp(noiseScale_, 0.1f, 5.0f);
    riseWeight_ = std::clamp(riseWeight_, 0.0f, 1.0f);
    progress_ = std::clamp(progress_, 0.0f, 1.0f);
}

std::shared_ptr<Drawing::RuntimeEffect> GEHeatDistortionFilter::GetHeatDistortionEffect()
{
    static std::shared_ptr<Drawing::RuntimeEffect> g_heatDistortionShader = nullptr;
    if (g_heatDistortionShader == nullptr) {
        g_heatDistortionShader = Drawing::RuntimeEffect::CreateForShader(shaderStringHeatDistortion);
    }
    return g_heatDistortionShader;
}

} // namespace Rosen
} // namespace OHOS
