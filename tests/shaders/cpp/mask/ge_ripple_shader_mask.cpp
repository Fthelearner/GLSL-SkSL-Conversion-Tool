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

#include <chrono>
 
#include "ge_log.h"
#include "ge_ripple_shader_mask.h"
#include "ge_trace.h"
 
namespace OHOS {
namespace Rosen {
namespace Drawing {

GERippleShaderMask::GERippleShaderMask(const GERippleShaderMaskParams& param) : param_(param) {}

std::shared_ptr<ShaderEffect> GERippleShaderMask::GenerateDrawingShader(float width, float height) const
{
    GE_TRACE_NAME_FMT("GERippleShaderMask::GenerateDrawingShader, Type: %s, Width: %g, Height: %g",
        Drawing::GE_MASK_RIPPLE, width, height);
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = nullptr;
    builder = GetRippleShaderMaskBuilder();
    if (!builder) {
        LOGE("GERippleShaderMask::GenerateDrawingShaderHas builder error");
        return nullptr;
    }
    builder->SetUniform("iResolution", width, height);
    builder->SetUniform("centerPos", param_.center_.first, param_.center_.second);
    builder->SetUniform("rippleRadius", param_.radius_);
    builder->SetUniform("rippleWidth", param_.width_);
    builder->SetUniform("widthCenterOffset", param_.widthCenterOffset_);
    auto rippleMaskEffectShader = builder->MakeShader(nullptr, false);
    if (!rippleMaskEffectShader) {
        LOGE("GERippleShaderMask::GenerateDrawingShaderHas effect error");
    }
    return rippleMaskEffectShader;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GERippleShaderMask::GetRippleShaderMaskBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> rippleShaderMaskBuilder = nullptr;
    if (rippleShaderMaskBuilder) {
        return rippleShaderMaskBuilder;
    }

    static constexpr char prog[] = R"(
        uniform half2 iResolution;
        uniform half2 centerPos;
        uniform half rippleRadius;
        uniform half rippleWidth;
        uniform half widthCenterOffset;

        half4 main(vec2 fragCoord)
        {
            half2 uv = fragCoord.xy/iResolution.xy;
            half screenRatio = iResolution.x/iResolution.y;
            // Mask Info
            half2 MaskCenterUVs = uv - centerPos;
            MaskCenterUVs.x *= screenRatio;
            half offsetWidth = rippleWidth * widthCenterOffset;
            half uvDistance = length(MaskCenterUVs) - rippleRadius;
            half maskAlpha = smoothstep(rippleWidth, offsetWidth, uvDistance) *
                             smoothstep(-rippleWidth, offsetWidth, uvDistance);
            return half4(maskAlpha);
        }
    )";

    auto rippleShaderMaskEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (!rippleShaderMaskEffect) {
        LOGE("GERippleShaderMask::GetRippleShaderMaskBuilder effect error");
        return nullptr;
    }

    rippleShaderMaskBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(rippleShaderMaskEffect);
    return rippleShaderMaskBuilder;
}

std::shared_ptr<ShaderEffect> GERippleShaderMask::GenerateDrawingShaderHasNormal(float width, float height) const
{
    GE_TRACE_NAME_FMT("GERippleShaderMask::GenerateDrawingShaderHasNormal, Type: %s, Width: %g, Height: %g",
        Drawing::GE_MASK_RIPPLE, width, height);
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = nullptr;
    builder = GetRippleShaderNormalMaskBuilder();
    if (!builder) {
        LOGE("GERippleShaderMask::GenerateDrawingShaderHasNormal builder error");
        return nullptr;
    }
    builder->SetUniform("iResolution", width, height);
    builder->SetUniform("centerPos", param_.center_.first, param_.center_.second);
    builder->SetUniform("rippleRadius", param_.radius_);
    builder->SetUniform("rippleWidth", param_.width_);
    builder->SetUniform("widthCenterOffset", param_.widthCenterOffset_);
    auto rippleMaskEffectShader = builder->MakeShader(nullptr, false);
    if (!rippleMaskEffectShader) {
        LOGE("GERippleShaderMask::GenerateDrawingShaderHasNormal effect error");
    }
    return rippleMaskEffectShader;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GERippleShaderMask::GetRippleShaderNormalMaskBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> rippleShaderMaskNormalBuilder = nullptr;
    if (rippleShaderMaskNormalBuilder) {
        return rippleShaderMaskNormalBuilder;
    }

    static constexpr char prog[] = R"(
        uniform half2 iResolution;
        uniform half2 centerPos;
        uniform half rippleRadius;
        uniform half rippleWidth;
        uniform half widthCenterOffset;

        half4 main(vec2 fragCoord)
        {
            half2 uv = fragCoord.xy / iResolution.xy;
            half screenRatio = iResolution.x / iResolution.y;
            // Mask Info
            half2 MaskCenterUVs = uv - centerPos;
            MaskCenterUVs.x *= screenRatio;
            half dist = length(MaskCenterUVs);
            half offsetWidth = rippleWidth * widthCenterOffset;
            half uvDistance = dist - rippleRadius - offsetWidth;
            uvDistance = clamp(uvDistance, -1.0, 1.0);

            half mask = smoothstep(rippleWidth - offsetWidth, 0.0, uvDistance) *
                        smoothstep(-rippleWidth - offsetWidth, 0.0, uvDistance);

            half2 directionVector = MaskCenterUVs * (uvDistance * mask * 0.5 / (dist + 1e-4)) + 0.5;
            return half4(directionVector, 1.0, mask);
        }
    )";

    auto rippleShaderMaskNormalEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (!rippleShaderMaskNormalEffect) {
        LOGE("GERippleShaderMask::GetRippleShaderNormalMaskBuilder effect error");
        return nullptr;
    }

    rippleShaderMaskNormalBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(rippleShaderMaskNormalEffect);
    return rippleShaderMaskNormalBuilder;
}

} // namespace Drawing
} // namespace Rosen
} // namespace OHOS