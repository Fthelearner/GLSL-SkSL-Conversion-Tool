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

#include "ge_sdf_union_op_shader_shape.h"

#include "ge_log.h"

#include "ge_trace.h"

namespace OHOS {
namespace Rosen {
namespace Drawing {

std::shared_ptr<ShaderEffect> GESDFUnionOpShaderShape::GenerateDrawingShader(float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFUnionOpShaderShape::GenerateDrawingShader, Type: %s , Width: %g, Height: %g",
        params_.op == GESDFUnionOp::UNION ? "UNION" : "SMOOTH_UNION", width, height);

    auto leftShader = params_.left ? params_.left->GenerateDrawingShader(width, height) : nullptr;
    auto rightShader = params_.right ? params_.right->GenerateDrawingShader(width, height) : nullptr;
    if (!leftShader && !rightShader) {
        return nullptr;
    }

    if (!leftShader) {
        return rightShader;
    }

    if (!rightShader) {
        return leftShader;
    }
    return GenerateUnionOpDrawingShader(leftShader, rightShader, false);
}

std::shared_ptr<ShaderEffect> GESDFUnionOpShaderShape::GenerateDrawingShaderHasNormal(float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFUnionOpShaderShape::GenerateDrawingShaderHasNormal, Type: %s , Width: %g, Height: %g",
        params_.op == GESDFUnionOp::UNION ? "UNION" : "SMOOTH_UNION", width, height);
    auto leftShader = params_.left ? params_.left->GenerateDrawingShaderHasNormal(width, height) : nullptr;
    auto rightShader = params_.right ? params_.right->GenerateDrawingShaderHasNormal(width, height) : nullptr;
    if (!leftShader && !rightShader) {
        return nullptr;
    }

    if (!leftShader) {
        return rightShader;
    }

    if (!rightShader) {
        return leftShader;
    }

    return GenerateUnionOpDrawingShader(leftShader, rightShader, true);
}

void GESDFUnionOpShaderShape::Preprocess(Canvas& canvas, const Rect& rect, bool hasNormal)
{
    if (params_.left) {
        params_.left->Preprocess(canvas, rect, hasNormal);
    }

    if (params_.right) {
        params_.right->Preprocess(canvas, rect, hasNormal);
    }
}

std::shared_ptr<ShaderEffect> GESDFUnionOpShaderShape::GenerateUnionOpDrawingShader(
    std::shared_ptr<ShaderEffect> leftShader, std::shared_ptr<ShaderEffect> rightShader, bool hasNormal) const
{
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = params_.op == GESDFUnionOp::UNION ? GetSDFUnionBuilder()
        : hasNormal ? GetSDFNormalSmoothUnionBuilder() : GetSDFSmoothUnionBuilder();
    if (!builder) {
        LOGE("GESDFUnionOpShaderShape::GenerateDrawingShader has builder error");
        return nullptr;
    }
    return params_.op == GESDFUnionOp::UNION ? GenerateUnionShaderEffect(leftShader, rightShader, builder)
                                             : GenerateSmoothUnionShaderEffect(leftShader, rightShader, builder);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFUnionOpShaderShape::GetSDFUnionBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> sdfUnionShaderShapeBuilder = nullptr;
    if (sdfUnionShaderShapeBuilder) {
        return sdfUnionShaderShapeBuilder;
    }

    static constexpr char prog[] = R"(
        uniform shader left;
        uniform shader right;
        half4 main(vec2 fragCoord)
        {
            vec4 leftShape = left.eval(fragCoord);
            vec4 rightShape = right.eval(fragCoord);

            // It is required for input SDF to be in this format: vec4(any, any, any, SDF)
            return leftShape.a < rightShape.a ? leftShape : rightShape;
        }
    )";

    auto sdfUnionShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (!sdfUnionShaderBuilderEffect) {
        LOGE("GESDFUnionOpShaderShape::GetSDFUnionBuilder effect error");
        return nullptr;
    }

    sdfUnionShaderShapeBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(sdfUnionShaderBuilderEffect);
    return sdfUnionShaderShapeBuilder;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFUnionOpShaderShape::GetSDFSmoothUnionBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> sdfSmoothUnionShaderShapeBuilder = nullptr;
    if (sdfSmoothUnionShaderShapeBuilder) {
        return sdfSmoothUnionShaderShapeBuilder;
    }

    static constexpr char prog[] = R"(
        uniform float spacing;
        uniform shader left;
        uniform shader right;

