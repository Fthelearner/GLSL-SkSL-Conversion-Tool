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

#include "ge_sdf_transform_shader_shape.h"
#include "ge_log.h"
#include "ge_trace.h"

namespace OHOS {
namespace Rosen {
namespace Drawing {
std::shared_ptr<ShaderEffect> GESDFTransformShaderShape::GenerateDrawingShader(float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFTransformShaderShape::GenerateDrawingShader, Width: %g, Height: %g", width, height);
    if (!params_.shape) {
        return nullptr;
    }
    
    if (params_.unionMode == 1) {
        return GenerateGravityPullDrawingShader(width, height);
    }
    
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = GetSDFTransformShaderShapeBuilder();
    if (!builder) {
        LOGE("GESDFTransformShaderShape::GenerateDrawingShader has builder error");
        return nullptr;
    }
    auto shapeShader = params_.shape->GenerateDrawingShader(width, height);
    if (!shapeShader) {
        LOGE("GESDFTransformShaderShape::GenerateDrawingShader has empty shader");
        return nullptr;
    }
    auto sdfTransformShapeShader = GenerateShaderEffect(width, height, shapeShader, builder);
    return sdfTransformShapeShader;
}

std::shared_ptr<ShaderEffect> GESDFTransformShaderShape::GenerateDrawingShaderHasNormal(
    float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFTransformShaderShape::GenerateDrawingShaderHasNormal, Width: %g, Height: %g",
        width, height);
    if (!params_.shape) {
        return nullptr;
    }
    
    if (params_.unionMode == 1) {
        return GenerateGravityPullDrawingShaderHasNormal(width, height);
    }
    
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = GetSDFTransformShaderHasNormalShapeBuilder();
    if (!builder) {
        LOGE("GESDFTransformShaderShape::GenerateDrawingShaderHasNormal has builder error");
        return nullptr;
    }
    auto shapeShader = params_.shape->GenerateDrawingShaderHasNormal(width, height);
    if (!shapeShader) {
        LOGE("GESDFTransformShaderShape::GenerateDrawingShaderHasNormal has empty shader");
        return nullptr;
    }
    auto sdfTransformShapeShader = GenerateShaderEffect(width, height, shapeShader, builder);
    return sdfTransformShapeShader;
}

void GESDFTransformShaderShape::Preprocess(Canvas& canvas, const Rect& rect, bool hasNormal)
{
    if (params_.shape) {
        params_.shape->Preprocess(canvas, rect, hasNormal);
    }
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFTransformShaderShape::GetSDFTransformShaderShapeBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> sdfTransformShaderShapeBuilder = nullptr;
    if (sdfTransformShaderShapeBuilder) {
        return sdfTransformShaderShapeBuilder;
    }

    static constexpr char prog[] = R"(
        uniform shader shapeShader;
        uniform float3x3 transformMatrix;

        half4 main(vec2 fragCoord) {
            vec3 transformedCoord = transformMatrix * vec3(fragCoord, 1.0);
            vec2 perspectiveCoord = transformedCoord.xy;
            if (abs(transformedCoord.z) > 0.00001) {
                perspectiveCoord = transformedCoord.xy / transformedCoord.z;
            }
            return shapeShader.eval(perspectiveCoord);
        }
    )";

    auto sdfTransformShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (!sdfTransformShaderBuilderEffect) {
        LOGE("GESDFTransformShaderShape::GetSDFTransformShaderShapeBuilder effect error");
        return nullptr;
    }

    sdfTransformShaderShapeBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(sdfTransformShaderBuilderEffect);
    return sdfTransformShaderShapeBuilder;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFTransformShaderShape::GetSDFTransformShaderHasNormalShapeBuilder()
    const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> sdfTransformShaderHasNormalShapeBuilder = nullptr;
    if (sdfTransformShaderHasNormalShapeBuilder) {
        return sdfTransformShaderHasNormalShapeBuilder;
    }

