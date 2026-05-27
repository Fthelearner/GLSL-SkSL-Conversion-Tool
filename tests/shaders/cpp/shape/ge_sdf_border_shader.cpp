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
#include "ge_sdf_border_shader.h"


namespace OHOS {
namespace Rosen {
static constexpr float SDF_BORDER_MIN_THRESHOLD = 0.0001f;

GESDFBorderShader::GESDFBorderShader(const Drawing::GESDFBorderShaderParams& params) : params_(params)
{}

void GESDFBorderShader::MakeDrawingShader(Drawing::Canvas& canvas, const Drawing::Rect &rect, float progress)
{
    drShader_ = MakeSDFBorderShader(canvas, rect);
}

void GESDFBorderShader::OnDrawShader(Drawing::Canvas& canvas, const Drawing::Rect& rect)
{
    Preprocess(canvas, rect); // to calculate your cache data
    MakeDrawingShader(canvas, rect, -1.f); // not use progress
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

std::shared_ptr<Drawing::ShaderEffect> GESDFBorderShader::MakeSDFBorderShader(Drawing::Canvas& canvas,
    const Drawing::Rect &rect)
{
    auto width = rect.GetWidth();
    auto height = rect.GetHeight();
    if (height < 1e-6 || width < 1e-6) {
        return nullptr;
    }

    if (!params_.shape) {
        GE_LOGE("GESDFBorderShader::MakeSDFBorderShader shape is invalid.");
        return nullptr;
    }

    auto sdfShader = params_.shape->GenerateDrawingShader(canvas, width, height);
    if (sdfShader == nullptr) {
        GE_LOGE("GESDFBorderShader: failed generate GESDFBorderShader.");
        return nullptr;
    }

    auto sdfEffect = GetSDFBorderEffect();
    if (sdfEffect == nullptr) {
        GE_LOGE("GESDFBorderShader: failed GetSDFBorderEffect.");
        return nullptr;
    }
    auto shaderEffectBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(sdfEffect);
    shaderEffectBuilder->SetChild("sdfShape", sdfShader);
    shaderEffectBuilder->SetUniformVec4("u_borderColor", params_.border.color.GetRedF(),
        params_.border.color.GetGreenF(), params_.border.color.GetBlueF(), params_.border.color.GetAlphaF());
    shaderEffectBuilder->SetUniform("u_borderWidth", std::max(params_.border.width, SDF_BORDER_MIN_THRESHOLD));
    auto outShader = shaderEffectBuilder->MakeShader(nullptr, false);
    if (outShader == nullptr) {
        GE_LOGE("GESDFBorderShader::MakeSDFBorderShader sdfBorderShader is nullptr");
        return nullptr;
    }
    return outShader;
}

std::shared_ptr<Drawing::RuntimeEffect> GESDFBorderShader::GetSDFBorderEffect()
{
    if (params_.border.isOutline) {
        thread_local std::shared_ptr<Drawing::RuntimeEffect> sdfOutlineBorderEffect = nullptr;
        if (sdfOutlineBorderEffect == nullptr) {
            sdfOutlineBorderEffect = Drawing::RuntimeEffect::CreateForShader(outlineShaderCode_);
        }
        return sdfOutlineBorderEffect;
    } else {
        thread_local std::shared_ptr<Drawing::RuntimeEffect> sdfBorderEffect = nullptr;
        if (sdfBorderEffect == nullptr) {
            sdfBorderEffect = Drawing::RuntimeEffect::CreateForShader(shaderCode_);
        }
        return sdfBorderEffect;
    }
}
}
}