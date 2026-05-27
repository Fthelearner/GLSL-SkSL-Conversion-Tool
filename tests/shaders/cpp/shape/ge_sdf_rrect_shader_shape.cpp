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

#include "ge_sdf_rrect_shader_shape.h"
#include <algorithm>
#include <sstream>

#include "ge_log.h"
#include "ge_trace.h"

namespace OHOS {
namespace Rosen {
namespace Drawing {
constexpr float HALF = 0.5;
constexpr float EXTEND = 0.5; // Fixing edge difference between SDF and Skia RRect
constexpr float MIN_SIZE = 0.0001f;
constexpr float EPSILON = 1e-6f;
static constexpr char SDF_GRAD_PROG[] = R"(
    uniform vec2 centerPos;
    uniform vec2 halfSize;
    uniform vec2 cornerRadiusTL;
    uniform vec2 cornerRadiusTR;
    uniform vec2 cornerRadiusBR;
    uniform vec2 cornerRadiusBL;

    const float N_EPS = 1e-6;
    const float N_SCALE = 2048.0;

    vec2 safeNorm(vec2 v)
    {
        return v / max(length(v), N_EPS);
    }

    vec2 selectCornerRadius(vec2 p, vec2 radiusTL, vec2 radiusTR, vec2 radiusBR, vec2 radiusBL)
    {
        float isRight = step(0.0, p.x);
        float isBottom = step(0.0, p.y);
        vec2 topRadius = mix(radiusTL, radiusTR, isRight);
        vec2 bottomRadius = mix(radiusBL, radiusBR, isRight);
        return mix(topRadius, bottomRadius, isBottom);
    }

    float sdEllipse(vec2 p, vec2 r)
    {
        vec2 safeR = max(r, vec2(N_EPS));
        vec2 invR = p / safeR;
        vec2 invR2 = p / max(safeR * safeR, vec2(N_EPS));
        float k0 = length(invR);
        float k1 = max(length(invR2), N_EPS);
        return k0 * (k0 - 1.0) / k1;
    }

    vec2 ellipseGrad(vec2 p, vec2 r)
    {
        vec2 safeR = max(r, vec2(N_EPS));
        vec2 scaled = p / safeR;
        float scaledLen = max(length(scaled), N_EPS);
        return safeNorm(p / max(safeR * safeR * scaledLen, vec2(N_EPS)));
    }

    vec3 sdgRRect(vec2 p, vec2 b, vec2 radiusTL, vec2 radiusTR, vec2 radiusBR, vec2 radiusBL)
    {
        vec2 r = selectCornerRadius(p, radiusTL, radiusTR, radiusBR, radiusBL);
        vec2 s = sign(p);
        vec2 local = abs(p);
        vec2 cornerCenter = b - r;
        vec2 cornerPos = local - cornerCenter;
        float useCorner = step(0.0, min(cornerPos.x, cornerPos.y));

        float edgeSd = max(local.x - b.x, local.y - b.y);
        float cornerSd = sdEllipse(cornerPos, r);
        float sd = mix(edgeSd, cornerSd, useCorner);

        vec2 edgeDelta = local - b;
        float pickX = step(edgeDelta.y, edgeDelta.x);
        vec2 gradEdge = vec2(pickX, 1.0 - pickX);
        vec2 gradCorner = ellipseGrad(cornerPos, r);
        vec2 grad = s * mix(gradEdge, gradCorner, useCorner);
        return vec3(sd, grad);
    }

    float EncodeDir(vec2 dir)
    {
        float xPos = floor(dir.x + N_SCALE);
        float yPos = floor(dir.y + N_SCALE);
        return xPos + (yPos / N_SCALE) / 2.0;
    }

    vec4 main(float2 fragCoord)
    {
        vec2 posFromCenter = fragCoord - centerPos;
        vec3 sdg = sdgRRect(posFromCenter, halfSize, cornerRadiusTL, cornerRadiusTR, cornerRadiusBR, cornerRadiusBL);
        float packedDir = EncodeDir(posFromCenter);
        return vec4(sdg.yz, packedDir, sdg.x);
    }
)";

namespace {
constexpr char UNIFORM_RRECT_SHADER_PROG[] = R"(
    uniform vec2 centerPos;
    uniform vec2 halfSize;
    uniform float radius;

    float sdfRRect(in vec2 coord, in vec2 p, in vec2 b, in float r)
    {
        vec2 d = abs(coord - p) - (b - r);
        return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
    }

    half4 main(vec2 fragCoord)
    {
        float sdf = sdfRRect(fragCoord, centerPos, halfSize, radius);
        return half4(0, 0, 0, sdf);
    }
)";

constexpr char UNIFORM_SDF_GRAD_PROG[] = R"(
    uniform vec2 centerPos;
    uniform vec2 halfSize;
    uniform float radius;