    static constexpr char prog[] = R"(
        uniform shader shapeShader;
        uniform float3x3 transformMatrix;
        const float nScale = 2048.0;
        float EncodeDir(vec2 dir)
        {
            float xPos = floor(dir.x + nScale);
            float yPos = floor(dir.y + nScale);
            return xPos + (yPos / nScale) / 2.0;
        }
        vec2 DecodeDir(float z) {
            float xPos = floor(z);
            float yPos = (z - xPos) * nScale * 2.0 - nScale;
            xPos -= nScale;
            return vec2(xPos, yPos);
        }
        half4 main(vec2 fragCoord) {
            // only apply transform to rgba's a pipeline
            float2x2 invtransformMatrix =
                float2x2(transformMatrix[0][0], transformMatrix[1][0], transformMatrix[0][1], transformMatrix[1][1]);
            vec3 transformedCoord = transformMatrix * vec3(fragCoord, 1.0);
            vec2 perspectiveCoord = transformedCoord.xy;

            // but scaling may be applied to all ?
            if (abs(transformedCoord.z) > 0.00001) {
                perspectiveCoord = transformedCoord.xy / transformedCoord.z;
            }
            // calculate rgb
            half4 res = shapeShader.eval(perspectiveCoord);
            res.rg = invtransformMatrix * res.rg;
            vec2 centerVec = invtransformMatrix * DecodeDir(res.b);
            res.b = EncodeDir(centerVec);
            return res;
        }
    )";

    auto sdfTransformShaderHasNormalBuilderEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (!sdfTransformShaderHasNormalBuilderEffect) {
        LOGE("GESDFTransformShaderShape::sdfTransformShaderHasNormalShapeBuilder effect error");
        return nullptr;
    }

    sdfTransformShaderHasNormalShapeBuilder =
        std::make_shared<Drawing::RuntimeShaderBuilder>(sdfTransformShaderHasNormalBuilderEffect);
    return sdfTransformShaderHasNormalShapeBuilder;
}

std::shared_ptr<ShaderEffect> GESDFTransformShaderShape::GenerateShaderEffect(float width, float height,
    std::shared_ptr<ShaderEffect> shapeShader, std::shared_ptr<Drawing::RuntimeShaderBuilder> builder) const
{
    if (!builder) {
        LOGE("GESDFTransformShaderShape::GenerateShaderEffect builder error");
        return nullptr;
    }

    if (params_.matrix.IsIdentity()) {
        return shapeShader;
    }

    // turn transform  from shape to fragcoord
    Drawing::Matrix invertMatrix;
    if (params_.matrix.Invert(invertMatrix)) {
        builder->SetUniform("transformMatrix", invertMatrix);
    } else {
        LOGE("GESDFTransformShaderShape::GenerateShaderEffect, invert matrix failed");
        return Drawing::ShaderEffect::CreateColorShader(0);
    }
    builder->SetChild("shapeShader", shapeShader);

    auto sdfTransformShapeShader = builder->MakeShader(nullptr, false);
    if (!sdfTransformShapeShader) {
        LOGE("GESDFTransformShaderShape::GenerateShaderEffect shaderEffect error");
    }
    return sdfTransformShapeShader;
}

bool GESDFTransformShaderShape::HasType(const GESDFShapeType type) const
{
    if (type == GetSDFShapeType()) {
        return true;
    }
    if (params_.shape ? params_.shape->HasType(type) : false) {
        return true;
    }
    return false;
}

bool GESDFTransformShaderShape::TryGetCenterAndHalfSize(float& outX, float& outY, Vector2f& shapeHalfSize) const
{
    if (!params_.shape) {
        return false;
    }

    float childX = 0.0f;
    float childY = 0.0f;
    Vector2f childShapeHalfSize = Vector2f(0.0f, 0.0f);
    if (!params_.shape->TryGetCenterAndHalfSize(childX, childY, childShapeHalfSize)) {
        return false;
    }

    std::vector<Point> src = {Point(childX, childY)};
    std::vector<Point> dst(1);
    params_.matrix.MapPoints(dst, src, 1);
    outX = dst[0].GetX();
    outY = dst[0].GetY();
    shapeHalfSize = childShapeHalfSize;
    return true;
}

struct GravityPullInfluenceParams {
    float radiusX = 0.0f;
    float radiusY = 0.0f;
    float useEllipse = 0.0f;
};

