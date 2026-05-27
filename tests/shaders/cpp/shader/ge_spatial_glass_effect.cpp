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

#include "ge_spatial_glass_effect.h"

#include <cmath>
#include "ge_log.h"
#include "ge_visual_effect_impl.h"
#include "ge_sdf_rrect_shader_shape.h"

namespace OHOS {
namespace Rosen {

static constexpr char MAIN_SHADER_PROG[] = R"(
    uniform vec2 iResolution;
    uniform shader baseBlurImg;
    uniform shader sdfNormalImg;

    float Get2DSDF(vec2 uv) {
        return sdfNormalImg.eval(uv).a / iResolution.x;
    }

    half4 main(vec2 fragCoord) {
        vec2 uv = fragCoord.xy / iResolution.xy;
        float sd = Get2DSDF(fragCoord.xy);

        return baseBlurImg.eval(fragCoord) * smoothstep(-1.0, 1.0, -sd);
    }
)";

GESpatialGlassEffect::GESpatialGlassEffect(const Drawing::GESpatialGlassEffectParams& params)
{
    spatialGlassEffectParams_ = params;
}

void GESpatialGlassEffect::MakeDrawingShader(const Drawing::Rect& rect, float progress)
{
    drShader_ = nullptr;
    if ((rect.GetWidth() < 1e-6f || rect.GetHeight() < 1e-6f)) {
        return;
    }

    std::shared_ptr<Drawing::Image> cachedBlurImage = spatialGlassEffectParams_.blurImage.lock();
    if (cachedBlurImage == nullptr) {
        GE_LOGE("GESpatialGlassEffect::MakeDrawingShader cachedBlurImage is nullptr");
        return;
    }

    auto baseBlurShader = Drawing::ShaderEffect::CreateImageShader(*cachedBlurImage, Drawing::TileMode::CLAMP,
        Drawing::TileMode::CLAMP, Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), Drawing::Matrix());
    if (baseBlurShader == nullptr) {
        GE_LOGE("GESpatialGlassEffect::create shader failed.");
        return;
    }

    auto builder = MakeSpatialGlassShader(baseBlurShader, rect);
    if (builder == nullptr) {
        GE_LOGE("GESpatialGlassEffect::OnProcessImage builder is null");
        return;
    }

    auto spatialGlassShader = builder->MakeShader(nullptr, false);
    if (spatialGlassShader == nullptr) {
        GE_LOGE("GESpatialGlassEffect::MakeDrawingShader spatialGlassShader is nullptr");
        return;
    }
    drShader_ = spatialGlassShader;
}

thread_local static std::shared_ptr<Drawing::RuntimeEffect> g_spatialGlassShaderEffect = nullptr;

bool GESpatialGlassEffect::InitSpatialGlassEffect()
{
    if (g_spatialGlassShaderEffect == nullptr) {
        g_spatialGlassShaderEffect = Drawing::RuntimeEffect::CreateForShader(MAIN_SHADER_PROG);
        if (g_spatialGlassShaderEffect == nullptr) {
            GE_LOGE("InitSpatialGlassEffect::RuntimeShader effect error\n");
            return false;
        }
    }
    return true;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESpatialGlassEffect::MakeSpatialGlassShader(
    std::shared_ptr<Drawing::ShaderEffect> imageShader, const Drawing::Rect& rect)
{
    float imageWidth = rect.GetWidth();
    float imageHeight = rect.GetHeight();

    if (g_spatialGlassShaderEffect == nullptr) {
        if (!InitSpatialGlassEffect()) {
            GE_LOGE("GESpatialGlassEffect::failed when initializing MagnifierEffect.");
            return nullptr;
        }
    }

    std::shared_ptr<Drawing::ShaderEffect> sdfNormalShader;
    if (auto shape = spatialGlassEffectParams_.sdfShape) {
        sdfNormalShader = shape->GenerateDrawingShaderHasNormal(imageWidth, imageHeight);
    } else {
        GE_LOGE("GESpatialGlassEffect::MakeSpatialGlassShader sdfShapeShader is null");
        return nullptr;
    }

    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder =
        std::make_shared<Drawing::RuntimeShaderBuilder>(g_spatialGlassShaderEffect);
    // Common inputs
    builder->SetUniform("iResolution", imageWidth, imageHeight);
    builder->SetChild("sdfNormalImg", sdfNormalShader);
    builder->SetChild("baseBlurImg", imageShader);
    return builder;
}

} // namespace Rosen
} // namespace OHOS