    const float N_EPS = 1e-6;
    const float N_SCALE = 2048.0;

    vec2 safeNorm(vec2 v)
    {
        return v / max(length(v), N_EPS);
    }

    vec3 sdgRRect(vec2 p, vec2 b, float r)
    {
        vec2 s = sign(p);
        vec2 w = abs(p) - b + r;
        float g = max(w.x, w.y);
        vec2 q = max(w, 0.0);
        float l = length(q);
        float outside = step(0.0, g);
        float sd = mix(g, l, outside) - r;
        float pickX = step(w.y, w.x);
        vec2 gradIn = vec2(pickX, 1.0 - pickX);
        vec2 gradOut = safeNorm(q);
        vec2 grad = s * mix(gradIn, gradOut, outside);
        return vec3(sd, grad);
    }

    float EncodeDir(vec2 dir)
    {
        float xPos = floor(dir.x + N_SCALE);
        float yPos = floor(dir.y + N_SCALE);
        return xPos + (yPos / N_SCALE) / 2.0;
    }

    vec4 main(float2 fragCoord)
    {
        vec2 posFromCenter = fragCoord - centerPos;
        vec3 sdg = sdgRRect(posFromCenter, halfSize, radius);
        float packedDir = EncodeDir(posFromCenter);
        return vec4(sdg.yz, packedDir, sdg.x);
    }
)";

constexpr char RRECT_SHADER_PROG[] = R"(
    uniform vec2 centerPos;
    uniform vec2 halfSize;
    uniform vec2 cornerRadiusTL;
    uniform vec2 cornerRadiusTR;
    uniform vec2 cornerRadiusBR;
    uniform vec2 cornerRadiusBL;

    vec2 selectCornerRadius(vec2 p, vec2 radiusTL, vec2 radiusTR, vec2 radiusBR, vec2 radiusBL)
    {
        float isRight = step(0.0, p.x);
        float isBottom = step(0.0, p.y);
        vec2 topRadius = mix(radiusTL, radiusTR, isRight);
        vec2 bottomRadius = mix(radiusBL, radiusBR, isRight);
        return mix(topRadius, bottomRadius, isBottom);
    }

    float sdEllipse(vec2 p, vec2 r)
    {
        const float EPS = 1e-6;
        vec2 safeR = max(r, vec2(EPS));
        vec2 invR = p / safeR;
        vec2 invR2 = p / max(safeR * safeR, vec2(EPS));
        float k0 = length(invR);
        float k1 = max(length(invR2), EPS);
        return k0 * (k0 - 1.0) / k1;
    }

    float sdfRRect(in vec2 coord, in vec2 p, in vec2 b, in vec2 radiusTL, in vec2 radiusTR,
        in vec2 radiusBR, in vec2 radiusBL)
    {
        vec2 local = abs(coord - p);
        vec2 radius = selectCornerRadius(coord - p, radiusTL, radiusTR, radiusBR, radiusBL);
        vec2 cornerPos = local - (b - radius);
        if (cornerPos.x >= 0.0 && cornerPos.y >= 0.0) {
            return sdEllipse(cornerPos, radius);
        }
        return max(local.x - b.x, local.y - b.y);
    }

    half4 main(vec2 fragCoord)
    {
        float sdf = sdfRRect(fragCoord, centerPos, halfSize, cornerRadiusTL, cornerRadiusTR, cornerRadiusBR,
            cornerRadiusBL);
        return half4(0, 0, 0, sdf);
    }
)";

float ClampCornerRadius(float radius, float maxRadius)
{
    return std::clamp(radius, 0.0f, maxRadius);
}

std::string RadiiToString(const Vector2f* radii)
{
    std::stringstream ss;
    ss << "[(" << radii[GERRect::TOP_LEFT].x_ << "," << radii[GERRect::TOP_LEFT].y_ << "),("
       << radii[GERRect::TOP_RIGHT].x_ << "," << radii[GERRect::TOP_RIGHT].y_ << "),("
       << radii[GERRect::BOTTOM_RIGHT].x_ << "," << radii[GERRect::BOTTOM_RIGHT].y_ << "),("
       << radii[GERRect::BOTTOM_LEFT].x_ << "," << radii[GERRect::BOTTOM_LEFT].y_ << ")]";
    return ss.str();
}
} // namespace