static GravityPullInfluenceParams ComputeGravityPullInfluenceParams(float spacing, const Vector2f& shapeHalfSize,
    const Vector2f& shapeCenterPos, const Vector2f& centerPos)
{
    constexpr float epsilon = 1e-6f;
    constexpr float squareTolerance = 1e-3f;

    float warpRadius = std::max(spacing, epsilon);

    float halfWidth = std::max(std::abs(shapeHalfSize[0]), epsilon);
    float halfHeight = std::max(std::abs(shapeHalfSize[1]), epsilon);

    float shortHalfSize = std::min(halfWidth, halfHeight);
    float longHalfSize = std::max(halfWidth, halfHeight);

    bool isRectangle = (longHalfSize - shortHalfSize) > squareTolerance * longHalfSize;
    bool spacingTooLarge = 2.0f * shortHalfSize < warpRadius;

    GravityPullInfluenceParams params;
    if (!isRectangle || !spacingTooLarge) {
        params.radiusX = warpRadius;
        params.radiusY = warpRadius;
        params.useEllipse = 0.0f;
        return params;
    }

    float deltaX = std::abs(centerPos[0] - shapeCenterPos[0]);
    float deltaY = std::abs(centerPos[1] - shapeCenterPos[1]);

    params.radiusX = warpRadius;
    params.radiusY = warpRadius;
    params.useEllipse = 1.0f;

    if (halfWidth <= halfHeight) {
        float shortAxisRadius = deltaX + halfWidth;
        params.radiusX = std::max(std::min(warpRadius, shortAxisRadius), epsilon);
    } else {
        float shortAxisRadius = deltaY + halfHeight;
        params.radiusY = std::max(std::min(warpRadius, shortAxisRadius), epsilon);
    }

    return params;
}

static constexpr char GRAVITY_PULL_PROG[] = R"(
    uniform float spacing;
    uniform float warpStrength;
    uniform float hotZone;
    uniform vec2 shapeCenterPos;
    uniform vec2 centerPos;
    uniform vec2 influenceRadii;
    uniform float useEllipse;
    uniform shader shapeShader;
    const float kEpsilon = 1e-6;
    vec2 NormalizeWithLenSq(vec2 v, float lenSq)
    {
        return (lenSq > kEpsilon) ? v * inversesqrt(lenSq) : vec2(0.0, 0.0);
    }
    float RationalFalloff(float t, float k)
    {
        t = clamp(t, 0.0, 1.0);
        return (1.0 - t) / (1.0 + k * t);
    }
    float BlendedRationalFalloff(float localT)
    {
        localT = clamp(localT, 0.0, 1.0);
        float k0 = 4.0;
        float k1 = 15.0;
        float start = 0.05;
        float end = 0.75;
        float a = RationalFalloff(localT, k0);
        float b = RationalFalloff(localT, k1);
        float w = smoothstep(start, end, localT);
        return mix(a, b, w);
    }
    float InfluenceT(vec2 fromSmallCenter, float warpRadiusSq)
    {
        if (useEllipse > 0.5) {
            vec2 radii = max(abs(influenceRadii), vec2(kEpsilon, kEpsilon));
            vec2 q = fromSmallCenter / radii;
            return dot(q, q);
        }
        return dot(fromSmallCenter, fromSmallCenter) / warpRadiusSq;
    }
    float InfluenceRadiusAlongDir(vec2 dir, float warpRadius)
    {
        if (useEllipse > 0.5) {
            vec2 radii = max(abs(influenceRadii), vec2(kEpsilon, kEpsilon));
            vec2 q = dir / radii;
            float denom = dot(q, q);
            return (denom > kEpsilon) ? inversesqrt(denom) : min(radii.x, radii.y);
        }
        return warpRadius;
    }
    vec2 ApplyLongAxisDamping(vec2 warpOffset)
    {
        if (useEllipse <= 0.5) {
            return warpOffset;
        }
        vec2 radii = max(abs(influenceRadii), vec2(kEpsilon, kEpsilon));
        if (abs(radii.x - radii.y) <= kEpsilon) {
            return warpOffset;
        }
        float longAxisScale = 0.3;
        if (radii.x > radii.y) {
            warpOffset.x *= longAxisScale;
        } else {
            warpOffset.y *= longAxisScale;
        }
        return warpOffset;
    }
    vec2 WarpedSamplePos(vec2 p)
    {
        float warpRadius = max(spacing, kEpsilon);
        float warpRadiusSq = warpRadius * warpRadius;
        vec2 fromSmallCenter = p - centerPos;
        float localT = InfluenceT(fromSmallCenter, warpRadiusSq);
        if (localT >= 1.0) {
            return p;
        }
        vec2 delta = centerPos - shapeCenterPos;
        float centerDistSq = dot(delta, delta);
        if (centerDistSq <= kEpsilon) {
            return p;
        }
        float insideValue = shapeShader.eval(centerPos).a;
        float insideGate = smoothstep(-hotZone, 0.0, insideValue);
        if (insideGate <= kEpsilon) {
            return p;
        }
        float hotGate = 1.0 - smoothstep(0.0, hotZone, insideValue);
        float rawWarpScale = warpStrength * insideGate * hotGate;
        vec2 pullDir = NormalizeWithLenSq(delta, centerDistSq);
        float centerDist = sqrt(centerDistSq);
        float radiusAlongPull = InfluenceRadiusAlongDir(pullDir, warpRadius);
        float maxWarpScale = min(0.8 * radiusAlongPull, 0.8 * centerDist);
        float warpScale = clamp(rawWarpScale, -maxWarpScale, maxWarpScale);
        if (abs(warpScale) <= kEpsilon) {
            return p;
        }
        float falloff = BlendedRationalFalloff(localT);
        vec2 warpOffset = pullDir * (warpScale * falloff);
        warpOffset = ApplyLongAxisDamping(warpOffset);
        return p - warpOffset;
    }
    float MergedSdfAt(vec2 p)
    {
        return shapeShader.eval(WarpedSamplePos(p)).a;
    }
    half4 main(vec2 p)
    {
        return half4(0.0, 0.0, 0.0, MergedSdfAt(p));
    }
)";

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFTransformShaderShape::GetGravityPullDrawingShaderBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> gravityPullShaderBuilder = nullptr;
    if (gravityPullShaderBuilder) {
        return gravityPullShaderBuilder;
    }

    auto gravityPullShaderEffect = Drawing::RuntimeEffect::CreateForShader(GRAVITY_PULL_PROG);
    if (!gravityPullShaderEffect) {
        LOGE("GESDFTransformShaderShape::GetGravityPullDrawingShaderBuilder effect error");
        return nullptr;
    }

    gravityPullShaderBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(gravityPullShaderEffect);
    return gravityPullShaderBuilder;
}

