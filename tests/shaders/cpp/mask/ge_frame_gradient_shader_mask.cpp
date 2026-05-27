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
#include "ge_frame_gradient_shader_mask.h"

namespace OHOS {
namespace Rosen {
namespace Drawing {
namespace {
thread_local static std::shared_ptr<Drawing::RuntimeEffect> frameGradientMaskShaderEffect_ = nullptr;

static constexpr float EPSILON = 1e-6f;
static constexpr float SQRT_2 = 1.41421356f;
static constexpr float NORMALIZATION_FACTOR = 2.0f;
static constexpr float PI = 3.14159265358979323846f;
static constexpr float GRADIENT_EPSILON = 1e-5f;
static constexpr float AXIAL_EPSILON = 1e-5f;

inline static const std::string maskString = R"(
    uniform half2 iResolution;
    uniform half3 innerBezierCoeff;
    uniform half3 outerBezierCoeff;

    uniform half innerFrameWidth;
    uniform half outerFrameWidth;
    uniform half invInnerFrameWidth;
    uniform half invOuterFrameWidth;
    
    uniform half2 boxHalfSize;
    uniform half clampedCornerRadius;
    uniform half2 rectPos;

    uniform half2 localBasis0;
    uniform half2 localBasis1;

    uniform half axialFeatherStrength;
    uniform half axialEnable;
    uniform half2 axialCoordWeights;
    uniform half axialInvSpan;
    uniform half axialRiseEnd;
    uniform half axialFallStart;

    half sdRoundedBox(half2 p, half2 b, half r)
    {
        half2 q = abs(p) - b + r;
        return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
    }

    half cubicBezierInterpolationFromCoeff(half3 coeff, half t)
    {
        return ((coeff.x * t + coeff.y) * t + coeff.z) * t;
    }

    half AxialEnvelope(half t)
    {
        half rise = smoothstep(0.0, axialRiseEnd, t);
        half fall = 1.0 - smoothstep(axialFallStart, 1.0, t);
        return rise * fall;
    }