std::shared_ptr<ShaderEffect> GESDFRRectShaderShape::GenerateDrawingShader(float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFRRectShaderShape::GenerateDrawingShader, Width: %g, Height: %g", width, height);
    bool useUniformRadius = UseUniformRadiusFastPath();
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = useUniformRadius ?
        GetUniformSDFRRectShaderShapeBuilder() : GetSDFRRectShaderShapeBuilder();
    if (!builder) {
        LOGE("GESDFRRectShaderShape::GenerateDrawingShader has builder error");
        return nullptr;
    }
    return GenerateShaderEffect(builder, useUniformRadius);
}

std::shared_ptr<ShaderEffect> GESDFRRectShaderShape::GenerateDrawingShaderHasNormal(float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFRRectShaderShape::GenerateDrawingShaderHasNormal, Width: %g, Height: %g", width, height);
    bool useUniformRadius = UseUniformRadiusFastPath();
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = useUniformRadius ?
        GetUniformSDFRRectNormalShapeBuilder() : GetSDFRRectNormalShapeBuilder();
    if (!builder) {
        LOGE("GESDFRRectShaderShape::GenerateDrawingShaderHasNormal has builder error");
        return nullptr;
    }
    return GenerateShaderEffect(builder, useUniformRadius);
}

bool GESDFRRectShaderShape::UseUniformRadiusFastPath() const
{
    return params_.rrect.HasUniformCornerRadii() && params_.rrect.HasCircularCornerRadii();
}

GESDFRRectShaderShape::CornerRadii GESDFRRectShaderShape::ResolveCornerRadii(float halfWidth, float halfHeight) const
{
    CornerRadii radii {};
    CornerRadii rawRadii {};
    for (uint32_t index = 0; index < GERRect::CORNER_COUNT; ++index) {
        rawRadii[index] = params_.rrect.radius_[index];
        radii[index].x_ = ClampCornerRadius(params_.rrect.radius_[index].x_ + EXTEND, halfWidth);
        radii[index].y_ = ClampCornerRadius(params_.rrect.radius_[index].y_ + EXTEND, halfHeight);
    }
    float width = halfWidth * 2.0f;
    float height = halfHeight * 2.0f;

    float scaleX = 1.0f;
    scaleX = std::min(scaleX, width / std::max(radii[GERRect::TOP_LEFT].x_ + radii[GERRect::TOP_RIGHT].x_, EPSILON));
    scaleX = std::min(scaleX,
        width / std::max(radii[GERRect::BOTTOM_LEFT].x_ + radii[GERRect::BOTTOM_RIGHT].x_, EPSILON));

    float scaleY = 1.0f;
    scaleY = std::min(scaleY,
        height / std::max(radii[GERRect::TOP_LEFT].y_ + radii[GERRect::BOTTOM_LEFT].y_, EPSILON));
    scaleY = std::min(scaleY,
        height / std::max(radii[GERRect::TOP_RIGHT].y_ + radii[GERRect::BOTTOM_RIGHT].y_, EPSILON));
    if (scaleX < 1.0f || scaleY < 1.0f) {
        for (uint32_t index = 0; index < GERRect::CORNER_COUNT; ++index) {
            radii[index].x_ *= scaleX;
            radii[index].y_ *= scaleY;
        }
    }
    LOGD("GESDFRRectShaderShape::ResolveCornerRadii raw=%{public}s resolved=%{public}s halfSize=(%{public}.2f,"
        "%{public}.2f) scaleX=%{public}.4f scaleY=%{public}.4f",
        RadiiToString(rawRadii.data()).c_str(), RadiiToString(radii.data()).c_str(), halfWidth, halfHeight,
        scaleX, scaleY);
    return radii;
}

