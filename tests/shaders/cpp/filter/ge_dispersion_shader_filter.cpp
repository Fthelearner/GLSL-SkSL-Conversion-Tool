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

#include "ge_dispersion_shader_filter.h"

#include <typeinfo>
#include "ge_log.h"
#include "effect/runtime_effect.h"

namespace OHOS {
namespace Rosen {

GEDispersionShaderFilter::GEDispersionShaderFilter(const Drawing::GEDispersionShaderFilterParams& params)
    : params_(params)
{}

std::shared_ptr<Drawing::Image> GEDispersionShaderFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (image == nullptr || params_.mask == nullptr) { return image; }

    auto imageInfo = image->GetImageInfo();
    if (imageInfo.GetHeight() < 1e-6 || imageInfo.GetWidth() < 1e-6) {
        return nullptr;
    }

    auto dispersionShader = GetDispersionEffect();
    if (dispersionShader == nullptr) {
        LOGE("GEDispersionShaderFilter::OnProcessImage dispersionShader init failed.");
        return image;
    }
    Drawing::RuntimeShaderBuilder builder(dispersionShader);

    Drawing::Matrix matrix = canvasInfo_.mat;
    matrix.PostTranslate(-canvasInfo_.tranX, -canvasInfo_.tranY);
    Drawing::Matrix invertMatrix;
    if (!matrix.Invert(invertMatrix)) {
        LOGE("GEDispersionShaderFilter::OnProcessImage invert Matrix failed.");
        return image;
    }

    float lowValue = std::min(canvasInfo_.geoWidth, canvasInfo_.geoHeight);
    float aspectX = (std::abs(lowValue) > 1e-6f) ? (canvasInfo_.geoWidth / lowValue) : 1.0f;
    float aspectY = (std::abs(lowValue) > 1e-6f) ? (-canvasInfo_.geoHeight / lowValue) : 1.0f;

    auto imageShader = Drawing::ShaderEffect::CreateImageShader(*image, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), invertMatrix);
    if (imageShader == nullptr) {
        LOGE("GEDispersionShaderFilter::OnProcessImage create imageShader failed.");
        return image;
    }
    builder.SetChild("image", imageShader);

    auto maskShader = params_.mask->GenerateDrawingShader(canvasInfo_.geoWidth, canvasInfo_.geoHeight);
    if (maskShader == nullptr) {
        LOGE("GEDispersionShaderFilter::OnProcessImage mask generate failed.");
        return image;
    }
    builder.SetChild("mask", maskShader);
    builder.SetUniform("iResolution", canvasInfo_.geoWidth, canvasInfo_.geoHeight);
    builder.SetUniform("opacity", params_.opacity);
    builder.SetUniform("redOffset", params_.redOffset.first / aspectX, params_.redOffset.second / aspectY);
    builder.SetUniform("greenOffset", params_.greenOffset.first / aspectX, params_.greenOffset.second / aspectY);
    builder.SetUniform("blueOffset", params_.blueOffset.first / aspectX, params_.blueOffset.second / aspectY);

#ifdef RS_ENABLE_GPU
    auto resultImage = builder.MakeImage(canvas.GetGPUContext().get(), &(matrix), imageInfo, false);
#else
    auto resultImage = builder.MakeImage(nullptr, &(matrix), imageInfo, false);
#endif
    if (resultImage == nullptr) {
        LOGE("GEDispersionShaderFilter::OnProcessImage make image failed.");
        return image;
    }
    return resultImage;
}

std::shared_ptr<Drawing::RuntimeEffect> GEDispersionShaderFilter::GetDispersionEffect()
{
    static std::shared_ptr<Drawing::RuntimeEffect> g_dispersionShader = nullptr;
    if (g_dispersionShader == nullptr) {
        g_dispersionShader = Drawing::RuntimeEffect::CreateForShader(g_shaderStringDispersion);
    }
    return g_dispersionShader;
}

} // namespace Rosen
} // namespace OHOS