    half4 main(float2 fragCoord)
    {
        half2 uv = fragCoord / iResolution.xy;
        half2 centered = (uv - 0.5) * 2.0 - rectPos;

        // Transform into local box space using the precomputed basis.
        half2 localPos = half2(dot(centered, localBasis0), dot(centered, localBasis1));

        half sdfrect = sdRoundedBox(localPos, boxHalfSize, clampedCornerRadius);

        // Normalize the inner and outer transition bands separately.
        half innerGradient = (sdfrect + innerFrameWidth) * invInnerFrameWidth;
        half outerGradient = (-sdfrect + outerFrameWidth) * invOuterFrameWidth;
        half gradient = min(innerGradient, outerGradient);

        // Outside both bands: no contribution.
        if (gradient <= 0.0) {
            return half4(0.0);
        }

        // Use different Bezier response curves for inner and outer regions.
        half3 bezierCoeff = (sdfrect > 0.0) ? outerBezierCoeff : innerBezierCoeff;
        gradient = cubicBezierInterpolationFromCoeff(bezierCoeff, gradient);

        // Optional axial modulation along a precomputed direction.
        if (axialEnable > 0.0) {
            half t = clamp(dot(localPos, axialCoordWeights) * axialInvSpan + 0.5, 0.0, 1.0);
            half envelope = AxialEnvelope(t);

            // Equivalent to mixing the gradient by the axial envelope strength.
            gradient *= (1.0 + axialFeatherStrength * (envelope - 1.0));
        }

        return half4(gradient);
    }
)";

} // namespace

GEFrameGradientShaderMask::GEFrameGradientShaderMask(const GEFrameGradientMaskParams& param)
    : innerBezier_(param.innerBezier),
      outerBezier_(param.outerBezier),
      cornerRadius_(std::max(param.cornerRadius, 0.0f)),
      innerFrameWidth_(std::max(param.innerFrameWidth, 0.0f)),
      outerFrameWidth_(std::max(param.outerFrameWidth, 0.0f)),
      rectWH_(std::make_pair(std::max(param.rectWH.first, 0.0f), std::max(param.rectWH.second, 0.0f))),
      rectPos_(param.rectPos), axialFeatherStrength_(param.axialFeatherStrength), axialCenter_(param.axialCenter),
      axialCoreWidth_(param.axialCoreWidth), axialDirection_(param.axialDirection), boxAngleDeg_(param.boxAngleDeg)
{}

bool GEFrameGradientShaderMask::ValidateParams(float width, float height) const
{
    if (width < EPSILON || height < EPSILON) {
        GE_LOGE("GEFrameGradientShaderMask::ValidateParams size zero");
        return false;
    }
    if (innerFrameWidth_ < EPSILON && outerFrameWidth_ < EPSILON) {
        GE_LOGE("GEFrameGradientShaderMask::ValidateParams FrameWidth zero");
        return false;
    }
    if (rectWH_.first < EPSILON && rectWH_.second < EPSILON) {
        GE_LOGE("GEFrameGradientShaderMask::ValidateParams RectWH zero");
        return false;
    }

    GE_LOGD("GEFrameGradientShaderMask::CreateFrameGradientMaskShader -> "
        "innerFrameWidth: %{public}f, outerFrameWidth: %{public}f, cornerRadius: %{public}f, "
        "rectPos: (%{public}f, %{public}f), "
        "rectWH: (%{public}f, %{public}f), "
        "innerBezier: (%{public}f, %{public}f, %{public}f, %{public}f), "
        "outerBezier: (%{public}f, %{public}f, %{public}f, %{public}f)",
        innerFrameWidth_, outerFrameWidth_, cornerRadius_, rectPos_.first, rectPos_.second, rectWH_.first,
        rectWH_.second, innerBezier_[0], innerBezier_[1], innerBezier_[2], innerBezier_[3], outerBezier_[0],
        outerBezier_[1], outerBezier_[2], outerBezier_[3]);
    return true;
}

std::shared_ptr<ShaderEffect> GEFrameGradientShaderMask::GenerateDrawingShader(float width, float height) const
{
    return CreateFrameGradientMaskShader(width, height);
}

std::shared_ptr<ShaderEffect> GEFrameGradientShaderMask::GenerateDrawingShaderHasNormal(float width, float height) const
{
    return CreateFrameGradientMaskShader(width, height);
}

std::shared_ptr<ShaderEffect> GEFrameGradientShaderMask::CreateFrameGradientMaskShader(float width, float height) const
{
    MakeFrameGradientMaskShaderEffect();
    if (!frameGradientMaskShaderEffect_ || !ValidateParams(width, height)) {
        GE_LOGE("GEFrameGradientShaderMask::CreateFrameGradientMaskShader create builder error");
        return nullptr;
    }

    auto maskBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(frameGradientMaskShaderEffect_);
    maskBuilder->SetUniform("iResolution", width, height);
    // Pre-expand the cubic Bezier mapping into polynomial coefficients.
    auto MakeBezierCoeff = [](float y1, float y2) -> std::array<float, 3> {
        return {
            1.0f + 3.0f * y1 - 3.0f * y2,
            -6.0f * y1 + 3.0f * y2,
            3.0f * y1
        };
    };

    // controlPoints.yw are the effective y-values used by the interpolation polynomial
    auto innerCoeff = MakeBezierCoeff(innerBezier_[1], innerBezier_[3]); // 1, 3: controlPoints.yw
    auto outerCoeff = MakeBezierCoeff(outerBezier_[1], outerBezier_[3]); // 1, 3: controlPoints.yw
    maskBuilder->SetUniform("innerBezierCoeff", innerCoeff.data(), 3); // 3: cubic polynomial coefficients
    maskBuilder->SetUniform("outerBezierCoeff", outerCoeff.data(), 3); // 3: cubic polynomial coefficients

    // Normalize frame widths to the shader's normalized coordinate space.
    const float innerFrameWidth = innerFrameWidth_ * NORMALIZATION_FACTOR / height;
    const float outerFrameWidth = outerFrameWidth_ * NORMALIZATION_FACTOR / height;
    maskBuilder->SetUniform("innerFrameWidth", innerFrameWidth);
    maskBuilder->SetUniform("outerFrameWidth", outerFrameWidth);
    maskBuilder->SetUniform("invInnerFrameWidth", 1.0f / (innerFrameWidth + GRADIENT_EPSILON));
    maskBuilder->SetUniform("invOuterFrameWidth", 1.0f / (outerFrameWidth + GRADIENT_EPSILON));

    // Precompute screen-ratio-dependent rounded-box parameters.
    const float screenRatio = static_cast<float>(width) / std::max(static_cast<float>(height), EPSILON);
    const float boxHalfSizeX = screenRatio * rectWH_.first;
    const float boxHalfSizeY = rectWH_.second;
    const float cornerRadius = cornerRadius_ * NORMALIZATION_FACTOR / height;
    const float clampedCornerRadius = std::clamp(cornerRadius, 0.0f, std::min(boxHalfSizeX, boxHalfSizeY));
    maskBuilder->SetUniform("boxHalfSize", boxHalfSizeX, boxHalfSizeY);
    maskBuilder->SetUniform("clampedCornerRadius", clampedCornerRadius);
    maskBuilder->SetUniform("rectPos", rectPos_.first, rectPos_.second);

    // Merge aspect-ratio scaling and box rotation into two local-space basis vectors.
    const float angleRad = boxAngleDeg_ * PI / 180.0f;
    const float cosA = std::cos(angleRad);
    const float sinA = std::sin(angleRad);
    maskBuilder->SetUniform("localBasis0", cosA * screenRatio,  sinA);
    maskBuilder->SetUniform("localBasis1", -sinA * screenRatio, cosA);

    ComputeAndSetUniforms(maskBuilder, screenRatio);

    auto maskShader = maskBuilder->MakeShader(nullptr, false);
    if (!maskShader) {
        GE_LOGE("GEFrameGradientShaderMask::CreateFrameGradientMaskShader fail");
    }
    return maskShader;
}

void GEFrameGradientShaderMask::ComputeAndSetUniforms(
    const std::shared_ptr<Drawing::RuntimeShaderBuilder>& builder, float screenRatio) const
{
    // Precompute axial modulation parameters.
    const float dirXRaw = axialDirection_.first;
    const float dirYRaw = axialDirection_.second;
    const float dirLen2 = dirXRaw * dirXRaw + dirYRaw * dirYRaw;

    float axialEnable = 0.0f;
    float axialCoordWeightX = 0.0f;
    float axialCoordWeightY = 0.0f;
    float axialInvSpan = 0.0f;

    if (dirLen2 >= AXIAL_EPSILON && axialFeatherStrength_ >= AXIAL_EPSILON) {
        const float invLen = 1.0f / std::sqrt(dirLen2);
        const float dirX = dirXRaw * invLen;
        const float dirY = dirYRaw * invLen;

        // Project the normalized axial direction onto the local box extents
        // to obtain the half span of the modulation range.
        const float halfExtent = std::abs(dirX) * rectWH_.first + std::abs(dirY) * rectWH_.second;

        axialCoordWeightX = dirX / std::max(screenRatio, AXIAL_EPSILON);
        axialCoordWeightY = dirY;
        axialInvSpan = 1.0f / (2.0f * std::max(halfExtent, AXIAL_EPSILON));
        axialEnable = 1.0f;
    }
    builder->SetUniform("axialFeatherStrength", axialFeatherStrength_);
    builder->SetUniform("axialEnable", axialEnable);
    builder->SetUniform("axialCoordWeights", axialCoordWeightX, axialCoordWeightY);
    builder->SetUniform("axialInvSpan", axialInvSpan);

    // Precompute the rising and falling boundaries of the axial envelope.
    builder->SetUniform("axialRiseEnd", axialCenter_ - 0.5f * axialCoreWidth_);
    builder->SetUniform("axialFallStart", axialCenter_ + 0.5f * axialCoreWidth_);
}

void GEFrameGradientShaderMask::MakeFrameGradientMaskShaderEffect() const
{
    if (frameGradientMaskShaderEffect_) {
        return;
    }

    frameGradientMaskShaderEffect_ = Drawing::RuntimeEffect::CreateForShader(maskString);
    if (frameGradientMaskShaderEffect_ == nullptr) {
        GE_LOGE("GEFrameGradientShaderMask::MakeFrameGradientMaskShaderEffect error");
    }
}

Drawing::Rect GEFrameGradientShaderMask::GetSubtractedRect(float width, float height) const
{
    if (!ValidateParams(width, height)) {
        GE_LOGE("GEFrameGradientShaderMask::GetSubtractedRect invalid input");
        return Drawing::Rect();
    }

    float screenRatio = height / width;

    // rectPos_ [-1, 1] Normalized to [0, 1]
    float centerXNormalized = (rectPos_.first + 1.0f) / NORMALIZATION_FACTOR;
    float centerYNormalized = (rectPos_.second + 1.0f) / NORMALIZATION_FACTOR;

    // Calculate top-left coordinates of the rounded rectangle
    float rectLeft = centerXNormalized - rectWH_.first / NORMALIZATION_FACTOR;
    float rectTop = centerYNormalized - rectWH_.second / NORMALIZATION_FACTOR;
    float rectRight = rectLeft + rectWH_.first;
    float rectBottom = rectTop + rectWH_.second;

    // Calculate half of the inner border width
    float innerBorderWidth = innerFrameWidth_ / height;

    // Calculate minimum edge to determine real corner radius
    float minEdge = std::min(height * rectWH_.second, width * rectWH_.first) / NORMALIZATION_FACTOR;
    float realCornerRadius = std::min(cornerRadius_, minEdge) / height;

    // EPSILON:Small offset for precision
    // Calculate initial adjusted boundary values based on border width
    float adjustedLeft = rectLeft + innerBorderWidth * screenRatio + EPSILON;
    float adjustedTop = rectTop + innerBorderWidth + EPSILON;
    float adjustedRight = rectRight - innerBorderWidth * screenRatio - EPSILON;
    float adjustedBottom = rectBottom - innerBorderWidth - EPSILON;

    // If border width is greater than or equal to corner radius, calculate inscribed rectangle directly by border width
    if (innerBorderWidth < realCornerRadius) {
        // Calculate inscribed rectangle based on aspect ratio and corner radius relationship
        if (std::abs(screenRatio * rectWH_.second - rectWH_.first) > realCornerRadius) {
            if (screenRatio * rectWH_.second > rectWH_.first) {
                // Vertical rectangle case
                adjustedTop = rectTop + realCornerRadius;
                adjustedBottom = rectBottom - realCornerRadius;
            } else {
                // Horizontal rectangle case
                adjustedLeft = rectLeft + realCornerRadius * screenRatio;
                adjustedRight = rectRight - realCornerRadius * screenRatio;
            }
        } else {
            // Calculate inscribed rectangle for corner area (diagonal case)
            float innerRadius = realCornerRadius - innerBorderWidth;
            float diagonalOffset = realCornerRadius - innerRadius / SQRT_2;
            adjustedTop = rectTop + diagonalOffset + EPSILON;
            adjustedBottom = rectBottom - diagonalOffset - EPSILON;
            adjustedLeft = rectLeft + diagonalOffset * screenRatio + EPSILON;
            adjustedRight = rectRight - diagonalOffset * screenRatio - EPSILON;
        }
    }
    GE_LOGD("GEFrameGradientShaderMask::GetSubtractedRect "
        "l: %{public}f, t: %{public}f, r: %{public}f, b: %{public}f, radius: %{public}f;",
        adjustedLeft, adjustedTop, adjustedRight, adjustedBottom, realCornerRadius);
    // Check rectangle validity
    if (adjustedLeft > adjustedRight || adjustedTop > adjustedBottom) {
        return Drawing::Rect();
    }
    return Drawing::Rect(adjustedLeft, adjustedTop, adjustedRight, adjustedBottom);
}

} // namespace Drawing
} // namespace Rosen
} // namespace OHOS