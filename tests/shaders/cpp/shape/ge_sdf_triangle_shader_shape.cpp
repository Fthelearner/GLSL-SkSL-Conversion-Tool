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

#include "ge_sdf_triangle_shader_shape.h"
#include "ge_log.h"
#include "ge_trace.h"

namespace OHOS {
namespace Rosen {
namespace Drawing {

static constexpr char SDF_GRAD_PROG[] = R"(
    uniform vec2 vertex0;
    uniform vec2 vertex1;
    uniform vec2 vertex2;
    uniform float radius;

    const float N_EPS = 1e-6;
    const float N_SCALE = 2048.0;

    vec2 safeNorm(vec2 v)
    {
        return v / max(length(v), N_EPS);
    }

    float sign2(vec2 p, vec2 v0, vec2 v1)
    {
        return (p.x - v1.x) * (v0.y - v1.y) - (v0.x - v1.x) * (p.y - v1.y);
    }

    vec3 sdgTriangle(vec2 p, vec2 v0, vec2 v1, vec2 v2)
    {
        vec2 e0 = v1 - v0;
        vec2 e1 = v2 - v1;
        float crossValue = e0.x * e1.y - e0.y * e1.x;

        if (abs(crossValue) < 1e-6) {
            float d = length(p - v0);
            d = max(d, 1e-6);
            return vec3(d, (p - v0) / d);
        }
        bool isClockwise = crossValue > 0.0;

        vec2 adjustedV1 = isClockwise ? v1 : v2;
        vec2 adjustedV2 = isClockwise ? v2 : v1;

        vec2 ae0 = adjustedV1 - v0;
        vec2 ae1 = adjustedV2 - adjustedV1;
        vec2 ae2 = v0 - adjustedV2;

        vec2 v0p = p - v0;
        vec2 v1p = p - adjustedV1;
        vec2 v2p = p - adjustedV2;

        vec2 pq0 = v0p - ae0 * clamp(dot(v0p, ae0) / dot(ae0, ae0), 0.0, 1.0);
        vec2 pq1 = v1p - ae1 * clamp(dot(v1p, ae1) / dot(ae1, ae1), 0.0, 1.0);
        vec2 pq2 = v2p - ae2 * clamp(dot(v2p, ae2) / dot(ae2, ae2), 0.0, 1.0);

        float d0 = dot(pq0, pq0);
        float d1 = dot(pq1, pq1);
        float d2 = dot(pq2, pq2);

        float d = min(min(d0, d1), d2);

        vec2 pq = (d == d0) ? pq0 : ((d == d1) ? pq1 : pq2);

        float s0 = sign2(p, v0, adjustedV1);
        float s1 = sign2(p, adjustedV1, adjustedV2);
        float s2 = sign2(p, adjustedV2, v0);

        bool inside = (s0 >= 0.0) && (s1 >= 0.0) && (s2 >= 0.0);

        float sd = sqrt(d) * (inside ? -1.0 : 1.0);
        vec2 grad = safeNorm(pq) * (inside ? -1.0 : 1.0);

        return vec3(sd, grad);
    }

    vec3 sdgRoundedTriangle(vec2 p, vec2 v0, vec2 v1, vec2 v2, float r)
    {
        float a = length(v1 - v0);
        float b = length(v2 - v1);
        float c = length(v0 - v2);
        float perimeter = a + b + c;

        vec2 incenter = (b * v0 + c * v1 + a * v2) / perimeter;
        float area = abs((v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y)) * 0.5;
        float inradius = area / (perimeter * 0.5);

        float clampedR = clamp(r, 0.0, inradius);

        float scale = (inradius - clampedR) / inradius;
        vec2 v0p = incenter + (v0 - incenter) * scale;
        vec2 v1p = incenter + (v1 - incenter) * scale;
        vec2 v2p = incenter + (v2 - incenter) * scale;

        vec3 sdg = sdgTriangle(p, v0p, v1p, v2p);

        return vec3(sdg.x - clampedR, sdg.yz);
    }

    float EncodeDir(vec2 dir)
    {
        float xPos = floor(dir.x + N_SCALE);
        float yPos = floor(dir.y + N_SCALE);
        return xPos + (yPos / N_SCALE) / 2.0;
    }

    vec4 main(float2 fragCoord)
    {
        vec3 sdg = sdgRoundedTriangle(fragCoord, vertex0, vertex1, vertex2, radius);
        float packedDir = EncodeDir(sdg.yz);
        return vec4(sdg.yz, packedDir, sdg.x);
    }
)";

std::shared_ptr<ShaderEffect> GESDFTriangleShaderShape::GenerateDrawingShader(float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFTriangleShaderShape::GenerateDrawingShader, Width: %g, Height: %g", width, height);
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = GetSDFTriangleShaderShapeBuilder();
    if (!builder) {
        LOGE("GESDFTriangleShaderShape::GenerateDrawingShader has builder error");
        return nullptr;
    }
    return GenerateShaderEffect(builder);
}

std::shared_ptr<ShaderEffect> GESDFTriangleShaderShape::GenerateDrawingShaderHasNormal(
    float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFTriangleShaderShape::GenerateDrawingShaderHasNormal, Width: %g, Height: %g",
        width, height);
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = GetSDFTriangleNormalShapeBuilder();
    if (!builder) {
        LOGE("GESDFTriangleShaderShape::GenerateDrawingShaderHasNormal has builder error");
        return nullptr;
    }
    return GenerateShaderEffect(builder);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFTriangleShaderShape::GetSDFTriangleShaderShapeBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> sdfTriangleShaderShapeBuilder = nullptr;
    if (sdfTriangleShaderShapeBuilder) {
        return sdfTriangleShaderShapeBuilder;
    }

