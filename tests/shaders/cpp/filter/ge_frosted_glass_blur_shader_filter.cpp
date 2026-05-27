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
#include "ge_frosted_glass_blur_shader_filter.h"

#include "ge_image_cache_provider.h"
#include "ge_log.h"
#include "ge_mesa_blur_shader_filter.h"

namespace OHOS {
namespace Rosen {

GEFrostedGlassBlurShaderFilter::GEFrostedGlassBlurShaderFilter(
    const Drawing::GEFrostedGlassBlurShaderFilterParams& params)
{
    blurParams_ = params;
}

std::shared_ptr<Drawing::Image> GEFrostedGlassBlurShaderFilter::OnProcessImage(Drawing::Canvas& canvas,
    const std::shared_ptr<Drawing::Image> image, const Drawing::Rect& src, const Drawing::Rect& dst)
{
    if (image == nullptr) {
        GE_LOGE("GEFrostedGlassBlurShaderFilter::OnProcessImage input is invalid");
        return nullptr;
    }

    Drawing::GEMESABlurShaderFilterParams params;
    params.radius = blurParams_.radius;
    GEMESABlurShaderFilter blurFilter(params);

    std::shared_ptr<Drawing::Image> blurImage = image;
    if (blurParams_.radius > 0) {
        blurImage = blurFilter.OnProcessImageWithoutUpSampling(canvas, image, src, dst);
    }

    Drawing::SamplingOptions linear(Drawing::FilterMode::LINEAR, Drawing::MipmapMode::NONE);
    const float factor = 1.0f / (std::abs(blurParams_.radiusScale) > std::numeric_limits<float>::epsilon() ?
        blurParams_.radiusScale : 1.0f);

    GEImageCache tmp;

    tmp.data = blurFilter.DownSamplingForEdge(canvas, blurImage, src, linear, factor);
    if (cacheProvider_ != nullptr) {
        cacheProvider_->Store(tmp);
    }

    return blurImage;
}

void GEFrostedGlassBlurShaderFilter::SetCacheProvider(IGECacheProvider* cacheProvider)
{
    cacheProvider_ = cacheProvider;
}
} // namespace Rosen
} // namespace OHOS