std::shared_ptr<ShaderEffect> GESDFTransformShaderShape::GenerateGravityPullDrawingShader(
    float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFTransformShaderShape::GenerateGravityPullDrawingShader, Width: %g, Height: %g",
        width, height);
    
    auto baseShapeShader = params_.shape->GenerateDrawingShader(width, height);
    if (!baseShapeShader) {
        LOGE("GESDFTransformShaderShape::GenerateGravityPullDrawingShader has empty base shader");
        return nullptr;
    }

    auto transformBuilder = GetSDFTransformShaderShapeBuilder();
    if (!transformBuilder) {
        LOGE("GESDFTransformShaderShape::GenerateGravityPullDrawingShader has transform builder error");
        return nullptr;
    }
    auto transformedShapeShader = GenerateShaderEffect(width, height, baseShapeShader, transformBuilder);
    if (!transformedShapeShader) {
        LOGE("GESDFTransformShaderShape::GenerateGravityPullDrawingShader has transformed shader error");
        return nullptr;
    }
    
    auto gravityPullShaderBuilder = GetGravityPullDrawingShaderBuilder();
    if (!gravityPullShaderBuilder) {
        LOGE("GESDFTransformShaderShape::GenerateGravityPullDrawingShader has gravity pull shader builder error");
        return nullptr;
    }

    float shapeCenterX;
    float shapeCenterY;
    Vector2f shapeHalfSize;
    if (!TryGetCenterAndHalfSize(shapeCenterX, shapeCenterY, shapeHalfSize)) {
        LOGE("GESDFTransformShaderShape::GenerateGravityPullDrawingShader get transformed shape center error");
    }

    float safeSpacing = std::max(params_.spacing, 0.0001f);
    float safeHotZone = std::max(params_.hotZone, 0.0001f);

    Vector2f shapeCenterPos = { shapeCenterX, shapeCenterY };
    Vector2f centerPos = {
        params_.centerPosition[0],
        params_.centerPosition[1]
    };

    GravityPullInfluenceParams influenceParams =
        ComputeGravityPullInfluenceParams(safeSpacing, shapeHalfSize, shapeCenterPos, centerPos);

    gravityPullShaderBuilder->SetUniform("spacing", safeSpacing);
    gravityPullShaderBuilder->SetUniform("warpStrength", params_.warpStrength);
    gravityPullShaderBuilder->SetUniform("hotZone", safeHotZone);
    gravityPullShaderBuilder->SetUniform("shapeCenterPos", shapeCenterX, shapeCenterY);
    gravityPullShaderBuilder->SetUniform("centerPos", params_.centerPosition[0], params_.centerPosition[1]);
    gravityPullShaderBuilder->SetUniform("influenceRadii", influenceParams.radiusX, influenceParams.radiusY);
    gravityPullShaderBuilder->SetUniform("useEllipse", influenceParams.useEllipse);
    gravityPullShaderBuilder->SetChild("shapeShader", transformedShapeShader);

    auto gravityPullShader = gravityPullShaderBuilder->MakeShader(nullptr, false);
    if (!gravityPullShader) {
        LOGE("GESDFTransformShaderShape::GenerateGravityPullDrawingShader gravity pull shader effect error");
        return nullptr;
    }
    
    return gravityPullShader;
}

