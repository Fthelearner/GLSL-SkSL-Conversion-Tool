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

#include "common/rs_common_def.h"
#include "common/rs_vector4.h"
#include "effect/shader_effect.h"
#include "ge_image_shader_mask.h"
#include "ge_log.h"
#include "ge_shader_filter_params.h"
#include "platform/common/rs_log.h"
#include "utils/matrix.h"
#include "utils/rect.h"
#include "utils/sampling_options.h"
#include "utils/scalar.h"

namespace OHOS {
namespace Rosen {
namespace Drawing {

bool GEImageShaderMask::IsValid() const
{
    if (param_.image == nullptr) {
        LOGE("GEImageShaderMask::IsValid image is nullptr");
        return false;
    }

    return true;
}

std::shared_ptr<ShaderEffect> GEImageShaderMask::GenerateDrawingShader(float width, float height) const
{
    if (!IsValid()) {
        LOGE("GEImageShaderMask::GenerateDrawingShader param is invalid");
        return nullptr;
    }
    static const Drawing::SamplingOptions option(Drawing::FilterMode::LINEAR, Drawing::MipmapMode::NONE);
    auto imageMaskEffectShader = Drawing::ShaderEffect::CreateImageShader(*param_.image,
        Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, option, Drawing::Matrix());
    if (imageMaskEffectShader == nullptr) {
        LOGE("GEImageShaderMask::GenerateDrawingShader make shader failed");
        return nullptr;
    }
    return imageMaskEffectShader;
}

std::shared_ptr<ShaderEffect> GEImageShaderMask::GenerateDrawingShaderHasNormal(float width,
    float height) const
{
    return GenerateDrawingShader(width, height);
}

} // namespace Drawing
} // namespace Rosen
} // namespace OHOS