        vec4 sdfSmoothUnion(vec4 d1, vec4 d2, float k)
        {
            k*= 4.0;
            vec4 h = max(k - abs(d1 - d2), 0.0);
            return min(d1, d2) - h * h * 0.25 / k;
        }

        half4 main(vec2 fragCoord)
        {
            vec4 leftShape = left.eval(fragCoord);
            vec4 rightShape = right.eval(fragCoord);
            return sdfSmoothUnion(leftShape, rightShape, spacing);
        }
    )";

    auto sdfSmoothUnionShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (!sdfSmoothUnionShaderBuilderEffect) {
        LOGE("GESDFUnionOpShaderShape::GetSDFSmoothUnionBuilder effect error");
        return nullptr;
    }

    sdfSmoothUnionShaderShapeBuilder =
        std::make_shared<Drawing::RuntimeShaderBuilder>(sdfSmoothUnionShaderBuilderEffect);
    return sdfSmoothUnionShaderShapeBuilder;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFUnionOpShaderShape::GetSDFNormalSmoothUnionBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> sdfNormalSmoothUnionShaderShapeBuilder = nullptr;
    if (sdfNormalSmoothUnionShaderShapeBuilder) {
        return sdfNormalSmoothUnionShaderShapeBuilder;
    }

    static constexpr char prog[] = R"(
        uniform float spacing;
        uniform shader left;
        uniform shader right;

        vec4 sdgSmoothUnion(vec4 d1, vec4 d2, float k)
        {
            k *= 4.0;
            float h = max(k - abs(d1.a - d2.a), 0.0) / (2.0 * k);
            return vec4(mix(d1.xyz, d2.xyz, (d1.a < d2.a) ? h : 1.0 - h), min(d1.a, d2.a) - h * h * k);
        }

        half4 main(vec2 fragCoord)
        {
            vec4 leftShape = left.eval(fragCoord);
            vec4 rightShape = right.eval(fragCoord);
            return sdgSmoothUnion(leftShape, rightShape, spacing);
        }
    )";

    auto sdfNormalSmoothUnionShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (!sdfNormalSmoothUnionShaderBuilderEffect) {
        LOGE("GESDFUnionOpShaderShape::GetSDFNormalSmoothUnionBuilder effect error");
        return nullptr;
    }

    sdfNormalSmoothUnionShaderShapeBuilder =
        std::make_shared<Drawing::RuntimeShaderBuilder>(sdfNormalSmoothUnionShaderBuilderEffect);
    return sdfNormalSmoothUnionShaderShapeBuilder;
}

std::shared_ptr<ShaderEffect> GESDFUnionOpShaderShape::GenerateUnionShaderEffect(
    std::shared_ptr<ShaderEffect> leftShader, std::shared_ptr<ShaderEffect> rightShader,
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder) const
{
    if (!builder) {
        LOGE("GESDFUnionOpShaderShape::GenerateUnionShaderEffect builder error");
        return nullptr;
    }

    builder->SetChild("left", leftShader);
    builder->SetChild("right", rightShader);

    auto sdfUnionShapeShader = builder->MakeShader(nullptr, false);
    if (!sdfUnionShapeShader) {
        LOGE("GESDFRRectShaderShape::GenerateUnionShaderEffect shaderEffect error");
    }
    return sdfUnionShapeShader;
}

std::shared_ptr<ShaderEffect> GESDFUnionOpShaderShape::GenerateSmoothUnionShaderEffect(
    std::shared_ptr<ShaderEffect> leftShader, std::shared_ptr<ShaderEffect> rightShader,
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder) const
{
    if (!builder) {
        LOGE("GESDFUnionOpShaderShape::GenerateSmoothUnionShaderEffect builder error");
        return nullptr;
    }

    builder->SetChild("left", leftShader);
    builder->SetChild("right", rightShader);
    builder->SetUniform("spacing", std::max(params_.spacing, 0.0001f));

    auto sdfSmoothUnionShapeShader = builder->MakeShader(nullptr, false);
    if (!sdfSmoothUnionShapeShader) {
        LOGE("GESDFRRectShaderShape::GenerateSmoothUnionShaderEffect shaderEffect error");
    }
    return sdfSmoothUnionShapeShader;
}

bool GESDFUnionOpShaderShape::HasType(const GESDFShapeType type) const
{
    if (type == GetSDFShapeType()) {
        return true;
    }
    if (params_.left ? params_.left->HasType(type) : false) {
        return true;
    }
    if (params_.right ? params_.right->HasType(type) : false) {
        return true;
    }
    return false;
}
} // namespace Drawing
} // namespace Rosen
} // namespace OHOS