static constexpr char GRAVITY_PULL_NORMAL_PROG[] = R"(
    uniform float spacing;
    uniform float warpStrength;
    uniform float hotZone;
    uniform vec2 shapeCenterPos;
    uniform vec2 centerPos;
    uniform vec2 influenceRadii;
    uniform float useEllipse;
    uniform shader shapeShader;
    const float kEpsilon = 1e-6;
    vec2 NormalizeWithLenSq(vec2 v, float lenSq)
    {
        return (lenSq > kEpsilon) ? v * inversesqrt(lenSq) : vec2(0.0, 0.0);
    }
    float RationalFalloff(float t, float k)
    {
        t = clamp(t, 0.0, 1.0);
        return (1.0 - t) / (1.0 + k * t);
    }
    float BlendedRationalFalloff(float localT)
    {
        localT = clamp(localT, 0.0, 1.0);
        float k0 = 4.0;
        float k1 = 15.0;
        float start = 0.05;
        float end = 0.75;
        float a = RationalFalloff(localT, k0);
        float b = RationalFalloff(localT, k1);
        float w = smoothstep(start, end, localT);
        return mix(a, b, w);
    }
    float InfluenceT(vec2 fromSmallCenter, float warpRadiusSq)
    {
        if (useEllipse > 0.5) {
            vec2 radii = max(abs(influenceRadii), vec2(kEpsilon, kEpsilon));
            vec2 q = fromSmallCenter / radii;
            return dot(q, q);
        }
        return dot(fromSmallCenter, fromSmallCenter) / warpRadiusSq;
    }
    float InfluenceRadiusAlongDir(vec2 dir, float warpRadius)
    {
        if (useEllipse > 0.5) {
            vec2 radii = max(abs(influenceRadii), vec2(kEpsilon, kEpsilon));
            vec2 q = dir / radii;
            float denom = dot(q, q);
            return (denom > kEpsilon) ? inversesqrt(denom) : min(radii.x, radii.y);
        }
        return warpRadius;
    }
    vec2 ApplyLongAxisDamping(vec2 warpOffset)
    {
        if (useEllipse <= 0.5) {
            return warpOffset;
        }
        vec2 radii = max(abs(influenceRadii), vec2(kEpsilon, kEpsilon));
        if (abs(radii.x - radii.y) <= kEpsilon) {
            return warpOffset;
        }
        float longAxisScale = 0.3;
        if (radii.x > radii.y) {
            warpOffset.x *= longAxisScale;
        } else {
            warpOffset.y *= longAxisScale;
        }
        return warpOffset;
    }
    vec2 WarpedSamplePos(vec2 p)
    {
        float warpRadius = max(spacing, kEpsilon);
        float warpRadiusSq = warpRadius * warpRadius;
        vec2 fromSmallCenter = p - centerPos;
        float localT = InfluenceT(fromSmallCenter, warpRadiusSq);
        if (localT >= 1.0) {
            return p;
        }
        vec2 delta = centerPos - shapeCenterPos;
        float centerDistSq = dot(delta, delta);
        if (centerDistSq <= kEpsilon) {
            return p;
        }
        float insideValue = shapeShader.eval(centerPos).a;
        float insideGate = smoothstep(-hotZone, 0.0, insideValue);
        if (insideGate <= kEpsilon) {
            return p;
        }
        float hotGate = 1.0 - smoothstep(0.0, hotZone, insideValue);
        float rawWarpScale = warpStrength * insideGate * hotGate;
        vec2 pullDir = NormalizeWithLenSq(delta, centerDistSq);
        float centerDist = sqrt(centerDistSq);
        float radiusAlongPull = InfluenceRadiusAlongDir(pullDir, warpRadius);
        float maxWarpScale = min(0.8 * radiusAlongPull, 0.8 * centerDist);
        float warpScale = clamp(rawWarpScale, -maxWarpScale, maxWarpScale);
        if (abs(warpScale) <= kEpsilon) {
            return p;
        }
        float falloff = BlendedRationalFalloff(localT);
        vec2 warpOffset = pullDir * (warpScale * falloff);
        warpOffset = ApplyLongAxisDamping(warpOffset);
        return p - warpOffset;
    }
    half4 main(vec2 p)
    {
        vec4 shape = shapeShader.eval(WarpedSamplePos(p));
        return half4(shape.x, shape.y, 0.0, shape.a);
    }
)";

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFTransformShaderShape::GetGravPullDrawingShaderHasNormBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> gravPullShaderHasNormalBuilder = nullptr;
    if (gravPullShaderHasNormalBuilder) {
        return gravPullShaderHasNormalBuilder;
    }

    auto gravityPullShaderHasNormalEffect = Drawing::RuntimeEffect::CreateForShader(GRAVITY_PULL_NORMAL_PROG);
    if (!gravityPullShaderHasNormalEffect) {
        LOGE("GESDFTransformShaderShape::GetGravPullDrawingShaderHasNormBuilder effect error");
        return nullptr;
    }

    gravPullShaderHasNormalBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(gravityPullShaderHasNormalEffect);
    return gravPullShaderHasNormalBuilder;
}

