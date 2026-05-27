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

#include <chrono>
 
#include "ge_log.h"
#include "ge_sdf_color_shader.h"
 
namespace OHOS {
namespace Rosen {

GESDFColorShader::GESDFColorShader(const Drawing::GESDFColorShaderParams& params) : params_(params)
{}

void GESDFColorShader::OnDrawShader(Drawing::Canvas& canvas, const Drawing::Rect& rect)
{
    Preprocess(canvas, rect); // to calculate your cache data
    MakeDrawingShader(canvas, rect, -1.f);
    auto shader = GetDrawingShader();
    if (shader == nullptr) {
        return;
    }
    Drawing::Brush brush;
    brush.SetShaderEffect(shader);
    canvas.AttachBrush(brush);
    canvas.DrawRect(rect);
    canvas.DetachBrush();
}

void GESDFColorShader::MakeDrawingShader(Drawing::Canvas& canvas, const Drawing::Rect& rect, float progress)
{
    if (!rect.IsValid()) {
        GE_LOGE("GESDFColorShader::MakeSDFColorShader rect is invalid.");
        return;
    }
    drShader_ = MakeSDFColorShader(canvas, rect);
}

std::shared_ptr<Drawing::ShaderEffect> GESDFColorShader::MakeSDFColorShader(Drawing::Canvas& canvas,
    const Drawing::Rect& rect)
{
    if (!params_.shape) {
        GE_LOGE("GESDFColorShader::MakeSDFColorShader shape is invalid.");
        return nullptr;
    }

    auto sdfShader = params_.shape->GenerateDrawingShader(canvas, rect.GetWidth(), rect.GetHeight());
    if (sdfShader == nullptr) {
        GE_LOGE("GESDFColorShader: failed generate GESDFColorShader.");
        return nullptr;
    }

    auto sdfEffect = GetSDFColorEffect();
    if (sdfEffect == nullptr) {
        GE_LOGE("GESDFColorShader: failed GetSDFColorEffect.");
        return nullptr;
    }
    auto shaderEffectBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(sdfEffect);
    shaderEffectBuilder->SetChild("sdfShape", sdfShader);
    shaderEffectBuilder->SetUniformVec4(
        "color", params_.color.x_, params_.color.y_, params_.color.z_, params_.color.w_);
    auto outShader = shaderEffectBuilder->MakeShader(nullptr, false);
    if (outShader == nullptr) {
        GE_LOGE("GESDFColorShader::MakeSDFColorShader sdfColorShader is nullptr.");
        return nullptr;
    }
    return outShader;
}

std::shared_ptr<Drawing::RuntimeEffect> GESDFColorShader::GetSDFColorEffect()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> sdfColorShader = nullptr;
    if (sdfColorShader == nullptr) {
        sdfColorShader = Drawing::RuntimeEffect::CreateForShader(shaderCode_);
    }
    return sdfColorShader;
}

} // namespace Rosen
} // namespace OHOS