    static constexpr char prog[] = R"(
        uniform vec2 vertex0;
        uniform vec2 vertex1;
        uniform vec2 vertex2;
        uniform float radius;

        float sign2(vec2 p, vec2 v0, vec2 v1)
        {
            return (p.x - v1.x) * (v0.y - v1.y) - (v0.x - v1.x) * (p.y - v1.y);
        }

        float sdfTriangle(vec2 p, vec2 v0, vec2 v1, vec2 v2)
        {
            vec2 e0 = v1 - v0;
            vec2 e1 = v2 - v1;
            float crossValue = e0.x * e1.y - e0.y * e1.x;

            if (abs(crossValue) < 1e-6) {
                return length(p - v0);
            }
            bool isClockwise = crossValue > 0.0;

            vec2 adjustedV1 = isClockwise ? v1 : v2;
            vec2 adjustedV2 = isClockwise ? v2 : v1;

            vec2 ae0 = adjustedV1 - v0;
            vec2 ae1 = adjustedV2 - adjustedV1;
            vec2 ae2 = v0 - adjustedV2;

            vec2 v0p = p - v0;
            vec2 v1p = p - adjustedV1;
            vec2 v2p = p - adjustedV2;

            vec2 pq0 = v0p - ae0 * clamp(dot(v0p, ae0) / dot(ae0, ae0), 0.0, 1.0);
            vec2 pq1 = v1p - ae1 * clamp(dot(v1p, ae1) / dot(ae1, ae1), 0.0, 1.0);
            vec2 pq2 = v2p - ae2 * clamp(dot(v2p, ae2) / dot(ae2, ae2), 0.0, 1.0);

            float d = min(min(dot(pq0, pq0), dot(pq1, pq1)), dot(pq2, pq2));

            float s0 = sign2(p, v0, adjustedV1);
            float s1 = sign2(p, adjustedV1, adjustedV2);
            float s2 = sign2(p, adjustedV2, v0);

            bool inside = (s0 >= 0.0) && (s1 >= 0.0) && (s2 >= 0.0);

            return sqrt(d) * (inside ? -1.0 : 1.0);
        }

        float sdfRoundedTriangle(vec2 p, vec2 v0, vec2 v1, vec2 v2, float r)
        {
            float a = length(v1 - v0);
            float b = length(v2 - v1);
            float c = length(v0 - v2);
            float perimeter = a + b + c;

            vec2 incenter = (b * v0 + c * v1 + a * v2) / perimeter;
            float area = abs((v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y)) * 0.5;
            float inradius = area / (perimeter * 0.5);

            float clampedR = clamp(r, 0.0, inradius);
            float scale = (inradius - clampedR) / inradius;
            vec2 v0p = incenter + (v0 - incenter) * scale;
            vec2 v1p = incenter + (v1 - incenter) * scale;
            vec2 v2p = incenter + (v2 - incenter) * scale;

            return sdfTriangle(p, v0p, v1p, v2p) - clampedR;
        }

        half4 main(vec2 fragCoord)
        {
            float sdf = sdfRoundedTriangle(fragCoord, vertex0, vertex1, vertex2, radius);
            return half4(0, 0, 0, sdf);
        }
    )";

    auto sdfTriangleShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (!sdfTriangleShaderBuilderEffect) {
        LOGE("GESDFTriangleShaderShape::GetSDFTriangleShaderShapeBuilder effect error");
        return nullptr;
    }

    sdfTriangleShaderShapeBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(sdfTriangleShaderBuilderEffect);
    return sdfTriangleShaderShapeBuilder;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFTriangleShaderShape::GetSDFTriangleNormalShapeBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> sdfTriangleNormalShaderShapeBuilder = nullptr;
    if (sdfTriangleNormalShaderShapeBuilder) {
        return sdfTriangleNormalShaderShapeBuilder;
    }

    auto sdfTriangleNormalShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(SDF_GRAD_PROG);
    if (!sdfTriangleNormalShaderBuilderEffect) {
        LOGE("GESDFTriangleShaderShape::GetSDFTriangleNormalShapeBuilder effect error");
        return nullptr;
    }

    sdfTriangleNormalShaderShapeBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(
        sdfTriangleNormalShaderBuilderEffect);
    return sdfTriangleNormalShaderShapeBuilder;
}

std::shared_ptr<ShaderEffect> GESDFTriangleShaderShape::GenerateShaderEffect(
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder) const
{
    if (!builder) {
        LOGE("GESDFTriangleShaderShape::GenerateShaderEffect builder error");
        return nullptr;
    }

    float triangleArea = (params_.vertex1.x_ - params_.vertex0.x_) * (params_.vertex2.y_ - params_.vertex0.y_) -
        (params_.vertex2.x_ - params_.vertex0.x_) * (params_.vertex1.y_ - params_.vertex0.y_);
    triangleArea = triangleArea > 0 ? triangleArea : -triangleArea;
    if (triangleArea < 0.0001f) {
        LOGE("GESDFTriangleShaderShape::GenerateShaderEffect invalid triangle area");
        return nullptr;
    }
    builder->SetUniform("vertex0", params_.vertex0.x_, params_.vertex0.y_);
    builder->SetUniform("vertex1", params_.vertex1.x_, params_.vertex1.y_);
    builder->SetUniform("vertex2", params_.vertex2.x_, params_.vertex2.y_);
    builder->SetUniform("radius", std::max(0.0f, params_.radius));

    auto sdfTriangleShapeShader = builder->MakeShader(nullptr, false);
    if (!sdfTriangleShapeShader) {
        LOGE("GESDFTriangleShaderShape::GenerateShaderEffect shaderEffect error");
    }
    return sdfTriangleShapeShader;
}
} // Drawing
} // namespace Rosen
} // namespace OHOS
