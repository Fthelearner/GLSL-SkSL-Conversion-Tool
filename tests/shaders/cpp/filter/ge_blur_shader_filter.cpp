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

#include "ge_blur_shader_filter.h"

#include <algorithm>
#include <cmath>

#include "draw/surface.h"
#include "ge_log.h"
#include "ge_mesa_blur_shader_filter.h"

namespace OHOS {
namespace Rosen {

GEBlurShaderFilter::GEBlurShaderFilter(const Drawing::GEBlurShaderFilterParams& params)
{
    blurParams_ = params;
}

std::shared_ptr<Drawing::Image> GEBlurShaderFilter::ProcessImageWithMesa(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, int extension)
{
    auto surface = canvas.GetSurface();
    if (surface == nullptr || image == nullptr) {
        return image;
    }
    auto width = image->GetWidth() + extension * 2;
    auto height = image->GetHeight() + extension * 2;
    auto extensionSurface = surface->MakeSurface(width, height);
    if (extensionSurface == nullptr) {
        return image;
    }

    Drawing::Matrix matrix;
    matrix.Translate(extension, extension);
    Drawing::SamplingOptions linear(Drawing::FilterMode::LINEAR, Drawing::MipmapMode::NONE);
    auto extensionShader = Drawing::ShaderEffect::CreateImageShader(*image,
        Drawing::TileMode::DECAL, Drawing::TileMode::DECAL, linear, matrix);

    auto extensionCanvas = extensionSurface->GetCanvas();
    if (extensionCanvas == nullptr) {
        return image;
    }
    Drawing::Brush brush;
    brush.SetShaderEffect(extensionShader);
    extensionCanvas->DrawBackground(brush);

    auto extensionImage = extensionSurface->GetImageSnapshot();

    extensionCanvas->Clear(0x00000000);
    Drawing::GEMESABlurShaderFilterParams mesaParams;
    mesaParams.radius = blurParams_.radiusX;
    GEMESABlurShaderFilter mesaFilter(mesaParams);

    Drawing::Rect extensionSrc(0, 0, width, height);
    return mesaFilter.OnProcessImage(*extensionCanvas, extensionImage, extensionSrc, extensionSrc);
}

std::shared_ptr<Drawing::Image> GEBlurShaderFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (image == nullptr) {
        GE_LOGE("GEBlurShaderFilter::OnProcessImage input is invalid");
        return nullptr;
    }

    // Guard against NaN and infinity before static_cast
    if (std::isnan(blurParams_.radiusX) || !std::isfinite(blurParams_.radiusX)) {
        GE_LOGE("GEBlurShaderFilter::OnProcessImage param is invalid, radius: %{public}f", blurParams_.radiusX);
        return image;
    }

    Drawing::Rect srcRect(0, 0, image->GetWidth(), image->GetHeight());
    if (!blurParams_.expandDrawRegion) {
        Drawing::Rect dstRect = srcRect;
        Drawing::GEMESABlurShaderFilterParams mesaParams;
        mesaParams.radius = blurParams_.radiusX;
        GEMESABlurShaderFilter mesaFilter(mesaParams);
        return mesaFilter.OnProcessImage(canvas, image, srcRect, dstRect);
    }

    int extension = static_cast<int>(std::ceil(blurParams_.radiusX * 3));
    return ProcessImageWithMesa(canvas, image, srcRect, extension);
}
} // namespace Rosen
} // namespace OHOS
