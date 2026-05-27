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
#include "ge_log.h"
#include "ge_pixel_map_shader_mask.h"
#include "ge_shader_filter_params.h"
#include "platform/common/rs_log.h"
#include "utils/matrix.h"
#include "utils/rect.h"
#include "utils/sampling_options.h"
#include "utils/scalar.h"

namespace OHOS {
namespace Rosen {
namespace Drawing {

GEPixelMapShaderMask::GEPixelMapShaderMask(const GEPixelMapMaskParams& param) : param_(param)
{
    for (size_t i = 0; i < Vector4f::V4SIZE; ++i) {
        param_.fillColor.data_[i] = std::clamp(param_.fillColor.data_[i], 0.f, 1.f);
    }
}

bool GEPixelMapShaderMask::IsValid() const
{
    if (param_.image == nullptr) {
        LOGE("GEPixelMapShaderMask::IsValid image is nullptr");
        return false;
    }

    if (!param_.src.IsValid()) {
        LOGE("GEPixelMapShaderMask::IsValid src is invalid");
        return false;
    }

    if (ROSEN_LE(param_.src.GetWidth() * param_.image->GetWidth(), 0.f)) {
        LOGE("GEPixelMapShaderMask::IsValid src width is zero");
        return false;
    }

    if (ROSEN_LE(param_.src.GetHeight() * param_.image->GetHeight(), 0.f)) {
        LOGE("GEPixelMapShaderMask::IsValid src height is zero");
        return false;
    }

    return true;
}

const GEPixelMapMaskParams& GEPixelMapShaderMask::GetGEPixelMapMaskParams()
{
    return param_;
}

std::shared_ptr<ShaderEffect> GEPixelMapShaderMask::GenerateDrawingShader(float width, float height) const
{
    if (!IsValid()) {
        LOGE("GEPixelMapShaderMask::GenerateDrawingShader param is invalid");
        return nullptr;
    }
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = nullptr;
    builder = GetPixelMapShaderMaskBuilder();
    if (builder == nullptr) {
        LOGE("GEPixelMapShaderMask::GenerateDrawingShaderHas get builder error");
        return nullptr;
    }
    static const Drawing::SamplingOptions option(Drawing::FilterMode::LINEAR, Drawing::MipmapMode::NONE);
    Drawing::Matrix matrix;
    auto sx = param_.dst.GetWidth() * width / (param_.src.GetWidth() * param_.image->GetWidth());
    auto sy = param_.dst.GetHeight() * height / (param_.src.GetHeight() * param_.image->GetHeight());
    auto tx = param_.dst.left_ * width - param_.src.left_ * param_.image->GetWidth() * sx;
    auto ty = param_.dst.top_ * height - param_.src.top_ * param_.image->GetHeight() * sy;
    matrix.SetScaleTranslate(sx, sy, tx, ty);
    builder->SetChild("image", Drawing::ShaderEffect::CreateImageShader(*param_.image,
        Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP, option, matrix));
    builder->SetUniformVec4("dst",
        param_.dst.left_ * width, param_.dst.top_ * height, param_.dst.right_ * width, param_.dst.bottom_ * height);
    builder->SetUniformVec4("fillPixel",
        param_.fillColor.x_, param_.fillColor.y_, param_.fillColor.z_, param_.fillColor.w_);

    auto pixelMapMaskEffectShader = builder->MakeShader(nullptr, false);
    if (pixelMapMaskEffectShader == nullptr) {
        LOGE("GEPixelMapShaderMask::GenerateDrawingShader make shader failed");
        return nullptr;
    }
    return pixelMapMaskEffectShader;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GEPixelMapShaderMask::GetPixelMapShaderMaskBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> pixelMapShaderMaskBuilder = nullptr;
    if (pixelMapShaderMaskBuilder != nullptr) {
        return pixelMapShaderMaskBuilder;
    }

    static constexpr char prog[] = R"(
        uniform shader image;
        uniform vec4 dst;
        uniform vec4 fillPixel;

        vec4 main(float2 fragCoord)
        {
            if (dst.x < fragCoord.x && fragCoord.x < dst.z &&
                dst.y < fragCoord.y && fragCoord.y < dst.w) {
                return image.eval(fragCoord);
            }

            return fillPixel;
        }
    )";

    auto pixelMapShaderMaskEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (pixelMapShaderMaskEffect == nullptr) {
        LOGE("GEPixelMapShaderMask::GetPixelMapShaderMaskBuilder effect error");
        return nullptr;
    }

    pixelMapShaderMaskBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(pixelMapShaderMaskEffect);
    return pixelMapShaderMaskBuilder;
}

std::shared_ptr<ShaderEffect> GEPixelMapShaderMask::GenerateDrawingShaderHasNormal(float width, float height) const
{
    return GenerateDrawingShader(width, height);
}


std::shared_ptr<Drawing::RuntimeShaderBuilder> GEPixelMapShaderMask::GetPixelMapShaderNormalMaskBuilder() const
{
    return GetPixelMapShaderMaskBuilder();
}

} // namespace Drawing
} // namespace Rosen
} // namespace OHOS