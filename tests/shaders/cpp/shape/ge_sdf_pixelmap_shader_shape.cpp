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

#include "ge_sdf_pixelmap_shader_shape.h"

#include "ge_log.h"

#include "ge_trace.h"

namespace OHOS {
namespace Rosen {
namespace Drawing {
constexpr float HALF = 0.5;
std::shared_ptr<ShaderEffect> GESDFPixelmapShaderShape::GenerateDrawingShader(float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFPixelmapShaderShape::GenerateDrawingShader, Width: %g, Height: %g", width, height);
    if (params_.image == nullptr) {
        LOGE("GESDFPixelmapShaderShape::GenerateDrawingShader has empty image");
        return nullptr;
    }
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = GetSDFPixelmapShaderShapeBuilder();
    if (!builder) {
        LOGE("GESDFPixelmapShaderShape::GenerateDrawingShader has builder error");
        return nullptr;
    }
    auto pixelmapShader = GeneratePixelmapShader();
    if (!pixelmapShader) {
        LOGE("GESDFPixelmapShaderShape::GenerateDrawingShader has empty shader");
        return nullptr;
    }
    auto sdfPixelmapShapeShader = GenerateSDFShaderEffect(pixelmapShader, params_.image->GetImageInfo(), builder);
    return sdfPixelmapShapeShader;
}

std::shared_ptr<ShaderEffect> GESDFPixelmapShaderShape::GenerateDrawingShaderHasNormal(float width, float height) const
{
    GE_TRACE_NAME_FMT("GESDFPixelmapShaderShape::GenerateDrawingShaderHasNormal, Width: %g, Height: %g", width, height);
    if (params_.image == nullptr) {
        LOGE("GESDFPixelmapShaderShape::GenerateDrawingShaderHasNormal has empty image");
        return nullptr;
    }
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder = GetSDFPixelmapNormalShaderShapeBuilder();
    if (!builder) {
        LOGE("GESDFPixelmapShaderShape::GenerateDrawingShaderHasNormal has builder error");
        return nullptr;
    }
    auto pixelmapShader = GeneratePixelmapShader();
    if (!pixelmapShader) {
        LOGE("GESDFPixelmapShaderShape::GenerateDrawingShaderHasNormal has empty shader");
        return nullptr;
    }
    auto sdfPixelmapShapeShader =
        GenerateSDFNormalShaderEffect(pixelmapShader, params_.image->GetImageInfo(), width, height, builder);
    return sdfPixelmapShapeShader;
}

std::shared_ptr<ShaderEffect> GESDFPixelmapShaderShape::GeneratePixelmapShader() const
{
    Drawing::Matrix matrix;
    return Drawing::ShaderEffect::CreateImageShader(*params_.image, Drawing::TileMode::CLAMP, Drawing::TileMode::CLAMP,
        Drawing::SamplingOptions(Drawing::FilterMode::LINEAR), matrix);
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFPixelmapShaderShape::GetSDFPixelmapShaderShapeBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> sdfPixelmapShaderShapeBuilder = nullptr;
    if (sdfPixelmapShaderShapeBuilder) {
        return sdfPixelmapShaderShapeBuilder;
    }

    static constexpr char prog[] = R"(
        uniform shader pixelmapShader;
        uniform vec2 pixelmapSize;

        vec4 SafeFetch(vec2 fragCoord) {
            if (fragCoord.x < 0 || fragCoord.x >= pixelmapSize.x || fragCoord.y < 0 || fragCoord.y >= pixelmapSize.y) {
                return vec4(0.5, 0.5, 0.0, 1);
            }
            return pixelmapShader.eval(fragCoord);
        }

        half4 main(vec2 fragCoord)
        {
            // SDF will be mapped from [0, 1] to [-63.5, 64]
            vec4 pixelmapShape = SafeFetch(fragCoord);
            return half4(pixelmapShape.xyz, pixelmapShape.a * 127.5 - 63.5);
        }
    )";

    auto sdfPixelmapShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (!sdfPixelmapShaderBuilderEffect) {
        LOGE("GESDFPixelmapShaderShape::GetSDFPixelmapShaderShapeBuilder effect error");
        return nullptr;
    }

    sdfPixelmapShaderShapeBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(sdfPixelmapShaderBuilderEffect);
    return sdfPixelmapShaderShapeBuilder;
}

std::shared_ptr<Drawing::RuntimeShaderBuilder> GESDFPixelmapShaderShape::GetSDFPixelmapNormalShaderShapeBuilder() const
{
    thread_local std::shared_ptr<Drawing::RuntimeShaderBuilder> sdfPixelmapShaderShapeBuilder = nullptr;
    if (sdfPixelmapShaderShapeBuilder) {
        return sdfPixelmapShaderShapeBuilder;
    }

    static constexpr char prog[] = R"(
        uniform shader pixelmapShader;
        uniform vec2 pixelmapSize;

        vec4 SafeFetch(vec2 fragCoord) {
            if (fragCoord.x < 0 || fragCoord.x >= pixelmapSize.x || fragCoord.y < 0 || fragCoord.y >= pixelmapSize.y) {
                return vec4(0.5, 0.5, 0.0, 1);
            }
            return pixelmapShader.eval(fragCoord);
        }

        uniform vec2 centerPos;
        const float N_SCALE = 2048.0;

        float EncodeDir(vec2 dir)
        {
            float xPos = floor(dir.x + N_SCALE);
            float yPos = floor(dir.y + N_SCALE);
            return xPos + (yPos / N_SCALE) / 2.0;
        }

        vec4 main(vec2 fragCoord)
        {
            // Gradient(rg channel) will be mapped from [0, 1] to [-1, 1]
            // SDF(a channel) will be mapped from [0, 1] to [-63.5, 64]
            vec4 pixelmapShape = SafeFetch(fragCoord);
            return half4(pixelmapShape.xy * 2 - 1, EncodeDir(fragCoord - centerPos), pixelmapShape.a * 127.5 - 63.5);
        }
    )";

    auto sdfPixelmapShaderBuilderEffect = Drawing::RuntimeEffect::CreateForShader(prog);
    if (!sdfPixelmapShaderBuilderEffect) {
        LOGE("GESDFPixelmapShaderShape::GetSDFPixelmapNormalShaderShapeBuilder effect error");
        return nullptr;
    }

    sdfPixelmapShaderShapeBuilder = std::make_shared<Drawing::RuntimeShaderBuilder>(sdfPixelmapShaderBuilderEffect);
    return sdfPixelmapShaderShapeBuilder;
}

std::shared_ptr<ShaderEffect> GESDFPixelmapShaderShape::GenerateSDFShaderEffect(
    std::shared_ptr<ShaderEffect> pixelmapShader, const Drawing::ImageInfo& pixelmapInfo,
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder) const
{
    if (!builder) {
        LOGE("GESDFPixelmapShaderShape::GenerateSDFShaderEffect builder error");
        return nullptr;
    }

    builder->SetChild("pixelmapShader", pixelmapShader);
    builder->SetUniform("pixelmapSize", pixelmapInfo.GetWidth(), pixelmapInfo.GetHeight());

    auto sdfPixelmapShapeShader = builder->MakeShader(nullptr, false);
    if (!sdfPixelmapShapeShader) {
        LOGE("GESDFPixelmapShaderShape::GenerateSDFShaderEffect shaderEffect error");
    }
    return sdfPixelmapShapeShader;
}

std::shared_ptr<ShaderEffect> GESDFPixelmapShaderShape::GenerateSDFNormalShaderEffect(
    std::shared_ptr<ShaderEffect> pixelmapShader, const Drawing::ImageInfo& pixelmapInfo, float width, float height,
    std::shared_ptr<Drawing::RuntimeShaderBuilder> builder) const
{
    if (!builder) {
        LOGE("GESDFPixelmapShaderShape::GenerateSDFNormalShaderEffect builder error");
        return nullptr;
    }

    builder->SetChild("pixelmapShader", pixelmapShader);
    builder->SetUniform("centerPos", width * HALF, height * HALF);
    builder->SetUniform("pixelmapSize", pixelmapInfo.GetWidth(), pixelmapInfo.GetHeight());

    auto sdfPixelmapShapeShader = builder->MakeShader(nullptr, false);
    if (!sdfPixelmapShapeShader) {
        LOGE("GESDFPixelmapShaderShape::GenerateSDFNormalShaderEffect shaderEffect error");
    }
    return sdfPixelmapShapeShader;
}

} // namespace Drawing
} // namespace Rosen
} // namespace OHOS