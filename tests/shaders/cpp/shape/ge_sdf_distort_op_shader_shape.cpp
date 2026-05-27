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

#include "ge_sdf_distort_op_shader_shape.h"

#include "ge_log.h"
#include "ge_trace.h"

namespace OHOS {
namespace Rosen {
namespace Drawing {

static constexpr float EPSILON = 1e-6f;

std::shared_ptr<ShaderEffect> GESDFDistortOpShaderShape::GenerateDrawingShader(float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFDistortOpShaderShape::GenerateDrawingShader, Width: %g, Height: %g", width, height);
    auto shapeShader = params_.shape ? params_.shape->GenerateDrawingShader(width, height) : nullptr;
    if (!shapeShader) {
        return nullptr;
    }
    return GenerateDistortOpDrawingShader(shapeShader, false, width, height);
}

std::shared_ptr<ShaderEffect> GESDFDistortOpShaderShape::GenerateDrawingShaderHasNormal(float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFDistortOpShaderShape::GenerateDrawingShaderHasNormal, Width: %g, Height: %g",
        width, height);
    auto shapeShader = params_.shape ? params_.shape->GenerateDrawingShader(width, height) : nullptr;
    if (!shapeShader) {
        return nullptr;
    }
    return GenerateDistortOpDrawingShader(shapeShader, true, width, height);
}

void GESDFDistortOpShaderShape::Preprocess(Canvas& canvas, const Rect& rect, bool hasNormal)
{
    if (params_.shape) {
        params_.shape->Preprocess(canvas, rect, hasNormal);
    }
}

std::shared_ptr<ShaderEffect> GESDFDistortOpShaderShape::GenerateDistortOpDrawingShader(
    std::shared_ptr<ShaderEffect> shapeShader, bool hasNormal, float width, float height) const
{
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = GetSDFDistortBuilder();
    if (!builder) {
        LOGE("GESDFDistortOpShaderShape::GenerateDrawingShader has builder error");
        return nullptr;
    }
    return GenerateDistortShaderEffect(shapeShader, builder, width, height);
}

constexpr char SHADER[] = R"(
    uniform shader shape;

    uniform vec2 iResolution;
    uniform vec2 invResolution;

    uniform vec2 lu;
    uniform vec2 e;
    uniform vec2 f;
    uniform vec2 g;

    uniform float k2;
    uniform float ik2;
    uniform float k1Base;
    uniform float k0Base;

    uniform vec4 barrelDistortion;
    uniform float distortionEnable;

    float Cross2d(vec2 a, vec2 b)
    {
        return a.x * b.y - a.y * b.x;
    }

    vec2 InvBilinear(vec2 p)
    {
        vec2 res = vec2(-1.0);

        vec2 h = p - lu;

        float localK1 = k1Base + Cross2d(p, g);
        float localK0 = k0Base + Cross2d(p, e);

        if (abs(k2) < 0.001) {
            res = vec2((h.x * localK1 + f.x * localK0) / (e.x * localK1 - g.x * localK0), -localK0 / localK1);
        } else {
            float w = localK1 * localK1 - 4.0 * localK0 * k2;
            if (w < 0.0) {
                return vec2(-1.0);
            }

            w = sqrt(w);

            float v = (-localK1 - w) * ik2;
            float u = (h.x - f.x * v) / (e.x + g.x * v);

            if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0) {
                v = (-localK1 + w) * ik2;
                u = (h.x - f.x * v) / (e.x + g.x * v);
            }
            res = vec2(u, v);
        }

        return res;
    }

    half4 main(vec2 fragCoord)
    {
        vec2 uv = fragCoord * invResolution;
        vec2 newUV = InvBilinear(uv);

        if (distortionEnable > 0.0) {
            vec2 lerpDistortion = vec2(mix(barrelDistortion.x, barrelDistortion.y, newUV.x),
                                       mix(barrelDistortion.z, barrelDistortion.w, newUV.y));
            vec2 centerNewUV = newUV - vec2(0.5);
            vec2 normFactor = 1.0 / (1.0 + lerpDistortion * 0.5); // keeps the corners aligned with the input parameters
            float l2 = dot(centerNewUV, centerNewUV);
            centerNewUV *= 1.0 + lerpDistortion * l2;
            centerNewUV *= normFactor;
            newUV = centerNewUV + vec2(0.5);
        }

        vec2 newCoord = newUV * iResolution;
        return shape.eval(newCoord);
    }
)";

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFDistortOpShaderShape::GetSDFDistortBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> sdfDistortShaderShapeBuilder = nullptr;
    if (sdfDistortShaderShapeBuilder) {
        return sdfDistortShaderShapeBuilder;
    }

    auto sdfDistortShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(SHADER);
    if (!sdfDistortShaderBuilderEffect) {
        LOGE("GESDFDistortOpShaderShape::GetSDFDistortBuilder effect error");
        return nullptr;
    }

    sdfDistortShaderShapeBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(sdfDistortShaderBuilderEffect);
    return sdfDistortShaderShapeBuilder;
}