float GESDFRRectShaderShape::ResolveUniformRadius(float halfWidth, float halfHeight) const
{
    float maxRadius = std::min(halfWidth, halfHeight);
    return ClampCornerRadius(params_.rrect.GetCommonRadiusX() + EXTEND, maxRadius);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFRRectShaderShape::GetUniformSDFRRectShaderShapeBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> uniformSdfRRectShaderShapeBuilder = nullptr;
    if (uniformSdfRRectShaderShapeBuilder) {
        return uniformSdfRRectShaderShapeBuilder;
    }

    auto uniformSdfRRectShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(UNIFORM_RRECT_SHADER_PROG);
    if (!uniformSdfRRectShaderBuilderEffect) {
        LOGE("GESDFRRectShaderShape::GetUniformSDFRRectShaderShapeBuilder effect error");
        return nullptr;
    }

    uniformSdfRRectShaderShapeBuilder =
        std::make_shared<Drawing::RuntimeShaderBuilder>(uniformSdfRRectShaderBuilderEffect);
    return uniformSdfRRectShaderShapeBuilder;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFRRectShaderShape::GetUniformSDFRRectNormalShapeBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> uniformSdfRRectNormalShaderShapeBuilder = nullptr;
    if (uniformSdfRRectNormalShaderShapeBuilder) {
        return uniformSdfRRectNormalShaderShapeBuilder;
    }

    auto uniformSdfRRectNormalShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(UNIFORM_SDF_GRAD_PROG);
    if (!uniformSdfRRectNormalShaderBuilderEffect) {
        LOGE("GESDFRRectShaderShape::GetUniformSDFRRectNormalShapeBuilder effect error");
        return nullptr;
    }

    uniformSdfRRectNormalShaderShapeBuilder =
        std::make_shared<Drawing::RuntimeShaderBuilder>(uniformSdfRRectNormalShaderBuilderEffect);
    return uniformSdfRRectNormalShaderShapeBuilder;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFRRectShaderShape::GetSDFRRectShaderShapeBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> sdfRRectShaderShapeBuilder = nullptr;
    if (sdfRRectShaderShapeBuilder) {
        return sdfRRectShaderShapeBuilder;
    }

    auto sdfRRectShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(RRECT_SHADER_PROG);
    if (!sdfRRectShaderBuilderEffect) {
        LOGE("GESDFRRectShaderShape::GetSDFRRectShaderShapeBuilder effect error");
        return nullptr;
    }

    sdfRRectShaderShapeBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(sdfRRectShaderBuilderEffect);
    return sdfRRectShaderShapeBuilder;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFRRectShaderShape::GetSDFRRectNormalShapeBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> sdfRRectNormalShaderShapeBuilder = nullptr;
    if (sdfRRectNormalShaderShapeBuilder) {
        return sdfRRectNormalShaderShapeBuilder;
    }

    auto sdfRRectNormalShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(SDF_GRAD_PROG);
    if (!sdfRRectNormalShaderBuilderEffect) {
        LOGE("GESDFRRectShaderShape::GettSDFRRectNormalShapeBuilder effect error");
        return nullptr;
    }

    sdfRRectNormalShaderShapeBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(
        sdfRRectNormalShaderBuilderEffect);
    return sdfRRectNormalShaderShapeBuilder;
}

std::shared_ptr<ShaderEffect> GESDFRRectShaderShape::GenerateShaderEffect(
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder, bool useUniformRadius) const
{
    if (!builder) {
        LOGE("GESDFRRectShaderShape::GenerateShaderEffect builder error");
        return nullptr;
    }

    if (params_.rrect.width_ < MIN_SIZE || params_.rrect.height_ < MIN_SIZE) {
        return nullptr;
    }

    builder->SetUniform("centerPos", params_.rrect.left_ + params_.rrect.width_ * HALF,
        params_.rrect.top_ + params_.rrect.height_ * HALF);
    float halfWidth = params_.rrect.width_ * HALF + EXTEND;
    float halfHeight = params_.rrect.height_ * HALF + EXTEND;
    builder->SetUniform("halfSize", halfWidth, halfHeight);
    if (useUniformRadius) {
        builder->SetUniform("radius", ResolveUniformRadius(halfWidth, halfHeight));
    } else {
        CornerRadii cornerRadii = ResolveCornerRadii(halfWidth, halfHeight);
        builder->SetUniform("cornerRadiusTL", cornerRadii[GERRect::TOP_LEFT].x_, cornerRadii[GERRect::TOP_LEFT].y_);
        builder->SetUniform("cornerRadiusTR", cornerRadii[GERRect::TOP_RIGHT].x_,
            cornerRadii[GERRect::TOP_RIGHT].y_);
        builder->SetUniform("cornerRadiusBR", cornerRadii[GERRect::BOTTOM_RIGHT].x_,
            cornerRadii[GERRect::BOTTOM_RIGHT].y_);
        builder->SetUniform("cornerRadiusBL", cornerRadii[GERRect::BOTTOM_LEFT].x_,
            cornerRadii[GERRect::BOTTOM_LEFT].y_);
    }

    auto sdfRRectShapeShader = builder->MakeShader(nullptr, false);
    if (!sdfRRectShapeShader) {
        LOGE("GESDFRRectShaderShape::GenerateShaderEffect shaderEffect error");
    }
    return sdfRRectShapeShader;
}

bool GESDFRRectShaderShape::TryGetCenterAndHalfSize(float& outX, float& outY, Vector2f& shapeHalfSize) const
{
    const GERRect& rect = GetRRect();
    outX = rect.left_ + rect.width_ * 0.5f;
    outY = rect.top_ + rect.height_ * 0.5f;
    shapeHalfSize  = Vector2f(rect.width_ * 0.5f, rect.height_ * 0.5f);
    return true;
}
} // Drawing
} // namespace Rosen
} // namespace OHOS
