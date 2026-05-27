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
#include "ge_sdf_shadow_shader.h"
#include "common/rs_common_def.h"

namespace OHOS {
namespace Rosen {
constexpr float SDF_SHADOW_MIN_THRESHOLD = 0.0001f;
constexpr float RADIUS_FACTOR = 1.5f;

// Elevation shadow constants (aligned with Skia SkDrawShadowInfo.h)
constexpr float DEFAULT_LIGHT_HEIGHT = 600.0f;
constexpr float DEFAULT_LIGHT_RADIUS = 800.0f;
constexpr float AMBIENT_HEIGHT_FACTOR = 1.0f / 128.0f;
constexpr float AMBIENT_GEOM_FACTOR = 64.0f;
constexpr float MAX_AMBIENT_RADIUS = 150.0f;
constexpr uint8_t DEFAULT_AMBIENT_ALPHA = 0x0A;
constexpr float EPSILON = 1e-6f;

GESDFShadowShader::GESDFShadowShader(const Drawing::GESDFShadowShaderParams& params) : params_(params)
{
    if (IsElevationMode()) {
        ComputeElevationParams();
    }
}

void GESDFShadowShader::MakeDrawingShader(Drawing::Canvas& canvas, const Drawing::Rect& rect, float progress)
{
    if (IsElevationMode()) {
        drShader_ = MakeElevationShadowShader(canvas, rect);
    } else {
        drShader_ = MakeSDFShadowShader(canvas, rect);
    }
}

void GESDFShadowShader::UpdateRectForShadow(Drawing::Rect& rect)
{
    float radius = std::max(params_.shadow.radius * RADIUS_FACTOR, SDF_SHADOW_MIN_THRESHOLD);
    float offsetLeft = params_.shadow.offsetX - radius;
    float offsetTop = params_.shadow.offsetY - radius;
    float offsetRight = params_.shadow.offsetX + radius;
    float offsetBottom =  params_.shadow.offsetY + radius;
    if (ROSEN_LNE(offsetLeft, 0.0f)) {
        rect.SetLeft(rect.GetLeft() + offsetLeft);
    }
    if (ROSEN_LNE(offsetTop, 0.0f)) {
        rect.SetTop(rect.GetTop() + offsetTop);
    }
    if (ROSEN_GNE(offsetRight, 0.0f)) {
        rect.SetRight(rect.GetRight() + offsetRight);
    }
    if (ROSEN_GNE(offsetBottom, 0.0f)) {
        rect.SetBottom(rect.GetBottom() + offsetBottom);
    }
}

void GESDFShadowShader::OnDrawShader(Drawing::Canvas& canvas, const Drawing::Rect& rect)
{
    if (!IsElevationMode() && params_.shadow.radius < 0.0f) {
        return;
    }

    Drawing::Rect newRect = rect;
    if (IsElevationMode()) {
        UpdateRectForElevationShadow(newRect);
    } else {
        UpdateRectForShadow(newRect);
    }
    MakeDrawingShader(canvas, newRect, -1.f); // not use progress
    auto shader = GetDrawingShader();
    if (shader == nullptr) {
        return;
    }
    Drawing::Brush brush;
    brush.SetShaderEffect(shader);
    canvas.AttachBrush(brush);
    canvas.DrawRect(newRect);
    canvas.DetachBrush();
}

std::shared_ptr<Drawing::ShaderEffect> GESDFShadowShader::MakeSDFShadowShader(Drawing::Canvas& canvas,
    const Drawing::Rect& rect)
{
    auto width = rect.GetWidth();
    auto height = rect.GetHeight();
    if (height < 1e-6 || width < 1e-6) {
        return nullptr;
    }

    if (!params_.shape) {
        GE_LOGE("GESDFShadowShader::MakeSDFShadowShader shape is invalid.");
        return nullptr;
    }

    auto sdfShader = params_.shape->GenerateDrawingShader(canvas,
        canvasInfo_.geoWidth != 0 ? canvasInfo_.geoWidth : width,
        canvasInfo_.geoHeight != 0 ? canvasInfo_.geoHeight : height);
    if (sdfShader == nullptr) {
        GE_LOGE("GESDFShadowShader: failed generate GESDFShadowShader.");
        return nullptr;
    }

    auto sdfEffect = GetSDFShadowEffect();
    if (sdfEffect == nullptr) {
        GE_LOGE("GESDFShadowShader: failed GetSDFShadowEffect.");
        return nullptr;
    }
    auto shaderEffectBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(sdfEffect);
    shaderEffectBuilder->SetChild("sdfShape", sdfShader);
    shaderEffectBuilder->SetUniform("iResolution", static_cast<float>(width), static_cast<float>(height));
    shaderEffectBuilder->SetUniformVec4("shadowColor", params_.shadow.color.GetRedF(),
        params_.shadow.color.GetGreenF(), params_.shadow.color.GetBlueF(), params_.shadow.color.GetAlphaF());
    shaderEffectBuilder->SetUniform("shadowOffset", params_.shadow.offsetX, params_.shadow.offsetY);
    shaderEffectBuilder->SetUniform("shadowRadius",
        std::max(params_.shadow.radius * RADIUS_FACTOR, SDF_SHADOW_MIN_THRESHOLD));
    shaderEffectBuilder->SetUniform("isFilled", static_cast<float>(params_.shadow.isFilled));

    auto outShader = shaderEffectBuilder->MakeShader(nullptr, false);
    if (outShader == nullptr) {
        GE_LOGE("GESDFShadowShader::MakeSDFShadowShader sdfShadowShader is nullptr.");
        return nullptr;
    }
    return outShader;
}

std::shared_ptr<Drawing::RuntimeEffect> GESDFShadowShader::GetSDFShadowEffect()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> sdfShadowShader = nullptr;
    if (sdfShadowShader == nullptr) {
        sdfShadowShader = Drawing::RuntimeEffect::CreateForShader(shaderCode_);
    }
    return sdfShadowShader;
}

void GESDFShadowShader::ComputeElevationParams()
{
    float elevation = params_.shadow.elevation;

    // Ambient shadow params (aligned with Skia)
    // outset = AmbientBlurRadius = min(elevation/2, 150)
    // recipAlpha = AmbientRecipAlpha = 1 + elevation/128
    // effective blur = outset * recipAlpha (matches Skia's actual blur width)
    float outset = std::min(elevation * AMBIENT_HEIGHT_FACTOR * AMBIENT_GEOM_FACTOR, MAX_AMBIENT_RADIUS);
    float recipAlpha = 1.0f + std::max(elevation * AMBIENT_HEIGHT_FACTOR, 0.0f);
    ambientBlurRadius_ = outset * recipAlpha;
    ambientOutset_ = outset;

    // Ambient alpha (raw value, not divided by recipAlpha)
    float ambientAlpha = static_cast<float>(DEFAULT_AMBIENT_ALPHA) / 255.0f;
    ambientColor_ = Drawing::Color::ColorQuadSetARGB(
        static_cast<uint8_t>(std::clamp(ambientAlpha * 255.0f, 0.0f, 255.0f)), 0, 0, 0);

    // Spot shadow params (aligned with Skia GetSpotParams, isLimitElevation=true)
    float zRatio = std::clamp(elevation / std::max(DEFAULT_LIGHT_HEIGHT - elevation, EPSILON), 0.0f, 0.95f);
    spotBlurRadius_ = DEFAULT_LIGHT_RADIUS * zRatio;

    // Spot color: use raw shadow color (no Tonal Color transform)
    spotColor_ = params_.shadow.color;
}

std::shared_ptr<Drawing::RuntimeEffect> GESDFShadowShader::GetElevationShadowEffect()
{
    thread_local std::shared_ptr<Drawing::RuntimeEffect> elevationShadowShader = nullptr;
    if (elevationShadowShader == nullptr) {
        elevationShadowShader = Drawing::RuntimeEffect::CreateForShader(elevationShaderCode_);
    }
    return elevationShadowShader;
}

void GESDFShadowShader::UpdateRectForElevationShadow(Drawing::Rect& rect)
{
    float maxBlur = std::max({ambientBlurRadius_, spotBlurRadius_, SDF_SHADOW_MIN_THRESHOLD});
    rect.SetLeft(rect.GetLeft() + params_.shadow.offsetX - maxBlur);
    rect.SetTop(rect.GetTop() + params_.shadow.offsetY - maxBlur);
    rect.SetRight(rect.GetRight() + params_.shadow.offsetX + maxBlur);
    rect.SetBottom(rect.GetBottom() + params_.shadow.offsetY + maxBlur);
}

std::shared_ptr<Drawing::ShaderEffect> GESDFShadowShader::MakeElevationShadowShader(Drawing::Canvas& canvas,
    const Drawing::Rect& rect)
{
    auto width = rect.GetWidth();
    auto height = rect.GetHeight();
    if (height < 1e-6 || width < 1e-6) {
        return nullptr;
    }

    if (!params_.shape) {
        GE_LOGE("GESDFShadowShader::MakeElevationShadowShader shape is invalid.");
        return nullptr;
    }

    auto sdfShader = params_.shape->GenerateDrawingShader(canvas,
        canvasInfo_.geoWidth != 0 ? canvasInfo_.geoWidth : width,
        canvasInfo_.geoHeight != 0 ? canvasInfo_.geoHeight : height);
    if (sdfShader == nullptr) {
        GE_LOGE("GESDFShadowShader::MakeElevationShadowShader failed generate SDF shader.");
        return nullptr;
    }

    auto effect = GetElevationShadowEffect();
    if (effect == nullptr) {
        GE_LOGE("GESDFShadowShader::MakeElevationShadowShader failed GetElevationShadowEffect.");
        return nullptr;
    }

    auto builder = std::make_shared<Drawing::RuntimeShaderBuilder>(effect);
    builder->SetChild("sdfShape", sdfShader);
    builder->SetUniform("iResolution", static_cast<float>(width), static_cast<float>(height));
    builder->SetUniformVec4("ambientColor", ambientColor_.GetRedF(),
        ambientColor_.GetGreenF(), ambientColor_.GetBlueF(), ambientColor_.GetAlphaF());
    builder->SetUniform("ambientBlurRadius", ambientBlurRadius_);
    builder->SetUniform("ambientOutset", ambientOutset_);
    builder->SetUniformVec4("spotColor", spotColor_.GetRedF(),
        spotColor_.GetGreenF(), spotColor_.GetBlueF(), spotColor_.GetAlphaF());
    builder->SetUniform("spotBlurRadius", spotBlurRadius_);
    builder->SetUniform("isFilled", static_cast<float>(params_.shadow.isFilled));
    builder->SetUniform("shadowOffset", params_.shadow.offsetX, params_.shadow.offsetY);

    auto outShader = builder->MakeShader(nullptr, false);
    if (outShader == nullptr) {
        GE_LOGE("GESDFShadowShader::MakeElevationShadowShader outShader is nullptr.");
        return nullptr;
    }
    return outShader;
}

} // namespace Rosen
} // namespace OHOS