std::shared_ptr<ShaderEffect> GESDFTransformShaderShape::GenerateGravityPullDrawingShaderHasNormal(
    float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFTransformShaderShape::GenerateGravityPullDrawingShaderHasNormal, Width: %g, Height: %g",
        width, height);
    
    auto baseShapeShader = params_.shape->GenerateDrawingShaderHasNormal(width, height);
    if (!baseShapeShader) {
        LOGE("GESDFTransformShaderShape::GenerateGravityPullDrawingShaderHasNormal has empty base shader");
        return nullptr;
    }

    auto transformBuilder = GetSDFTransformShaderHasNormalShapeBuilder();
    if (!transformBuilder) {
        LOGE("GESDFTransformShaderShape::GenerateGravityPullDrawingShaderHasNormal has transform builder error");
        return nullptr;
    }
    auto transformedShapeShader = GenerateShaderEffect(width, height, baseShapeShader, transformBuilder);
    if (!transformedShapeShader) {
        LOGE("GESDFTransformShaderShape::GenerateGravityPullDrawingShaderHasNormal has transformed shader error");
        return nullptr;
    }
    
    auto gravityPullShaderBuilder = GetGravPullDrawingShaderHasNormBuilder();
    if (!gravityPullShaderBuilder) {
        LOGE("GESDFTransformShaderShape::GenerateGravPullDrawingShaderHasNormal has gravity pull shader builder error");
        return nullptr;
    }

    float shapeCenterX;
    float shapeCenterY;
    Vector2f shapeHalfSize;
    if (!TryGetCenterAndHalfSize(shapeCenterX, shapeCenterY, shapeHalfSize)) {
        LOGE("GESDFTransformShaderShape::GenerateGravityPullDrawingShader get transformed shape center error");
    }

    float safeSpacing = std::max(params_.spacing, 0.0001f);
    float safeHotZone = std::max(params_.hotZone, 0.0001f);

    Vector2f shapeCenterPos = { shapeCenterX, shapeCenterY };
    Vector2f centerPos = {
        params_.centerPosition[0],
        params_.centerPosition[1]
    };

    GravityPullInfluenceParams influenceParams =
        ComputeGravityPullInfluenceParams(safeSpacing, shapeHalfSize, shapeCenterPos, centerPos);

    gravityPullShaderBuilder->SetUniform("spacing", safeSpacing);
    gravityPullShaderBuilder->SetUniform("warpStrength", params_.warpStrength);
    gravityPullShaderBuilder->SetUniform("hotZone", safeHotZone);
    gravityPullShaderBuilder->SetUniform("shapeCenterPos", shapeCenterX, shapeCenterY);
    gravityPullShaderBuilder->SetUniform("centerPos", params_.centerPosition[0], params_.centerPosition[1]);
    gravityPullShaderBuilder->SetUniform("influenceRadii", influenceParams.radiusX, influenceParams.radiusY);
    gravityPullShaderBuilder->SetUniform("useEllipse", influenceParams.useEllipse);
    gravityPullShaderBuilder->SetChild("shapeShader", transformedShapeShader);

    auto gravityPullShader = gravityPullShaderBuilder->MakeShader(nullptr, false);
    if (!gravityPullShader) {
        LOGE("GESDFTransformShaderShape::GenerateGravityPullDrawingShaderHasNormal gravity pull shader effect error");
        return nullptr;
    }
    
    return gravityPullShader;
}
} // Drawing
} // namespace Rosen
} // namespace OHOS