std::shared_ptr<ShaderEffect> GESDFDistortOpShaderShape::GenerateDistortShaderEffect(
    std::shared_ptr<ShaderEffect> shapeShader, std::shared_ptr<Drawing::RuntimeShaderBuilder> builder, float width,
    float height) const
{
    if (!builder) {
        LOGE("GESDFDistortOpShaderShape::GenerateDistortShaderEffect builder error");
        return nullptr;
    }

    builder->SetChild("shape", shapeShader);
    builder->SetUniform("iResolution", width, height);
    builder->SetUniform("invResolution", 1.0f / std::max(width, EPSILON), 1.0f / std::max(height, EPSILON));

    ComputeAndSetUniforms(builder);
 
    auto sdfDistortShapeShader = builder->MakeShader(nullptr, false);
    if (!sdfDistortShapeShader) {
        LOGE("GESDFDistortOpShaderShape::GenerateDistortShaderEffect shaderEffect error");
    }
    return sdfDistortShapeShader;
}

void GESDFDistortOpShaderShape::ComputeAndSetUniforms(
    const std::shared_ptr<Drawing::RuntimeShaderBuilder>& builder) const
{
    const float luX = params_.LUCorner.GetX();
    const float luY = params_.LUCorner.GetY();
    const float ruX = params_.RUCorner.GetX();
    const float ruY = params_.RUCorner.GetY();
    const float rbX = params_.RBCorner.GetX();
    const float rbY = params_.RBCorner.GetY();
    const float lbX = params_.LBCorner.GetX();
    const float lbY = params_.LBCorner.GetY();

    auto Cross2d = [](float ax, float ay, float bx, float by) -> float {
        return ax * by - ay * bx;
    };

    // Precompute constants used by inverse bilinear mapping.
    const float ex = ruX - luX;
    const float ey = ruY - luY;
    const float fx = lbX - luX;
    const float fy = lbY - luY;
    const float gx = luX - ruX + rbX - lbX;
    const float gy = luY - ruY + rbY - lbY;

    const float k2 = Cross2d(gx, gy, fx, fy);
    const float safeK2 = (std::abs(k2) > EPSILON) ? k2 : ((k2 >= 0.0f) ? EPSILON : -EPSILON);
    const float ik2 = 0.5f / safeK2; // Keep the original formula v = (-k1 ± w) * 0.5 / k2
    const float k1Base = Cross2d(ex, ey, fx, fy) - Cross2d(luX, luY, gx, gy);
    const float k0Base = -Cross2d(luX, luY, ex, ey);

    builder->SetUniform("lu", luX, luY);
    builder->SetUniform("e", ex, ey);
    builder->SetUniform("f", fx, fy);
    builder->SetUniform("g", gx, gy);
    builder->SetUniform("k2", k2);
    builder->SetUniform("ik2", ik2);
    builder->SetUniform("k1Base", k1Base);
    builder->SetUniform("k0Base", k0Base);

    const int barrelDistortionSize = 4;
    float barrelDistortion[] = {params_.barrelDistortion[0], params_.barrelDistortion[1],
        params_.barrelDistortion[2], params_.barrelDistortion[3]};
    builder->SetUniform("barrelDistortion", barrelDistortion, barrelDistortionSize);

    const float distortionEnable = (std::abs(params_.barrelDistortion[0]) > EPSILON ||
        std::abs(params_.barrelDistortion[1]) > EPSILON || std::abs(params_.barrelDistortion[2]) > EPSILON ||
        std::abs(params_.barrelDistortion[3]) > EPSILON) ? 1.0f : 0.0f;
    builder->SetUniform("distortionEnable", distortionEnable);
}

bool GESDFDistortOpShaderShape::HasType(const GESDFShapeType type) const
{
    if (type == GetSDFShapeType()) {
        return true;
    }
    if (params_.shape ? params_.shape->HasType(type) : false) {
        return true;
    }
    return false;
}
} // namespace Drawing
} // namespace Rosen
} // namespace OHOS
